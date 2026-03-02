/**
 * @file serial_comm.cpp
 * @brief Seri Port İletişimi - Implementasyon
 *
 * Linux: POSIX termios ile /dev/ttyUSB0 veya /dev/ttyACM0
 * Diğer: Mock mod (log'a yazar)
 */
#include "sancak/serial_comm.hpp"
#include "sancak/logger.hpp"

#include <array>
#include <sstream>
#include <cstring>
#include <chrono>

#ifdef __linux__
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <errno.h>
#include <sys/select.h>
#endif

namespace sancak {

SerialComm::~SerialComm() {
    stopAsyncRead();
    close();
}

bool SerialComm::open(const SerialConfig& config) {
    config_ = config;

    if (!config_.enabled) {
        SANCAK_LOG_INFO("Seri port devre dışı (config)");
        return true;
    }

#ifdef __linux__
    fd_ = ::open(config_.port.c_str(), O_RDWR | O_NOCTTY | O_CLOEXEC | O_NONBLOCK);
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

    // Timeout: readLoop() select() ile timeout yönetecek (non-blocking).
    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = 0;

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
    stopAsyncRead();
#ifdef __linux__
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
        SANCAK_LOG_INFO("Seri port kapatıldı");
    }
#endif
    is_open_ = false;
}

bool SerialComm::startAsyncRead(PacketCallback callback) {
    if (!is_open_) {
        SANCAK_LOG_WARN("startAsyncRead çağrıldı ama seri port açık değil");
        return false;
    }

    if (running_.exchange(true)) {
        return true;
    }

    {
        std::lock_guard<std::mutex> lock(callback_mutex_);
        callback_ = std::move(callback);
    }

    resetParser();
    last_byte_time_ = std::chrono::steady_clock::now();

    read_thread_ = std::thread([this]() { readLoop(); });
    return true;
}

void SerialComm::stopAsyncRead() {
    if (!running_.exchange(false)) {
        return;
    }

    if (read_thread_.joinable()) {
        read_thread_.join();
    }
}

void SerialComm::resetParser() {
    state_ = ParsingState::WAIT_H1;
    type_ = 0;
    payload_len_ = 0;
    payload_.clear();
    payload_idx_ = 0;
    crc_bytes_[0] = 0;
    crc_bytes_[1] = 0;
    crc_idx_ = 0;
    crc_running_ = 0xFFFFu;
}

void SerialComm::feedByte(std::uint8_t byte) {
    constexpr std::uint8_t kMaxPayload = 255;

    switch (state_) {
        case ParsingState::WAIT_H1:
            if (byte == UART_H1) {
                crc_running_ = 0xFFFFu;
                crc_running_ = crc16_ccitt_false_update(crc_running_, byte);
                state_ = ParsingState::WAIT_H2;
            }
            break;

        case ParsingState::WAIT_H2:
            if (byte == UART_H2) {
                crc_running_ = crc16_ccitt_false_update(crc_running_, byte);
                state_ = ParsingState::GET_TYPE;
            } else if (byte == UART_H1) {
                // Resync toleransı: 0x55 0x55 ... 0xAA
                crc_running_ = 0xFFFFu;
                crc_running_ = crc16_ccitt_false_update(crc_running_, byte);
                state_ = ParsingState::WAIT_H2;
            } else {
                resetParser();
            }
            break;

        case ParsingState::GET_TYPE:
            type_ = byte;
            crc_running_ = crc16_ccitt_false_update(crc_running_, byte);
            state_ = ParsingState::GET_LEN;
            break;

        case ParsingState::GET_LEN:
            payload_len_ = byte;
            crc_running_ = crc16_ccitt_false_update(crc_running_, byte);

            if (payload_len_ > kMaxPayload) {
                resetParser();
                break;
            }

            payload_.assign(payload_len_, 0);
            payload_idx_ = 0;
            crc_idx_ = 0;
            state_ = (payload_len_ == 0) ? ParsingState::CHECK_CRC : ParsingState::GET_PAYLOAD;
            break;

        case ParsingState::GET_PAYLOAD:
            if (payload_idx_ >= payload_.size()) {
                resetParser();
                break;
            }

            payload_[payload_idx_++] = byte;
            crc_running_ = crc16_ccitt_false_update(crc_running_, byte);
            if (payload_idx_ >= payload_.size()) {
                crc_idx_ = 0;
                state_ = ParsingState::CHECK_CRC;
            }
            break;

        case ParsingState::CHECK_CRC:
            if (crc_idx_ >= 2) {
                resetParser();
                break;
            }

            crc_bytes_[crc_idx_++] = byte;
            if (crc_idx_ < 2) {
                break;
            }

            // Paket formatı: CRC high byte sonra low byte.
            const std::uint16_t crc_rx =
                static_cast<std::uint16_t>((static_cast<std::uint16_t>(crc_bytes_[0]) << 8) |
                                           static_cast<std::uint16_t>(crc_bytes_[1]));

            if (crc_rx == crc_running_) {
                PacketCallback cb;
                {
                    std::lock_guard<std::mutex> lock(callback_mutex_);
                    cb = callback_;
                }

                if (cb) {
                    GenericPacket packet;
                    packet.type = static_cast<MsgType>(type_);
                    packet.payload = payload_;
                    packet.crc = crc_rx;
                    cb(packet);
                }
            } else {
                SANCAK_LOG_TRACE("UART CRC hatası: rx=0x{:04X} calc=0x{:04X}", crc_rx, crc_running_);
            }

            resetParser();
            break;
    }
}

void SerialComm::readLoop() {
#ifndef __linux__
    while (running_.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return;
#else
    if (fd_ < 0) {
        SANCAK_LOG_WARN("readLoop başladı ama fd geçersiz");
        running_.store(false);
        return;
    }

    constexpr int kSelectTimeoutMs = 20;
    constexpr auto kPacketTimeout = std::chrono::milliseconds(100);

    std::array<std::uint8_t, 256> buf{};
    last_byte_time_ = std::chrono::steady_clock::now();

    while (running_.load()) {
        fd_set read_set;
        FD_ZERO(&read_set);
        FD_SET(fd_, &read_set);

        timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = kSelectTimeoutMs * 1000;

        const int sel = ::select(fd_ + 1, &read_set, nullptr, nullptr, &tv);
        if (sel < 0) {
            if (errno == EINTR) {
                continue;
            }
            SANCAK_LOG_WARN("select hatası: {}", strerror(errno));
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        if (sel == 0) {
            // Timeout: paket ortasında kaldıysak state'i resetle.
            const auto now = std::chrono::steady_clock::now();
            if (state_ != ParsingState::WAIT_H1 && (now - last_byte_time_) > kPacketTimeout) {
                resetParser();
            }
            continue;
        }

        if (!FD_ISSET(fd_, &read_set)) {
            continue;
        }

        const ssize_t n = ::read(fd_, buf.data(), buf.size());
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                continue;
            }
            SANCAK_LOG_WARN("read hatası: {}", strerror(errno));
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            continue;
        }

        if (n == 0) {
            // Karşı taraf kapatmış olabilir; busy-loop yapma.
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }

        const auto now = std::chrono::steady_clock::now();
        last_byte_time_ = now;
        for (ssize_t i = 0; i < n; ++i) {
            feedByte(buf[static_cast<std::size_t>(i)]);
        }
    }
#endif
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
        << "," << static_cast<int>(aim.corrected.y);

    std::string payload = oss.str();
    // Basit XOR checksum (tüm karakterlerin XOR'u)
    uint8_t csum = 0;
    for (char ch : payload) csum ^= static_cast<uint8_t>(ch);
    char hex[3] = {0};
    std::snprintf(hex, sizeof(hex), "%02X", csum);
    payload += "*";
    payload += hex;
    payload += "\n";
    writeBytes(payload);
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
