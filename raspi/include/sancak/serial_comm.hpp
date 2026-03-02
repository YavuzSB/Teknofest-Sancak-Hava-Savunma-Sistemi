/**
 * @file serial_comm.hpp
 * @brief Sancak Hava Savunma Sistemi - Asenkron Seri Haberleşme
 *
 * Raspberry Pi tarafında UART üzerinden asenkron okuma/yazma yapar.
 * Okuma tarafı; 0x55 0xAA header + Type + Len + Payload + CRC16 frame'lerini
 * state-machine ile parse eder ve geçerli paketleri callback ile bildirir.
 *
 * Frame:
 *   0x55 0xAA Type(1) PayloadSize(1) Payload(N) CRC16(2)
 * CRC: CRC-16/CCITT-FALSE (poly 0x1021, init 0xFFFF)
 */
#pragma once

#include "sancak/types.hpp"
#include "sancak/config_manager.hpp"

#include "protocol/ProtocolDef.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <string>
#include <mutex>
#include <thread>
#include <vector>

namespace sancak {

struct GenericPacket {
    MsgType type;
    std::vector<std::uint8_t> payload;
    std::uint16_t crc;
};

using PacketCallback = std::function<void(const GenericPacket&)>;

enum class ParsingState : std::uint8_t {
    WAIT_H1,
    WAIT_H2,
    GET_TYPE,
    GET_LEN,
    GET_PAYLOAD,
    CHECK_CRC,
};

/**
 * @class SerialComm
 * @brief Arduino/STM32 ile seri iletişim
 *
 * Komutlar:
 *   AIM:dx,dy      → Nişan noktası delta (piksel)
 *   FIRE            → Ateş emri
 *   IDLE            → Bekleme moduna geç
 *   OFFSET:dx,dy    → Manuel offset ayarla
 */
class SerialComm {
public:
    SerialComm() = default;
    ~SerialComm();

    /**
     * @brief Seri portu aç
     * @param config Seri port ayarları
     * @return Başarılı mı?
     */
    bool open(const SerialConfig& config);

    /**
     * @brief Seri portu kapat
     */
    void close();

    /**
     * @brief Asenkron okuma thread'ini başlatır.
     * @param callback Geçerli bir paket oluştuğunda çağrılır.
     * @return Başarılı mı?
     */
    bool startAsyncRead(PacketCallback callback);

    /**
     * @brief Okuma thread'ini durdurur.
     */
    void stopAsyncRead();

    /**
     * @brief Nişan komutu gönder
     * @param aim Nişan noktası
     */
    void sendAimCommand(const AimPoint& aim);

    /**
     * @brief Ateş komutu gönder
     */
    void sendFireCommand();

    /**
     * @brief Bekleme komutu gönder
     */
    void sendIdleCommand();

    /**
     * @brief Ham komut gönder
     * @param command Komut string'i
     */
    void sendRaw(const std::string& command);

    /// Port açık mı?
    bool isOpen() const { return is_open_; }

    bool isReading() const { return running_.load(); }

private:
    void readLoop();
    void resetParser();
    void feedByte(std::uint8_t byte);

    /// Platform bağımsız seri yazma
    bool writeBytes(const std::string& data);

    SerialConfig config_;
    bool is_open_ = false;
    std::mutex write_mutex_;

    std::atomic<bool> running_{false};
    std::thread read_thread_;
    std::mutex callback_mutex_;
    PacketCallback callback_;

    ParsingState state_ = ParsingState::WAIT_H1;
    std::uint8_t type_ = 0;
    std::uint8_t payload_len_ = 0;
    std::vector<std::uint8_t> payload_;
    std::size_t payload_idx_ = 0;
    std::uint8_t crc_bytes_[2] = {0, 0};
    std::size_t crc_idx_ = 0;
    std::uint16_t crc_running_ = 0xFFFFu;
    std::chrono::steady_clock::time_point last_byte_time_{};

#ifdef __linux__
    int fd_ = -1;  ///< Linux file descriptor
#else
    // Windows/diğer platformlar için mock
    bool mock_mode_ = true;
#endif
};

} // namespace sancak
