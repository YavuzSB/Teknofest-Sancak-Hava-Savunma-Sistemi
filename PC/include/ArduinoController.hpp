#pragma once

#include <string>
#include <memory>

namespace teknofest {

/**
 * @brief Arduino seri port iletişim kontrolcüsü (PIMPL)
 *
 * Şu anda stub implementasyon. İleride Win32 CreateFile ile
 * gerçek seri port haberleşmesi eklenecek.
 */
class ArduinoController final {
public:
    explicit ArduinoController(const std::string& port);
    ~ArduinoController();

    ArduinoController(const ArduinoController&) = delete;
    ArduinoController& operator=(const ArduinoController&) = delete;

    bool open();
    void close();
    [[nodiscard]] bool isOpen() const;

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace teknofest
