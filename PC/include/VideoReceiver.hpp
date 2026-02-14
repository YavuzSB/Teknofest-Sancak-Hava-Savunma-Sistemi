#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>

namespace teknofest {

/**
 * @brief UDP üzerinden JPEG kare alıcısı
 *
 * Belirtilen UDP portunu dinler, gelen JPEG datagramlarını stb_image ile
 * decode eder ve piksel verisini thread-safe olarak sunar.
 */
class VideoReceiver final {
public:
    explicit VideoReceiver(uint16_t port, const std::string& bindIp = "0.0.0.0");
    ~VideoReceiver();

    VideoReceiver(const VideoReceiver&) = delete;
    VideoReceiver& operator=(const VideoReceiver&) = delete;

    void start();
    void stop();

    /// Son alınan kareyi al. Yeni kare varsa true döner.
    bool getLatestFrame(std::vector<uint8_t>& pixels, int& width, int& height);

    enum class State { Idle, Listening, Receiving, Error, Stopped };
    State getState() const { return m_state.load(std::memory_order_acquire); }

private:
    void receiverLoop();

    uint16_t    m_port;
    std::string m_bindIp;
    std::thread m_thread;
    std::atomic<bool>  m_running{false};
    std::atomic<State> m_state{State::Idle};

    std::mutex           m_frameMutex;
    std::vector<uint8_t> m_framePixels;
    int  m_frameWidth  = 0;
    int  m_frameHeight = 0;
    bool m_hasNewFrame = false;
};

} // namespace teknofest
