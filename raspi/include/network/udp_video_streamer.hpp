#pragma once

#include "network/protocol.hpp"

#include <opencv2/core.hpp>

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>

namespace sancak::net {

struct UdpVideoConfig {
    std::string host = "192.168.1.10";
    std::uint16_t port = 5005;
    std::size_t mtu_bytes = 1200; // UDP payload hedefi (IP/UDP header hariç değil, güvenli sınır)
    int jpeg_quality = 70;
};

/**
 * @brief Combat pipeline frame'lerini UDP üzerinden JPEG olarak yayınlar.
 *
 * - Her frame: OpenCV imencode(".jpg")
 * - JPEG datası MTU sınırına göre parçalara bölünür.
 * - Gönderim ayrı thread'de yapılır, pipeline bloklanmaz.
 * - Queue büyümez: her zaman "son frame" gönderilir (drop-old policy).
 */
class UdpVideoStreamer final {
public:
    UdpVideoStreamer();
    ~UdpVideoStreamer();

    UdpVideoStreamer(const UdpVideoStreamer&) = delete;
    UdpVideoStreamer& operator=(const UdpVideoStreamer&) = delete;

    bool start(UdpVideoConfig cfg);
    void stop();
    [[nodiscard]] bool isRunning() const { return running_.load(std::memory_order_acquire); }

    /// Son işlenmiş kareyi gönderim için submit eder (thread-safe). Frame kopyalanır.
    void submit(const cv::Mat& bgrFrame);

private:
    void workerLoop();
    bool openSocket();
    void closeSocket();
    void sendJpeg(const std::vector<std::uint8_t>& jpeg, std::uint64_t frame_timestamp_us);

    UdpVideoConfig cfg_;
    std::thread worker_;
    std::atomic<bool> running_{false};
    std::atomic<std::uint32_t> frame_id_{1};

    std::mutex frame_mutex_;
    cv::Mat latest_frame_;
    std::uint64_t latest_timestamp_us_ = 0; // Unix epoch (system_clock) microseconds
    bool has_frame_ = false;

    std::mutex cv_mutex_;
    std::condition_variable cv_;
    bool wake_ = false;

    // platform socket
    struct SocketHandle;
    SocketHandle* sock_ = nullptr;
};

} // namespace sancak::net
