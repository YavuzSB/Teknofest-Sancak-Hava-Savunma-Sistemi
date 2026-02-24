#include "turret_controller.hpp"

#include <chrono>
#include <thread>

#ifdef __linux__
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <cstring>
#endif

namespace sancak {

#ifndef __linux__

TurretController::TurretController(const std::string& port, int baud)
    : m_portName(port), m_baudRate(baud) {
    (void)m_portName;
    (void)m_baudRate;
}

TurretController::~TurretController() = default;

void TurretController::sendCommand(float pan, float tilt, bool fire) {
    (void)pan;
    (void)tilt;
    (void)fire;
}

void TurretController::sendSafeLock() {}

bool TurretController::isConnected() const {
    return false;
}

std::string TurretController::getPortName() const {
    return m_portName;
}

void TurretController::workerLoop() {}
bool TurretController::openPort() { return false; }
void TurretController::closePort() {}
bool TurretController::writeLine(const std::string&) { return false; }

#else

TurretController::TurretController(const std::string& port, int baud)
    : m_portName(port), m_baudRate(baud)
{
    m_running.store(true);
    m_thread = std::thread(&TurretController::workerLoop, this);
}

TurretController::~TurretController() {
    m_running.store(false);
    if (m_thread.joinable()) m_thread.join();
    closePort();
}

void TurretController::sendCommand(float pan, float tilt, bool fire) {
    std::lock_guard<std::mutex> lock(m_queueMutex);
    // Hareket komutu
    if (pan != m_lastPan || tilt != m_lastTilt) {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "<M:%.2f,%.2f>\n", pan, tilt);
        m_cmdQueue.push(std::string(buf));
        m_lastPan = pan;
        m_lastTilt = tilt;
    }
    // Ateş komutu
    if (fire != m_lastFire) {
        m_cmdQueue.push(fire ? "<F:1>\n" : "<F:0>\n");
        m_lastFire = fire;
    }
}

void TurretController::sendSafeLock() {
    m_safeLock.store(true);
}

bool TurretController::isConnected() const {
    return m_fd >= 0;
}

std::string TurretController::getPortName() const {
    return m_portName;
}

void TurretController::workerLoop() {
    using namespace std::chrono_literals;
    while (m_running.load()) {
        if (!isConnected()) {
            openPort();
            std::this_thread::sleep_for(1s);
            continue;
        }
        // SafeLock durumu
        if (m_safeLock.exchange(false)) {
            writeLine("<S>\n");
        }
        // Komut kuyruğu
        {
            std::lock_guard<std::mutex> lock(m_queueMutex);
            while (!m_cmdQueue.empty()) {
                writeLine(m_cmdQueue.front());
                m_cmdQueue.pop();
            }
        }
        std::this_thread::sleep_for(10ms);
    }
}

bool TurretController::openPort() {
    closePort();
    m_fd = ::open(m_portName.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (m_fd < 0) return false;
    struct termios tty{};
    if (tcgetattr(m_fd, &tty) != 0) {
        closePort();
        return false;
    }
    cfsetospeed(&tty, B115200);
    cfsetispeed(&tty, B115200);
    tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;
    tty.c_cflag |= CLOCAL | CREAD;
    tty.c_cflag &= ~(PARENB | PARODD | CSTOPB | CRTSCTS);
    tty.c_iflag = 0;
    tty.c_oflag = 0;
    tty.c_lflag = 0;
    tty.c_cc[VMIN]  = 0;
    tty.c_cc[VTIME] = 1;
    tcflush(m_fd, TCIFLUSH);
    if (tcsetattr(m_fd, TCSANOW, &tty) != 0) {
        closePort();
        return false;
    }
    return true;
}

void TurretController::closePort() {
    if (m_fd >= 0) {
        ::close(m_fd);
        m_fd = -1;
    }
}

bool TurretController::writeLine(const std::string& line) {
    if (m_fd < 0) return false;
    ssize_t n = ::write(m_fd, line.c_str(), line.size());
    return n == static_cast<ssize_t>(line.size());
}

#endif

} // namespace sancak
