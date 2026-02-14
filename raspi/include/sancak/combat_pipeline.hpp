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
#include "sancak/camera_controller.hpp"
#include "sancak/serial_comm.hpp"

namespace sancak {

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
    CombatPipeline() = default;

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

private:
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

    // Modüller
    YoloDetector      yolo_;
    BalloonSegmentor  segmentor_;
    AimSolver         aim_solver_;
    TargetTracker     tracker_;
    CameraController  camera_;
    SerialComm        serial_;

    // Durum
    SystemConfig config_;
    PipelineState state_ = PipelineState::kIdle;
    bool running_ = false;
    TimePoint last_frame_time_;
    double fps_ = 0.0;
};

} // namespace sancak
