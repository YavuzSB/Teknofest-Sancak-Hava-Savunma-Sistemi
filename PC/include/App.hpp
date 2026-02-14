#pragma once

#include "VideoReceiver.hpp"
#include "TelemetryClient.hpp"
#include "ArduinoController.hpp"
#include "TextureHelper.hpp"

#include <memory>
#include <string>
#include <vector>
#include <chrono>

struct GLFWwindow;

namespace teknofest {

/**
 * @brief Ana uygulama sınıfı – tüm ImGui render ve iş mantığını yönetir
 *
 * Mimari:
 *   main.cpp  →  GLFW/OpenGL init  →  App::initialize()
 *   her kare  →  App::render(window)  →  ImGui panelleri çizilir
 *   çıkışta   →  App::shutdown()
 */
class App final {
public:
    App();
    ~App();

    App(const App&) = delete;
    App& operator=(const App&) = delete;

    /// Alt sistemleri başlat (video, telemetri, arduino)
    bool initialize();

    /// Alt sistemleri durdur ve kaynakları serbest bırak
    void shutdown();

    /// Her karede çağrılır – tüm ImGui panellerini çizer
    void render(GLFWwindow* window);

    /// Çıkış istendi mi?
    [[nodiscard]] bool shouldQuit() const { return m_shouldQuit; }

private:
    // ── Tema ────────────────────────────────────────────────────────────────
    void setupTheme();

    // ── UI Panelleri ────────────────────────────────────────────────────────
    void renderVideoPanel();
    void renderControlPanel();
    void renderCalibrationPanel();
    void renderStatusBar();

    // ── Aksiyonlar ──────────────────────────────────────────────────────────
    void toggleAutonomousDetection();
    void toggleFullAutonomous();
    void updateMotorFake();
    void updateAreaFake();
    void setStatus(const std::string& message, float durationSec = 2.0f);

    // ── Alt sistemler ───────────────────────────────────────────────────────
    std::unique_ptr<VideoReceiver>    m_video;
    std::unique_ptr<TelemetryClient>  m_telemetry;
    std::unique_ptr<ArduinoController> m_arduino;

    // ── Video texture ───────────────────────────────────────────────────────
    TextureID            m_videoTexture = 0;
    int                  m_videoWidth   = 0;
    int                  m_videoHeight  = 0;
    std::vector<uint8_t> m_frameBuffer;

    // ── Uygulama durumu ─────────────────────────────────────────────────────
    bool m_connected      = false;
    bool m_fullAuto       = false;
    bool m_overlayEnabled = true;
    bool m_shouldQuit     = false;
    int  m_activeTab      = 0;

    // ── Motor bilgisi ───────────────────────────────────────────────────────
    std::string m_motorSpeed     = "0.0 m/s";
    std::string m_motorDirection = "0 deg";
    std::string m_motorFire      = "Hazir degil";

    // ── Alan bilgisi ────────────────────────────────────────────────────────
    std::string m_areaCenter = "(0, 0)";
    std::string m_areaSize   = "0 x 0";
    std::string m_areaId     = "-";

    // ── HSV kalibrasyon ─────────────────────────────────────────────────────
    struct HsvParam {
        std::string name;
        int value;
        int minVal;
        int maxVal;
    };
    std::vector<HsvParam> m_hsvParams;

    // ── Durum çubuğu ────────────────────────────────────────────────────────
    std::string m_statusMessage;
    std::chrono::steady_clock::time_point m_statusExpiry;

    // ── Yapılandırma ────────────────────────────────────────────────────────
    std::string m_serverHost   = "127.0.0.1";
    uint16_t    m_videoPort    = 5005;
    uint16_t    m_telemetryPort = 5000;
    std::string m_arduinoPort;

    // ── GLFW pencere referansı (render sırasında geçici) ────────────────────
    GLFWwindow* m_window = nullptr;
};

} // namespace teknofest
