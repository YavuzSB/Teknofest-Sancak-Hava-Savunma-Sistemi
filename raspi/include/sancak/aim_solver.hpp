/**
 * @file aim_solver.hpp
 * @brief Sancak Hava Savunma Sistemi - Nihai Nişan Noktası Hesaplayıcı
 *
 * Balon merkezi + balistik düzeltmeleri birleştirerek
 * son nişan koordinatını üretir.
 *
 * @author Sancak Takımı
 * @date 2026
 */
#pragma once

#include "sancak/types.hpp"
#include "sancak/ballistics_manager.hpp"
#include "sancak/distance_estimator.hpp"

namespace sancak {

/**
 * @class AimSolver
 * @brief Tüm düzeltmeleri birleştirerek final nişan noktası çıkarır
 *
 * Sıralama:
 *  1. Balon merkezini al
 *  2. Mesafe tahmin et
 *  3. Balistik düzeltmeleri hesapla
 *  4. Düzeltmeleri uygula
 *  5. Frame sınırlarını kontrol et
 */
class AimSolver {
public:
    AimSolver() = default;

    /**
     * @brief Modülleri başlat
     */
    void initialize(const BallisticsConfig& balConfig,
                    const DistanceConfig& distConfig);

    /**
     * @brief Bir tespit+balon sonucundan nişan noktası hesapla
     *
     * @param balloon    Balon segmentasyon sonucu
     * @param detection  YOLO tespiti
     * @param velocity   Hedef hız vektörü (px/frame)
     * @param fps        Mevcut FPS
     * @param frame_size Frame boyutu
     * @return AimPoint  Hesaplanmış nişan noktası
     */
    [[nodiscard]] AimPoint solve(const BalloonResult& balloon,
                   const Detection& detection,
                   const cv::Point2f& velocity,
                   double fps,
                   const cv::Size& frame_size) const;

    /// Balistik yönetici erişimi (runtime ayar için)
    BallisticsManager& ballistics() { return ballistics_; }
    const BallisticsManager& ballistics() const { return ballistics_; }

    /// Mesafe tahmincisi erişimi
    const DistanceEstimator& distance() const { return distance_; }

private:
    /// Noktayı frame sınırları içinde tut
    static cv::Point2f clampToFrame(const cv::Point2f& pt,
                              const cv::Size& frame_size);

    BallisticsManager ballistics_;
    DistanceEstimator distance_;
};

} // namespace sancak
