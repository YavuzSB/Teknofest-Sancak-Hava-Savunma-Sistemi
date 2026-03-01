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
#include "sancak/mock_lidar.hpp"

#include <opencv2/imgproc.hpp>
#include <opencv2/highgui.hpp>
#include <csignal>
#include <atomic>
#include <algorithm>
#include <cmath>
#include <cctype>
#include <filesystem>
#include <string_view>
#include <sstream>
#include <optional>
#include <thread>

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

namespace {

std::string_view trimView(std::string_view s) {
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t' || s.front() == '\r' || s.front() == '\n')) {
        s.remove_prefix(1);
    }
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r' || s.back() == '\n')) {
        s.remove_suffix(1);
    }
    return s;
}

std::string toUpperCopy(std::string_view s) {
    std::string out;
    out.reserve(s.size());
    for (unsigned char c : s) {
        out.push_back(static_cast<char>(std::toupper(c)));
    }
    return out;
}

bool parseFourFloats(std::string_view s, float& a, float& b, float& c, float& d) {
    // Format: a,b,c,d (spaces allowed)
    std::stringstream ss(std::string(trimView(s)));
    char comma1 = 0, comma2 = 0, comma3 = 0;
    if (!(ss >> a)) return false;
    if (!(ss >> comma1) || comma1 != ',') return false;
    if (!(ss >> b)) return false;
    if (!(ss >> comma2) || comma2 != ',') return false;
    if (!(ss >> c)) return false;
    if (!(ss >> comma3) || comma3 != ',') return false;
    if (!(ss >> d)) return false;
    return true;
}

bool parseInt(std::string_view s, int& out) {
    try {
        out = std::stoi(std::string(trimView(s)));
        return true;
    } catch (...) {
        return false;
    }
}

std::string deriveFp16ModelPath(const std::string& fp32_path) {
    // Konvansiyon: foo.onnx -> foo_fp16.onnx
    // (FP16 modeli yoksa fallback yapılacak.)
    if (fp32_path.size() >= 5 && fp32_path.rfind(".onnx") == fp32_path.size() - 5) {
        return fp32_path.substr(0, fp32_path.size() - 5) + "_fp16.onnx";
    }
    return fp32_path + "_fp16";
}

bool fileExists(const std::string& path) {
    if (path.empty()) return false;
    std::error_code ec;
    return std::filesystem::exists(std::filesystem::path(path), ec);
}

} // namespace

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
    // FP16 modeli (opsiyonel) path'ini burada hesaplıyoruz.
    yolo_model_fp32_path_ = config_.yolo.model_path;
    yolo_model_fp16_path_ = deriveFp16ModelPath(yolo_model_fp32_path_);
    fp16_enabled_ = false; // default güvenli: FP32

    config_.yolo.model_path = yolo_model_fp32_path_;

    SANCAK_LOG_INFO("YOLO26-Nano modeli yükleniyor... (model='{}')", config_.yolo.model_path);
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

    // 8. Lidar (opsiyonel): arka planda ölçüm güncelle + thread-safe cache
    // Not: Şimdilik sadece MockLidar destekleniyor (use_mock=true).
    if (lidar_running_.exchange(false)) {
        if (lidar_thread_.joinable()) lidar_thread_.join();
    } else {
        if (lidar_thread_.joinable()) lidar_thread_.join();
    }
    lidar_.reset();
    {
        std::lock_guard<std::mutex> lock(lidar_mutex_);
        lidar_distance_m_.reset();
    }

    if (config_.lidar.enabled) {
        if (config_.lidar.use_mock) {
            lidar_ = std::make_unique<MockLidar>(config_.lidar);
        } else {
            SANCAK_LOG_WARN("Lidar enabled ama real sensor sürücüsü tanımlı değil (use_mock=false). Lidar devre dışı.");
        }

        if (lidar_) {
            const float hz = (config_.lidar.mock_update_hz > 0.0f) ? config_.lidar.mock_update_hz : 20.0f;
            const auto period = std::chrono::duration<double>(1.0 / static_cast<double>(hz));

            lidar_running_.store(true);
            lidar_thread_ = std::thread([this, period]() {
                while (lidar_running_.load()) {
                    lidar_->update();
                    auto d = lidar_->getDistanceMeters();
                    {
                        std::lock_guard<std::mutex> lock(lidar_mutex_);
                        lidar_distance_m_ = d;
                    }
                    std::this_thread::sleep_for(period);
                }
            });

            SANCAK_LOG_INFO("Lidar thread başlatıldı (mock={}, hz={:.1f})",
                            config_.lidar.use_mock ? "EVET" : "HAYIR",
                            hz);
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

    // PC -> RasPi komutlarını her frame tüket (komutlar TCP server thread'inde kuyruklanır)
    consumeNetworkCommands();

    // Kamera Watchdog (Fail-Safe)
    const auto now = std::chrono::steady_clock::now();
    const bool frame_ok = !frame.empty();
    if (frame_ok) {
        last_valid_frame_time_ = now;
    }

    double inference_ms = 0.0;
    double aim_solve_ms = 0.0;

    // Lidar cache snapshot (frame başına bir kez oku)
    std::optional<float> lidar_distance_m = std::nullopt;
    if (config_.lidar.enabled) {
        std::lock_guard<std::mutex> lock(lidar_mutex_);
        lidar_distance_m = lidar_distance_m_;
    }
    const cv::Point3f lidar_offset_m(config_.lidar.offset_x_m,
                                    config_.lidar.offset_y_m,
                                    config_.lidar.offset_z_m);

    sancak::AimResult aimRes{};
    const TrackedTarget* locked = nullptr;
    std::uint8_t telem_state = static_cast<std::uint8_t>(core::CombatState::Idle);
    std::int32_t telem_target_id = -1;
    float telem_distance_m = 0.0f;

    auto finalize = [&]() -> PipelineOutput {
        // Görselleştirme
        if (overlay_enabled_ && (!config_.headless || config_.network.video_enabled) && !output.display_frame.empty()) {
            drawOverlay(output.display_frame, output);
        }

        // Network
        if (config_.network.video_enabled && !output.display_frame.empty()) {
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

    // Uzaktan tespit kapalıysa CPU harcama: YOLO/segmentasyon çalıştırma.
    if (!detection_enabled_) {
        state_ = PipelineState::kIdle;
        output.state = state_;
        tracker_.reset();
        trigger_controller_.reset();
        if (turret_controller_) {
            turret_controller_->sendSafeLock();
            turret_controller_->sendCommand(current_pan_deg_, current_tilt_deg_, false);
        }
        return finalize();
    }

    // ========== AŞAMA 1: YOLO TESPİT ==========
    auto detections = yolo_.detect(frame);
    inference_ms = yolo_.lastInferenceMs();
    output.inference_ms = inference_ms;

    // Optimizasyon: Frame başına tek BGR->HSV dönüşümü (IFF + segmentasyon aynı HSV'i paylaşır)
    cv::Mat hsv_frame;
    if (!detections.empty()) {
        cv::cvtColor(frame, hsv_frame, cv::COLOR_BGR2HSV);
    }

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
        // HSV ROI: precomputed frame üzerinden views (ek cvtColor yok)
        const cv::Mat hsv_roi = hsv_frame(roi);

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

        target.balloon = segmentor_.segmentHsv(hsv_frame, target.detection.bbox);
        if (!target.balloon.found) continue;

        state_ = PipelineState::kTracking;

        const auto tAim0 = std::chrono::steady_clock::now();
        target.aim = aim_solver_.solve(
            target.balloon,
            target.detection,
            target.velocity,
            output.fps,
            frame.size(),
            inference_ms,
            lidar_distance_m,
            lidar_offset_m);
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
            // Kamera okunamazsa da fail-safe'in devreye girebilmesi için boş frame işle.
            // (Aksi halde processFrame çağrılmaz ve son gönderilen turret komutu “takılı” kalabilir.)
            (void)processFrame(cv::Mat{});
            cv::waitKey(100);
            continue;
        }

        auto output = processFrame(frame);

        // Ekran gösterimi (headless değilse)
        if (!config_.headless && !output.display_frame.empty()) {
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

    // Lidar thread'i güvenli kapat
    if (lidar_running_.exchange(false)) {
        if (lidar_thread_.joinable()) lidar_thread_.join();
    } else {
        if (lidar_thread_.joinable()) lidar_thread_.join();
    }
    lidar_.reset();
    {
        std::lock_guard<std::mutex> lock(lidar_mutex_);
        lidar_distance_m_.reset();
    }

    camera_.release();
    serial_.close();
    turret_controller_.reset();
    video_streamer_.stop();
    telemetry_server_.stop();
    telemetry_sender_.stop();
    if (!config_.headless) cv::destroyAllWindows();
    SANCAK_LOG_INFO("Pipeline durduruldu");
}

void CombatPipeline::consumeNetworkCommands() {
    if (!config_.network.telemetry_enabled) return;
    if (!telemetry_server_.isRunning()) return;

    std::string line;
    int safety = 0;
    while (telemetry_server_.tryPopCommand(line)) {
        handleNetworkCommand(line);
        if (++safety >= 64) {
            SANCAK_LOG_WARN("Komut tüketimi safety-cap'e takıldı (64). Kalan komutlar sonraki frame'e kaldı.");
            break;
        }
    }
}

void CombatPipeline::handleNetworkCommand(const std::string& line) {
    std::string_view s = trimView(line);
    if (s.empty()) return;

    // <...> wrapper varsa çıkar
    if (s.size() >= 2 && s.front() == '<' && s.back() == '>') {
        s = s.substr(1, s.size() - 2);
        s = trimView(s);
    }
    if (s.empty()) return;

    const auto colon = s.find(':');
    const std::string cmd = toUpperCopy(colon == std::string_view::npos ? s : s.substr(0, colon));
    const std::string_view args = (colon == std::string_view::npos) ? std::string_view{} : trimView(s.substr(colon + 1));

    if (cmd == "DETECT") {
        const std::string a = toUpperCopy(args);
        if (a == "START") {
            detection_enabled_ = true;
            SANCAK_LOG_INFO("Remote DETECT:START");
        } else if (a == "STOP") {
            detection_enabled_ = false;
            tracker_.reset();
            trigger_controller_.reset();
            if (turret_controller_) {
                turret_controller_->sendSafeLock();
                turret_controller_->sendCommand(current_pan_deg_, current_tilt_deg_, false);
            }
            SANCAK_LOG_WARN("Remote DETECT:STOP");
        } else {
            SANCAK_LOG_WARN("Bilinmeyen DETECT arg: '{}'", std::string(args));
        }
        return;
    }

    if (cmd == "MODE") {
        const std::string a = toUpperCopy(args);
        if (a == "FULL_AUTO") {
            config_.autonomous = true;
            ConfigManager::instance().setAutonomous(true);
            detection_enabled_ = true;
            SANCAK_LOG_WARN("Remote MODE:FULL_AUTO (autonomous=true)");
        } else if (a == "MANUAL") {
            config_.autonomous = false;
            ConfigManager::instance().setAutonomous(false);
            SANCAK_LOG_INFO("Remote MODE:MANUAL (autonomous=false)");
        } else {
            SANCAK_LOG_WARN("Bilinmeyen MODE arg: '{}'", std::string(args));
        }
        return;
    }

    if (cmd == "OVERLAY") {
        const std::string a = toUpperCopy(args);
        if (a == "ON") {
            overlay_enabled_ = true;
            SANCAK_LOG_INFO("Remote OVERLAY:ON");
        } else if (a == "OFF") {
            overlay_enabled_ = false;
            SANCAK_LOG_INFO("Remote OVERLAY:OFF");
        } else {
            SANCAK_LOG_WARN("Bilinmeyen OVERLAY arg: '{}'", std::string(args));
        }
        return;
    }

    if (cmd == "FP16") {
        const std::string a = toUpperCopy(args);
        const bool requested = (a == "ON");
        if (!(a == "ON" || a == "OFF")) {
            SANCAK_LOG_WARN("Bilinmeyen FP16 arg: '{}'", std::string(args));
            return;
        }

        // Testlerde initialize() çağırmadan komutlar test ediliyor.
        // Bu yüzden model path'lerini lazy-init ediyoruz.
        if (yolo_model_fp32_path_.empty()) {
            yolo_model_fp32_path_ = config_.yolo.model_path;
        }
        if (yolo_model_fp16_path_.empty()) {
            yolo_model_fp16_path_ = deriveFp16ModelPath(yolo_model_fp32_path_);
        }

        const std::string targetModelPath = requested ? yolo_model_fp16_path_ : yolo_model_fp32_path_;

#if defined(SANCAK_ENABLE_TEST_HOOKS)
        fp16_enabled_ = requested;
        config_.yolo.model_path = targetModelPath;
        SANCAK_LOG_INFO("Remote FP16:{} (TEST_HOOKS) model='{}'", requested ? "ON" : "OFF", config_.yolo.model_path);
        return;
#else
        if (requested && !fileExists(targetModelPath)) {
            SANCAK_LOG_WARN("FP16 modeli bulunamadı: '{}' (FP32'de kalınıyor)", targetModelPath);
            fp16_enabled_ = false;
            config_.yolo.model_path = yolo_model_fp32_path_;
            return;
        }

        if (fp16_enabled_ == requested && config_.yolo.model_path == targetModelPath) {
            SANCAK_LOG_DEBUG("FP16 zaten istenen durumda: {}", requested ? "ON" : "OFF");
            return;
        }

        const auto oldFp16 = fp16_enabled_;
        const auto oldModel = config_.yolo.model_path;

        fp16_enabled_ = requested;
        config_.yolo.model_path = targetModelPath;

        SANCAK_LOG_WARN("Remote FP16:{} -> YOLO yeniden yükleniyor (model='{}')", requested ? "ON" : "OFF", config_.yolo.model_path);
        if (!yolo_.initialize(config_.yolo)) {
            SANCAK_LOG_ERROR("YOLO yeniden yükleme başarısız. Eski modele geri dönülüyor.");
            fp16_enabled_ = oldFp16;
            config_.yolo.model_path = oldModel;
            (void)yolo_.initialize(config_.yolo);
        }
        return;
#endif
    }

    if (cmd == "MOVE") {
        const std::string a = toUpperCopy(args);
        constexpr float kStepDeg = 1.0f;

        float nextPan = current_pan_deg_;
        float nextTilt = current_tilt_deg_;

        if (a == "LEFT") {
            nextPan -= kStepDeg;
        } else if (a == "RIGHT") {
            nextPan += kStepDeg;
        } else if (a == "FORWARD") {
            nextTilt += kStepDeg;
        } else if (a == "BACK") {
            nextTilt -= kStepDeg;
        } else {
            SANCAK_LOG_WARN("Bilinmeyen MOVE arg: '{}'", std::string(args));
            return;
        }

        nextPan = std::clamp(nextPan, config_.geofence.pan_min_deg, config_.geofence.pan_max_deg);
        nextTilt = std::clamp(nextTilt, config_.geofence.tilt_min_deg, config_.geofence.tilt_max_deg);

        current_pan_deg_ = nextPan;
        current_tilt_deg_ = nextTilt;

        if (turret_controller_) {
            turret_controller_->sendCommand(current_pan_deg_, current_tilt_deg_, false);
        }
        return;
    }

    if (cmd == "GEOFENCE") {
        float panMin = 0.0f, panMax = 0.0f, tiltMin = 0.0f, tiltMax = 0.0f;
        if (!parseFourFloats(args, panMin, panMax, tiltMin, tiltMax)) {
            SANCAK_LOG_WARN("GEOFENCE parse edilemedi: '{}'", std::string(args));
            return;
        }
        if (panMin > panMax) std::swap(panMin, panMax);
        if (tiltMin > tiltMax) std::swap(tiltMin, tiltMax);

        config_.geofence.pan_min_deg = panMin;
        config_.geofence.pan_max_deg = panMax;
        config_.geofence.tilt_min_deg = tiltMin;
        config_.geofence.tilt_max_deg = tiltMax;

        // State machine geofence'ini de güncelle
        if (combat_state_machine_) {
            core::Geofence fence;
            fence.pan_min = panMin;
            fence.pan_max = panMax;
            fence.tilt_min = tiltMin;
            fence.tilt_max = tiltMax;
            combat_state_machine_->setGeofence(fence);
        }

        // Anlık pan/tilt geofence dışında kaldıysa kırp
        current_pan_deg_ = std::clamp(current_pan_deg_, panMin, panMax);
        current_tilt_deg_ = std::clamp(current_tilt_deg_, tiltMin, tiltMax);

        SANCAK_LOG_INFO("Remote GEOFENCE: pan[{},{}] tilt[{},{}]", panMin, panMax, tiltMin, tiltMax);
        return;
    }

    if (cmd == "SET") {
        // Format: KEY,VALUE  (ör: H_MIN,10)
        const auto comma = args.find(',');
        if (comma == std::string_view::npos) {
            SANCAK_LOG_WARN("SET format hatası: '{}'", std::string(args));
            return;
        }
        const std::string key = toUpperCopy(trimView(args.substr(0, comma)));
        const std::string_view valSv = trimView(args.substr(comma + 1));

        int v = 0;
        if (!parseInt(valSv, v)) {
            SANCAK_LOG_WARN("SET value parse edilemedi: key='{}' val='{}'", key, std::string(valSv));
            return;
        }

        // PC UI'daki H/S/V slider'larını balon HSV aralığına mapliyoruz.
        // (IFF için ayrı komut tasarlanabilir; şu an UI tek set gönderiyor.)
        if (key == "H_MIN") config_.balloon.h_min = std::clamp(v, 0, 179);
        else if (key == "H_MAX") config_.balloon.h_max = std::clamp(v, 0, 179);
        else if (key == "S_MIN") config_.balloon.s_min = std::clamp(v, 0, 255);
        else if (key == "S_MAX") config_.balloon.s_max = std::clamp(v, 0, 255);
        else if (key == "V_MIN") config_.balloon.v_min = std::clamp(v, 0, 255);
        else if (key == "V_MAX") config_.balloon.v_max = std::clamp(v, 0, 255);
        else {
            SANCAK_LOG_WARN("SET bilinmeyen key: '{}'", key);
            return;
        }

        if (config_.balloon.h_min > config_.balloon.h_max) std::swap(config_.balloon.h_min, config_.balloon.h_max);

        segmentor_.updateHsvRange(
            config_.balloon.h_min, config_.balloon.h_max,
            config_.balloon.s_min, config_.balloon.s_max,
            config_.balloon.v_min, config_.balloon.v_max);

        ConfigManager::instance().setBalloonHsv(
            config_.balloon.h_min, config_.balloon.h_max,
            config_.balloon.s_min, config_.balloon.s_max,
            config_.balloon.v_min, config_.balloon.v_max);

        return;
    }

    if (cmd == "ORDER") {
        // Format: F16,IHA,HELI,MISSILE (sıra = öncelik)
        std::map<core::TargetClass, core::TargetRule> rules = ConfigManager::instance().getTargetRules();
        if (rules.empty()) {
            rules[core::TargetClass::Drone] = {core::TargetClass::Drone, 0.0f, 50.0f, 2};
            rules[core::TargetClass::F16] = {core::TargetClass::F16, 0.0f, 50.0f, 1};
            rules[core::TargetClass::Helicopter] = {core::TargetClass::Helicopter, 0.0f, 50.0f, 3};
            rules[core::TargetClass::Missile] = {core::TargetClass::Missile, 0.0f, 50.0f, 0};
        }

        auto mapName = [](const std::string& name) -> std::optional<core::TargetClass> {
            if (name == "F16") return core::TargetClass::F16;
            if (name == "IHA" || name == "DRONE") return core::TargetClass::Drone;
            if (name == "HELI" || name == "HELICOPTER") return core::TargetClass::Helicopter;
            if (name == "MISSILE" || name == "FUZE" || name == "BALISTIK_FUZE") return core::TargetClass::Missile;
            return std::nullopt;
        };

        int prio = 0;
        std::string_view rest = args;
        while (!rest.empty()) {
            const auto comma = rest.find(',');
            std::string_view token = (comma == std::string_view::npos) ? rest : rest.substr(0, comma);
            rest = (comma == std::string_view::npos) ? std::string_view{} : rest.substr(comma + 1);
            token = trimView(token);
            if (token.empty()) continue;

            const std::string upper = toUpperCopy(token);
            if (auto tc = mapName(upper)) {
                auto it = rules.find(*tc);
                if (it != rules.end()) {
                    it->second.priority = prio;
                } else {
                    core::TargetRule r;
                    r.target = *tc;
                    r.min_range_m = 0.0f;
                    r.max_range_m = 50.0f;
                    r.priority = prio;
                    rules[*tc] = r;
                }
                prio++;
            }
        }

        if (combat_state_machine_) {
            combat_state_machine_->setRules(std::move(rules));
        }

        SANCAK_LOG_INFO("Remote ORDER uygulandı ({} hedef)", prio);
        return;
    }

    SANCAK_LOG_DEBUG("Bilinmeyen komut: '{}'", std::string(s));
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
