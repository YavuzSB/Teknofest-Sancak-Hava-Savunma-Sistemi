/**
 * @file combat_pipeline.hpp
 * @brief Sancak Hava Savunma Sistemi - Ana Savaş Pipeline'ı
 *
 * Tüm modülleri orkestra ederek tek bir frame'den
 * nişan noktası üretir.
 *
 * Akış:
 *   Kamera → YOLO Tespit → Balon Segmentasyon → Mesafe Tahmini
 *   → Takip → Balistik Düzeltme → Nişan → Seri İletişim
 *
 * @author Sancak Takımı
 * @date 2026
 */
#pragma once

#include "sancak/types.hpp"
#include "sancak/config_manager.hpp"
#include "sancak/yolo_detector.hpp"
#include "sancak/balloon_segmentor.hpp"
#include "sancak/aim_solver.hpp"
#include "sancak/target_tracker.hpp"
#include "sancak/trigger_controller.hpp"
#include "sancak/camera_controller.hpp"
#include "sancak/serial_comm.hpp"

#include "core/combat_state_machine.hpp"
#include "turret_controller.hpp"

#include "network/udp_video_streamer.hpp"
#include "network/tcp_telemetry_server.hpp"
#include "network/telemetry_sender.hpp"

#include <memory>
#include <chrono>
#include <string>
#include <optional>
#include <mutex>
#include <thread>
#include <atomic>

namespace sancak {

class ILidarSensor;

/**
 * @class CombatPipeline
 * @brief Ana orkestrasyon sınıfı
 *
 * Her frame için:
 *  1. Kameradan frame al
 *  2. YOLO ile hedefleri tespit et
 *  3. Her hedef bbox'ında balonu bul
 *  4. Hedefleri takip et (ID + hız)
 *  5. Balistik düzeltme uygulayarak nişan noktası hesapla
 *  6. En öncelikli hedefi seç
 *  7. Arduino'ya komut gönder
 *  8. Görselleştirme (headless değilse)
 */
class CombatPipeline {
public:
    explicit CombatPipeline(INetworkSender* net = nullptr) : net_sender_(net) {}

    // Kamera donma koruması (Watchdog)
    std::chrono::steady_clock::time_point last_valid_frame_time_;

    /**
     * @brief Tüm modülleri başlat
     * @param config Sistem konfigürasyonu
     * @return Başarılı mı?
     */
    bool initialize(const SystemConfig& config);

    /**
     * @brief Tek bir frame işle
     * @param frame Giriş BGR frame
     * @return Pipeline çıkışı
     */
    PipelineOutput processFrame(const cv::Mat& frame);

    /**
     * @brief Ana döngü (kameradan okuyup işle)
     * Ctrl+C veya 'q' ile durur.
     */
    void run();

    /**
     * @brief Pipeline'ı durdur
     */
    void stop();

    /// Modül erişimleri (runtime ayar için)
    AimSolver& aimSolver() { return aim_solver_; }
    TargetTracker& tracker() { return tracker_; }

    /// Frame üzerine tüm bilgileri çiz
    void drawOverlay(cv::Mat& frame, const PipelineOutput& output) const;

    /// HUD (heads-up display) çiz
    void drawHud(cv::Mat& frame, const PipelineOutput& output) const;

    /// Nişan artısı çiz
    void drawCrosshair(cv::Mat& frame, const AimPoint& aim) const;

    /// Tespit kutularını çiz
    void drawDetections(cv::Mat& frame,
                        const std::vector<TrackedTarget>& targets) const;

    /// FPS hesapla
    double updateFps();

#if defined(SANCAK_ENABLE_TEST_HOOKS)
    void _testSetConfig(const SystemConfig& cfg) { config_ = cfg; }
    const SystemConfig& _testConfig() const { return config_; }

    void _testSetPoseDeg(float pan_deg, float tilt_deg) {
        current_pan_deg_ = pan_deg;
        current_tilt_deg_ = tilt_deg;
    }
    float _testPanDeg() const { return current_pan_deg_; }
    float _testTiltDeg() const { return current_tilt_deg_; }

    bool _testDetectionEnabled() const { return detection_enabled_; }
    bool _testOverlayEnabled() const { return overlay_enabled_; }

    bool _testFp16Enabled() const { return fp16_enabled_; }
    const std::string& _testYoloModelPath() const { return config_.yolo.model_path; }
    const std::string& _testYoloModelFp32Path() const { return yolo_model_fp32_path_; }
    const std::string& _testYoloModelFp16Path() const { return yolo_model_fp16_path_; }

    void _testHandleNetworkCommand(const std::string& line) { handleNetworkCommand(line); }
#endif

private:
    void consumeNetworkCommands();
    void handleNetworkCommand(const std::string& line);

    // Modüller
    YoloDetector      yolo_;
    BalloonSegmentor  segmentor_;
    AimSolver         aim_solver_;
    TargetTracker     tracker_;
    TriggerController trigger_controller_;
    CameraController  camera_;
    SerialComm        serial_;

    // Kural motoru (state machine) ve donanım sürücüsü
    std::unique_ptr<core::CombatStateMachine> combat_state_machine_;
    std::unique_ptr<TurretController> turret_controller_;
    float current_pan_deg_ = 0.0f;
    float current_tilt_deg_ = 0.0f;

    // Ağ katmanı
    net::UdpVideoStreamer   video_streamer_;
    net::TcpTelemetryServer telemetry_server_;
    net::TelemetrySender    telemetry_sender_;
    std::uint32_t           net_frame_id_ = 1;
    INetworkSender*         net_sender_ = nullptr;

    // Durum
    SystemConfig config_;
    PipelineState state_ = PipelineState::kIdle;
    bool running_ = false;
    TimePoint last_frame_time_;
    double fps_ = 0.0;

    // Uzaktan kontrol (PC -> RasPi) durumları
    bool detection_enabled_ = true; // <DETECT:START/STOP>
    bool overlay_enabled_ = true;   // <OVERLAY:ON/OFF>

    // Model seçimi (FP32/FP16)
    bool fp16_enabled_ = false; // <FP16:ON/OFF>
    std::string yolo_model_fp32_path_;
    std::string yolo_model_fp16_path_;

    // Lidar (opsiyonel) - arka planda güncellenen thread-safe cache
    std::unique_ptr<ILidarSensor> lidar_;
    std::thread lidar_thread_;
    std::atomic<bool> lidar_running_{false};
    mutable std::mutex lidar_mutex_;
    std::optional<float> lidar_distance_m_;
};

} // namespace sancak
