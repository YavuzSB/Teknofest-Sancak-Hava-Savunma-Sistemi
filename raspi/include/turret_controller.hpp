#pragma once

#include <string>
#include <thread>
#include <mutex>
#include <atomic>
#include <queue>

namespace sancak {

class TurretController {
public:
    TurretController(const std::string& port, int baud = 115200);
    ~TurretController();

    TurretController(const TurretController&) = delete;
    TurretController& operator=(const TurretController&) = delete;

    // Pan, tilt ve ateş komutunu thread-safe olarak kuyruğa ekler
    void sendCommand(float pan, float tilt, bool fire);
    // SafeLock için motorları durdur
    void sendSafeLock();

    // Port ve bağlantı durumu
    bool isConnected() const;
    std::string getPortName() const;

private:
    void workerLoop();
    bool openPort();
    void closePort();
    bool writeLine(const std::string& line);

    std::string m_portName;
    int m_baudRate;
    int m_fd = -1;
    std::atomic<bool> m_running{false};
    std::thread m_thread;

    std::mutex m_queueMutex;
    std::queue<std::string> m_cmdQueue;
    bool m_lastFire = false;
    float m_lastPan = 0.0f;
    float m_lastTilt = 0.0f;
    std::atomic<bool> m_safeLock{false};
};

} // namespace sancak
