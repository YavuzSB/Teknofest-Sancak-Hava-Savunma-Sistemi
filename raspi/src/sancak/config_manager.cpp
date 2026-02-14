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

        // --- Kamera ---
        if (!fs["camera"].empty()) {
            auto node = fs["camera"];
            node["device_index"]    >> config_.camera.device_index;
            node["frame_width"]     >> config_.camera.frame_width;
            node["frame_height"]    >> config_.camera.frame_height;
            node["fps_target"]      >> config_.camera.fps_target;
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
                for (auto it = cn.begin(); it != cn.end(); ++it) {
                    std::string name;
                    (*it) >> name;
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
                for (auto it = lt.begin(); it != lt.end(); ++it) {
                    BallisticsConfig::CorrectionEntry entry;
                    (*it)["distance_m"]  >> entry.distance_m;
                    (*it)["y_offset_px"] >> entry.y_offset_px;
                    (*it)["x_offset_px"] >> entry.x_offset_px;
                    config_.ballistics.lookup_table.push_back(entry);
                }
            }
        }

        // --- Takip ---
        if (!fs["tracking"].empty()) {
            auto node = fs["tracking"];
            node["iou_threshold"]      >> config_.tracking.iou_threshold;
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
