/**
 * @file yolo_detector.cpp
 * @brief YOLO26-Nano ONNX Algılayıcısı - Implementasyon
 *
 * OpenCV DNN ile .onnx modelini yükler ve inference yapar.
 * YOLO26 çıkış formatı: [1, (4 + num_classes), num_predictions]
 */
#include "sancak/yolo_detector.hpp"
#include "sancak/logger.hpp"

#include <opencv2/dnn.hpp>
#include <opencv2/imgproc.hpp>
#include <algorithm>
#include <numeric>

namespace sancak {

bool YoloDetector::initialize(const YoloConfig& config) {
    config_ = config;

    try {
        net_ = cv::dnn::readNetFromONNX(config_.model_path);
    } catch (const cv::Exception& e) {
        SANCAK_LOG_FATAL("ONNX model yüklenemedi: {} | Hata: {}", config_.model_path, e.what());
        return false;
    }

    // Backend seçimi
    if (config_.use_cuda) {
        net_.setPreferableBackend(cv::dnn::DNN_BACKEND_CUDA);
        net_.setPreferableTarget(cv::dnn::DNN_TARGET_CUDA);
        SANCAK_LOG_INFO("YOLO backend: CUDA");
    } else {
        net_.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
        net_.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);
        SANCAK_LOG_INFO("YOLO backend: CPU (OpenCV)");
    }

    ready_ = true;
    SANCAK_LOG_INFO("YOLO26-Nano modeli yüklendi: {} | Giriş: {}x{} | Sınıf sayısı: {}",
                    config_.model_path, config_.input_size, config_.input_size,
                    config_.class_names.size());
    return true;
}

cv::Mat YoloDetector::letterbox(const cv::Mat& src, cv::Point2f& scale, cv::Point2f& pad) {
    int  in_w = src.cols;
    int  in_h = src.rows;
    int  tgt  = config_.input_size;

    float r = std::min(static_cast<float>(tgt) / static_cast<float>(in_w),
                       static_cast<float>(tgt) / static_cast<float>(in_h));

    int new_w = static_cast<int>(std::round(static_cast<float>(in_w) * r));
    int new_h = static_cast<int>(std::round(static_cast<float>(in_h) * r));

    float dw = static_cast<float>(tgt - new_w) / 2.0F;
    float dh = static_cast<float>(tgt - new_h) / 2.0F;

    cv::Mat resized;
    cv::resize(src, resized, cv::Size(new_w, new_h), 0, 0, cv::INTER_LINEAR);

    cv::Mat out(tgt, tgt, CV_8UC3, cv::Scalar(114, 114, 114));
    resized.copyTo(out(cv::Rect(static_cast<int>(dw), static_cast<int>(dh), new_w, new_h)));

    scale = cv::Point2f(r, r);
    pad   = cv::Point2f(dw, dh);

    return out;
}

std::vector<Detection> YoloDetector::detect(const cv::Mat& frame) {
    if (!ready_ || frame.empty()) { return {}; }

    auto t0 = SteadyClock::now();

    // 1. Letterbox ön-işleme
    cv::Point2f scale, pad;
    cv::Mat letterboxed = letterbox(frame, scale, pad);

    // 2. Blob oluştur (BGR→RGB, normalize 0-1, CHW)
    cv::Mat blob = cv::dnn::blobFromImage(letterboxed, 1.0 / 255.0,
                                           cv::Size(config_.input_size, config_.input_size),
                                           cv::Scalar(0, 0, 0), true, false, CV_32F);

    // 3. Forward pass
    net_.setInput(blob);
    std::vector<cv::Mat> outputs;
    net_.forward(outputs, net_.getUnconnectedOutLayersNames());

    auto t1 = SteadyClock::now();
    last_inference_ms_ = std::chrono::duration<double, std::milli>(t1 - t0).count();

    // 4. Parse + NMS
    if (outputs.empty()) { return {}; }

    return parseOutput(outputs[0], scale, pad, frame.cols, frame.rows);
}

std::vector<Detection> YoloDetector::parseOutput(const cv::Mat& output,
                                                   const cv::Point2f& scale,
                                                   const cv::Point2f& pad,
                                                   int orig_w, int orig_h) {
    // YOLO26 çıkış: [1, (4 + num_classes), num_predictions]
    // Transpose → [num_predictions, (4 + num_classes)]
    const int num_classes = static_cast<int>(config_.class_names.size());

    // output.size: [1, 4 + num_classes, N]
    // Reshape and transpose
    cv::Mat det = output.reshape(1, output.size[1]);  // [4+nc, N]
    cv::transpose(det, det);  // [N, 4+nc]

    const int num_preds = det.rows;
    const int feat_dim  = det.cols;

    if (feat_dim < 4 + num_classes) {
        SANCAK_LOG_WARN("Model çıkış boyutu beklenenden küçük: {} < {}",
                        feat_dim, 4 + num_classes);
        return {};
    }

    // NMS için geçici listeler
    std::vector<cv::Rect>  boxes_nms;
    std::vector<float>     scores_nms;
    std::vector<int>       class_ids_nms;

    for (int i = 0; i < num_preds; ++i) {
        const float* row = det.ptr<float>(i);

        // Sınıf skorları → en yüksek skoru bul
        float max_score = 0.0f;
        int   max_cls   = 0;
        for (int c = 0; c < num_classes; ++c) {
            float s = row[4 + c];
            if (s > max_score) {
                max_score = s;
                max_cls   = c;
            }
        }

        if (max_score < config_.conf_threshold) { continue; }

        // cx, cy, w, h → x1, y1, x2, y2 (letterbox koordinatları)
        float cx = row[0];
        float cy = row[1];
        float w  = row[2];
        float h  = row[3];

        float x1 = cx - w / 2.0F;
        float y1 = cy - h / 2.0F;

        // Letterbox → orijinal frame koordinatlarına
        x1 = (x1 - pad.x) / scale.x;
        y1 = (y1 - pad.y) / scale.y;
        w  = w / scale.x;
        h  = h / scale.y;

        // Sınır kontrolü
        x1 = std::max(0.0F, x1);
        y1 = std::max(0.0F, y1);
        if (x1 + w > static_cast<float>(orig_w)) w = static_cast<float>(orig_w) - x1;
        if (y1 + h > static_cast<float>(orig_h)) h = static_cast<float>(orig_h) - y1;

        boxes_nms.emplace_back(static_cast<int>(x1), static_cast<int>(y1),
                               static_cast<int>(w),  static_cast<int>(h));
        scores_nms.push_back(max_score);
        class_ids_nms.push_back(max_cls);
    }

    // 5. NMS uygula
    std::vector<int> indices;
    cv::dnn::NMSBoxes(boxes_nms, scores_nms, config_.conf_threshold,
                       config_.nms_threshold, indices);

    // 6. Sonuçları Detection struct'larına dönüştür
    std::vector<Detection> results;
    results.reserve(indices.size());

    for (int idx : indices) {
        Detection d;
        d.bbox = cv::Rect2f(
            static_cast<float>(boxes_nms[idx].x),
            static_cast<float>(boxes_nms[idx].y),
            static_cast<float>(boxes_nms[idx].width),
            static_cast<float>(boxes_nms[idx].height)
        );
        d.confidence   = scores_nms[idx];
        d.class_id     = class_ids_nms[idx];
        d.target_class = mapClassId(class_ids_nms[idx]);
        results.push_back(d);
    }

    SANCAK_LOG_TRACE("YOLO: {} tespit ({:.1f} ms)", results.size(), last_inference_ms_);
    return results;
}

TargetClass YoloDetector::mapClassId(int class_id) const {
    if (class_id < 0 || class_id >= static_cast<int>(config_.class_names.size())) {
        return TargetClass::kUnknown;
    }

    const std::string& name = config_.class_names[class_id];

    if (name == "drone")      return TargetClass::kDrone;
    if (name == "plane")      return TargetClass::kPlane;
    if (name == "helicopter") return TargetClass::kHelicopter;
    if (name == "jet")        return TargetClass::kJet;
    if (name == "rocket")     return TargetClass::kRocket;
    if (name == "friendly")   return TargetClass::kFriendly;

    // Bilinmeyen sınıfları düşman olarak kabul et (güvenli taraf)
    // Ancak "friendly" veya "dost" içeriyorsa dost say
    if (name.find("friend") != std::string::npos ||
        name.find("dost")   != std::string::npos) {
        return TargetClass::kFriendly;
    }

    return TargetClass::kUnknown;
}

} // namespace sancak
