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
#include <array>
#include <cstring>

#if defined(SANCAK_USE_ONNXRUNTIME)
#include <onnxruntime_c_api.h>

// CUDA/XNNPACK gibi provider factory fonksiyonları bu header'da bulunur.
#if __has_include(<onnxruntime_provider_factory.h>)
#include <onnxruntime_provider_factory.h>
#define SANCAK_HAS_ORT_PROVIDER_FACTORY 1
#else
#define SANCAK_HAS_ORT_PROVIDER_FACTORY 0
#endif
#endif

namespace sancak {

bool YoloDetector::initialize(const YoloConfig& config) {
    std::unique_lock<std::shared_mutex> lock(session_mutex_);
    config_ = config;

#if defined(SANCAK_USE_ONNXRUNTIME)
    try {
        if (!env_) {
            env_ = std::make_unique<Ort::Env>(ORT_LOGGING_LEVEL_WARNING, "sancak");
        }
        session_options_ = std::make_unique<Ort::SessionOptions>();

        // Performans odaklı ayarlar
        session_options_->SetIntraOpNumThreads(std::max(1, config_.num_threads));
        session_options_->SetInterOpNumThreads(1);
        session_options_->SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
        session_options_->SetExecutionMode(ExecutionMode::ORT_SEQUENTIAL);
        session_options_->EnableCpuMemArena();
        session_options_->EnableMemPattern();

        // Execution Providers
        // Varsayılan: CPU EP. Ayrıca mümkünse XNNPACK ve (opsiyonel) CUDA EP denenir.
#if SANCAK_HAS_ORT_PROVIDER_FACTORY
        // XNNPACK (Pi/ARM için performans)
        try {
            // İmza: OrtSessionOptionsAppendExecutionProvider_XNNPACK(OrtSessionOptions*, int intra_op_num_threads)
            Ort::ThrowOnError(OrtSessionOptionsAppendExecutionProvider_XNNPACK(
                session_options_->Get(), std::max(1, config_.num_threads)));
            SANCAK_LOG_INFO("YOLO EP: XNNPACK (enabled)");
        } catch (const Ort::Exception& e) {
            SANCAK_LOG_INFO("YOLO EP: XNNPACK eklenemedi (CPU ile devam): {}", e.what());
        }

        // CUDA (opsiyonel)
        if (config_.use_cuda) {
            try {
                // İmza: OrtSessionOptionsAppendExecutionProvider_CUDA(OrtSessionOptions*, int device_id)
                Ort::ThrowOnError(OrtSessionOptionsAppendExecutionProvider_CUDA(session_options_->Get(), 0));
                SANCAK_LOG_WARN("YOLO EP: CUDA (enabled)");
            } catch (const Ort::Exception& e) {
                SANCAK_LOG_WARN("use_cuda=ON ama CUDA EP eklenemedi (CPU ile devam): {}", e.what());
            }
        }
#else
        if (config_.use_cuda) {
            SANCAK_LOG_WARN("use_cuda=ON ama onnxruntime_provider_factory.h bulunamadı; CUDA EP compile-time devre dışı (CPU ile devam)");
        }
#endif

        // CPU Execution Provider (explicit). use_arena=1 → arena optimizasyonları.
        Ort::ThrowOnError(OrtSessionOptionsAppendExecutionProvider_CPU(session_options_->Get(), 1));

        session_ = std::make_unique<Ort::Session>(*env_, config_.model_path.c_str(), *session_options_);
        cacheIoNames();

        ready_.store(true, std::memory_order_release);
        SANCAK_LOG_INFO("YOLO26-Nano (ONNX Runtime) yüklendi: {} | Giriş: {}x{} | Sınıf: {}",
                        config_.model_path, config_.input_size, config_.input_size,
                        config_.class_names.size());
        return true;
    } catch (const Ort::Exception& e) {
        SANCAK_LOG_FATAL("ONNX Runtime session oluşturulamadı: {} | Hata: {}", config_.model_path, e.what());
        ready_.store(false, std::memory_order_release);
        session_.reset();
        session_options_.reset();
        return false;
    }
#else
    try {
        net_ = cv::dnn::readNetFromONNX(config_.model_path);
    } catch (const cv::Exception& e) {
        SANCAK_LOG_FATAL("ONNX model yüklenemedi: {} | Hata: {}", config_.model_path, e.what());
        return false;
    }

    if (config_.use_cuda) {
        net_.setPreferableBackend(cv::dnn::DNN_BACKEND_CUDA);
        net_.setPreferableTarget(cv::dnn::DNN_TARGET_CUDA);
        SANCAK_LOG_INFO("YOLO backend: CUDA (OpenCV DNN)");
    } else {
        net_.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
        net_.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);
        SANCAK_LOG_INFO("YOLO backend: CPU (OpenCV DNN)");
    }

    ready_.store(true, std::memory_order_release);
    SANCAK_LOG_INFO("YOLO26-Nano (OpenCV DNN) yüklendi: {} | Giriş: {}x{} | Sınıf sayısı: {}",
                    config_.model_path, config_.input_size, config_.input_size,
                    config_.class_names.size());
    return true;
#endif
}

cv::Mat YoloDetector::letterbox(const cv::Mat& src, cv::Point2f& scale, cv::Point2f& pad) const {
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

#if defined(SANCAK_USE_ONNXRUNTIME)

void YoloDetector::cacheIoNames() {
    if (!session_) return;

    Ort::AllocatorWithDefaultOptions allocator;

    // Tek input/tek output varsayımı (mevcut model için)
    {
        auto name = session_->GetInputNameAllocated(0, allocator);
        input_name_ = name.get();
    }
    {
        auto name = session_->GetOutputNameAllocated(0, allocator);
        output_name_ = name.get();
    }
}

std::vector<float> YoloDetector::makeInputTensorNchw(const cv::Mat& letterboxed_bgr) const {
    // Beklenen: CV_8UC3 BGR
    const int H = config_.input_size;
    const int W = config_.input_size;
    const int stride_hw = H * W;
    std::vector<float> nchw(3 * stride_hw);

    // Tek geçişte: resize + BGR->RGB + normalize + NCHW
    // (cv::Mat yerine doğrudan buffer'a yaz)
    for (int y = 0; y < H; ++y) {
        for (int x = 0; x < W; ++x) {
            // Kaynak pikseli orantılı olarak al (bilinear interpolation)
            float src_y = static_cast<float>(y) * letterboxed_bgr.rows / static_cast<float>(H);
            float src_x = static_cast<float>(x) * letterboxed_bgr.cols / static_cast<float>(W);
            int y0 = static_cast<int>(src_y);
            int x0 = static_cast<int>(src_x);
            y0 = std::min(y0, letterboxed_bgr.rows - 1);
            x0 = std::min(x0, letterboxed_bgr.cols - 1);
            const cv::Vec3b& bgr = letterboxed_bgr.at<cv::Vec3b>(y0, x0);
            // BGR -> RGB ve normalize [0,1]
            float r = static_cast<float>(bgr[2]) / 255.0f;
            float g = static_cast<float>(bgr[1]) / 255.0f;
            float b = static_cast<float>(bgr[0]) / 255.0f;
            int hw = y * W + x;
            nchw[0 * stride_hw + hw] = r;
            nchw[1 * stride_hw + hw] = g;
            nchw[2 * stride_hw + hw] = b;
        }
    }
    return nchw;
}

#endif

std::vector<Detection> YoloDetector::detect(const cv::Mat& frame) {
    if (!ready_.load(std::memory_order_acquire) || frame.empty()) { return {}; }

    // initialize() ile yarışmayı engelle (detect paralel olabilir)
    std::shared_lock<std::shared_mutex> lock(session_mutex_);

    auto t0 = SteadyClock::now();

    // 1. Letterbox ön-işleme
    cv::Point2f scale, pad;
    // Letterbox fonksiyonu zaten yeni mat döndürüyor, gereksiz kopya yok.
    cv::Mat letterboxed = letterbox(frame, scale, pad);

#if defined(SANCAK_USE_ONNXRUNTIME)
    if (!session_) { return {}; }

    // 2. Input tensor (float32 NCHW)
    std::vector<float> input_nchw = makeInputTensorNchw(letterboxed);
    const int H = config_.input_size;
    const int W = config_.input_size;
    std::array<int64_t, 4> input_shape{1, 3, H, W};

    Ort::MemoryInfo mem_info = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
    Ort::Value input_tensor = Ort::Value::CreateTensor<float>(
        mem_info,
        input_nchw.data(),
        input_nchw.size(),
        input_shape.data(),
        input_shape.size());

    const char* input_names[] = { input_name_.c_str() };
    const char* output_names[] = { output_name_.c_str() };

    Ort::RunOptions run_opts;
    auto outputs = session_->Run(run_opts, input_names, &input_tensor, 1, output_names, 1);

    auto t1 = SteadyClock::now();
    last_inference_ms_.store(std::chrono::duration<double, std::milli>(t1 - t0).count(), std::memory_order_relaxed);

    if (outputs.empty() || !outputs[0].IsTensor()) { return {}; }
    Ort::Value& out = outputs[0];
    const float* out_data = out.GetTensorData<float>();
    auto shape = out.GetTensorTypeAndShapeInfo().GetShape();
    if (shape.size() != 3 || shape[0] != 1) {
        SANCAK_LOG_WARN("YOLO output shape beklenmiyor (dims={}): [{}]", shape.size(),
                        [&]() {
                            std::string s; s.reserve(64);
                            for (size_t i = 0; i < shape.size(); ++i) {
                                s += std::to_string(shape[i]);
                                if (i + 1 < shape.size()) s += ",";
                            }
                            return s;
                        }());
        return {};
    }

    // Beklenen: [1, (4+nc), N]  (OpenCV DNN yoluyla aynı)
    // Bazı exportlarda [1, N, (4+nc)] gelebilir; onu da destekle.
    const int num_classes = static_cast<int>(config_.class_names.size());
    const int expected_feat = 4 + num_classes;

    int64_t dim1 = shape[1];
    int64_t dim2 = shape[2];
    int feat_dim = 0;
    int num_preds = 0;
    bool transposed = false;
    if (dim1 >= expected_feat && dim1 <= expected_feat + 1) {
        feat_dim = static_cast<int>(dim1);
        num_preds = static_cast<int>(dim2);
        transposed = false;
    } else if (dim2 >= expected_feat && dim2 <= expected_feat + 1) {
        feat_dim = static_cast<int>(dim2);
        num_preds = static_cast<int>(dim1);
        transposed = true;
    } else {
        // Yine de ilerleyelim: en az 4+nc olacak şekilde birini feat seç
        if (dim1 > dim2) {
            feat_dim = static_cast<int>(dim1);
            num_preds = static_cast<int>(dim2);
            transposed = false;
        } else {
            feat_dim = static_cast<int>(dim2);
            num_preds = static_cast<int>(dim1);
            transposed = true;
        }
    }

    if (feat_dim < expected_feat) {
        SANCAK_LOG_WARN("Model çıkış feature dim küçük: {} < {}", feat_dim, expected_feat);
        return {};
    }

    // out_data -> cv::Mat adaptörü (kopyasız) ve mevcut parseOutput'u kullan
    // parseOutput bekliyor: cv::Mat output, size=[1, feat, N]
    int sizes[3] = {1, feat_dim, num_preds};
    cv::Mat outMat(3, sizes, CV_32F);
    if (!transposed) {
        std::memcpy(outMat.data, out_data, static_cast<size_t>(feat_dim) * static_cast<size_t>(num_preds) * sizeof(float));
    } else {
        // [1, N, feat] -> [1, feat, N]
        // index: src[n][i][j] -> dst[n][j][i]
        float* dst = reinterpret_cast<float*>(outMat.data);
        for (int i = 0; i < num_preds; ++i) {
            for (int j = 0; j < feat_dim; ++j) {
                dst[j * num_preds + i] = out_data[i * feat_dim + j];
            }
        }
    }

    return parseOutput(outMat, scale, pad, frame.cols, frame.rows);

#else
    // OpenCV DNN yolu thread-safe değildir → serialize
    lock.unlock();
    std::unique_lock<std::shared_mutex> uniqueLock(session_mutex_);

    // OpenCV DNN yolu
    cv::Mat blob = cv::dnn::blobFromImage(letterboxed, 1.0 / 255.0,
                                           cv::Size(config_.input_size, config_.input_size),
                                           cv::Scalar(0, 0, 0), true, false, CV_32F);

    net_.setInput(blob);
    std::vector<cv::Mat> outputs;
    net_.forward(outputs, net_.getUnconnectedOutLayersNames());

    auto t1 = SteadyClock::now();
    last_inference_ms_.store(std::chrono::duration<double, std::milli>(t1 - t0).count(), std::memory_order_relaxed);

    if (outputs.empty()) { return {}; }
    return parseOutput(outputs[0], scale, pad, frame.cols, frame.rows);
#endif
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
