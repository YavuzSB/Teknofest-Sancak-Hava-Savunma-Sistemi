/**
 * @file distance_estimator.cpp
 * @brief Mesafe Tahmini - Implementasyon
 *
 * Pinhole kamera modeli: D = (Gerçek_Boyut × Odak_Uzaklığı) / Piksel_Boyutu
 */
#include "sancak/distance_estimator.hpp"
#include "sancak/logger.hpp"

#include <algorithm>
#include <cmath>

namespace sancak {

void DistanceEstimator::initialize(const DistanceConfig& config) {
    config_ = config;
    SANCAK_LOG_INFO("Distance Estimator başlatıldı | Balon çapı: {:.3f}m | Odak: {:.1f}px",
                    config_.known_balloon_diameter_m, config_.focal_length_px);
}

DistanceEstimate DistanceEstimator::fromBalloonRadius(float balloon_radius_px) const {
    DistanceEstimate est;

    if (balloon_radius_px <= 0.0f) return est;

    float diameter_px = balloon_radius_px * 2.0f;

    // D = (gerçek_çap × odak) / piksel_çap
    est.distance_m = (config_.known_balloon_diameter_m * config_.focal_length_px) / diameter_px;

    // Sınırla
    est.distance_m = std::clamp(est.distance_m, config_.min_distance_m, config_.max_distance_m);

    // Güven: yarıçap ne kadar büyükse o kadar güvenilir
    est.confidence = std::min(1.0f, balloon_radius_px / 50.0f);

    return est;
}

DistanceEstimate DistanceEstimator::fromBboxHeight(float bbox_height_px,
                                                     float real_height_m) const {
    DistanceEstimate est;

    if (bbox_height_px <= 0.0f || real_height_m <= 0.0f) return est;

    est.distance_m = (real_height_m * config_.focal_length_px) / bbox_height_px;
    est.distance_m = std::clamp(est.distance_m, config_.min_distance_m, config_.max_distance_m);
    est.confidence = std::min(1.0f, bbox_height_px / 100.0f);

    return est;
}

DistanceEstimate DistanceEstimator::combined(float balloon_radius_px,
                                               float bbox_height_px,
                                               float real_height_m) const {
    auto e1 = fromBalloonRadius(balloon_radius_px);
    auto e2 = fromBboxHeight(bbox_height_px, real_height_m);

    DistanceEstimate combined;

    if (e1.confidence > 0 && e2.confidence > 0) {
        // Ağırlıklı ortalama (güven bazlı)
        float total = e1.confidence + e2.confidence;
        combined.distance_m = (e1.distance_m * e1.confidence +
                               e2.distance_m * e2.confidence) / total;
        combined.confidence = (e1.confidence + e2.confidence) / 2.0f;
    } else if (e1.confidence > 0) {
        combined = e1;
    } else {
        combined = e2;
    }

    return combined;
}

} // namespace sancak
