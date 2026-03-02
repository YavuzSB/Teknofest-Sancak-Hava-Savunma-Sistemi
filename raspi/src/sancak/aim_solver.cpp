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
                         double projectile_velocity_mps,
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

    // 2. Hedefin hızını px/frame'den m/s'ye çevir
    float px_to_m = (aim.distance_m > 0.0f) ? (aim.distance_m / 600.0f) : 0.0f; // odak uzaklığı yaklaşık
    float vx_mps = velocity.x * static_cast<float>(fps) * px_to_m;
    float vy_mps = velocity.y * static_cast<float>(fps) * px_to_m;

    // 3. Mermi uçuş süresi (ToF)
    float distance = aim.distance_m;
    float muzzle_v = (projectile_velocity_mps > 0.0) ? static_cast<float>(projectile_velocity_mps) : 70.0f;
    float t_flight = (muzzle_v > 0.0f) ? (distance / muzzle_v) : 0.0f;
    float t_processing = static_cast<float>(inference_delay_ms * 0.001);
    float t_total = t_flight + t_processing;

    // 4. Gelecek konum kestirimi (lead prediction)
    float lead_x = vx_mps * t_total;
    float lead_y = vy_mps * t_total;
    cv::Point2f lead_point = balloon.center + cv::Point2f(lead_x / px_to_m, lead_y / px_to_m); // px cinsinden

    // 5. Yerçekimi etkisi (balistik düşüş)
    float drop_m = 0.5f * static_cast<float>(kGravity) * t_flight * t_flight;
    float drop_px = (px_to_m > 0.0f) ? (drop_m / px_to_m) : 0.0f;

    // 6. Son düzeltme: lead + drop
    aim.corrected.x = lead_point.x;
    aim.corrected.y = lead_point.y + drop_px; // Y ekseninde aşağıya düzelt

    // 7. Frame sınırlarına kırp
    aim.corrected = clampToFrame(aim.corrected, frame_size);
    aim.distance_m = distance;
    aim.correction.dx_px = aim.corrected.x - balloon.center.x;
    aim.correction.dy_px = aim.corrected.y - balloon.center.y;
    aim.correction.lead_m = std::sqrt(lead_x * lead_x + lead_y * lead_y);
    aim.correction.drop_m = drop_m;
    aim.valid = true;

    SANCAK_LOG_INFO("Lead Prediction: ToF={:.3f}s | Lead=({:.2f},{:.2f})m | Drop={:.2f}m | LeadPt=({:.1f},{:.1f})",
        t_total, lead_x, lead_y, drop_m, aim.corrected.x, aim.corrected.y);

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
