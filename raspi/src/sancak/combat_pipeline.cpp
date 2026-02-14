/**
 * @file combat_pipeline.cpp
 * @brief Ana Savaş Pipeline'ı - Implementasyon
 *
 * Tüm modülleri sıralı olarak çalıştırıp frame bazlı
 * nişan çıktısı üretir.
 *
 * Akış: Kamera → YOLO → Balon Seg → Takip → Balistik → Nişan → Seri
 */
#include "sancak/combat_pipeline.hpp"
#include "sancak/logger.hpp"

#include <opencv2/imgproc.hpp>
#include <opencv2/highgui.hpp>
#include <csignal>
#include <atomic>

namespace {
    std::atomic<bool> g_signal_stop{false};
    void signalHandler(int) { g_signal_stop.store(true); }
}

namespace sancak {

bool CombatPipeline::initialize(const SystemConfig& config) {
    config_ = config;

    SANCAK_LOG_INFO("========================================");
    SANCAK_LOG_INFO("SANCAK HAVA SAVUNMA SİSTEMİ v2.0");
    SANCAK_LOG_INFO("========================================");

    // Log seviyesi ayarla
    if (config_.log_level == "trace") {
        log::Logger::instance().setLevel(log::Level::kTrace);
    } else if (config_.log_level == "debug") {
        log::Logger::instance().setLevel(log::Level::kDebug);
    } else if (config_.log_level == "warn") {
        log::Logger::instance().setLevel(log::Level::kWarn);
    } else {
        log::Logger::instance().setLevel(log::Level::kInfo);
    }

    // 1. YOLO modeli yükle
    SANCAK_LOG_INFO("YOLO26-Nano modeli yükleniyor...");
    if (!yolo_.initialize(config_.yolo)) {
        SANCAK_LOG_FATAL("YOLO modeli yüklenemedi! Çıkılıyor.");
        return false;
    }

    // 2. Balon segmentörü başlat
    segmentor_.initialize(config_.balloon);

    // 3. Nişan çözücüyü başlat
    aim_solver_.initialize(config_.ballistics, config_.distance);

    // 4. Takip sistemi başlat
    tracker_.initialize(config_.tracking);

    // 5. Kamera aç
    SANCAK_LOG_INFO("Kamera açılıyor...");
    if (!camera_.open(config_.camera)) {
        SANCAK_LOG_WARN("Kamera açılamadı, video dosyası veya mock modda çalışılacak");
    }

    // 6. Seri port aç
    if (config_.serial.enabled) {
        serial_.open(config_.serial);
    }

    state_ = PipelineState::kIdle;
    last_frame_time_ = SteadyClock::now();

    SANCAK_LOG_INFO("Pipeline başlatıldı | Mod: {} | Otonom: {}",
                    config_.headless ? "HEADLESS" : "DISPLAY",
                    config_.autonomous ? "EVET" : "HAYIR");
    SANCAK_LOG_INFO("========================================");

    return true;
}

PipelineOutput CombatPipeline::processFrame(const cv::Mat& frame) {
    PipelineOutput output;
    output.display_frame = frame.clone();
    output.fps = updateFps();

    if (frame.empty()) {
        output.state = PipelineState::kIdle;
        return output;
    }

    // ========== AŞAMA 1: YOLO TESPİT ==========
    auto detections = yolo_.detect(frame);
    output.inference_ms = yolo_.lastInferenceMs();

    if (detections.empty()) {
        state_ = PipelineState::kIdle;
        serial_.sendIdleCommand();
        output.state = state_;
        if (!config_.headless) drawOverlay(output.display_frame, output);
        return output;
    }

    state_ = PipelineState::kDetecting;

    // ========== AŞAMA 2: TAKİP GÜNCELLE ==========
    auto tracked = tracker_.update(detections);

    // ========== AŞAMA 3: HER HEDEF İÇİN BALON SEG + NİŞAN ==========
    for (auto& target : tracked) {
        // Sadece düşman hedefleri işle
        if (!IsEnemy(target.detection.target_class)) continue;

        // Balon segmentasyonu
        target.balloon = segmentor_.segment(frame, target.detection.bbox);

        if (target.balloon.found) {
            state_ = PipelineState::kTracking;

            // Nişan noktası hesapla
            target.aim = aim_solver_.solve(
                target.balloon,
                target.detection,
                target.velocity,
                output.fps,
                frame.size()
            );

            if (target.aim.valid && target.is_priority) {
                state_ = PipelineState::kLocked;
            }
        }
    }

    output.targets = tracked;

    // ========== AŞAMA 4: ÖNCELİKLİ HEDEF SEÇ ==========
    const auto* priority = tracker_.getPriorityTarget();
    if (priority && priority->aim.valid) {
        output.primary_aim = priority->aim;

        // Arduino'ya nişan komutu gönder
        serial_.sendAimCommand(priority->aim);

        // Otonom modda ve kilitliyse state güncelle
        if (config_.autonomous && state_ == PipelineState::kLocked) {
            state_ = PipelineState::kEngaging;
        }
    }

    output.state = state_;

    // ========== AŞAMA 5: GÖRSELLEŞTİRME ==========
    if (!config_.headless) {
        drawOverlay(output.display_frame, output);
    }

    return output;
}

void CombatPipeline::run() {
    running_ = true;
    g_signal_stop.store(false);
    std::signal(SIGINT, signalHandler);

    SANCAK_LOG_INFO("Ana döngü başlatıldı (Ctrl+C ile dur)");

    cv::Mat frame;
    while (running_ && !g_signal_stop.load()) {
        if (!camera_.read(frame)) {
            SANCAK_LOG_WARN("Frame okunamadı, bekleniyor...");
            cv::waitKey(100);
            continue;
        }

        auto output = processFrame(frame);

        // Ekran gösterimi (headless değilse)
        if (!config_.headless) {
            cv::imshow("SANCAK - Combat View", output.display_frame);
            int key = cv::waitKey(1);
            if (key == 27 || key == 'q') {  // ESC veya Q
                SANCAK_LOG_INFO("Kullanıcı çıkış istedi");
                break;
            }
            // Runtime kontroller
            if (key == 'a') {
                config_.autonomous = !config_.autonomous;
                SANCAK_LOG_INFO("Otonom mod: {}", config_.autonomous ? "AÇIK" : "KAPALI");
            }
            if (key == '+' || key == '=') {
                config_.ballistics.manual_offset_y_px += 1.0f;
                aim_solver_.ballistics().setManualOffset(
                    config_.ballistics.manual_offset_x_px,
                    config_.ballistics.manual_offset_y_px);
            }
            if (key == '-') {
                config_.ballistics.manual_offset_y_px -= 1.0f;
                aim_solver_.ballistics().setManualOffset(
                    config_.ballistics.manual_offset_x_px,
                    config_.ballistics.manual_offset_y_px);
            }
        }
    }

    stop();
}

void CombatPipeline::stop() {
    running_ = false;
    camera_.release();
    serial_.close();
    if (!config_.headless) cv::destroyAllWindows();
    SANCAK_LOG_INFO("Pipeline durduruldu");
}

double CombatPipeline::updateFps() {
    auto now = SteadyClock::now();
    double dt = std::chrono::duration<double>(now - last_frame_time_).count();
    last_frame_time_ = now;
    if (dt > 0.0) fps_ = 1.0 / dt;
    return fps_;
}

// ============================================================================
// GÖRSELLEŞTİRME
// ============================================================================

void CombatPipeline::drawOverlay(cv::Mat& frame, const PipelineOutput& output) const {
    drawDetections(frame, output.targets);
    drawHud(frame, output);
    if (output.primary_aim.has_value()) {
        drawCrosshair(frame, output.primary_aim.value());
    }
}

void CombatPipeline::drawDetections(cv::Mat& frame,
                                     const std::vector<TrackedTarget>& targets) const {
    for (const auto& t : targets) {
        bool enemy = IsEnemy(t.detection.target_class);
        cv::Scalar color = enemy ? cv::Scalar(0, 0, 255) : cv::Scalar(0, 255, 0);

        // Bounding box
        cv::Rect bbox(
            static_cast<int>(t.detection.bbox.x),
            static_cast<int>(t.detection.bbox.y),
            static_cast<int>(t.detection.bbox.width),
            static_cast<int>(t.detection.bbox.height)
        );
        cv::rectangle(frame, bbox, color, 2);

        // Etiket
        std::string label = std::string(TargetClassName(t.detection.target_class)) +
                            " #" + std::to_string(t.track_id) +
                            " " + std::to_string(static_cast<int>(t.detection.confidence * 100)) + "%";
        int baseline = 0;
        cv::Size sz = cv::getTextSize(label, cv::FONT_HERSHEY_SIMPLEX, 0.5, 1, &baseline);
        cv::rectangle(frame,
                      cv::Point(bbox.x, bbox.y - sz.height - 8),
                      cv::Point(bbox.x + sz.width, bbox.y),
                      color, cv::FILLED);
        cv::putText(frame, label, cv::Point(bbox.x, bbox.y - 4),
                    cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(255, 255, 255), 1);

        // Balon merkezi
        if (t.balloon.found) {
            cv::circle(frame,
                       cv::Point(static_cast<int>(t.balloon.center.x),
                                 static_cast<int>(t.balloon.center.y)),
                       static_cast<int>(t.balloon.radius),
                       cv::Scalar(0, 165, 255), 2);
            cv::circle(frame,
                       cv::Point(static_cast<int>(t.balloon.center.x),
                                 static_cast<int>(t.balloon.center.y)),
                       3, cv::Scalar(0, 165, 255), -1);
        }

        // Mesafe
        if (t.aim.valid) {
            std::string dist_str = std::to_string(static_cast<int>(t.aim.distance_m * 10) / 10.0).substr(0, 4) + "m";
            cv::putText(frame, dist_str,
                        cv::Point(bbox.x, bbox.y + bbox.height + 15),
                        cv::FONT_HERSHEY_SIMPLEX, 0.45, color, 1);
        }
    }
}

void CombatPipeline::drawCrosshair(cv::Mat& frame, const AimPoint& aim) const {
    int cx = static_cast<int>(aim.corrected.x);
    int cy = static_cast<int>(aim.corrected.y);
    int size = 20;

    cv::Scalar crossColor(0, 255, 0);
    if (state_ == PipelineState::kLocked || state_ == PipelineState::kEngaging) {
        crossColor = cv::Scalar(0, 0, 255);  // Kilitli → kırmızı
    }

    // Artı işareti
    cv::line(frame, cv::Point(cx - size, cy), cv::Point(cx + size, cy), crossColor, 2);
    cv::line(frame, cv::Point(cx, cy - size), cv::Point(cx, cy + size), crossColor, 2);

    // İç daire
    cv::circle(frame, cv::Point(cx, cy), 6, crossColor, 1);

    // Dış daire
    cv::circle(frame, cv::Point(cx, cy), size, crossColor, 1);

    // Ham merkez → düzeltilmiş merkez arası çizgi (düzeltme vektörü)
    int rx = static_cast<int>(aim.raw_center.x);
    int ry = static_cast<int>(aim.raw_center.y);
    if (std::abs(rx - cx) > 1 || std::abs(ry - cy) > 1) {
        cv::line(frame, cv::Point(rx, ry), cv::Point(cx, cy),
                 cv::Scalar(255, 255, 0), 1, cv::LINE_AA);
    }
}

void CombatPipeline::drawHud(cv::Mat& frame, const PipelineOutput& output) const {
    int y = 20;
    int dy = 18;
    cv::Scalar textColor(0, 255, 0);

    auto putLine = [&](const std::string& text) {
        cv::putText(frame, text, cv::Point(10, y),
                    cv::FONT_HERSHEY_SIMPLEX, 0.45, textColor, 1);
        y += dy;
    };

    // Durum bilgileri
    putLine(std::string("DURUM: ") + PipelineStateName(output.state));
    putLine("FPS: " + std::to_string(static_cast<int>(output.fps)));
    putLine("YOLO: " + std::to_string(static_cast<int>(output.inference_ms)) + " ms");
    putLine("Hedef: " + std::to_string(output.targets.size()));
    putLine(std::string("Otonom: ") + (config_.autonomous ? "EVET" : "HAYIR"));

    // Nişan bilgisi
    if (output.primary_aim.has_value()) {
        const auto& aim = output.primary_aim.value();
        putLine("Mesafe: " + std::to_string(static_cast<int>(aim.distance_m * 10) / 10.0).substr(0, 4) + " m");
        putLine("Offset: dx=" + std::to_string(static_cast<int>(aim.correction.dx_px)) +
                " dy=" + std::to_string(static_cast<int>(aim.correction.dy_px)));
    }

    // Kontroller (alt kısım)
    int bh = frame.rows - 10;
    cv::putText(frame, "[A]otonom [+/-]offset [Q]cikis",
                cv::Point(10, bh), cv::FONT_HERSHEY_SIMPLEX, 0.35,
                cv::Scalar(128, 128, 128), 1);
}

} // namespace sancak
