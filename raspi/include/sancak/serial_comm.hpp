/**
 * @file serial_comm.hpp
 * @brief Sancak Hava Savunma Sistemi - Seri Port İletişimi
 *
 * Arduino/STM32 ile UART üzerinden nişan komutları gönderir.
 * Protokol: "CMD:param1,param2\n"
 *
 * @author Sancak Takımı
 * @date 2026
 */
#pragma once

#include "sancak/types.hpp"
#include "sancak/config_manager.hpp"

#include <string>
#include <mutex>
#include <fstream>

namespace sancak {

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

private:
    /// Platform bağımsız seri yazma
    bool writeBytes(const std::string& data);

    SerialConfig config_;
    bool is_open_ = false;
    std::mutex write_mutex_;

#ifdef __linux__
    int fd_ = -1;  ///< Linux file descriptor
#else
    // Windows/diğer platformlar için mock
    bool mock_mode_ = true;
#endif
};

} // namespace sancak
