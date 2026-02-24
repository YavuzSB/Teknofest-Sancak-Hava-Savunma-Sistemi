#include "sancak/trigger_controller.hpp"

#include <algorithm>
#include <cmath>

namespace sancak {

float TriggerController::distancePx(const cv::Point2f& a, const cv::Point2f& b) {
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    return std::sqrt(dx * dx + dy * dy);
}

void TriggerController::initialize(float aim_tolerance_px,
                                  int lock_duration_ms,
                                  int burst_duration_ms,
                                  int cooldown_ms) {
    aim_tolerance_px_ = std::max(0.0f, aim_tolerance_px);
    lock_duration_ = std::chrono::milliseconds(std::max(0, lock_duration_ms));
    burst_duration_ = std::chrono::milliseconds(std::max(0, burst_duration_ms));
    cooldown_ = std::chrono::milliseconds(std::max(0, cooldown_ms));
    reset();
}

void TriggerController::reset() {
    state_ = State::SEARCHING;
    lock_start_time_ = std::chrono::steady_clock::time_point{};
    state_start_ = std::chrono::steady_clock::time_point{};
}

bool TriggerController::update(const cv::Point2f& target_center, const cv::Point2f& crosshair_center) {
    const auto now = std::chrono::steady_clock::now();

    const bool within = (distancePx(target_center, crosshair_center) <= aim_tolerance_px_);
    if (!within) {
        lock_start_time_ = std::chrono::steady_clock::time_point{};
    } else if (lock_start_time_ == std::chrono::steady_clock::time_point{}) {
        lock_start_time_ = now;
    }

    // Zaman bazlı state'ler
    if (state_ == State::FIRING) {
        if (now - state_start_ >= burst_duration_) {
            state_ = State::COOLDOWN;
            state_start_ = now;
            lock_start_time_ = std::chrono::steady_clock::time_point{};
            return false;
        }
        return true;
    }

    if (state_ == State::COOLDOWN) {
        if (now - state_start_ >= cooldown_) {
            state_ = within ? State::LOCKING : State::SEARCHING;
            if (within) {
                lock_start_time_ = now;
            }
        }
        return false;
    }

    // SEARCHING / LOCKING
    state_ = within ? State::LOCKING : State::SEARCHING;

    if (within && lock_start_time_ != std::chrono::steady_clock::time_point{} &&
        (now - lock_start_time_) >= lock_duration_) {
        state_ = State::FIRING;
        state_start_ = now;
        lock_start_time_ = std::chrono::steady_clock::time_point{};
        return true;
    }

    return false;
}

} // namespace sancak
