/**
 * @file ArduinoController.cpp
 * @brief Arduino seri port stub implementasyonu (PIMPL)
 */
#include "ArduinoController.hpp"

namespace teknofest {

// ── PIMPL iç sınıf ─────────────────────────────────────────────────────────

class ArduinoController::Impl {
public:
    explicit Impl(const std::string& port) : m_port(port) {}

    std::string m_port;
    bool m_isOpen = false;

    // İleride: HANDLE m_hSerial = INVALID_HANDLE_VALUE;
};

// ── Public API ──────────────────────────────────────────────────────────────

ArduinoController::ArduinoController(const std::string& port)
    : m_impl(std::make_unique<Impl>(port))
{}

ArduinoController::~ArduinoController() {
    close();
}

bool ArduinoController::open() {
    if (m_impl->m_isOpen) {
        return true;
    }

    if (m_impl->m_port.empty()) {
        return false;
    }

    // TODO: Win32 CreateFile ile seri port açılınca bu stub güncellenecek.
    m_impl->m_isOpen = true;
    return true;
}

void ArduinoController::close() {
    // TODO: CloseHandle ile seri port kapat
    m_impl->m_isOpen = false;
}

bool ArduinoController::isOpen() const {
    return m_impl->m_isOpen;
}

} // namespace teknofest
