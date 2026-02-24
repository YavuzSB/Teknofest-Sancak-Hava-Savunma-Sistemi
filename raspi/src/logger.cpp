#include "logger.hpp"
#include <iomanip>
#include <iostream>
#include <filesystem>

namespace sancak {

Logger& Logger::instance() {
    static Logger inst;
    return inst;
}

Logger::Logger() {
    std::filesystem::create_directories("logs");
    m_logFile.open("logs/system.log", std::ios::app);
    m_thread = std::thread(&Logger::writerThread, this);
}

Logger::~Logger() {
    shutdown();
}

void Logger::shutdown() {
    m_running.store(false);
    if (m_thread.joinable()) m_thread.join();
    // Kuyruktaki son logları yaz
    std::lock_guard<std::mutex> lock(m_mutex);
    while (!m_queue.empty()) {
        auto& [level, msg] = m_queue.front();
        std::string line = formatLine(level, msg);
        if (m_logFile.is_open()) m_logFile << line << std::endl;
        std::cout << line << std::endl;
        m_queue.pop();
    }
    m_logFile.close();
}

void Logger::log(LogLevel level, const std::string& msg) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_queue.emplace(level, msg);
}

std::string Logger::formatLine(LogLevel level, const std::string& msg) {
    using namespace std::chrono;
    auto now = system_clock::now();
    auto ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;
    std::time_t t = system_clock::to_time_t(now);
    std::tm tm{};
    localtime_r(&t, &tm);
    std::ostringstream oss;
    oss << '[' << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") << '.' << std::setw(3) << std::setfill('0') << ms.count() << "] ";
    switch (level) {
        case LogLevel::Info:  oss << "[INFO] "; break;
        case LogLevel::Warn:  oss << "[WARN] "; break;
        case LogLevel::Error: oss << "[ERROR] "; break;
        case LogLevel::Debug: oss << "[DEBUG] "; break;
    }
    oss << msg;
    return oss.str();
}

void Logger::writerThread() {
    using namespace std::chrono_literals;
    while (m_running.load()) {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            while (!m_queue.empty()) {
                auto& [level, msg] = m_queue.front();
                std::string line = formatLine(level, msg);
                if (m_logFile.is_open()) m_logFile << line << std::endl;
                std::cout << line << std::endl;
                m_queue.pop();
            }
        }
        std::this_thread::sleep_for(10ms);
    }
}

} // namespace sancak
