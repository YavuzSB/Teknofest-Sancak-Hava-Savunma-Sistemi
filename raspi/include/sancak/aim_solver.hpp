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

#include <optional>

namespace sancak {


/**
 * @class AimSolver
 * @brief Hareketli hedefler için "Lead Prediction" ve balistik düzeltmeli nişan noktası hesaplayıcı
 *
 * Özellikler:
 *  - Hedefin hız vektörü ve anlık pozisyonundan, mermi uçuş süresiyle gelecekteki konumu tahmin eder.
 *  - Mermi çıkış hızı (muzzle_velocity) ve yerçekimi etkisini (drop) hesaba katar.
 *  - Taretin yönlenmesi gereken pitch/yaw açılarını ve lead point'i hesaplar.
 *  - Sıfıra bölme ve trigonometrik güvenlik kontrolleri içerir.
 */
class AimSolver {
public:
    AimSolver() = default;

    void initialize(const BallisticsConfig& balConfig,
                    const DistanceConfig& distConfig);

    /**
     * @brief Hareketli hedef için lead prediction ve balistik düzeltmeli nişan noktası hesapla
     *
     * @param balloon    Balon segmentasyon sonucu
     * @param detection  YOLO tespiti
     * @param velocity   Hedef hız vektörü (px/frame)
     * @param fps        Mevcut FPS
     * @param frame_size Frame boyutu
     * @param projectile_velocity_mps Mermi çıkış hızı (m/s)
     * @return AimPoint  Hesaplanmış nişan noktası
     */
    [[nodiscard]] AimPoint solve(const BalloonResult& balloon,
                   const Detection& detection,
                   const cv::Point2f& velocity,
                   double fps,
                   const cv::Size& frame_size,
                   double projectile_velocity_mps = 70.0,
                   double inference_delay_ms = 50.0,
                   std::optional<float> lidar_distance_m = std::nullopt,
                   const cv::Point3f& lidar_offset_m = cv::Point3f(0.0f, 0.0f, 0.0f)) const;

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
