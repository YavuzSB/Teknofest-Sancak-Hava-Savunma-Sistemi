/**
 * @file distance_estimator.hpp
 * @brief Sancak Hava Savunma Sistemi - Mesafe Tahmini
 *
 * YOLO bounding box boyutundan ve/veya balon yarıçapından
 * hedefin mesafesini tahmin eder.
 *
 * Formül: Mesafe = (Gerçek Boyut × Odak Uzaklığı) / Piksel Boyutu
 *
 * @author Sancak Takımı
 * @date 2026
 */
#pragma once

#include "sancak/types.hpp"
#include "sancak/config_manager.hpp"

namespace sancak {

/**
 * @class DistanceEstimator
 * @brief Pinhole kamera modeli ile mesafe tahmini
 */
class DistanceEstimator {
public:
    DistanceEstimator() = default;

    /**
     * @brief Konfigürasyon ile başlat
     */
    void initialize(const DistanceConfig& config);

    /**
     * @brief Balon yarıçapından mesafe tahmin et
     * @param balloon_radius_px Balonun piksel yarıçapı
     * @return Mesafe tahmini
     */
    [[nodiscard]] DistanceEstimate fromBalloonRadius(float balloon_radius_px) const;

    /**
     * @brief Bounding box yüksekliğinden mesafe tahmin et
     * @param bbox_height_px  Bounding box yüksekliği (piksel)
     * @param real_height_m   Hedefin gerçek yüksekliği (metre)
     * @return Mesafe tahmini
     */
    [[nodiscard]] DistanceEstimate fromBboxHeight(float bbox_height_px,
                                     float real_height_m) const;

    /**
     * @brief Her iki yöntemden ortalama mesafe
     */
    [[nodiscard]] DistanceEstimate combined(float balloon_radius_px,
                               float bbox_height_px,
                               float real_height_m) const;

private:
    DistanceConfig config_;
};

} // namespace sancak
