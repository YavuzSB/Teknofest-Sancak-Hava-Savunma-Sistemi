/**
 * @file config_manager.hpp
 * @brief Sancak Hava Savunma Sistemi - JSON Tabanlı Konfigürasyon Yönetimi
 *
 * Tüm çalışma zamanı parametrelerini merkezi bir yapıdan yönetir.
 * JSON dosyasından okuma/yazma, varsayılan değerler ve thread-safe erişim.
 *
 * @author Sancak Takımı
 * @date 2026
 */
#pragma once

#include "sancak/types.hpp"
#include "core/types.hpp"

#include <string>
#include <mutex>
#include <fstream>
#include <map>

namespace sancak {

/// Kamera ayarları
struct CameraConfig {
    int  device_index   = 0;
    int  frame_width    = kDefaultFrameWidth;
    int  frame_height   = kDefaultFrameHeight;
    int  fps_target     = 30;
    bool flip_horizontal = false;
    bool flip_vertical   = false;

    // FOV (derece) - piksel->açı dönüşümü için
    float h_fov_deg = 60.0f;
    float v_fov_deg = 45.0f;
};

/// Geofence (mekanik sınırlar) ayarları
struct GeofenceConfig {
    float pan_min_deg = -90.0f;
    float pan_max_deg = 90.0f;
    float tilt_min_deg = -10.0f;
    float tilt_max_deg = 45.0f;
};

/// Tetik disiplin motoru ayarları
struct TriggerConfig {
    float aim_tolerance_px = 15.0f;
    int   lock_duration_ms = 150;
    int   burst_duration_ms = 500;
    int   cooldown_ms = 1000;
};

/// IFF (Dost-Düşman) renk eşik ayarları
struct IffConfig {
    struct HsvRange {
        int h_min = 0;
        int h_max = 0;
        int s_min = 0;
        int s_max = 0;
        int v_min = 0;
        int v_max = 0;
    };

    HsvRange foe_red    = {0, 10, 100, 255, 100, 255};
    HsvRange friend_blue = {100, 140, 100, 255, 100, 255};
};

/// YOLO model ayarları
struct YoloConfig {
    std::string model_path      = "models/best.onnx";
    int         input_size      = 416; // ARM için optimize varsayılan
    int         num_threads     = 4;   // Pi 5 için çekirdek sayısı
    float       conf_threshold  = 0.45f;
    float       nms_threshold   = 0.50f;
    bool        use_cuda        = false;   ///< GPU varsa CUDA backend
    /// Sınıf isimleri (model çıktı sırasına göre)
    std::vector<std::string> class_names = {
        "drone", "plane", "helicopter", "jet", "rocket", "friendly"
    };
};

/// Balon segmentasyon ayarları (turuncu için HSV aralıkları)
struct BalloonConfig {
    int h_min = 5;    int h_max = 25;
    int s_min = 100;  int s_max = 255;
    int v_min = 100;  int v_max = 255;
    int morph_kernel_size = 5;
    int morph_iterations  = 2;
    float min_radius_px   = 8.0f;   ///< Minimum balon yarıçapı (piksel)
    float max_radius_px   = 200.0f; ///< Maksimum balon yarıçapı (piksel)
    double min_circularity = 0.55;   ///< Minimum dairesellik
};

/// Mesafe tahmin ayarları
struct DistanceConfig {
    float known_balloon_diameter_m = 0.12f;  ///< Gerçek balon çapı (metre)
    float focal_length_px          = 600.0f; ///< Kamera odak uzaklığı (piksel)
    float min_distance_m           = 3.0f;
    float max_distance_m           = 20.0f;
};

/// Balistik düzeltme ayarları
struct BallisticsConfig {
    /// Paralaks: Kamera-namlu arası mesafe (metre)
    float camera_barrel_offset_x_m = 0.0f;
    float camera_barrel_offset_y_m = 0.05f;  ///< Varsayılan 5cm yukarıda

    /// Namlu çıkış hızı (m/s) - boncuk/mermi
    float muzzle_velocity_mps = 90.0f;

    /// Sıfırlama mesafesi: hangi mesafede paralaks sıfırlanacak
    float zeroing_distance_m = 10.0f;

    /// Sahadan elde edilen düzeltme tablosu [mesafe_m, y_offset_px, x_offset_px]
    struct CorrectionEntry {
        float distance_m;
        float y_offset_px;
        float x_offset_px;
    };
    std::vector<CorrectionEntry> lookup_table = {
        {  5.0f,  15.0f, 0.0f },
        {  7.5f,  10.0f, 0.0f },
        { 10.0f,   6.0f, 0.0f },
        { 12.5f,   3.0f, 0.0f },
        { 15.0f,   1.0f, 0.0f }
    };

    /// Manuel offset (hassas ayar için joystick/config'den ayarlanır)
    float manual_offset_x_px = 0.0f;
    float manual_offset_y_px = 0.0f;
};

/// Takip ayarları
struct TrackingConfig {
    float iou_threshold      = 0.3f;   ///< IoU eşleşme eşiği
    float max_center_distance_px = 50.0f; ///< IoU yetersizse merkez mesafesi eşiği (px)
    int   max_lost_frames    = 15;     ///< Kaç frame sonra hedef düşürülür
    int   min_confirm_frames = 3;      ///< Kaç frame sonra onaylanır
    float velocity_smoothing = 0.7f;   ///< Hız EMA katsayısı
};

/// Seri port (Arduino) ayarları
struct SerialConfig {
    std::string port       = "/dev/ttyUSB0";
    int         baud_rate  = 115200;
    int         timeout_ms = 100;
    bool        enabled    = true;
};

/// Ağ (GCS iletişimi) ayarları
struct NetworkConfig {
    bool        video_enabled = true;
    bool        telemetry_enabled = true;
    std::string gcs_host = "192.168.1.10";
    int         video_udp_port = 5005;
    int         telemetry_tcp_port = 5000;
    int         telemetry_push_port = 5001; ///< Outbound telemetry için ayrı port

    int         jpeg_quality = 70;       ///< [10..95]
    int         udp_mtu_bytes = 1200;    ///< UDP payload hedefi
};

/// Lidar ayarları (opsiyonel)
struct LidarConfig {
    bool  enabled = false;
    bool  use_mock = true;

    // Mock davranışı
    float mock_fixed_distance_m = 0.0f; ///< >0 ise sabit mesafe döner, değilse random
    float mock_min_distance_m = 3.0f;
    float mock_max_distance_m = 20.0f;
    float mock_update_hz = 20.0f;

    // Lidar sensörünün taret merkezine ofseti (metre)
    float offset_x_m = 0.0f;
    float offset_y_m = 0.0f;
    float offset_z_m = 0.0f;
};

/// Tüm ayarları toplayan üst yapı
struct SystemConfig {
    CameraConfig     camera;
    YoloConfig       yolo;
    BalloonConfig    balloon;
    DistanceConfig   distance;
    BallisticsConfig ballistics;
    TrackingConfig   tracking;
    GeofenceConfig   geofence;
    TriggerConfig    trigger;
    IffConfig        iff;
    SerialConfig     serial;
    NetworkConfig    network;
    LidarConfig      lidar;

    /// Genel
    bool headless       = true;   ///< Monitörsüz çalışma
    bool autonomous     = false;  ///< Otonom ateş izni
    std::string log_level = "info";
};

/**
 * @class ConfigManager
 * @brief Thread-safe konfigürasyon yöneticisi
 *
 * JSON dosyasından okur, varsayılan değerlerle birleştirir,
 * çalışma zamanında parametre değişimine izin verir.
 */
class ConfigManager {
public:
    /// Singleton erişim
    static ConfigManager& instance();

    /**
     * @brief JSON dosyasından konfigürasyon yükler
     * @param path JSON dosya yolu
     * @return Başarılı mı?
     */
    [[nodiscard]] bool loadFromFile(const std::string& path);

    /**
     * @brief Mevcut konfigürasyonu JSON dosyasına yazar
     * @param path Dosya yolu
     * @return Başarılı mı?
     */
    [[nodiscard]] bool saveToFile(const std::string& path) const;

    /// Mevcut konfigürasyonu al (thread-safe kopya)
    [[nodiscard]] SystemConfig get() const;

    /// Konfigürasyonu güncelle (thread-safe)
    void set(const SystemConfig& cfg);

    /// Tek bir balistik offset güncelle
    void setManualOffset(float dx, float dy);

    /// Balloon HSV aralığını güncelle
    void setBalloonHsv(int h_min, int h_max, int s_min, int s_max,
                       int v_min, int v_max);

    /// Otonom modu değiştir
    void setAutonomous(bool enabled);

    // Target rules getter
    [[nodiscard]] std::optional<core::TargetRule> getRule(core::TargetClass type) const;
    [[nodiscard]] const std::map<core::TargetClass, core::TargetRule>& getTargetRules() const { return target_rules_; }
public:
    ConfigManager() = default;
    ConfigManager(const ConfigManager&) = delete;
    ConfigManager& operator=(const ConfigManager&) = delete;

private:
    mutable std::mutex mutex_;
    SystemConfig config_;
    std::map<core::TargetClass, core::TargetRule> target_rules_;
};

} // namespace sancak
