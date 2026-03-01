/**
 * @file aim_solver.cpp
 * @brief Nihai Nişan Noktası Hesaplayıcı - Implementasyon
 *
 * Balon merkezi + tüm balistik düzeltmeleri birleştirerek
 * frame üzerinde son nişan koordinatını üretir.
 */
#include "sancak/aim_solver.hpp"
#include "sancak/logger.hpp"

#include <algorithm>
#include <cmath>

namespace sancak {

namespace {

float adjustLidarRangeToTurretCenter(float lidar_range_m, const cv::Point3f& lidar_offset_m) {
    if (lidar_range_m <= 0.0f) return 0.0f;

    const float x = lidar_offset_m.x;
    const float y = lidar_offset_m.y;
    const float z = lidar_offset_m.z + lidar_range_m;
    return std::sqrt(x * x + y * y + z * z);
}

} // namespace

void AimSolver::initialize(const BallisticsConfig& balConfig,
                            const DistanceConfig& distConfig) {
    ballistics_.initialize(balConfig);
    distance_.initialize(distConfig);
    SANCAK_LOG_INFO("Aim Solver başlatıldı");
}

AimPoint AimSolver::solve(const BalloonResult& balloon,
                           const Detection& detection,
                           const cv::Point2f& velocity,
                           double fps,
                           const cv::Size& frame_size,
                           double inference_delay_ms,
                           std::optional<float> lidar_distance_m,
                           const cv::Point3f& lidar_offset_m) const {
    AimPoint aim;

    if (!balloon.found) return aim;

    aim.raw_center = balloon.center;

    // 1. Mesafe tahmini (balondan + bbox'tan birleşik)
    auto dist = distance_.combined(
        balloon.radius,
        detection.bbox.height,
        0.25F  // ortalama hedef yüksekliği ~25cm
    );
    aim.distance_m = dist.distance_m;

    if (lidar_distance_m.has_value() && lidar_distance_m.value() > 0.0f) {
        const float corrected = adjustLidarRangeToTurretCenter(lidar_distance_m.value(), lidar_offset_m);
        if (corrected > 0.0f) {
            aim.distance_m = corrected;
        }
    }

    // 2. Balistik düzeltme hesapla (inference gecikmesi dahil)
    double t_processing_s = inference_delay_ms * 0.001;
    aim.correction = ballistics_.calculate(aim.distance_m, velocity, fps, t_processing_s);

    // 3. Düzeltmeleri uygula
    aim.corrected.x = balloon.center.x + aim.correction.dx_px;
    aim.corrected.y = balloon.center.y + aim.correction.dy_px;

    // 4. Frame sınırlarına kırp
    aim.corrected = clampToFrame(aim.corrected, frame_size);

    aim.valid = true;

    SANCAK_LOG_TRACE("Aim: raw=({:.0f},{:.0f}) → corr=({:.0f},{:.0f}) | "
                     "dist={:.1f}m | dx={:.1f} dy={:.1f}",
                     aim.raw_center.x, aim.raw_center.y,
                     aim.corrected.x, aim.corrected.y,
                     aim.distance_m,
                     aim.correction.dx_px, aim.correction.dy_px);

    return aim;
}

cv::Point2f AimSolver::clampToFrame(const cv::Point2f& pt,
                                      const cv::Size& frame_size) {
    return cv::Point2f(
        std::clamp(pt.x, 0.0F, static_cast<float>(frame_size.width  - 1)),
        std::clamp(pt.y, 0.0F, static_cast<float>(frame_size.height - 1))
    );
}

} // namespace sancak
