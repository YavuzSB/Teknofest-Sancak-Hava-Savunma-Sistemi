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

#include <vector>
#include <string>
#include <memory>
#include <atomic>
#include <shared_mutex>

#if defined(SANCAK_USE_ONNXRUNTIME)
// ONNX Runtime C++ API
#include <onnxruntime_cxx_api.h>
#endif

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
    [[nodiscard]] double lastInferenceMs() const { return last_inference_ms_.load(std::memory_order_relaxed); }

    /// Model yüklü mü?
    [[nodiscard]] bool isReady() const { return ready_.load(std::memory_order_acquire); }

private:
    /// Letterbox ön-işleme (aspect ratio korunarak boyutlandırma)
    [[nodiscard]] cv::Mat letterbox(const cv::Mat& src, cv::Point2f& scale, cv::Point2f& pad) const;

#if defined(SANCAK_USE_ONNXRUNTIME)
    /// OpenCV cv::Mat (BGR) -> float32 [1,3,H,W] NCHW buffer
    [[nodiscard]] std::vector<float> makeInputTensorNchw(const cv::Mat& letterboxed_bgr) const;

    /// Ort::Session input/output isimlerini cache'ler
    void cacheIoNames();
#endif

    /// Model çıkış tensörünü parse eder
    [[nodiscard]] std::vector<Detection> parseOutput(const cv::Mat& output,
                                        const cv::Point2f& scale,
                                        const cv::Point2f& pad,
                                        int orig_w, int orig_h);

    /// TargetClass eşleştirmesi
    [[nodiscard]] TargetClass mapClassId(int class_id) const;

    YoloConfig   config_;
    std::atomic<bool>   ready_{false};
    std::atomic<double> last_inference_ms_{0.0};

    // initialize() ile detect() aynı anda çağrılırsa data race olmaması için.
    // detect() tarafında shared lock → paralel infer mümkün.
    mutable std::shared_mutex session_mutex_;

#if defined(SANCAK_USE_ONNXRUNTIME)
    // Ort objeleri üye olarak tutulmalı (performans + tekrar kullanım)
    std::unique_ptr<Ort::Env> env_;
    std::unique_ptr<Ort::Session> session_;
    std::unique_ptr<Ort::SessionOptions> session_options_;
    std::string input_name_;
    std::string output_name_;
#else
    // Geri uyumluluk: ONNX Runtime yoksa OpenCV DNN ile derlenebilir.
    // (CMake'de SANCAK_USE_ONNXRUNTIME=ON önerilir.)
    cv::dnn::Net net_;
#endif
};

} // namespace sancak
