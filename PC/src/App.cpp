/**
 * @file App.cpp
 * @brief Ana uygulama – ImGui render döngüsü, panel düzenleri, iş mantığı
 *
 * Qt'nin Signal/Slot mekanizması yerine ImGui'nin anlık (immediate-mode)
 * if(ImGui::Button(...)) kontrol yapısı kullanılır.
 */
#include "App.hpp"

#include "imgui.h"
#include <GLFW/glfw3.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <iomanip>

namespace teknofest {

// ═══════════════════════════════════════════════════════════════════════════
//  Ctor / Dtor
// ═══════════════════════════════════════════════════════════════════════════

App::App() {
    m_hsvParams = {
        {"H_MIN",   0, 0, 179},
        {"H_MAX", 179, 0, 179},
        {"S_MIN",   0, 0, 255},
        {"S_MAX", 255, 0, 255},
        {"V_MIN",   0, 0, 255},
        {"V_MAX", 255, 0, 255},
    };
}

App::~App() {
    shutdown();
}

// ═══════════════════════════════════════════════════════════════════════════
//  initialize / shutdown
// ═══════════════════════════════════════════════════════════════════════════

bool App::initialize() {
    auto readEnv = [](const char* key) -> std::string {
#ifdef _WIN32
        char* value = nullptr;
        size_t len = 0;
        if (_dupenv_s(&value, &len, key) != 0 || value == nullptr) {
            return {};
        }
        std::string out(value);
        free(value);
        return out;
#else
        if (const char* val = std::getenv(key)) return std::string(val);
        return {};
#endif
    };

    // Ortam değişkenlerinden yapılandırma oku
    if (const auto h = readEnv("RASPI_HOST"); !h.empty()) m_serverHost = h;
    if (const auto vp = readEnv("VIDEO_UDP_PORT"); !vp.empty()) m_videoPort = static_cast<uint16_t>(std::atoi(vp.c_str()));
    if (const auto tp = readEnv("TEL_TCP_PORT"); !tp.empty()) m_telemetryPort = static_cast<uint16_t>(std::atoi(tp.c_str()));
    if (const auto ap = readEnv("ARDUINO_PORT"); !ap.empty()) m_arduinoPort = ap;

    setupTheme();

    // Video texture oluştur
    m_videoTexture = TextureHelper::create();

    // Video alıcı
    m_video = std::make_unique<VideoReceiver>(m_videoPort);
    m_video->start();

    // Telemetri istemcisi
    m_telemetry = std::make_unique<TelemetryClient>(m_serverHost, m_telemetryPort);
    m_telemetry->setLogCallback([this](int level, const std::string& msg) {
        LogLevel lvl = LogLevel::Info;
        switch (level) {
        case 0: lvl = LogLevel::Info;  break;
        case 1: lvl = LogLevel::Warn;  break;
        case 2: lvl = LogLevel::Error; break;
        case 3: lvl = LogLevel::Debug; break;
        default: lvl = LogLevel::Info; break;
        }
        this->addLog(lvl, msg);
    });
    m_telemetry->start();

    // Arduino (port belirtilmişse)
    if (!m_arduinoPort.empty()) {
        m_arduino = std::make_unique<ArduinoController>(m_arduinoPort);
        m_arduino->open();
    }

    return true;
}

void App::shutdown() {
    if (m_video)     { m_video->stop();     m_video.reset();     }
    if (m_telemetry) { m_telemetry->stop(); m_telemetry.reset(); }
    if (m_arduino)   { m_arduino->close();  m_arduino.reset();   }

    if (m_videoTexture) {
        TextureHelper::destroy(m_videoTexture);
        m_videoTexture = 0;
    }
}

// ═══════════════════════════════════════════════════════════════════════════
//  Canlı Log Konsolu
// ═══════════════════════════════════════════════════════════════════════════

void App::addLog(LogLevel level, const std::string& msg) {
    std::lock_guard<std::mutex> lock(m_consoleMutex);
    m_consoleLogs.emplace_back(level, msg);
    if (m_consoleLogs.size() > kConsoleMaxLines) {
        const size_t extra = m_consoleLogs.size() - kConsoleMaxLines;
        m_consoleLogs.erase(m_consoleLogs.begin(), m_consoleLogs.begin() + static_cast<std::ptrdiff_t>(extra));
    }
    if (m_consoleAutoScroll) {
        m_consoleScrollToBottom = true;
    }
}

// ═══════════════════════════════════════════════════════════════════════════
//  Neon Sancak Teması
// ═══════════════════════════════════════════════════════════════════════════

void App::setupTheme() {
    ImGuiStyle& style = ImGui::GetStyle();

    // Köşe yuvarlaklıkları
    style.WindowRounding    = 8.0f;
    style.FrameRounding     = 6.0f;
    style.GrabRounding      = 4.0f;
    style.TabRounding       = 6.0f;
    style.ChildRounding     = 6.0f;
    style.PopupRounding     = 6.0f;
    style.ScrollbarRounding = 4.0f;

    // İç dolgu
    style.WindowPadding    = ImVec2(16, 16);
    style.FramePadding     = ImVec2(12, 8);
    style.ItemSpacing      = ImVec2(12, 8);
    style.ItemInnerSpacing = ImVec2(8, 6);

    // Kenarlıklar
    style.WindowBorderSize = 1.0f;
    style.FrameBorderSize  = 1.0f;
    style.TabBorderSize    = 1.0f;

    // ── Renk paleti ─────────────────────────────────────────────────────────
    ImVec4* c = style.Colors;

    const ImVec4 bg       {0.102f, 0.110f, 0.125f, 1.00f};   // #1a1c20
    const ImVec4 bgChild  {0.118f, 0.125f, 0.145f, 1.00f};
    const ImVec4 text     {0.961f, 0.965f, 0.973f, 1.00f};   // #f5f6f8
    const ImVec4 green    {0.000f, 1.000f, 0.255f, 1.00f};   // #00ff41
    const ImVec4 greenDim {0.000f, 0.700f, 0.180f, 1.00f};
    const ImVec4 border   {1.000f, 1.000f, 1.000f, 0.18f};
    const ImVec4 frameBg  {1.000f, 1.000f, 1.000f, 0.06f};
    const ImVec4 frameHov {1.000f, 1.000f, 1.000f, 0.10f};
    const ImVec4 frameAct {1.000f, 1.000f, 1.000f, 0.05f};
    const ImVec4 header   {0.000f, 1.000f, 0.255f, 0.15f};
    const ImVec4 headerH  {0.000f, 1.000f, 0.255f, 0.25f};
    const ImVec4 headerA  {0.000f, 1.000f, 0.255f, 0.35f};

    c[ImGuiCol_WindowBg]           = bg;
    c[ImGuiCol_ChildBg]            = bgChild;
    c[ImGuiCol_PopupBg]            = {0.12f, 0.13f, 0.15f, 0.95f};
    c[ImGuiCol_Text]               = text;
    c[ImGuiCol_TextDisabled]       = {0.50f, 0.50f, 0.50f, 1.00f};
    c[ImGuiCol_Border]             = border;
    c[ImGuiCol_BorderShadow]       = {0, 0, 0, 0};

    c[ImGuiCol_FrameBg]            = frameBg;
    c[ImGuiCol_FrameBgHovered]     = frameHov;
    c[ImGuiCol_FrameBgActive]      = frameAct;

    c[ImGuiCol_TitleBg]            = bg;
    c[ImGuiCol_TitleBgActive]      = {0.13f, 0.14f, 0.16f, 1.0f};
    c[ImGuiCol_TitleBgCollapsed]   = bg;
    c[ImGuiCol_MenuBarBg]          = bg;

    c[ImGuiCol_ScrollbarBg]            = {0, 0, 0, 0.10f};
    c[ImGuiCol_ScrollbarGrab]          = {0.3f, 0.3f, 0.3f, 0.50f};
    c[ImGuiCol_ScrollbarGrabHovered]   = {0.4f, 0.4f, 0.4f, 0.70f};
    c[ImGuiCol_ScrollbarGrabActive]    = green;

    c[ImGuiCol_CheckMark]          = green;
    c[ImGuiCol_SliderGrab]         = greenDim;
    c[ImGuiCol_SliderGrabActive]   = green;

    c[ImGuiCol_Button]             = frameBg;
    c[ImGuiCol_ButtonHovered]      = frameHov;
    c[ImGuiCol_ButtonActive]       = frameAct;

    c[ImGuiCol_Header]             = header;
    c[ImGuiCol_HeaderHovered]      = headerH;
    c[ImGuiCol_HeaderActive]       = headerA;

    c[ImGuiCol_Separator]          = border;
    c[ImGuiCol_SeparatorHovered]   = green;
    c[ImGuiCol_SeparatorActive]    = green;

    c[ImGuiCol_Tab]                = {0.15f, 0.16f, 0.18f, 1.0f};
    c[ImGuiCol_TabHovered]         = headerH;

    c[ImGuiCol_ResizeGrip]         = {0.0f, 1.0f, 0.255f, 0.45f};
    c[ImGuiCol_ResizeGripHovered]  = green;
    c[ImGuiCol_ResizeGripActive]   = green;

    c[ImGuiCol_PlotLines]          = green;
    c[ImGuiCol_PlotLinesHovered]   = {0.0f, 1.0f, 0.4f, 1.0f};
    c[ImGuiCol_PlotHistogram]      = green;
    c[ImGuiCol_PlotHistogramHovered] = {0.0f, 1.0f, 0.4f, 1.0f};

    c[ImGuiCol_DockingPreview]     = {0.0f, 1.0f, 0.255f, 0.30f};
    c[ImGuiCol_DockingEmptyBg]     = bg;
}

// ═══════════════════════════════════════════════════════════════════════════
//  Durum çubuğu
// ═══════════════════════════════════════════════════════════════════════════

void App::setStatus(const std::string& message, float durationSec) {
    m_statusMessage = message;
    m_statusExpiry  = std::chrono::steady_clock::now() +
                      std::chrono::milliseconds(static_cast<int>(durationSec * 1000.0f));
}

// ═══════════════════════════════════════════════════════════════════════════
//  Ana render (her karede çağrılır)
// ═══════════════════════════════════════════════════════════════════════════

void App::render(GLFWwindow* window) {
    m_window = window;

    // ── Video texture güncelle ──────────────────────────────────────────────
    int fw = 0, fh = 0;
    if (m_video && m_video->getLatestFrame(m_frameBuffer, fw, fh)) {
        TextureHelper::update(m_videoTexture, m_frameBuffer.data(), fw, fh, 3);
        m_videoWidth  = fw;
        m_videoHeight = fh;
    }

    // ── Telemetri güncelle ──────────────────────────────────────────────────
    TelemetryData telData;
    if (m_telemetry && m_telemetry->getLatestTelemetry(telData)) {
        if (!telData.speed.empty())     m_motorSpeed     = telData.speed     + " m/s";
        if (!telData.direction.empty()) m_motorDirection = telData.direction + " deg";
        if (!telData.mode.empty())      m_motorFire      = telData.mode;
    }

    // ── Klavye (WASD) ───────────────────────────────────────────────────────
    if (!ImGui::GetIO().WantCaptureKeyboard) {
        struct KeyCmd { ImGuiKey key; const char* cmd; const char* label; };
        static constexpr KeyCmd kKeys[] = {
            {ImGuiKey_W, "<MOVE:FORWARD>", "MOVE:FORWARD"},
            {ImGuiKey_S, "<MOVE:BACK>",    "MOVE:BACK"},
            {ImGuiKey_A, "<MOVE:LEFT>",    "MOVE:LEFT"},
            {ImGuiKey_D, "<MOVE:RIGHT>",   "MOVE:RIGHT"},
        };
        for (const auto& [key, cmd, label] : kKeys) {
            if (ImGui::IsKeyPressed(key, false)) {
                if (m_telemetry) m_telemetry->sendCommand(cmd);
                setStatus(label, 0.8f);
            }
        }
    }

    // ── Tam ekran ana pencere ───────────────────────────────────────────────
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(vp->WorkSize);
    ImGui::SetNextWindowViewport(vp->ID);

    constexpr ImGuiWindowFlags kHostFlags =
        ImGuiWindowFlags_NoTitleBar   | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoResize     | ImGuiWindowFlags_NoMove    |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoNavFocus   | ImGuiWindowFlags_NoBackground;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12, 12));
    ImGui::Begin("##MainHost", nullptr, kHostFlags);
    ImGui::PopStyleVar();

    const float totalW = ImGui::GetContentRegionAvail().x;
    const float totalH = ImGui::GetContentRegionAvail().y - 32.0f;   // status bar
    const float videoW = totalW * 0.74f;
    const float sideW  = totalW - videoW - ImGui::GetStyle().ItemSpacing.x;

    // ── Sol: Video paneli ──────────────────────────────────────────────
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.0f, 1.0f, 0.255f, 0.45f));
    ImGui::BeginChild("##VideoPanel", ImVec2(videoW, totalH), true);
    renderVideoPanel();
    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::SameLine();

    // ── Sağ: Sekme paneli ──────────────────────────────────────────────
    ImGui::BeginChild("##Sidebar", ImVec2(sideW, totalH), true);

    if (ImGui::BeginTabBar("##MainTabs")) {
        if (ImGui::BeginTabItem("KONTROL")) {
            m_activeTab = 0;
            renderControlPanel();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("GOREV")) {
            m_activeTab = 2;
            renderMissionPanel();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("KONSOL")) {
            m_activeTab = 3;
            renderConsolePanel();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("KALIBRASYON")) {
            m_activeTab = 1;
            renderCalibrationPanel();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    ImGui::EndChild();

    // ── Alt: Durum çubuğu ───────────────────────────────────────────────
    renderStatusBar();

    ImGui::End();
}

// ═══════════════════════════════════════════════════════════════════════════
//  Video Paneli
// ═══════════════════════════════════════════════════════════════════════════

void App::renderVideoPanel() {
    const ImVec2 avail = ImGui::GetContentRegionAvail();

    if (m_videoWidth > 0 && m_videoHeight > 0) {
        // Aspect-fit hesapla
        const float aspect = static_cast<float>(m_videoWidth) /
                             static_cast<float>(m_videoHeight);
        float displayW = avail.x;
        float displayH = displayW / aspect;

        if (displayH > avail.y) {
            displayH = avail.y;
            displayW = displayH * aspect;
        }

        // Ortala
        const float offX = (avail.x - displayW) * 0.5f;
        const float offY = (avail.y - displayH) * 0.5f;
        if (offX > 0) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offX);
        if (offY > 0) ImGui::SetCursorPosY(ImGui::GetCursorPosY() + offY);

        ImGui::Image(
            static_cast<ImTextureID>(static_cast<uintptr_t>(m_videoTexture)),
            ImVec2(displayW, displayH));

        // HUD Overlay: Telemetri AimResult ile crosshair ve bilgi etiketi çiz
        if (m_telemetry && m_overlayEnabled) {
            teknofest::AimResult aim;
            if (m_telemetry->getLatestAimResult(aim) && aim.valid) {
                // Frame koordinatlarını ImGui paneline orantıla
                float scaleX = displayW / static_cast<float>(m_videoWidth);
                float scaleY = displayH / static_cast<float>(m_videoHeight);
                float cx = aim.corrected_x * scaleX + offX;
                float cy = aim.corrected_y * scaleY + offY;

                ImDrawList* draw = ImGui::GetWindowDrawList();
                ImVec2 panelPos = ImGui::GetCursorScreenPos();
                // ImGui::Image sonrası cursor panelin altına iner, üst köşe için -displayH kadar yukarı git
                panelPos.y -= displayH;

                ImVec2 crosshairPos(panelPos.x + cx, panelPos.y + cy);
                ImU32 color = IM_COL32(0, 255, 65, 255); // Neon yeşil
                float crossLen = 18.0f;
                float crossThick = 2.0f;
                // Crosshair çiz
                draw->AddLine(ImVec2(crosshairPos.x - crossLen, crosshairPos.y), ImVec2(crosshairPos.x + crossLen, crosshairPos.y), color, crossThick);
                draw->AddLine(ImVec2(crosshairPos.x, crosshairPos.y - crossLen), ImVec2(crosshairPos.x, crosshairPos.y + crossLen), color, crossThick);

                // Info label
                char label[64];
                snprintf(label, sizeof(label), "ID:%d  (%.0f,%.0f)", aim.class_id, aim.corrected_x, aim.corrected_y);
                ImVec2 textSz = ImGui::CalcTextSize(label);
                ImVec2 labelPos(crosshairPos.x - textSz.x * 0.5f, crosshairPos.y - crossLen - textSz.y - 6.0f);
                const float padX = 4.0f;
                const float padY = 2.0f;
                const ImVec2 rectMin(labelPos.x - padX, labelPos.y - padY);
                const ImVec2 rectMax(labelPos.x + textSz.x + padX, labelPos.y + textSz.y + padY);
                draw->AddRectFilled(rectMin, rectMax, IM_COL32(16,20,28,220), 4.0f);
                draw->AddText(labelPos, color, label);
            } else if (m_telemetry) {
                // Geçersiz nişan: kırmızımsı crosshair (son bilinen nokta varsa)
                teknofest::AimResult lastAim;
                if (m_telemetry->getLatestAimResult(lastAim)) {
                    float scaleX = displayW / static_cast<float>(m_videoWidth);
                    float scaleY = displayH / static_cast<float>(m_videoHeight);
                    float cx = lastAim.corrected_x * scaleX + offX;
                    float cy = lastAim.corrected_y * scaleY + offY;
                    ImDrawList* draw = ImGui::GetWindowDrawList();
                    ImVec2 panelPos = ImGui::GetCursorScreenPos();
                    panelPos.y -= displayH;
                    ImVec2 crosshairPos(panelPos.x + cx, panelPos.y + cy);
                    ImU32 color = IM_COL32(255, 60, 60, 220);
                    float crossLen = 14.0f;
                    float crossThick = 2.0f;
                    draw->AddLine(ImVec2(crosshairPos.x - crossLen, crosshairPos.y), ImVec2(crosshairPos.x + crossLen, crosshairPos.y), color, crossThick);
                    draw->AddLine(ImVec2(crosshairPos.x, crosshairPos.y - crossLen), ImVec2(crosshairPos.x, crosshairPos.y + crossLen), color, crossThick);
                }
            }
        }
    } else {
        // Yer tutucu
        const ImVec2 textSz = ImGui::CalcTextSize("Beklemede - video yok");
        ImGui::SetCursorPos(ImVec2(
            (avail.x - textSz.x) * 0.5f,
            (avail.y - textSz.y) * 0.5f));
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f),
                           "Beklemede - video yok");
    }
}

// ═══════════════════════════════════════════════════════════════════════════
//  Kontrol Paneli
// ═══════════════════════════════════════════════════════════════════════════

void App::renderControlPanel() {
    const float buttonH = 44.0f;
    const float fullW   = ImGui::GetContentRegionAvail().x;

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 20.0f);

    // ── Butonlar ────────────────────────────────────────────────────────────
    if (ImGui::Button(m_connected ? "TESPITI DURDUR" : "OTONOM HEDEF TESPIT",
                      ImVec2(fullW, buttonH))) {
        toggleAutonomousDetection();
    }

    if (ImGui::Button(m_fullAuto ? "TAM OTONOMU DURDUR" : "TAM OTONOM",
                      ImVec2(fullW, buttonH))) {
        toggleFullAutonomous();
    }

    ImGui::Button("YASAKLI ALAN BELIRLE", ImVec2(fullW, buttonH));

    if (ImGui::Button("ALAN BILGISI AL", ImVec2(fullW, buttonH))) {
        updateAreaFake();
    }

    if (ImGui::Button("NESNE BILGISI AL", ImVec2(fullW, buttonH))) {
        updateMotorFake();
    }

    // Çıkış butonu (kırmızımsı hover)
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.6f, 0.1f, 0.1f, 0.7f));
    if (ImGui::Button("CIKIS", ImVec2(fullW, buttonH))) {
        m_shouldQuit = true;
    }
    ImGui::PopStyleColor();

    ImGui::PopStyleVar();   // FrameRounding

    ImGui::Spacing();

    // ── Overlay checkbox ────────────────────────────────────────────────────
    const bool overlayPrev = m_overlayEnabled;
    ImGui::Checkbox("Kareleri Goster", &m_overlayEnabled);
    if (m_overlayEnabled != overlayPrev && m_connected && m_telemetry) {
        m_telemetry->sendCommand(m_overlayEnabled ? "<OVERLAY:ON>" : "<OVERLAY:OFF>");
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // ── Motor Bilgisi ───────────────────────────────────────────────────────
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.0f, 1.0f, 0.255f, 0.6f));
    ImGui::BeginChild("##MotorInfo", ImVec2(fullW, 110), true);

    ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.255f, 1.0f), "MOTOR BILGISI");
    ImGui::Separator();

    const ImVec4 valColor(0.8f, 0.9f, 0.8f, 1.0f);
    ImGui::Text("Hiz:");  ImGui::SameLine(80); ImGui::TextColored(valColor, "%s", m_motorSpeed.c_str());
    ImGui::Text("Yon:");  ImGui::SameLine(80); ImGui::TextColored(valColor, "%s", m_motorDirection.c_str());
    ImGui::Text("Atis:"); ImGui::SameLine(80); ImGui::TextColored(valColor, "%s", m_motorFire.c_str());

    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::Spacing();

    // ── Alan Bilgisi ────────────────────────────────────────────────────────
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.0f, 1.0f, 0.255f, 0.6f));
    ImGui::BeginChild("##AreaInfo", ImVec2(fullW, 110), true);

    ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.255f, 1.0f), "ALAN BILGISI");
    ImGui::Separator();

    ImGui::Text("Merkez:"); ImGui::SameLine(80); ImGui::TextColored(valColor, "%s", m_areaCenter.c_str());
    ImGui::Text("Boyut:");  ImGui::SameLine(80); ImGui::TextColored(valColor, "%s", m_areaSize.c_str());
    ImGui::Text("ID:");     ImGui::SameLine(80); ImGui::TextColored(valColor, "%s", m_areaId.c_str());

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ═══════════════════════════════════════════════════════════════════════════
//  Kalibrasyon Paneli
// ═══════════════════════════════════════════════════════════════════════════

void App::renderCalibrationPanel() {
    const float fullW = ImGui::GetContentRegionAvail().x;

    for (auto& param : m_hsvParams) {
        ImGui::Text("%s:", param.name.c_str());

        ImGui::PushItemWidth(fullW * 0.70f);

        const std::string sliderId = "##slider_" + param.name;
        const int prevVal = param.value;

        if (ImGui::SliderInt(sliderId.c_str(), &param.value,
                             param.minVal, param.maxVal)) {
            if (param.value != prevVal && m_telemetry) {
                m_telemetry->sendCommand(
                    "<SET:" + param.name + "," + std::to_string(param.value) + ">");
            }
        }

        ImGui::PopItemWidth();

        ImGui::SameLine();
        ImGui::Text("%d", param.value);

        ImGui::Spacing();
    }
}

// ═══════════════════════════════════════════════════════════════════════════
//  Canlı Log Konsolu Paneli
// ═══════════════════════════════════════════════════════════════════════════

void App::renderConsolePanel() {
    const float fullW = ImGui::GetContentRegionAvail().x;

    // Üst bar
    if (ImGui::Button("Temizle", ImVec2(120.0f, 0.0f))) {
        std::lock_guard<std::mutex> lock(m_consoleMutex);
        m_consoleLogs.clear();
        m_consoleScrollToBottom = true;
    }
    ImGui::SameLine();
    ImGui::Checkbox("Auto-Scroll", &m_consoleAutoScroll);
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "(%zu/%zu)", m_consoleLogs.size(), kConsoleMaxLines);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Scrollable log bölgesi
    ImGui::BeginChild("LogRegion", ImVec2(fullW, 0.0f), true, ImGuiWindowFlags_HorizontalScrollbar);

    // Logları güvenli şekilde snapshot al
    std::vector<std::pair<LogLevel, std::string>> snapshot;
    snapshot.reserve(256);
    {
        std::lock_guard<std::mutex> lock(m_consoleMutex);
        snapshot = m_consoleLogs;
    }

    for (const auto& [lvl, line] : snapshot) {
        ImVec4 col(0.95f, 0.95f, 0.95f, 1.0f);
        switch (lvl) {
        case LogLevel::Info:  col = ImVec4(0.95f, 0.95f, 0.95f, 1.0f); break;
        case LogLevel::Warn:  col = ImVec4(1.00f, 0.85f, 0.10f, 1.0f); break;
        case LogLevel::Error: col = ImVec4(1.00f, 0.25f, 0.25f, 1.0f); break;
        case LogLevel::Debug: col = ImVec4(0.65f, 0.85f, 1.00f, 1.0f); break;
        }
        ImGui::PushStyleColor(ImGuiCol_Text, col);
        ImGui::TextUnformatted(line.c_str());
        ImGui::PopStyleColor();
    }

    // Kullanıcı yukarı kaydırdıysa auto-scroll'u durdur
    const float scrollY = ImGui::GetScrollY();
    const float scrollMaxY = ImGui::GetScrollMaxY();
    const bool atBottom = (scrollMaxY <= 0.0f) || (scrollY >= scrollMaxY - 2.0f);
    if (m_consoleAutoScroll && !atBottom && ImGui::IsWindowHovered() && (ImGui::GetIO().MouseWheel != 0.0f)) {
        m_consoleAutoScroll = false;
    }

    if (m_consoleAutoScroll && m_consoleScrollToBottom) {
        ImGui::SetScrollHereY(1.0f);
        m_consoleScrollToBottom = false;
    }

    ImGui::EndChild();
}

// ═══════════════════════════════════════════════════════════════════════════
//  Görev / Geofencing Paneli
// ═══════════════════════════════════════════════════════════════════════════

void App::renderMissionPanel() {
    const float fullW = ImGui::GetContentRegionAvail().x;

    // ── Aşama 1: Hedef sıralaması ──────────────────────────────────────────
    if (ImGui::CollapsingHeader("Asama 1: Hedef Siralamasi", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.255f, 1.0f), "GOREV ONCELIGI");
        ImGui::Separator();
        ImGui::Spacing();

        for (int i = 0; i < static_cast<int>(m_missionOrder.size()); ++i) {
            ImGui::PushID(i);

            ImGui::Text("%d.", i + 1);
            ImGui::SameLine(40);
            ImGui::TextColored(ImVec4(0.85f, 0.95f, 0.90f, 1.0f), "%s", m_missionOrder[i].c_str());

            ImGui::SameLine(fullW - 140.0f);
            const bool canUp = (i > 0);
            const bool canDown = (i < static_cast<int>(m_missionOrder.size()) - 1);

            if (!canUp) ImGui::BeginDisabled();
            if (ImGui::SmallButton("Yukari")) {
                std::swap(m_missionOrder[i], m_missionOrder[i - 1]);
            }
            if (!canUp) ImGui::EndDisabled();

            ImGui::SameLine();

            if (!canDown) ImGui::BeginDisabled();
            if (ImGui::SmallButton("Asagi")) {
                std::swap(m_missionOrder[i], m_missionOrder[i + 1]);
            }
            if (!canDown) ImGui::EndDisabled();

            ImGui::PopID();
        }

        ImGui::Spacing();
        if (ImGui::Button("Siralamayi Gonder", ImVec2(fullW, 40.0f))) {
            std::ostringstream oss;
            oss << "<ORDER:";
            for (size_t i = 0; i < m_missionOrder.size(); ++i) {
                if (i) oss << ',';
                oss << m_missionOrder[i];
            }
            oss << '>';
            if (m_telemetry) m_telemetry->sendCommand(oss.str());
            setStatus("Gorev siralamasi gonderildi", 1.2f);
        }
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // ── Geofencing ─────────────────────────────────────────────────────────
    if (ImGui::CollapsingHeader("Geofencing (Sanal Sinir) Ayarlari", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text("Pan limitleri (deg)");
        ImGui::PushItemWidth(fullW);
        ImGui::SliderFloat2("##pan_limits", m_panLimits, -180.0f, 180.0f, "%.1f");

        ImGui::Spacing();
        ImGui::Text("Tilt limitleri (deg)");
        ImGui::SliderFloat2("##tilt_limits", m_tiltLimits, -45.0f, 90.0f, "%.1f");
        ImGui::PopItemWidth();

        // Kullanıcı min/max'ı ters girerse düzelt
        if (m_panLimits[0] > m_panLimits[1]) std::swap(m_panLimits[0], m_panLimits[1]);
        if (m_tiltLimits[0] > m_tiltLimits[1]) std::swap(m_tiltLimits[0], m_tiltLimits[1]);

        ImGui::Spacing();
        if (ImGui::Button("Sinirlari Uygula", ImVec2(fullW, 40.0f))) {
            char cmd[128];
            std::snprintf(cmd, sizeof(cmd), "<GEOFENCE:%.1f,%.1f,%.1f,%.1f>",
                          m_panLimits[0], m_panLimits[1], m_tiltLimits[0], m_tiltLimits[1]);
            if (m_telemetry) m_telemetry->sendCommand(cmd);
            setStatus("Geofence gonderildi", 1.2f);
        }
    }
}

// ═══════════════════════════════════════════════════════════════════════════
//  Durum Çubuğu
// ═══════════════════════════════════════════════════════════════════════════

void App::renderStatusBar() {
    // Süresi dolmuş mesajı temizle
    if (!m_statusMessage.empty() &&
        std::chrono::steady_clock::now() > m_statusExpiry) {
        m_statusMessage.clear();
    }

    // Video durumu
    std::string videoState = "Video: ";
    if (m_video) {
        switch (m_video->getState()) {
        case VideoReceiver::State::Idle:      videoState += "Bekleniyor";  break;
        case VideoReceiver::State::Listening:  videoState += "Dinleniyor";  break;
        case VideoReceiver::State::Receiving:  videoState += "Aliniyor";    break;
        case VideoReceiver::State::Error:      videoState += "Hata!";       break;
        case VideoReceiver::State::Stopped:    videoState += "Durdu";       break;
        }
    }

    // Telemetri durumu
    std::string telState = "Telemetri: ";
    if (m_telemetry) {
        const auto cur = m_telemetry->getState();

        // Durum değişimini logla
        if (!m_prevTelStateValid || cur != m_prevTelState) {
            m_prevTelStateValid = true;
            m_prevTelState = cur;
            switch (cur) {
            case TelemetryClient::State::Connected:
                addLog(LogLevel::Info, "Telemetri baglandi");
                break;
            case TelemetryClient::State::Connecting:
                addLog(LogLevel::Info, "Telemetri baglaniyor...");
                break;
            case TelemetryClient::State::Retrying:
                addLog(LogLevel::Warn, "Telemetri yeniden deneniyor");
                break;
            case TelemetryClient::State::Disconnected:
                addLog(LogLevel::Error, "Telemetri baglantisi koptu");
                break;
            case TelemetryClient::State::Stopped:
                addLog(LogLevel::Warn, "Telemetri durduruldu");
                break;
            }
        }

        switch (cur) {
        case TelemetryClient::State::Disconnected: telState += "Bagli degil";       break;
        case TelemetryClient::State::Connecting:   telState += "Baglaniyor...";     break;
        case TelemetryClient::State::Connected:    telState += "Bagli";             break;
        case TelemetryClient::State::Retrying:     telState += "Yeniden deniyor";   break;
        case TelemetryClient::State::Stopped:      telState += "Durdu";             break;
        }
    }

    ImGui::Separator();

    const ImVec4 activeColor (0.0f, 1.0f, 0.255f, 1.0f);
    const ImVec4 dimColor    (0.6f, 0.6f, 0.6f,   1.0f);

    const bool videoActive = m_video &&
        m_video->getState() == VideoReceiver::State::Receiving;
    ImGui::TextColored(videoActive ? activeColor : dimColor,
                       "%s", videoState.c_str());

    ImGui::SameLine(0, 24);

    const bool telActive = m_telemetry &&
        m_telemetry->getState() == TelemetryClient::State::Connected;
    ImGui::TextColored(telActive ? activeColor : dimColor,
                       "%s", telState.c_str());

    if (!m_statusMessage.empty()) {
        ImGui::SameLine(0, 24);
        ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.0f, 1.0f),
                           "| %s", m_statusMessage.c_str());
    }
}

// ═══════════════════════════════════════════════════════════════════════════
//  Aksiyonlar (Qt Signal/Slot → ImGui if(Button) karşılıkları)
// ═══════════════════════════════════════════════════════════════════════════

void App::toggleAutonomousDetection() {
    if (!m_connected) {
        if (m_telemetry) m_telemetry->sendCommand("<DETECT:START>");
        m_connected = true;
        setStatus("Tespit baslatildi", 1.5f);
    } else {
        if (m_telemetry) m_telemetry->sendCommand("<DETECT:STOP>");
        m_connected = false;
        setStatus("Tespit durduruldu", 1.5f);
    }
}

void App::toggleFullAutonomous() {
    if (!m_fullAuto) {
        if (!m_connected) {
            if (m_telemetry) m_telemetry->sendCommand("<DETECT:START>");
            m_connected = true;
        }
        if (m_telemetry) m_telemetry->sendCommand("<MODE:FULL_AUTO>");
        m_fullAuto = true;
        setStatus("Tam otonom mod acik", 2.5f);
    } else {
        if (m_telemetry) m_telemetry->sendCommand("<MODE:MANUAL>");
        m_fullAuto = false;
        setStatus("Tam otonom mod kapali", 2.0f);
    }
}

void App::updateMotorFake() {
    m_motorSpeed     = "2.4 m/s";
    m_motorDirection = "15 deg";
    m_motorFire      = "Hazir";
    setStatus("Nesne bilgisi alindi (ornek)", 1.5f);
}

void App::updateAreaFake() {
    m_areaCenter = "(120, 80)";
    m_areaSize   = "300 x 180";
    m_areaId     = "ALAN-42";
    setStatus("Alan bilgisi guncellendi", 1.5f);
}

} // namespace teknofest
