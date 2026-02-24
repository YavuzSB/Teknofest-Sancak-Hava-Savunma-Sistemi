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
#include <algorithm>
#include <cmath>

namespace {
    static sancak::core::TargetClass toCoreTargetClass(sancak::TargetClass cls) {
        using S = sancak::TargetClass;
        using C = sancak::core::TargetClass;
        switch (cls) {
            case S::kDrone:      return C::Drone;
            case S::kHelicopter: return C::Helicopter;
            case S::kRocket:     return C::Missile;
            case S::kJet:        return C::F16;
            case S::kPlane:      return C::F16; // pratikte "uçak" sınıfını F16 kuralına mapliyoruz
            default:             return C::Unknown;
        }
    }

    static sancak::core::Affiliation toCoreAffiliation(sancak::Affiliation a) {
        switch (a) {
            case sancak::Affiliation::Friend:  return sancak::core::Affiliation::Friend;
            case sancak::Affiliation::Foe:     return sancak::core::Affiliation::Foe;
            default:                           return sancak::core::Affiliation::Unknown;
        }
    }

    static sancak::PipelineState toPipelineState(sancak::core::CombatState s) {
        using CS = sancak::core::CombatState;
        switch (s) {
            case CS::Idle:      return sancak::PipelineState::kIdle;
            case CS::Searching: return sancak::PipelineState::kDetecting;
            case CS::Tracking:  return sancak::PipelineState::kTracking;
            case CS::Engaging:  return sancak::PipelineState::kEngaging;
            case CS::SafeLock:  return sancak::PipelineState::kIdle;
            default:            return sancak::PipelineState::kIdle;
        }
    }

    static bool isInsideGeofence(const sancak::GeofenceConfig& g, float pan_deg, float tilt_deg) {
        return pan_deg >= g.pan_min_deg && pan_deg <= g.pan_max_deg &&
               tilt_deg >= g.tilt_min_deg && tilt_deg <= g.tilt_max_deg;
    }

    // IFF bilgisi varsa sınıf bazlı düşman bilgisini override et.
    static bool shouldTreatAsEnemy(const sancak::Detection& d) {
        if (d.affiliation == sancak::Affiliation::Friend) return false;
        if (d.affiliation == sancak::Affiliation::Foe) return true;
        return sancak::IsEnemy(d.target_class);
    }
}

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

    // 4.1 Trigger Controller
    trigger_controller_.initialize(
        config_.trigger.aim_tolerance_px,
        config_.trigger.lock_duration_ms,
        config_.trigger.burst_duration_ms,
        config_.trigger.cooldown_ms);

    // 4.2 Combat State Machine (kural motoru + geofence)
    {
        core::Geofence fence;
        fence.pan_min = config_.geofence.pan_min_deg;
        fence.pan_max = config_.geofence.pan_max_deg;
        fence.tilt_min = config_.geofence.tilt_min_deg;
        fence.tilt_max = config_.geofence.tilt_max_deg;

        auto rules = ConfigManager::instance().getTargetRules();
        if (rules.empty()) {
            // Güvenli varsayılanlar (konfig yoksa)
            rules[core::TargetClass::Drone] = {core::TargetClass::Drone, 0.0f, 50.0f, 2};
            rules[core::TargetClass::F16] = {core::TargetClass::F16, 0.0f, 50.0f, 1};
            rules[core::TargetClass::Helicopter] = {core::TargetClass::Helicopter, 0.0f, 50.0f, 3};
            rules[core::TargetClass::Missile] = {core::TargetClass::Missile, 0.0f, 50.0f, 0};
        }
        combat_state_machine_ = std::make_unique<core::CombatStateMachine>(rules, fence);
    }

    // 5. Kamera aç
    SANCAK_LOG_INFO("Kamera açılıyor...");
    if (!camera_.open(config_.camera)) {
        SANCAK_LOG_WARN("Kamera açılamadı, video dosyası veya mock modda çalışılacak");
    }

    // 6. Seri port aç
    if (config_.serial.enabled) {
        // Yeni protokol sürücüsü (kuyruklu + auto-reconnect)
        turret_controller_ = std::make_unique<TurretController>(config_.serial.port, config_.serial.baud_rate);
    }

    // 7. Ağ katmanı (opsiyonel)
    if (config_.network.video_enabled) {
        net::UdpVideoConfig vcfg;
        vcfg.host = config_.network.gcs_host;
        vcfg.port = static_cast<std::uint16_t>(config_.network.video_udp_port);
        vcfg.mtu_bytes = static_cast<std::size_t>(config_.network.udp_mtu_bytes);
        vcfg.jpeg_quality = config_.network.jpeg_quality;
        (void)video_streamer_.start(std::move(vcfg));
    }
    if (config_.network.telemetry_enabled) {
        net::TcpTelemetryConfig tcfg;
        tcfg.port = static_cast<std::uint16_t>(config_.network.telemetry_tcp_port);
        (void)telemetry_server_.start(tcfg);

        // Outbound telemetry opsiyonel: port çakışmasını engelle.
        // Sadece Pi->PC push isteniyorsa açılır (telemetry_push_port > 0).
        if (config_.network.telemetry_push_port > 0 &&
            config_.network.telemetry_push_port != config_.network.telemetry_tcp_port) {
            (void)telemetry_sender_.start(config_.network.gcs_host,
                              static_cast<std::uint16_t>(config_.network.telemetry_push_port));
        } else {
            SANCAK_LOG_WARN("TelemetrySender devre dışı: push_port={} tcp_port={} (server-only topoloji için normal)",
                            config_.network.telemetry_push_port,
                            config_.network.telemetry_tcp_port);
        }
    }

    state_ = PipelineState::kIdle;
    last_frame_time_ = SteadyClock::now();
    last_valid_frame_time_ = SteadyClock::now();

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

    // Kamera Watchdog (Fail-Safe)
    const auto now = std::chrono::steady_clock::now();
    const bool frame_ok = !frame.empty();
    if (frame_ok) {
        last_valid_frame_time_ = now;
    }

    double inference_ms = 0.0;
    double aim_solve_ms = 0.0;

    sancak::AimResult aimRes{};
    const TrackedTarget* locked = nullptr;
    std::uint8_t telem_state = static_cast<std::uint8_t>(core::CombatState::Idle);
    std::int32_t telem_target_id = -1;
    float telem_distance_m = 0.0f;

    auto finalize = [&]() -> PipelineOutput {
        // Görselleştirme
        if (!config_.headless || config_.network.video_enabled) {
            drawOverlay(output.display_frame, output);
        }

        // Network
        if (config_.network.video_enabled) {
            video_streamer_.submit(output.display_frame);
        }
        if (config_.network.telemetry_enabled) {
            telemetry_server_.publishAimResult(aimRes, net_frame_id_);

            net::AimTelemetry t;
            t.current_state = telem_state;
            t.target_id = telem_target_id;
            t.raw_x = aimRes.raw_xy.x;
            t.raw_y = aimRes.raw_xy.y;
            t.corrected_x = aimRes.corrected_xy.x;
            t.corrected_y = aimRes.corrected_xy.y;
            t.distance_m = telem_distance_m;
            telemetry_sender_.sendTelemetry(t);
        }

        // INetworkSender ile gelişmiş telemetri (opsiyonel)
        if (net_sender_) {
            AimTelemetry telem;
            telem.aim = aimRes;
            telem.inference_ms = inference_ms;
            telem.aim_solve_ms = aim_solve_ms;
            telem.frame_id = net_frame_id_;
            telem.monotonic_us = std::chrono::duration_cast<std::chrono::microseconds>(
                                     std::chrono::steady_clock::now().time_since_epoch())
                                     .count();
            net_sender_->publishAimTelemetry(telem);
        }

        net_frame_id_++;
        return output;
    };

    const auto ms_since_last = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_valid_frame_time_).count();
    if (ms_since_last > 500) {
        SANCAK_LOG_ERROR("Kamera sinyali koptu/dondu! FAIL-SAFE devrede! ({} ms)", ms_since_last);
        trigger_controller_.reset();
        tracker_.reset();
        telem_state = static_cast<std::uint8_t>(core::CombatState::SafeLock);
        state_ = PipelineState::kIdle;
        output.state = state_;
        if (turret_controller_) {
            turret_controller_->sendSafeLock();
            turret_controller_->sendCommand(current_pan_deg_, current_tilt_deg_, false);
        }
        return finalize();
    }

    if (frame.empty()) {
        state_ = PipelineState::kIdle;
        output.state = state_;
        trigger_controller_.reset();
        if (turret_controller_) {
            turret_controller_->sendCommand(current_pan_deg_, current_tilt_deg_, false);
        }
        return finalize();
    }

    // ========== AŞAMA 1: YOLO TESPİT ==========
    auto detections = yolo_.detect(frame);
    inference_ms = yolo_.lastInferenceMs();
    output.inference_ms = inference_ms;

    // IFF (Dost-Düşman) renk analizi
    const cv::Scalar red_lower(config_.iff.foe_red.h_min,
                               config_.iff.foe_red.s_min,
                               config_.iff.foe_red.v_min);
    const cv::Scalar red_upper(config_.iff.foe_red.h_max,
                               config_.iff.foe_red.s_max,
                               config_.iff.foe_red.v_max);
    const cv::Scalar blue_lower(config_.iff.friend_blue.h_min,
                                config_.iff.friend_blue.s_min,
                                config_.iff.friend_blue.v_min);
    const cv::Scalar blue_upper(config_.iff.friend_blue.h_max,
                                config_.iff.friend_blue.s_max,
                                config_.iff.friend_blue.v_max);

    for (auto& det : detections) {
        // Güvenli ROI kesimi
        cv::Rect roi = det.bbox;
        roi &= cv::Rect(0, 0, frame.cols, frame.rows);
        if (roi.width <= 0 || roi.height <= 0) {
            det.affiliation = Affiliation::Unknown;
            continue;
        }
        cv::Mat roi_mat = frame(roi);
        cv::Mat hsv_roi;
        cv::cvtColor(roi_mat, hsv_roi, cv::COLOR_BGR2HSV);

        cv::Mat mask_red, mask_blue;
        cv::inRange(hsv_roi, red_lower, red_upper, mask_red);
        cv::inRange(hsv_roi, blue_lower, blue_upper, mask_blue);

        int red_count = cv::countNonZero(mask_red);
        int blue_count = cv::countNonZero(mask_blue);

        if (red_count > blue_count && red_count > 0) {
            det.affiliation = Affiliation::Foe;
        } else if (blue_count > red_count && blue_count > 0) {
            det.affiliation = Affiliation::Friend;
        } else {
            det.affiliation = Affiliation::Unknown;
        }
    }

    if (detections.empty()) {
        state_ = PipelineState::kIdle;
        output.state = state_;
        trigger_controller_.reset();
        if (turret_controller_) {
            turret_controller_->sendCommand(current_pan_deg_, current_tilt_deg_, false);
        }
        return finalize();
    }

    // ========== AŞAMA 2: TAKİP ==========
    state_ = PipelineState::kDetecting;
    auto& tracked = tracker_.update(detections);

    // ========== AŞAMA 3: BALON + NİŞAN/BALİSTİK ==========
    for (auto& target : tracked) {
        if (!shouldTreatAsEnemy(target.detection)) continue;

        target.balloon = segmentor_.segment(frame, target.detection.bbox);
        if (!target.balloon.found) continue;

        state_ = PipelineState::kTracking;

        const auto tAim0 = std::chrono::steady_clock::now();
        target.aim = aim_solver_.solve(
            target.balloon,
            target.detection,
            target.velocity,
            output.fps,
            frame.size());
        const auto tAim1 = std::chrono::steady_clock::now();
        aim_solve_ms = std::chrono::duration<double, std::milli>(tAim1 - tAim0).count();

        if (target.aim.valid && target.is_priority) {
            state_ = PipelineState::kLocked;
        }
    }

    output.targets = tracked;

    // ========== AŞAMA 4: KARAR (STATE MACHINE) ==========
    std::vector<core::TrackedTarget> sm_targets;
    sm_targets.reserve(tracked.size());
    for (const auto& t : tracked) {
        if (!shouldTreatAsEnemy(t.detection)) continue;
        if (!t.aim.valid) continue;

        core::TrackedTarget ct;
        ct.id = t.track_id;
        ct.target_class = toCoreTargetClass(t.detection.target_class);
        ct.affiliation = toCoreAffiliation(t.detection.affiliation);
        ct.is_lost = (t.lost_frames > 0);
        ct.distance_m = t.aim.distance_m;
        ct.confidence = t.detection.confidence;
        sm_targets.push_back(ct);
    }

    core::CombatDecision decision;
    decision.state = sm_targets.empty() ? core::CombatState::Searching : core::CombatState::Tracking;
    decision.locked_target_id = -1;
    if (combat_state_machine_) {
        decision = combat_state_machine_->update(sm_targets, current_pan_deg_, current_tilt_deg_);
    }

    state_ = toPipelineState(decision.state);
    telem_state = static_cast<std::uint8_t>(decision.state);

    if (decision.locked_target_id >= 0) {
        for (const auto& t : tracked) {
            if (t.track_id == decision.locked_target_id && t.aim.valid) {
                locked = &t;
                break;
            }
        }
    }
    if (!locked) {
        const auto* priority = tracker_.getPriorityTarget();
        if (priority && priority->aim.valid && shouldTreatAsEnemy(priority->detection)) {
            locked = priority;
        }
    }

    if (locked) {
        telem_target_id = locked->track_id;
        telem_distance_m = locked->aim.distance_m;
    }

    // ========== AŞAMA 5: FOV MAPPING + TRIGGER + TARET ==========
    bool fire_flag = false;
    if (decision.state == core::CombatState::SafeLock) {
        trigger_controller_.reset();
        if (turret_controller_) {
            turret_controller_->sendSafeLock();
            turret_controller_->sendCommand(current_pan_deg_, current_tilt_deg_, false);
        }
    } else if (locked && (decision.state == core::CombatState::Tracking || decision.state == core::CombatState::Engaging)) {
        output.primary_aim = locked->aim;

        const cv::Point2f crosshair_center(frame.cols / 2.0f, frame.rows / 2.0f);
        const float dx_px = locked->aim.corrected.x - crosshair_center.x;
        const float dy_px = locked->aim.corrected.y - crosshair_center.y;

        const float deg_per_px_x = (frame.cols > 0)
                                      ? (config_.camera.h_fov_deg / static_cast<float>(frame.cols))
                                      : 0.0f;
        const float deg_per_px_y = (frame.rows > 0)
                                      ? (config_.camera.v_fov_deg / static_cast<float>(frame.rows))
                                      : 0.0f;

        float delta_pan_deg = dx_px * deg_per_px_x;
        float delta_tilt_deg = -dy_px * deg_per_px_y;

        // Deadzone: mikro salınımı engelle (hunting/oscillation).
        constexpr float kServoDeadzoneDeg = 0.5f;
        if (std::fabs(delta_pan_deg) < kServoDeadzoneDeg) {
            delta_pan_deg = 0.0f;
        }
        if (std::fabs(delta_tilt_deg) < kServoDeadzoneDeg) {
            delta_tilt_deg = 0.0f;
        }

        const float desired_pan = current_pan_deg_ + delta_pan_deg;
        const float desired_tilt = current_tilt_deg_ + delta_tilt_deg;

        if (!isInsideGeofence(config_.geofence, desired_pan, desired_tilt)) {
            state_ = PipelineState::kIdle;
            trigger_controller_.reset();
            if (turret_controller_) {
                turret_controller_->sendSafeLock();
                turret_controller_->sendCommand(current_pan_deg_, current_tilt_deg_, false);
            }
        } else {
            current_pan_deg_ = desired_pan;
            current_tilt_deg_ = desired_tilt;

            if (config_.autonomous && decision.state == core::CombatState::Engaging) {
                fire_flag = trigger_controller_.update(locked->aim.corrected, crosshair_center);
            } else {
                trigger_controller_.reset();
            }

            if (turret_controller_) {
                turret_controller_->sendCommand(current_pan_deg_, current_tilt_deg_, fire_flag);
            }
        }
    } else {
        trigger_controller_.reset();
        if (turret_controller_) {
            turret_controller_->sendCommand(current_pan_deg_, current_tilt_deg_, false);
        }
    }

    // ========== AŞAMA 6: AimResult doldur ==========
    if (locked && output.primary_aim.has_value()) {
        aimRes.raw_xy = output.primary_aim->raw_center;
        aimRes.corrected_xy = output.primary_aim->corrected;
        aimRes.class_id = locked->detection.class_id;
        aimRes.valid = output.primary_aim->valid;
    }

    output.state = state_;
    return finalize();
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
    turret_controller_.reset();
    video_streamer_.stop();
    telemetry_server_.stop();
    telemetry_sender_.stop();
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
        bool enemy = shouldTreatAsEnemy(t.detection);
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
