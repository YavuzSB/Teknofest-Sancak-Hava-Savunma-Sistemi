#pragma once

#include <fstream>
#include <string>
#include <filesystem>
#include <chrono>
#include "TelemetryClient.hpp"

namespace teknofest {

class CsvLogger {
public:
    CsvLogger();
    ~CsvLogger();

    CsvLogger(const CsvLogger&) = delete;
    CsvLogger& operator=(const CsvLogger&) = delete;

    void logTelemetry(const AimResult& data, const std::string& combatState, float distanceM);

private:
    std::ofstream m_file;
};

} // namespace teknofest
