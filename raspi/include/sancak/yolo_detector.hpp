/**
 * @file yolo_detector.hpp
 * @brief Sancak Hava Savunma Sistemi - YOLO26-Nano ONNX Algılayıcısı
 *
 * OpenCV DNN modülü üzerinden best.onnx modelini çalıştırır.
 * Giriş: BGR frame → Çıkış: Detection listesi
 *
 * @author Sancak Takımı
 * @date 2026
 */
#pragma once

#include "sancak/types.hpp"
#include "sancak/config_manager.hpp"

#include <opencv2/dnn.hpp>
#include <vector>
#include <string>

namespace sancak {

/**
 * @class YoloDetector
 * @brief YOLO26-Nano ONNX modeli ile hedef tespiti
 *
 * Akış:
 *  1. Frame'i letterbox ile model boyutuna ölçekle
 *  2. BGR→RGB, HWC→CHW, [0,1] normalize
 *  3. DNN forward pass
 *  4. Çıkış tensörünü parse et
 *  5. NMS uygula
 *  6. Orijinal frame koordinatlarına geri dönüştür
 */
class YoloDetector {
public:
    YoloDetector() = default;

    /**
     * @brief Modeli yükler ve DNN ağını hazırlar
     * @param config YOLO ayarları
     * @return Başarılı mı?
     */
    [[nodiscard]] bool initialize(const YoloConfig& config);

    /**
     * @brief Tek frame üzerinde çıkarım yapar
     * @param frame Giriş BGR frame
     * @return Tespit listesi
     */
    [[nodiscard]] std::vector<Detection> detect(const cv::Mat& frame);

    /// Son çıkarım süresi (ms)
    [[nodiscard]] double lastInferenceMs() const { return last_inference_ms_; }

    /// Model yüklü mü?
    [[nodiscard]] bool isReady() const { return ready_; }

private:
    /// Letterbox ön-işleme (aspect ratio korunarak boyutlandırma)
    static cv::Mat letterbox(const cv::Mat& src, cv::Point2f& scale, cv::Point2f& pad);

    /// Model çıkış tensörünü parse eder
    [[nodiscard]] std::vector<Detection> parseOutput(const cv::Mat& output,
                                        const cv::Point2f& scale,
                                        const cv::Point2f& pad,
                                        int orig_w, int orig_h);

    /// TargetClass eşleştirmesi
    [[nodiscard]] TargetClass mapClassId(int class_id) const;

    cv::dnn::Net net_;
    YoloConfig   config_;
    bool         ready_ = false;
    double       last_inference_ms_ = 0.0;
};

} // namespace sancak
