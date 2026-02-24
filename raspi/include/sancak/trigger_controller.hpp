/**
 * @file trigger_controller.hpp
 * @brief Otonom tetik disiplini motoru (Trigger Controller)
 *
 * Hedef-kilitlenme süresi, burst süresi ve cooldown süresi yönetimi.
 * false-positive ateşlemeyi azaltmak ve motor sarsıntısı geçene kadar
 * beklemek için basit bir durum makinesi uygular.
 */
#pragma once

#include "sancak/types.hpp"

#include <chrono>

namespace sancak {

class TriggerController {
public:
    enum class State {
        SEARCHING,
        LOCKING,
        FIRING,
        COOLDOWN
    };

    /// @brief Parametrelerle başlat.
    /// @param aim_tolerance_px Crosshair-target merkez mesafe eşiği (px)
    /// @param lock_duration_ms Ateş öncesi hedefin tolerans içinde kalma süresi (ms)
    /// @param burst_duration_ms Ateşin açık kalacağı süre (ms)
    /// @param cooldown_ms İki atış arası bekleme süresi (ms)
    void initialize(float aim_tolerance_px,
                    int lock_duration_ms,
                    int burst_duration_ms,
                    int cooldown_ms);

    /// @brief Durum makinesini güncelle.
    /// @return Bu frame'de ateş açık olmalı mı?
    bool update(const cv::Point2f& target_center, const cv::Point2f& crosshair_center);

    void reset();

    State state() const { return state_; }

private:
    static float distancePx(const cv::Point2f& a, const cv::Point2f& b);

    float aim_tolerance_px_ = 15.0f;
    std::chrono::milliseconds lock_duration_{150};
    std::chrono::milliseconds burst_duration_{500};
    std::chrono::milliseconds cooldown_{1000};

    State state_ = State::SEARCHING;
    std::chrono::steady_clock::time_point lock_start_time_{};
    std::chrono::steady_clock::time_point state_start_{};
};

} // namespace sancak
