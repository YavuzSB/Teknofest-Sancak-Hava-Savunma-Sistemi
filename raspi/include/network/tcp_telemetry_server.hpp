#pragma once

#include "network/protocol.hpp"

#include "sancak/types.hpp"

#include <atomic>
#include <cstdint>
#include <mutex>
#include <optional>
#include <queue>
#include <string>
#include <thread>
#include <vector>

namespace sancak::net {

struct TcpTelemetryConfig {
    std::uint16_t port = 5000;
};

/**
 * @brief Tek bağlantılı TCP telemetri sunucusu.
 *
 * - PC tarafı TelemetryClient gibi bir TCP client bağlanır.
 * - Sunucu gelen satır bazlı komutları tüketir (buffer dolmasın diye).
 * - Sunucu AimResult'u binary frame (SNK2) olarak yayınlar.
 */
class TcpTelemetryServer final {
public:
    TcpTelemetryServer();
    ~TcpTelemetryServer();

    TcpTelemetryServer(const TcpTelemetryServer&) = delete;
    TcpTelemetryServer& operator=(const TcpTelemetryServer&) = delete;

    bool start(TcpTelemetryConfig cfg);
    void stop();
    [[nodiscard]] bool isRunning() const { return running_.load(std::memory_order_acquire); }

    void publishAimResult(const sancak::AimResult& aim, std::uint32_t frame_id);

private:
    void serverLoop();
    void enqueueFrame(std::vector<std::uint8_t> frame);

    static std::vector<std::uint8_t> buildAimFrame(const sancak::AimResult& aim,
                                                   std::uint32_t frame_id);

    TcpTelemetryConfig cfg_;
    std::thread worker_;
    std::atomic<bool> running_{false};

    std::mutex out_mutex_;
    std::queue<std::vector<std::uint8_t>> out_queue_;

    // platform sockets
    struct SocketHandle;
    SocketHandle* listen_sock_ = nullptr;
};

} // namespace sancak::net
