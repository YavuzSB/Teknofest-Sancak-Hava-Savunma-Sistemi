/**
 * @file ballistics_manager.cpp
 * @brief Balistik Düzeltme Yöneticisi - Implementasyon
 *
 * 1. Paralaks Düzeltmesi  → θ = arctan(d_offset / D)
 * 2. Balistik Düşüş       → drop = 0.5 * g * t²  (t = D / v)
 * 3. Önleme (Lead)        → lead = v_target * t_total
 * 4. Lookup Table          → Saha testlerinden doğrusal interpolasyon
 */
#include "sancak/ballistics_manager.hpp"
#include "sancak/logger.hpp"

#include <algorithm>
#include <cmath>

namespace sancak {

void BallisticsManager::initialize(const BallisticsConfig& config) {
    config_ = config;

    // Lookup table'ı mesafeye göre sırala
    std::sort(config_.lookup_table.begin(), config_.lookup_table.end(),
              [](const auto& a, const auto& b) {
                  return a.distance_m < b.distance_m;
              });

    SANCAK_LOG_INFO("Ballistics Manager başlatıldı | Offset: ({:.3f}, {:.3f})m | V0: {:.1f} m/s | "
                    "Sıfırlama: {:.1f}m | Tablo girişi: {}",
                    config_.camera_barrel_offset_x_m, config_.camera_barrel_offset_y_m,
                    config_.muzzle_velocity_mps, config_.zeroing_distance_m,
                    config_.lookup_table.size());
}

float BallisticsManager::calculateParallax(float distance_m) const {
    if (distance_m <= 0.0f) return 0.0f;

    // Toplam fiziksel offset (x ve y bileşenleri)
    float offset = std::sqrt(
        config_.camera_barrel_offset_x_m * config_.camera_barrel_offset_x_m +
        config_.camera_barrel_offset_y_m * config_.camera_barrel_offset_y_m
    );

    // θ = arctan(offset / distance) (radyan)
    float theta = std::atan2(offset, distance_m);

    // Sıfırlama mesafesindeki paralaks
    float theta_zero = std::atan2(offset, config_.zeroing_distance_m);

    // Fark: sıfırlama noktasından ne kadar sapıyor
    return theta - theta_zero;
}

float BallisticsManager::calculateDrop(float distance_m) const {
    if (distance_m <= 0.0f || config_.muzzle_velocity_mps <= 0.0f) return 0.0f;

    // Uçuş süresi
    float t = distance_m / config_.muzzle_velocity_mps;

    // Yerçekimi düşüşü: drop = 0.5 * g * t²
    float drop = 0.5f * static_cast<float>(kGravity) * t * t;

    return drop;
}

float BallisticsManager::calculateLead(float target_speed_mps, float distance_m, float t_processing_s) const {
    if (distance_m <= 0.0f || config_.muzzle_velocity_mps <= 0.0f) return 0.0f;

    // Toplam gecikme:
    //   1. Uçuş süresi
    float t_flight = distance_m / config_.muzzle_velocity_mps;

    //   2. İşleme gecikmesi (YOLO + pipeline, dinamik)
    float t_total = t_flight + t_processing_s;

    // Önleme mesafesi = hız × toplam gecikme
    return target_speed_mps * t_total;
}

void BallisticsManager::interpolateFromTable(float distance_m,
                                               float& out_dx, float& out_dy) const {
    const auto& table = config_.lookup_table;

    if (table.empty()) {
        out_dx = 0.0f;
        out_dy = 0.0f;
        return;
    }

    // Sınır altı
    if (distance_m <= table.front().distance_m) {
        out_dx = table.front().x_offset_px;
        out_dy = table.front().y_offset_px;
        return;
    }

    // Sınır üstü
    if (distance_m >= table.back().distance_m) {
        out_dx = table.back().x_offset_px;
        out_dy = table.back().y_offset_px;
        return;
    }

    // Doğrusal interpolasyon
    for (size_t i = 1; i < table.size(); ++i) {
        if (distance_m <= table[i].distance_m) {
            float d0 = table[i - 1].distance_m;
            float d1 = table[i].distance_m;
            float ratio = (distance_m - d0) / (d1 - d0);

            out_dx = table[i - 1].x_offset_px +
                     ratio * (table[i].x_offset_px - table[i - 1].x_offset_px);
            out_dy = table[i - 1].y_offset_px +
                     ratio * (table[i].y_offset_px - table[i - 1].y_offset_px);
            return;
        }
    }
}

cv::Point2f BallisticsManager::applyCorrection(const cv::Point2f& raw_center, float distance_m) const {
    float dx = 0.0f;
    float dy = 0.0f;
    interpolateFromTable(distance_m, dx, dy);
    dx += config_.manual_offset_x_px;
    dy += config_.manual_offset_y_px;
    return cv::Point2f(raw_center.x + dx, raw_center.y + dy);
}

BallisticCorrection BallisticsManager::calculate(float distance_m,
                                                  const cv::Point2f& target_velocity,
                                                  double fps,
                                                  double t_processing_s) const {
    BallisticCorrection corr;

    if (distance_m <= 0.0f) return corr;

    // 1. Paralaks açısı
    corr.parallax_deg = calculateParallax(distance_m) * (180.0f / static_cast<float>(CV_PI));

    // 2. Balistik düşüş
    corr.drop_m = calculateDrop(distance_m);

    // 3. Lookup table'dan piksel düzeltmesi
    float table_dx = 0.0f, table_dy = 0.0f;
    interpolateFromTable(distance_m, table_dx, table_dy);

    // 4. Hareketli hedef önleme
    float target_speed_mps = 0.0f;
    if (fps > 0.0) {
        // Piksel/frame → m/s dönüşümü (yaklaşık)
        float px_to_m = distance_m / 600.0f;  // odak uzaklığı yaklaşık
        float speed_px_per_s = std::sqrt(target_velocity.x * target_velocity.x +
                                          target_velocity.y * target_velocity.y)
                               * static_cast<float>(fps);
        target_speed_mps = speed_px_per_s * px_to_m;
    }
    corr.lead_m = calculateLead(target_speed_mps, distance_m, static_cast<float>(t_processing_s));

    // 5. Toplam piksel düzeltmesi hesapla
    // Lookup table'dan gelen düzeltme (paralaks + düşüş dahil)
    corr.dx_px = table_dx + config_.manual_offset_x_px;
    corr.dy_px = table_dy + config_.manual_offset_y_px;

    // Lead düzeltmesini sadece x ekseninde uygula (örnek model)
    float lead_px = corr.lead_m / (distance_m / 600.0f); // m → px
    corr.dx_px += lead_px;

    return corr;
}

void BallisticsManager::setManualOffset(float dx, float dy) {
    config_.manual_offset_x_px = dx;
    config_.manual_offset_y_px = dy;
    SANCAK_LOG_INFO("Manuel balistik offset: dx={:.1f}, dy={:.1f}", dx, dy);
}

void BallisticsManager::addTableEntry(float distance_m, float y_offset_px, float x_offset_px) {
    config_.lookup_table.push_back({distance_m, y_offset_px, x_offset_px});
    // Yeniden sırala
    std::sort(config_.lookup_table.begin(), config_.lookup_table.end(),
              [](const auto& a, const auto& b) {
                  return a.distance_m < b.distance_m;
              });
    SANCAK_LOG_INFO("Balistik tablo girişi eklendi: {:.1f}m → ({:.1f}, {:.1f})px",
                    distance_m, x_offset_px, y_offset_px);
}

} // namespace sancak
