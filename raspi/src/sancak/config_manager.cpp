/**
 * @file config_manager.cpp
 * @brief Konfigürasyon Yöneticisi - Implementasyon
 *
 * Basit JSON parser kullanarak ayarları okur/yazar.
 * nlohmann/json bağımlılığı olmadan, OpenCV FileStorage ile çalışır.
 */
#include "sancak/config_manager.hpp"
#include "sancak/logger.hpp"

#include <opencv2/core/persistence.hpp>
#include <fstream>
#include <sstream>

namespace sancak {

ConfigManager& ConfigManager::instance() {
    static ConfigManager inst;
    return inst;
}

bool ConfigManager::loadFromFile(const std::string& path) {
    std::lock_guard<std::mutex> lock(mutex_);

    try {
        cv::FileStorage fs(path, cv::FileStorage::READ);
        if (!fs.isOpened()) {
            SANCAK_LOG_WARN("Config dosyası açılamadı: {}, varsayılan değerler kullanılıyor", path);
            return false;
        }

        // --- Target Rules ---
        target_rules_.clear();
        if (!fs["targets"].empty()) {
            auto node = fs["targets"];
            struct TargetYaml {
                const char* yaml_name;
                core::TargetClass class_enum;
            } map[] = {
                {"iha", core::TargetClass::Drone},
                {"f16", core::TargetClass::F16},
                {"helikopter", core::TargetClass::Helicopter},
                {"balistik_fuze", core::TargetClass::Missile},
            };
            for (const auto& entry : map) {
                if (!node[entry.yaml_name].empty()) {
                    auto tnode = node[entry.yaml_name];
                    core::TargetRule rule;
                    rule.target = entry.class_enum;
                    tnode["min_range_m"] >> rule.min_range_m;
                    tnode["max_range_m"] >> rule.max_range_m;
                    tnode["priority"] >> rule.priority;
                    target_rules_[entry.class_enum] = rule;
                }
            }
        }

        // --- Kamera ---
        if (!fs["camera"].empty()) {
            auto node = fs["camera"];
            node["device_index"]    >> config_.camera.device_index;
            node["frame_width"]     >> config_.camera.frame_width;
            node["frame_height"]    >> config_.camera.frame_height;
            node["fps_target"]      >> config_.camera.fps_target;

            if (!node["h_fov_deg"].empty()) node["h_fov_deg"] >> config_.camera.h_fov_deg;
            if (!node["v_fov_deg"].empty()) node["v_fov_deg"] >> config_.camera.v_fov_deg;
        }

        // --- Geofence ---
        if (!fs["geofence"].empty()) {
            auto node = fs["geofence"];
            if (!node["pan_deg"].empty()) {
                auto pn = node["pan_deg"];
                if (!pn["min"].empty()) pn["min"] >> config_.geofence.pan_min_deg;
                if (!pn["max"].empty()) pn["max"] >> config_.geofence.pan_max_deg;
            }
            if (!node["tilt_deg"].empty()) {
                auto tn = node["tilt_deg"];
                if (!tn["min"].empty()) tn["min"] >> config_.geofence.tilt_min_deg;
                if (!tn["max"].empty()) tn["max"] >> config_.geofence.tilt_max_deg;
            }
        }

        // --- Trigger Controller ---
        if (!fs["trigger"].empty()) {
            auto node = fs["trigger"];
            if (!node["aim_tolerance_px"].empty()) node["aim_tolerance_px"] >> config_.trigger.aim_tolerance_px;
            if (!node["lock_frames_required"].empty()) node["lock_frames_required"] >> config_.trigger.lock_frames_required;
            if (!node["burst_duration_ms"].empty()) node["burst_duration_ms"] >> config_.trigger.burst_duration_ms;
            if (!node["cooldown_ms"].empty()) node["cooldown_ms"] >> config_.trigger.cooldown_ms;
        }

        // --- IFF (Dost-Düşman renk eşikleri) ---
        if (!fs["iff"].empty()) {
            auto node = fs["iff"];
            if (!node["foe_red"].empty()) {
                auto red = node["foe_red"];
                if (!red["h_min"].empty()) red["h_min"] >> config_.iff.foe_red.h_min;
                if (!red["h_max"].empty()) red["h_max"] >> config_.iff.foe_red.h_max;
                if (!red["s_min"].empty()) red["s_min"] >> config_.iff.foe_red.s_min;
                if (!red["s_max"].empty()) red["s_max"] >> config_.iff.foe_red.s_max;
                if (!red["v_min"].empty()) red["v_min"] >> config_.iff.foe_red.v_min;
                if (!red["v_max"].empty()) red["v_max"] >> config_.iff.foe_red.v_max;
            }
            if (!node["friend_blue"].empty()) {
                auto blue = node["friend_blue"];
                if (!blue["h_min"].empty()) blue["h_min"] >> config_.iff.friend_blue.h_min;
                if (!blue["h_max"].empty()) blue["h_max"] >> config_.iff.friend_blue.h_max;
                if (!blue["s_min"].empty()) blue["s_min"] >> config_.iff.friend_blue.s_min;
                if (!blue["s_max"].empty()) blue["s_max"] >> config_.iff.friend_blue.s_max;
                if (!blue["v_min"].empty()) blue["v_min"] >> config_.iff.friend_blue.v_min;
                if (!blue["v_max"].empty()) blue["v_max"] >> config_.iff.friend_blue.v_max;
            }
        }

        // --- YOLO ---
        if (!fs["yolo"].empty()) {
            auto node = fs["yolo"];
            node["model_path"]      >> config_.yolo.model_path;
            node["input_size"]      >> config_.yolo.input_size;
            node["conf_threshold"]  >> config_.yolo.conf_threshold;
            node["nms_threshold"]   >> config_.yolo.nms_threshold;

            int use_cuda = 0;
            node["use_cuda"] >> use_cuda;
            config_.yolo.use_cuda = (use_cuda != 0);

            if (!node["class_names"].empty()) {
                config_.yolo.class_names.clear();
                auto cn = node["class_names"];
                for (auto&& it : cn) {
                    std::string name;
                    it >> name;
                    config_.yolo.class_names.push_back(name);
                }
            }
        }

        // --- Balon ---
        if (!fs["balloon"].empty()) {
            auto node = fs["balloon"];
            node["h_min"] >> config_.balloon.h_min;
            node["h_max"] >> config_.balloon.h_max;
            node["s_min"] >> config_.balloon.s_min;
            node["s_max"] >> config_.balloon.s_max;
            node["v_min"] >> config_.balloon.v_min;
            node["v_max"] >> config_.balloon.v_max;
            node["min_radius_px"]    >> config_.balloon.min_radius_px;
            node["max_radius_px"]    >> config_.balloon.max_radius_px;
            node["min_circularity"]  >> config_.balloon.min_circularity;
        }

        // --- Mesafe ---
        if (!fs["distance"].empty()) {
            auto node = fs["distance"];
            node["known_balloon_diameter_m"] >> config_.distance.known_balloon_diameter_m;
            node["focal_length_px"]          >> config_.distance.focal_length_px;
            node["min_distance_m"]           >> config_.distance.min_distance_m;
            node["max_distance_m"]           >> config_.distance.max_distance_m;
        }

        // --- Balistik ---
        if (!fs["ballistics"].empty()) {
            auto node = fs["ballistics"];
            node["camera_barrel_offset_x_m"] >> config_.ballistics.camera_barrel_offset_x_m;
            node["camera_barrel_offset_y_m"] >> config_.ballistics.camera_barrel_offset_y_m;
            node["muzzle_velocity_mps"]      >> config_.ballistics.muzzle_velocity_mps;
            node["zeroing_distance_m"]       >> config_.ballistics.zeroing_distance_m;
            node["manual_offset_x_px"]       >> config_.ballistics.manual_offset_x_px;
            node["manual_offset_y_px"]       >> config_.ballistics.manual_offset_y_px;

            // Lookup table
            if (!node["lookup_table"].empty()) {
                config_.ballistics.lookup_table.clear();
                auto lt = node["lookup_table"];
                for (auto&& it : lt) {
                    BallisticsConfig::CorrectionEntry entry;
                    it["distance_m"]  >> entry.distance_m;
                    it["y_offset_px"] >> entry.y_offset_px;
                    it["x_offset_px"] >> entry.x_offset_px;
                    config_.ballistics.lookup_table.push_back(entry);
                }
            }
        }

        // --- Takip ---
        if (!fs["tracking"].empty()) {
            auto node = fs["tracking"];
            node["iou_threshold"]      >> config_.tracking.iou_threshold;
            if (!node["max_center_distance_px"].empty()) {
                node["max_center_distance_px"] >> config_.tracking.max_center_distance_px;
            }
            node["max_lost_frames"]    >> config_.tracking.max_lost_frames;
            node["min_confirm_frames"] >> config_.tracking.min_confirm_frames;
            node["velocity_smoothing"] >> config_.tracking.velocity_smoothing;
        }

        // --- Seri ---
        if (!fs["serial"].empty()) {
            auto node = fs["serial"];
            node["port"]      >> config_.serial.port;
            node["baud_rate"] >> config_.serial.baud_rate;
            node["timeout_ms"] >> config_.serial.timeout_ms;
            int en = 1;
            node["enabled"] >> en;
            config_.serial.enabled = (en != 0);
        }

        // --- Network (opsiyonel) ---
        // Future-proof: hem eski (network) hem yeni (networking) şemasını destekle.
        cv::FileNode netNode;
        if (!fs["networking"].empty()) {
            netNode = fs["networking"];
        } else if (!fs["network"].empty()) {
            netNode = fs["network"];
        }

        if (!netNode.empty()) {
            auto node = netNode;
            int ve = config_.network.video_enabled ? 1 : 0;
            if (!node["video_enabled"].empty()) node["video_enabled"] >> ve;
            config_.network.video_enabled = (ve != 0);

            int te = config_.network.telemetry_enabled ? 1 : 0;
            if (!node["telemetry_enabled"].empty()) node["telemetry_enabled"] >> te;
            config_.network.telemetry_enabled = (te != 0);

            // Alan adları alias'ları:
            // - target_pc_ip  -> gcs_host
            // - video_port    -> video_udp_port
            // - telemetry_port-> telemetry_tcp_port
            // - telemetry_push_port -> telemetry_push_port
            if (!node["gcs_host"].empty()) node["gcs_host"] >> config_.network.gcs_host;
            if (!node["target_pc_ip"].empty()) node["target_pc_ip"] >> config_.network.gcs_host;

            if (!node["video_udp_port"].empty()) node["video_udp_port"] >> config_.network.video_udp_port;
            if (!node["video_port"].empty()) node["video_port"] >> config_.network.video_udp_port;

            if (!node["telemetry_tcp_port"].empty()) node["telemetry_tcp_port"] >> config_.network.telemetry_tcp_port;
            if (!node["telemetry_port"].empty()) node["telemetry_port"] >> config_.network.telemetry_tcp_port;
            if (!node["telemetry_push_port"].empty()) node["telemetry_push_port"] >> config_.network.telemetry_push_port;

            if (!node["jpeg_quality"].empty()) node["jpeg_quality"] >> config_.network.jpeg_quality;
            if (!node["udp_mtu_bytes"].empty()) node["udp_mtu_bytes"] >> config_.network.udp_mtu_bytes;
        }

        // --- Genel ---
        if (!fs["headless"].empty()) {
            int hl = 1;
            fs["headless"] >> hl;
            config_.headless = (hl != 0);
        }
        if (!fs["autonomous"].empty()) {
            int au = 0;
            fs["autonomous"] >> au;
            config_.autonomous = (au != 0);
        }
        if (!fs["log_level"].empty()) {
            fs["log_level"] >> config_.log_level;
        }

        fs.release();
        SANCAK_LOG_INFO("Konfigürasyon yüklendi: {}", path);
        return true;

    } catch (const std::exception& e) {
        SANCAK_LOG_ERROR("Config yükleme hatası: {}", e.what());
        return false;
    }
}

bool ConfigManager::saveToFile(const std::string& path) const {
    std::lock_guard<std::mutex> lock(mutex_);

    try {
        cv::FileStorage fs(path, cv::FileStorage::WRITE);
        if (!fs.isOpened()) {
            SANCAK_LOG_ERROR("Config dosyası yazılamadı: {}", path);
            return false;
        }

        // --- Kamera ---
        fs << "camera" << "{";
        fs << "device_index"    << config_.camera.device_index;
        fs << "frame_width"     << config_.camera.frame_width;
        fs << "frame_height"    << config_.camera.frame_height;
        fs << "fps_target"      << config_.camera.fps_target;
        fs << "h_fov_deg"       << config_.camera.h_fov_deg;
        fs << "v_fov_deg"       << config_.camera.v_fov_deg;
        fs << "}";

        // --- Geofence ---
        fs << "geofence" << "{";
        fs << "pan_deg" << "{";
        fs << "min" << config_.geofence.pan_min_deg;
        fs << "max" << config_.geofence.pan_max_deg;
        fs << "}";
        fs << "tilt_deg" << "{";
        fs << "min" << config_.geofence.tilt_min_deg;
        fs << "max" << config_.geofence.tilt_max_deg;
        fs << "}";
        fs << "}";

        // --- Trigger Controller ---
        fs << "trigger" << "{";
        fs << "aim_tolerance_px" << config_.trigger.aim_tolerance_px;
        fs << "lock_frames_required" << config_.trigger.lock_frames_required;
        fs << "burst_duration_ms" << config_.trigger.burst_duration_ms;
        fs << "cooldown_ms" << config_.trigger.cooldown_ms;
        fs << "}";

        // --- IFF (Dost-Düşman renk eşikleri) ---
        fs << "iff" << "{";
        fs << "foe_red" << "{";
        fs << "h_min" << config_.iff.foe_red.h_min;
        fs << "h_max" << config_.iff.foe_red.h_max;
        fs << "s_min" << config_.iff.foe_red.s_min;
        fs << "s_max" << config_.iff.foe_red.s_max;
        fs << "v_min" << config_.iff.foe_red.v_min;
        fs << "v_max" << config_.iff.foe_red.v_max;
        fs << "}";
        fs << "friend_blue" << "{";
        fs << "h_min" << config_.iff.friend_blue.h_min;
        fs << "h_max" << config_.iff.friend_blue.h_max;
        fs << "s_min" << config_.iff.friend_blue.s_min;
        fs << "s_max" << config_.iff.friend_blue.s_max;
        fs << "v_min" << config_.iff.friend_blue.v_min;
        fs << "v_max" << config_.iff.friend_blue.v_max;
        fs << "}";
        fs << "}";

        // --- YOLO ---
        fs << "yolo" << "{";
        fs << "model_path"      << config_.yolo.model_path;
        fs << "input_size"      << config_.yolo.input_size;
        fs << "conf_threshold"  << config_.yolo.conf_threshold;
        fs << "nms_threshold"   << config_.yolo.nms_threshold;
        fs << "use_cuda"        << (config_.yolo.use_cuda ? 1 : 0);
        fs << "class_names"     << "[";
        for (const auto& name : config_.yolo.class_names) {
            fs << name;
        }
        fs << "]" << "}";

        // --- Balon ---
        fs << "balloon" << "{";
        fs << "h_min" << config_.balloon.h_min;
        fs << "h_max" << config_.balloon.h_max;
        fs << "s_min" << config_.balloon.s_min;
        fs << "s_max" << config_.balloon.s_max;
        fs << "v_min" << config_.balloon.v_min;
        fs << "v_max" << config_.balloon.v_max;
        fs << "min_radius_px"   << config_.balloon.min_radius_px;
        fs << "max_radius_px"   << config_.balloon.max_radius_px;
        fs << "min_circularity" << config_.balloon.min_circularity;
        fs << "}";

        // --- Mesafe ---
        fs << "distance" << "{";
        fs << "known_balloon_diameter_m" << config_.distance.known_balloon_diameter_m;
        fs << "focal_length_px"          << config_.distance.focal_length_px;
        fs << "min_distance_m"           << config_.distance.min_distance_m;
        fs << "max_distance_m"           << config_.distance.max_distance_m;
        fs << "}";

        // --- Balistik ---
        fs << "ballistics" << "{";
        fs << "camera_barrel_offset_x_m" << config_.ballistics.camera_barrel_offset_x_m;
        fs << "camera_barrel_offset_y_m" << config_.ballistics.camera_barrel_offset_y_m;
        fs << "muzzle_velocity_mps"      << config_.ballistics.muzzle_velocity_mps;
        fs << "zeroing_distance_m"       << config_.ballistics.zeroing_distance_m;
        fs << "manual_offset_x_px"       << config_.ballistics.manual_offset_x_px;
        fs << "manual_offset_y_px"       << config_.ballistics.manual_offset_y_px;
        fs << "lookup_table" << "[";
        for (const auto& e : config_.ballistics.lookup_table) {
            fs << "{";
            fs << "distance_m"  << e.distance_m;
            fs << "y_offset_px" << e.y_offset_px;
            fs << "x_offset_px" << e.x_offset_px;
            fs << "}";
        }
        fs << "]" << "}";

        // --- Takip ---
        fs << "tracking" << "{";
        fs << "iou_threshold"      << config_.tracking.iou_threshold;
        fs << "max_center_distance_px" << config_.tracking.max_center_distance_px;
        fs << "max_lost_frames"    << config_.tracking.max_lost_frames;
        fs << "min_confirm_frames" << config_.tracking.min_confirm_frames;
        fs << "velocity_smoothing" << config_.tracking.velocity_smoothing;
        fs << "}";

        // --- Seri ---
        fs << "serial" << "{";
        fs << "port"       << config_.serial.port;
        fs << "baud_rate"  << config_.serial.baud_rate;
        fs << "timeout_ms" << config_.serial.timeout_ms;
        fs << "enabled"    << (config_.serial.enabled ? 1 : 0);
        fs << "}";

        // --- Network (opsiyonel) ---
        // Yeni şema adı ve anahtarlar: networking / target_pc_ip / video_port / telemetry_port
        fs << "networking" << "{";
        fs << "video_enabled"     << (config_.network.video_enabled ? 1 : 0);
        fs << "telemetry_enabled" << (config_.network.telemetry_enabled ? 1 : 0);
        fs << "target_pc_ip"      << config_.network.gcs_host;
        fs << "video_port"        << config_.network.video_udp_port;
        fs << "telemetry_port"    << config_.network.telemetry_tcp_port;
        fs << "telemetry_push_port" << config_.network.telemetry_push_port;
        fs << "jpeg_quality"      << config_.network.jpeg_quality;
        fs << "udp_mtu_bytes"     << config_.network.udp_mtu_bytes;
        fs << "}";

        // --- Genel ---
        fs << "headless"   << (config_.headless ? 1 : 0);
        fs << "autonomous" << (config_.autonomous ? 1 : 0);
        fs << "log_level"  << config_.log_level;

        fs.release();
        SANCAK_LOG_INFO("Konfigürasyon kaydedildi: {}", path);
        return true;

    } catch (const std::exception& e) {
        SANCAK_LOG_ERROR("Config kaydetme hatası: {}", e.what());
        return false;
    }
}

SystemConfig ConfigManager::get() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_;
}

std::optional<core::TargetRule> ConfigManager::getRule(core::TargetClass type) const {
    auto it = target_rules_.find(type);
    if (it != target_rules_.end()) return it->second;
    return std::nullopt;
}

void ConfigManager::set(const SystemConfig& cfg) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_ = cfg;
}

void ConfigManager::setManualOffset(float dx, float dy) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_.ballistics.manual_offset_x_px = dx;
    config_.ballistics.manual_offset_y_px = dy;
    SANCAK_LOG_INFO("Manuel offset güncellendi: dx={}, dy={}", dx, dy);
}

void ConfigManager::setBalloonHsv(int h_min, int h_max, int s_min, int s_max,
                                    int v_min, int v_max) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_.balloon.h_min = h_min;
    config_.balloon.h_max = h_max;
    config_.balloon.s_min = s_min;
    config_.balloon.s_max = s_max;
    config_.balloon.v_min = v_min;
    config_.balloon.v_max = v_max;
    SANCAK_LOG_INFO("Balloon HSV güncellendi: H[{}-{}] S[{}-{}] V[{}-{}]",
                    h_min, h_max, s_min, s_max, v_min, v_max);
}

void ConfigManager::setAutonomous(bool enabled) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_.autonomous = enabled;
    SANCAK_LOG_INFO("Otonom mod: {}", enabled ? "AÇIK" : "KAPALI");
}

} // namespace sancak
