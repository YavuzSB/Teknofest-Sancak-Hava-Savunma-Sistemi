#include "turret_controller.hpp"

#include "protocol/PacketBuilder.hpp"

#include <chrono>
#include <thread>

#ifdef __linux__
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <cstring>
#include <cerrno>
#endif

namespace sancak {

#ifndef __linux__

TurretController::TurretController(const std::string& port, int baud, TurretProtocol protocol)
    : m_portName(port), m_baudRate(baud), m_protocol(protocol) {
    (void)m_portName;
    (void)m_baudRate;
    (void)m_protocol;
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
bool TurretController::writeBytes(const uint8_t*, size_t) { return false; }

#else

TurretController::TurretController(const std::string& port, int baud, TurretProtocol protocol)
    : m_portName(port), m_baudRate(baud), m_protocol(protocol)
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

    if (m_protocol == TurretProtocol::Binary) {
        static protocol::PacketBuilder pb;

        if (pan != m_lastPan || tilt != m_lastTilt) {
            AimPayload p{};
            p.pan_deg = pan;
            p.tilt_deg = tilt;
            p.distance_m = 0.0f; // Turret kontrolünde opsiyonel

            m_binQueue.push(pb.build(MsgType::Aim, p));
            m_lastPan = pan;
            m_lastTilt = tilt;
        }

        if (fire != m_lastFire) {
            TriggerPayload t{};
            t.fire = fire ? 1u : 0u;
            m_binQueue.push(pb.build(MsgType::Trigger, t));
            m_lastFire = fire;
        }
        return;
    }

    // ASCII (mevcut davranış)
    if (pan != m_lastPan || tilt != m_lastTilt) {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "<M:%.2f,%.2f>\n", pan, tilt);
        m_cmdQueue.push(std::string(buf));
        m_lastPan = pan;
        m_lastTilt = tilt;
    }
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
            if (m_protocol == TurretProtocol::Binary) {
                static protocol::PacketBuilder pb;
                SafeLockPayload s{};
                s.enable = 1u;
                auto frame = pb.build(MsgType::SafeLock, s);
                writeBytes(frame.data(), frame.size());
            } else {
                writeLine("<S>\n");
            }
        }
        // Komut kuyruğu
        if (m_protocol == TurretProtocol::Binary) {
            std::lock_guard<std::mutex> lock(m_queueMutex);
            while (!m_binQueue.empty()) {
                auto& frame = m_binQueue.front();
                writeBytes(frame.data(), frame.size());
                m_binQueue.pop();
            }
        } else {
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
    speed_t baud = B115200;
    switch (m_baudRate) {
        case 9600: baud = B9600; break;
        case 19200: baud = B19200; break;
        case 38400: baud = B38400; break;
        case 57600: baud = B57600; break;
        case 115200: baud = B115200; break;
        default: baud = B115200; break;
    }
    cfsetospeed(&tty, baud);
    cfsetispeed(&tty, baud);
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
    return writeBytes(reinterpret_cast<const uint8_t*>(line.data()), line.size());
}

bool TurretController::writeBytes(const uint8_t* data, size_t len) {
    using namespace std::chrono_literals;

    if (m_fd < 0) return false;
    if (!data || len == 0) return true;

    const uint8_t* p = data;
    size_t left = len;
    while (left > 0 && m_running.load()) {
        const ssize_t n = ::write(m_fd, p, left);
        if (n > 0) {
            p += static_cast<size_t>(n);
            left -= static_cast<size_t>(n);
            continue;
        }
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            std::this_thread::sleep_for(1ms);
            continue;
        }
        return false;
    }
    return left == 0;
}

#endif

} // namespace sancak
