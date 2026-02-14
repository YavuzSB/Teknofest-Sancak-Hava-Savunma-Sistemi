#pragma once

#include <cstdint>
#include <string>
#include <queue>
#include <thread>
#include <mutex>
#include <atomic>

namespace teknofest {

/// Telemetri veri yapısı (parse edilmiş key-value)
struct TelemetryData {
    std::string speed;
    std::string direction;
    std::string mode;
};

/**
 * @brief TCP üzerinden çift yönlü telemetri istemcisi
 *
 * Raspberry Pi'ye TCP bağlantısı kurar, komut gönderir, telemetri verisini
 * parse ederek thread-safe olarak sunar. Bağlantı koparsa otomatik yeniden dener.
 */
class TelemetryClient final {
public:
    TelemetryClient(const std::string& host, uint16_t port);
    ~TelemetryClient();

    TelemetryClient(const TelemetryClient&) = delete;
    TelemetryClient& operator=(const TelemetryClient&) = delete;

    void start();
    void stop();

    /// Komut kuyruğuna ekle (thread-safe). Satır sonu otomatik eklenir.
    void sendCommand(const std::string& command);

    /// Son telemetri verisini al. Yeni veri varsa true döner.
    bool getLatestTelemetry(TelemetryData& data);

    enum class State { Disconnected, Connecting, Connected, Retrying, Stopped };
    State getState() const { return m_state.load(std::memory_order_acquire); }

private:
    void clientLoop();
    static TelemetryData parseTelemetry(const std::string& line);

    std::string m_host;
    uint16_t    m_port;
    std::thread m_thread;
    std::atomic<bool>  m_running{false};
    std::atomic<State> m_state{State::Disconnected};

    std::mutex              m_sendMutex;
    std::queue<std::string> m_sendQueue;

    std::mutex    m_dataMutex;
    TelemetryData m_latestData;
    bool          m_hasNewData = false;
};

} // namespace teknofest
