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
                                  int lock_frames_required,
                                  int burst_duration_ms,
                                  int cooldown_ms) {
    aim_tolerance_px_ = std::max(0.0f, aim_tolerance_px);
    lock_frames_required_ = std::max(0, lock_frames_required);
    burst_duration_ = std::chrono::milliseconds(std::max(0, burst_duration_ms));
    cooldown_ = std::chrono::milliseconds(std::max(0, cooldown_ms));
    reset();
}

void TriggerController::reset() {
    state_ = State::SEARCHING;
    locked_frames_ = 0;
    state_start_ = std::chrono::steady_clock::time_point{};
}

bool TriggerController::update(const cv::Point2f& target_center, const cv::Point2f& crosshair_center) {
    const auto now = std::chrono::steady_clock::now();

    const bool within = (distancePx(target_center, crosshair_center) <= aim_tolerance_px_);
    if (within) {
        ++locked_frames_;
    } else {
        locked_frames_ = 0;
    }

    // Zaman bazlı state'ler
    if (state_ == State::FIRING) {
        if (now - state_start_ >= burst_duration_) {
            state_ = State::COOLDOWN;
            state_start_ = now;
            locked_frames_ = 0; // cooldown sonrası yeniden kilitlenme gereksinimi
            return false;
        }
        return true;
    }

    if (state_ == State::COOLDOWN) {
        if (now - state_start_ >= cooldown_) {
            state_ = within ? State::LOCKING : State::SEARCHING;
        }
        return false;
    }

    // SEARCHING / LOCKING
    state_ = within ? State::LOCKING : State::SEARCHING;

    if (within && locked_frames_ >= lock_frames_required_) {
        state_ = State::FIRING;
        state_start_ = now;
        locked_frames_ = 0;
        return true;
    }

    return false;
}

} // namespace sancak
