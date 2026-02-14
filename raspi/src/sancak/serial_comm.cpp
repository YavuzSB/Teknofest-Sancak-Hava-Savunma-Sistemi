/**
 * @file serial_comm.cpp
 * @brief Seri Port İletişimi - Implementasyon
 *
 * Linux: POSIX termios ile /dev/ttyUSB0 veya /dev/ttyACM0
 * Diğer: Mock mod (log'a yazar)
 */
#include "sancak/serial_comm.hpp"
#include "sancak/logger.hpp"

#include <sstream>
#include <cstring>

#ifdef __linux__
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <errno.h>
#endif

namespace sancak {

SerialComm::~SerialComm() {
    close();
}

bool SerialComm::open(const SerialConfig& config) {
    config_ = config;

    if (!config_.enabled) {
        SANCAK_LOG_INFO("Seri port devre dışı (config)");
        return true;
    }

#ifdef __linux__
    fd_ = ::open(config_.port.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd_ < 0) {
        SANCAK_LOG_WARN("Seri port açılamadı: {} ({}). Mock mod.",
                        config_.port, strerror(errno));
        is_open_ = false;
        return false;
    }

    // termios ayarları
    struct termios tty{};
    if (tcgetattr(fd_, &tty) != 0) {
        SANCAK_LOG_ERROR("tcgetattr hatası: {}", strerror(errno));
        ::close(fd_);
        fd_ = -1;
        return false;
    }

    // Baud rate
    speed_t baud;
    switch (config_.baud_rate) {
        case 9600:   baud = B9600;   break;
        case 19200:  baud = B19200;  break;
        case 38400:  baud = B38400;  break;
        case 57600:  baud = B57600;  break;
        case 115200: baud = B115200; break;
        default:     baud = B115200; break;
    }
    cfsetispeed(&tty, baud);
    cfsetospeed(&tty, baud);

    // 8N1
    tty.c_cflag &= ~PARENB;
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;
    tty.c_cflag &= ~CRTSCTS;
    tty.c_cflag |= (CLOCAL | CREAD);

    // Raw mode
    tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    tty.c_iflag &= ~(IXON | IXOFF | IXANY);
    tty.c_oflag &= ~OPOST;

    // Timeout
    tty.c_cc[VMIN]  = 0;
    tty.c_cc[VTIME] = static_cast<cc_t>(config_.timeout_ms / 100);

    if (tcsetattr(fd_, TCSANOW, &tty) != 0) {
        SANCAK_LOG_ERROR("tcsetattr hatası: {}", strerror(errno));
        ::close(fd_);
        fd_ = -1;
        return false;
    }

    is_open_ = true;
    SANCAK_LOG_INFO("Seri port açıldı: {} @ {} baud", config_.port, config_.baud_rate);
    return true;

#else
    // Windows / diğer platformlar → mock mod
    SANCAK_LOG_WARN("Seri port: platform desteklenmiyor, mock mod");
    mock_mode_ = true;
    is_open_ = false;
    return false;
#endif
}

void SerialComm::close() {
#ifdef __linux__
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
        SANCAK_LOG_INFO("Seri port kapatıldı");
    }
#endif
    is_open_ = false;
}

bool SerialComm::writeBytes(const std::string& data) {
    if (!is_open_) {
        SANCAK_LOG_TRACE("[SERIAL-MOCK] {}", data);
        return false;
    }

#ifdef __linux__
    std::lock_guard<std::mutex> lock(write_mutex_);
    ssize_t written = ::write(fd_, data.c_str(), data.size());
    if (written < 0) {
        SANCAK_LOG_ERROR("Seri yazma hatası: {}", strerror(errno));
        return false;
    }
    return static_cast<size_t>(written) == data.size();
#else
    SANCAK_LOG_TRACE("[SERIAL-MOCK] {}", data);
    return false;
#endif
}

void SerialComm::sendAimCommand(const AimPoint& aim) {
    if (!aim.valid) return;

    // Frame merkezine göre delta hesapla
    // Not: PC tarafı frame boyutunu bilir, burada ham piksel gönderiyoruz
    std::ostringstream oss;
    oss << "AIM:" << static_cast<int>(aim.corrected.x)
        << "," << static_cast<int>(aim.corrected.y) << "\n";
    writeBytes(oss.str());
}

void SerialComm::sendFireCommand() {
    writeBytes("FIRE\n");
    SANCAK_LOG_WARN("ATES EMRI GONDERILDI!");
}

void SerialComm::sendIdleCommand() {
    writeBytes("IDLE\n");
}

void SerialComm::sendRaw(const std::string& command) {
    writeBytes(command + "\n");
}

} // namespace sancak
