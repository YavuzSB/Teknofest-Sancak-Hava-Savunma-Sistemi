/**
 * @file tracking_pipeline.hpp
 * @brief Otonom Takip Hatti - IDLE -> ROI -> TRACK durum makinesi
 *
 * Hareket algilama ile baslayip HSV filtre ile hedef arama ve
 * surekli takip yapan hibrit durum makinesi.
 */
#pragma once

#include "common_defs.hpp"

#include <opencv2/opencv.hpp>
#include <functional>
#include <optional>
#include <chrono>
#include "motion_detector.hpp"

namespace sancak {

/// Durum makinesi durumlari
enum class TrackingState {
    IDLE,    // Hareket bekleniyor
    ROI,     // Ilgi bolgesinde arama yapiliyor
    TRACK    // Hedef aktif takipte
};

/// HSV araliklarini saglayan fonksiyon tipi
using HsvRangeProvider = std::function<std::pair<cv::Scalar, cv::Scalar>()>;

/// Isleme sonucu
struct TrackingResult {
    cv::Mat          frame;      // Islenmis goruntu
    std::optional<cv::Point> target; // Hedef merkezi
    TrackingState       state;      // Mevcut durum
};

/**
 * @class TrackingPipeline
 * @brief Hibrit durum makinesi ile otonom hedef takip sistemi
 *
 * IDLE: Hareket algilama ile beklemede
 * ROI:  Hareket bolgesinde renk filtreleme ile arama
 * TRACK: Bulunmus hedefi kucuk pencerede yeniden dogrulayarak takip
 */
class TrackingPipeline {
public:
    /**
     * @param hsvRangeProvider Dis config'den HSV araliklarini saglar
     * @param roiExpansion ROI genisletme piksel miktari
     * @param trackReconfirmInterval TRACK modunda yeniden dogrulama frame araligi
     */
    TrackingPipeline(HsvRangeProvider hsvRangeProvider,
                     int roiExpansion = 20,
                     int trackReconfirmInterval = 5);

    /**
     * @brief Durumu sifirlar (IDLE'a doner)
     */
    void reset();

    /**
     * @brief Bir frame isler ve hedef koordinatini dondurur
     * @param frame BGR formatinda giris frame'i
     * @param autonomous Otonom mod aktif mi?
     * @return TrackingResult Islenmis frame, hedef ve durum
     */
    TrackingResult process(const cv::Mat& frame, bool autonomous);

    /**
     * @brief Mevcut durumu dondurur
     */
    TrackingState getCurrentState() const { return state_; }

private:
    /**
     * @brief Belirtilen bolgede HSV renk filtresi ile hedef arar
     * @return Hedef merkez noktalari veya nullopt
     */
    std::optional<cv::Point> searchTarget(const cv::Mat& frame, 
                                          const cv::Rect& region);

    // Hareket algilayici
    MotionDetector motionDetector_;

    // Dis HSV saglayici
    HsvRangeProvider hsvRangeProvider_;

    // Parametreler
    int roiExpansion_;
    int trackReconfirmInterval_;

    // Durum
    TrackingState state_;
    std::optional<cv::Rect>  roi_;
    std::optional<cv::Point> lastTarget_;
    std::chrono::steady_clock::time_point roiTimestamp_;
    int frameCount_;
};

} // namespace sancak