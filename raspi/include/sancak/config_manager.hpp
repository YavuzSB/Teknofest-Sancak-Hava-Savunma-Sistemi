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

#include <string>
#include <mutex>
#include <fstream>

namespace sancak {

/// Kamera ayarları
struct CameraConfig {
    int  device_index   = 0;
    int  frame_width    = kDefaultFrameWidth;
    int  frame_height   = kDefaultFrameHeight;
    int  fps_target     = 30;
    bool flip_horizontal = false;
    bool flip_vertical   = false;
};

/// YOLO model ayarları
struct YoloConfig {
    std::string model_path      = "models/best.onnx";
    int         input_size      = kModelInputSize;
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

/// Tüm ayarları toplayan üst yapı
struct SystemConfig {
    CameraConfig     camera;
    YoloConfig       yolo;
    BalloonConfig    balloon;
    DistanceConfig   distance;
    BallisticsConfig ballistics;
    TrackingConfig   tracking;
    SerialConfig     serial;

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
    bool loadFromFile(const std::string& path);

    /**
     * @brief Mevcut konfigürasyonu JSON dosyasına yazar
     * @param path Dosya yolu
     * @return Başarılı mı?
     */
    bool saveToFile(const std::string& path) const;

    /// Mevcut konfigürasyonu al (thread-safe kopya)
    SystemConfig get() const;

    /// Konfigürasyonu güncelle (thread-safe)
    void set(const SystemConfig& cfg);

    /// Tek bir balistik offset güncelle
    void setManualOffset(float dx, float dy);

    /// Balloon HSV aralığını güncelle
    void setBalloonHsv(int h_min, int h_max, int s_min, int s_max,
                       int v_min, int v_max);

    /// Otonom modu değiştir
    void setAutonomous(bool enabled);

private:
    ConfigManager() = default;
    ConfigManager(const ConfigManager&) = delete;
    ConfigManager& operator=(const ConfigManager&) = delete;

    mutable std::mutex mutex_;
    SystemConfig config_;
};

} // namespace sancak
