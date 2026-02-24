#pragma once

#include <cstdint>
#include <string>
#include <queue>
#include <thread>
#include <mutex>
#include <atomic>
#include <vector>
#include <functional>

namespace teknofest {

/// Telemetri veri yapısı (parse edilmiş key-value)
struct TelemetryData {
    std::string speed;
    std::string direction;
    std::string mode;
};

/// Raspi -> PC nişan telemetrisi (binary frame ile gelir)
struct AimResult {
    bool    valid = false;
    float   raw_x = 0.0f;
    float   raw_y = 0.0f;
    float   corrected_x = 0.0f;
    float   corrected_y = 0.0f;
    int32_t class_id = -1;
    uint32_t frame_id = 0;
};

/**
 * @brief TCP üzerinden çift yönlü telemetri istemcisi
 *
 * Raspberry Pi'ye TCP bağlantısı kurar, komut gönderir, telemetri verisini
 * parse ederek thread-safe olarak sunar. Bağlantı koparsa otomatik yeniden dener.
 */
class TelemetryClient final {
public:
    using LogCallback = std::function<void(int level, const std::string& msg)>;

    TelemetryClient(const std::string& host, uint16_t port);
    ~TelemetryClient();

    TelemetryClient(const TelemetryClient&) = delete;
    TelemetryClient& operator=(const TelemetryClient&) = delete;

    void start();
    void stop();

    /// Ağ thread'i içindeki olayları dışarıya loglamak için callback bağla.
    void setLogCallback(LogCallback cb);

    /// Komut kuyruğuna ekle (thread-safe). Satır sonu otomatik eklenir.
    void sendCommand(const std::string& command);

    /// Son telemetri verisini al. Yeni veri varsa true döner.
    bool getLatestTelemetry(TelemetryData& data);

    /// Son AimResult verisini al. Yeni veri varsa true döner.
    bool getLatestAimResult(AimResult& data);

    enum class State { Disconnected, Connecting, Connected, Retrying, Stopped };
    State getState() const { return m_state.load(std::memory_order_acquire); }

private:
    void clientLoop();
    static TelemetryData parseTelemetry(const std::string& line);

    bool tryParseAimFrame(std::vector<uint8_t>& buffer, AimResult& out);
    void emitLog(int level, const std::string& msg);

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

    AimResult m_latestAim;
    bool      m_hasNewAim = false;

    std::mutex   m_cbMutex;
    LogCallback  m_logCallback;
    std::chrono::steady_clock::time_point m_lastParseWarn{};
};

} // namespace teknofest
