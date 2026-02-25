/**
 * @file logger.hpp
 * @brief Sancak Hava Savunma Sistemi - Hafif Loglama Altyapısı
 *
 * Seviye tabanlı, thread-safe, header-only logger.
 * Derleme zamanında minimum seviye ayarlanabilir.
 *
 * Kullanım:
 *   SANCAK_LOG_INFO("Sistem başlatıldı, FPS: {}", fps);
 *   SANCAK_LOG_WARN("Hedef kayboldu, track_id={}", id);
 *
 * @author Sancak Takımı
 * @date 2026
 */
#pragma once

#include <iostream>
#include <sstream>
#include <string>
#include <mutex>
#include <chrono>
#include <iomanip>
#include <cstdint>

namespace sancak {
namespace log {

/// Log seviyeleri
enum class Level : uint8_t {
    kTrace = 0,
    kDebug = 1,
    kInfo  = 2,
    kWarn  = 3,
    kError = 4,
    kFatal = 5,
    kOff   = 6
};

inline const char* LevelTag(Level lvl) {
    switch (lvl) {
        case Level::kTrace: return "TRC";
        case Level::kDebug: return "DBG";
        case Level::kInfo:  return "INF";
        case Level::kWarn:  return "WRN";
        case Level::kError: return "ERR";
        case Level::kFatal: return "FTL";
        default:            return "???";
    }
}

inline const char* LevelColor(Level lvl) {
    switch (lvl) {
        case Level::kTrace: return "\033[90m";
        case Level::kDebug: return "\033[36m";
        case Level::kInfo:  return "\033[32m";
        case Level::kWarn:  return "\033[33m";
        case Level::kError: return "\033[31m";
        case Level::kFatal: return "\033[35;1m";
        default:            return "\033[0m";
    }
}

/// Tekil (singleton) logger
class Logger {
public:
    static Logger& instance() {
        static Logger inst;
        return inst;
    }

    void setLevel(Level lvl) { min_level_ = lvl; }
    [[nodiscard]] Level getLevel() const { return min_level_; }
    void enableColor(bool enable) { use_color_ = enable; }

    template <typename... Args>
    void log(Level lvl, const char* file, int line, const char* fmt, Args&&... args) {
        if (lvl < min_level_) { return; }

        std::ostringstream oss;

        // Zaman damgası
        auto now = std::chrono::system_clock::now();
        auto ms  = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;
        auto t   = std::chrono::system_clock::to_time_t(now);
        std::tm tm_buf{};
#ifdef _WIN32
        localtime_s(&tm_buf, &t);
#else
        localtime_r(&t, &tm_buf);
#endif

        if (use_color_) { oss << LevelColor(lvl); }
        oss << std::put_time(&tm_buf, "%H:%M:%S")
            << '.' << std::setfill('0') << std::setw(3) << ms.count()
            << " [" << LevelTag(lvl) << "] ";
        if (use_color_) { oss << "\033[0m"; }

        // Basit format: {} yer tutucularını argümanlarla değiştir
        formatTo(oss, fmt, std::forward<Args>(args)...);

        // Dosya:satır (debug ve altı için)
        if (lvl <= Level::kDebug) {
            oss << "  (" << extractFilename(file) << ':' << line << ')';
        }

        oss << '\n';

        std::lock_guard<std::mutex> lock(mutex_);
        std::cerr << oss.str();
    }

public:
    Logger() = default;

private:
    Level min_level_ = Level::kInfo;
    bool  use_color_ = true;
    std::mutex mutex_;

    static const char* extractFilename(const char* path) {
        const char* p = path;
        for (const char* s = path; *s; ++s) {
            if (*s == '/' || *s == '\\') { p = s + 1; }
        }
        return p;
    }

    // Recursive format: her {} için bir argüman yerleştir
    static void formatTo(std::ostringstream& oss, const char* fmt) {
        oss << fmt;
    }

    template <typename T, typename... Rest>
    static void formatTo(std::ostringstream& oss, const char* fmt, T&& val, Rest&&... rest) {
        while (*fmt) {
            if (*fmt == '{' && *(fmt + 1) == '}') {
                oss << std::forward<T>(val);
                formatTo(oss, fmt + 2, std::forward<Rest>(rest)...);
                return;
            }
            oss << *fmt++;
        }
    }
};

} // namespace log
} // namespace sancak

// ============================================================================
// MAKROLAR
// ============================================================================

#define SANCAK_LOG(level, ...) \
    sancak::log::Logger::instance().log(level, __FILE__, __LINE__, __VA_ARGS__)

#define SANCAK_LOG_TRACE(...) SANCAK_LOG(sancak::log::Level::kTrace, __VA_ARGS__)
#define SANCAK_LOG_DEBUG(...) SANCAK_LOG(sancak::log::Level::kDebug, __VA_ARGS__)
#define SANCAK_LOG_INFO(...)  SANCAK_LOG(sancak::log::Level::kInfo,  __VA_ARGS__)
#define SANCAK_LOG_WARN(...)  SANCAK_LOG(sancak::log::Level::kWarn,  __VA_ARGS__)
#define SANCAK_LOG_ERROR(...) SANCAK_LOG(sancak::log::Level::kError, __VA_ARGS__)
#define SANCAK_LOG_FATAL(...) SANCAK_LOG(sancak::log::Level::kFatal, __VA_ARGS__)
