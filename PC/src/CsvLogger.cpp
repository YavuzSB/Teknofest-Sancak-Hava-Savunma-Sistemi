#include "CsvLogger.hpp"
#include <iomanip>
#include <sstream>

namespace teknofest {

CsvLogger::CsvLogger() {
    std::filesystem::create_directories("logs");
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
    localtime_s(&tm, &t);
    std::ostringstream oss;
    oss << "logs/telemetry_" << std::put_time(&tm, "%Y%m%d_%H%M%S") << ".csv";
    m_file.open(oss.str(), std::ios::out);
    // CSV header
    m_file << "Timestamp,CombatState,TargetID,RawX,RawY,CorrectedX,CorrectedY,DistanceM\n";
    m_file.flush();
}

CsvLogger::~CsvLogger() {
    m_file.flush();
    m_file.close();
}

void CsvLogger::logTelemetry(const AimResult& data, const std::string& combatState, float distanceM) {
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
    localtime_s(&tm, &t);
    char timestamp[32];
    std::strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &tm);
    m_file << timestamp << '.' << std::setw(3) << std::setfill('0') << ms.count() << ',';
    m_file << combatState << ',';
    m_file << data.class_id << ',';
    m_file << data.raw_x << ',' << data.raw_y << ',';
    m_file << data.corrected_x << ',' << data.corrected_y << ',';
    m_file << distanceM << '\n';
    m_file.flush();
}

} // namespace teknofest
