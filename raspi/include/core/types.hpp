#pragma once

#include <cstdint>

namespace sancak::core {

/**
 * @brief Şartname ve YAML kural motoru için hedef sınıfı.
 *
 * Bu enum, model çıktısından (YOLO class_id) bağımsızdır. Amaç; yarışma
 * şartnamesindeki hedef kategorilerini stabil bir API ile temsil etmektir.
 */
enum class TargetClass : std::uint8_t {
    Balloon = 0,    ///< Balon
    Drone   = 1,    ///< İHA
    F16     = 2,    ///< F16
    Helicopter = 3, ///< Helikopter
    Missile = 4,    ///< Balistik Füze / Füze
    Unknown = 255,
};

/**
 * @brief Hedef aidiyeti (dost/düşman/bilinmiyor).
 */
enum class Affiliation : std::uint8_t {
    Friend  = 0, ///< Mavi ok
    Foe     = 1, ///< Kırmızı ok
    Unknown = 255,
};

/**
 * @brief Muharebe durum makinesi.
 */
enum class CombatState : std::uint8_t {
    Idle = 0,
    Searching,
    Tracking,
    Engaging,
    SafeLock, ///< Kural ihlali veya geofence aşımı
};

/**
 * @brief YAML'dan okunacak hedef kuralı.
 */
struct TargetRule {
    TargetClass target = TargetClass::Unknown;
    float min_range_m = 0.0f;
    float max_range_m = 0.0f;
    std::int32_t priority = 0; ///< Daha küçük değer = daha yüksek öncelik (tasarım tercihi)
};

/**
 * @brief Algoritma çıkışı nihai nişan sonucu.
 *
 * raw_*: model/algoritma ham çıktısı
 * corrected_*: balistik/servo/geofence gibi düzeltmeler sonrası
 */
struct AimResult {
    float raw_x = 0.0f;
    float raw_y = 0.0f;

    float corrected_x = 0.0f;
    float corrected_y = 0.0f;

    TargetClass target_class = TargetClass::Unknown;
    float confidence = 0.0f;          ///< [0,1]
    Affiliation affiliation = Affiliation::Unknown;
};

} // namespace sancak::core
