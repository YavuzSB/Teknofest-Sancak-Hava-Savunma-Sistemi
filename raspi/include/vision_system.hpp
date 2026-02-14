/**
 * @file vision_system.hpp
 * @brief Hibrit Durum Makinesi ile Balon (Renk Hedefi) Tespit - Header
 *
 * DetectionPipeline: IDLE -> ROI -> TRACK durum makinesi.
 *
 * Durumlar:
 *   IDLE  : Hareket algılanana kadar düşük işlem.
 *   ROI   : Hareket yakalanan bölgede HSV filtre ile arama.
 *   TRACK : Hedef bulunduysa takip; kaybolursa ROI -> IDLE geri dönüş.
 *
 * Algoritma:
 *   1. Frame farkı ile basit hareket algılama.
 *   2. Otonom değilse ham frame döner.
 *   3. Otonom ise durum makinesi yürütülür.
 */
#pragma once

#include "common_defs.hpp"

#include <opencv2/opencv.hpp>
#include <functional>
#include <optional>
#include <tuple>
#include <string>
#include <chrono>

/**
 * @brief HSV alt/üst sınırlarını döndüren callable tipi.
 *        İlk: lower bound, İkinci: upper bound.
 */
using HsvRangeProvider = std::function<std::pair<cv::Scalar, cv::Scalar>()>;

/// Durum makinesi durumları
enum class PipelineState {
    IDLE,
    ROI,
    TRACK
};

/// İşleme sonucu: (işlenmiş frame, hedef merkezi [opsiyonel])
struct ProcessResult {
    cv::Mat frame;
    std::optional<cv::Point> target;
};

/**
 * @class DetectionPipeline
 * @brief Hibrit durum makinesi ile balon tespit hattı.
 */
class DetectionPipeline {
public:
    /**
     * @param hsvRangeProvider  Dış config'den HSV aralıklarını sağlayan callable.
     * @param idleMotionThreshold  Hareket eşik değeri (0-255).
     * @param minMotionArea  Minimum hareket kontur alanı.
     * @param roiExpansion  ROI genişletme piksel miktarı.
     * @param trackReconfirmInterval  TRACK modunda yeniden doğrulama frame aralığı.
     */
    DetectionPipeline(HsvRangeProvider hsvRangeProvider,
                      int idleMotionThreshold = 25,
                      int minMotionArea = 300,
                      int roiExpansion = 20,
                      int trackReconfirmInterval = 5);

    /**
     * @brief Durumu sıfırlar (IDLE'a döner).
     */
    void reset();

    /**
     * @brief Bir frame işler ve hedef koordinatını döndürür.
     * @param frame  BGR formatında giriş frame'i.
     * @param autonomous  Otonom mod aktif mi?
     * @return ProcessResult  İşlenmiş frame ve opsiyonel hedef merkezi.
     */
    ProcessResult process(const cv::Mat& frame, bool autonomous);

private:
    /**
     * @brief Frame farkı ile hareket bölgesi tespit eder.
     * @return Hareket bounding rect veya nullopt.
     */
    std::optional<cv::Rect> motionDetect(const cv::Mat& gray);

    /**
     * @brief Belirtilen bölgede HSV renk filtresi ile hedef arar.
     * @return Hedef merkez noktası veya nullopt.
     */
    std::optional<cv::Point> hsvSearch(const cv::Mat& frame, const cv::Rect& region);

    // Dış HSV sağlayıcı
    HsvRangeProvider hsvRangeProvider_;

    // Parametreler
    int idleMotionThreshold_;
    int minMotionArea_;
    int roiExpansion_;
    int trackReconfirmInterval_;

    // Durum
    PipelineState state_;
    cv::Mat lastFrameGray_;
    std::optional<cv::Rect> roi_;
    std::optional<cv::Point> lastTarget_;
    std::chrono::steady_clock::time_point roiTimestamp_;
    int frameCount_;
};
