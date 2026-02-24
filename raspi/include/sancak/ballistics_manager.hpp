/**
 * @file ballistics_manager.hpp
 * @brief Sancak Hava Savunma Sistemi - Balistik Düzeltme Yöneticisi
 *
 * Üç temel düzeltmeyi yönetir:
 *  1. Paralaks Düzeltmesi  (kamera-namlu offset)
 *  2. Balistik Düşüş       (yerçekimi + hava direnci)
 *  3. Önleme Hesabı        (hareketli hedefler)
 *
 * Sahadan toplanan verilerle doldurulan lookup table ve
 * doğrusal interpolasyon kullanır.
 *
 * @author Sancak Takımı
 * @date 2026
 */
#pragma once

#include "sancak/types.hpp"
#include "sancak/config_manager.hpp"

#include <vector>
#include <cmath>

namespace sancak {

/**
 * @class BallisticsManager
 * @brief Paralaks, balistik düşüş ve önleme hesapları
 */
class BallisticsManager {
public:
    BallisticsManager() = default;

    /**
     * @brief Konfigürasyon ile başlat
     */
    void initialize(const BallisticsConfig& config);

    /// ConfigManager üzerinden SystemConfig.ballistics okuyarak başlat
    void initialize(const SystemConfig& cfg) { initialize(cfg.ballistics); }
    void initialize(const ConfigManager& mgr) { initialize(mgr.get().ballistics); }

    /// Ham merkez + mesafeye göre piksel düzeltmesini uygula (lookup_table + manuel offset)
    [[nodiscard]] cv::Point2f applyCorrection(const cv::Point2f& raw_center, float distance_m) const;

    /**
     * @brief Tüm balistik düzeltmeleri hesaplar
     *
     * @param distance_m       Hedef mesafesi (metre)
     * @param target_velocity  Hedef hız vektörü (piksel/frame)
     * @param fps              Mevcut FPS (hız→gerçek birime çevirmek için)
     * @return BallisticCorrection tüm düzeltmeler
     */
    [[nodiscard]] BallisticCorrection calculate(float distance_m,
                                   const cv::Point2f& target_velocity = {0, 0},
                                   double fps = 30.0) const;

    /**
     * @brief Paralaks düzeltme açısını hesaplar
     * @param distance_m Hedef mesafesi
     * @return Paralaks açısı (radyan)
     */
    [[nodiscard]] float calculateParallax(float distance_m) const;

    /**
     * @brief Yerçekimi kaynaklı düşüş miktarını hesaplar
     * @param distance_m Hedef mesafesi
     * @return Düşüş miktarı (metre)
     */
    [[nodiscard]] float calculateDrop(float distance_m) const;

    /**
     * @brief Hareketli hedef için önleme mesafesi hesaplar
     * @param target_speed_mps Hedef hızı (m/s)
     * @param distance_m       Hedef mesafesi
     * @return Önleme mesafesi (metre)
     */
    [[nodiscard]] float calculateLead(float target_speed_mps, float distance_m) const;

    /**
     * @brief Lookup table'dan doğrusal interpolasyon ile düzeltme al
     * @param distance_m Mesafe
     * @param out_dx     Yatay değereltme (piksel)
     * @param out_dy     Dikey düzeltme (piksel)
     */
    void interpolateFromTable(float distance_m,
                               float& out_dx, float& out_dy) const;

    /**
     * @brief Manuel offset güncelle
     */
    void setManualOffset(float dx, float dy);

    /**
     * @brief Lookup table'a yeni giriş ekle (saha testi sonucu)
     */
    void addTableEntry(float distance_m, float y_offset_px, float x_offset_px);

private:
    BallisticsConfig config_;
};

} // namespace sancak
