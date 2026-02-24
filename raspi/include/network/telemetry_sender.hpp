#pragma once

#include "core/types.hpp"
#include "network/protocol.hpp"

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <thread>

namespace sancak::net {

struct AimTelemetry {
    std::uint8_t current_state = static_cast<std::uint8_t>(core::CombatState::Idle);
    std::int32_t target_id = -1;

    float raw_x = 0.0f;
    float raw_y = 0.0f;

    float corrected_x = 0.0f;
    float corrected_y = 0.0f;

    float distance_m = 0.0f;
};

class TelemetrySender {
public:
    TelemetrySender();
    ~TelemetrySender();

    TelemetrySender(const TelemetrySender&) = delete;
    TelemetrySender& operator=(const TelemetrySender&) = delete;

    bool start(std::string ip, std::uint16_t port);
    void stop();

    void sendTelemetry(const AimTelemetry& data);

private:
    void workerLoop();

    bool ensureConnected();
    void disconnect();

    static bool sendAll(int sock, const std::uint8_t* data, std::size_t size);
    static std::vector<std::uint8_t> buildAimTelemetryFrame(const AimTelemetry& t);

    std::atomic<bool> running_{false};

    std::string ip_;
    std::uint16_t port_ = 0;

    std::thread worker_;

    std::mutex mutex_;
    std::condition_variable cv_;
    std::deque<AimTelemetry> queue_;
    std::size_t max_queue_ = 32;

    int sock_ = -1;
};

} // namespace sancak::net
