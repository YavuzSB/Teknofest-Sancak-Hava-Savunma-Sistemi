#pragma once

#include <string>
#include <mutex>
#include <queue>
#include <thread>
#include <atomic>
#include <chrono>
#include <fstream>

namespace sancak {

enum class LogLevel {
    Info,
    Warn,
    Error,
    Debug
};

class Logger {
public:
    static Logger& instance();

    void log(LogLevel level, const std::string& msg);
    void shutdown();

private:
    Logger();
    ~Logger();
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    void writerThread();
    std::string formatLine(LogLevel level, const std::string& msg);

    std::queue<std::pair<LogLevel, std::string>> m_queue;
    std::mutex m_mutex;
    std::atomic<bool> m_running{true};
    std::thread m_thread;
    std::ofstream m_logFile;
};

} // namespace sancak

// Log makroları
#define SANCAK_LOG_INFO(msg)   sancak::Logger::instance().log(sancak::LogLevel::Info,   msg)
#define SANCAK_LOG_WARN(msg)   sancak::Logger::instance().log(sancak::LogLevel::Warn,   msg)
#define SANCAK_LOG_ERROR(msg)  sancak::Logger::instance().log(sancak::LogLevel::Error,  msg)
#define SANCAK_LOG_DEBUG(msg)  sancak::Logger::instance().log(sancak::LogLevel::Debug,  msg)
