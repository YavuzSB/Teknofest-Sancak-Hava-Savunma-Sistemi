/**
 * @file types.hpp
 * @brief Sancak Hava Savunma Sistemi - Ortak Tip Tanımları
 *
 * Tüm modüller arasında paylaşılan enum, struct ve sabitler.
 *
 * @author Sancak Takımı
 * @date 2026
 */
#pragma once

#include <opencv2/core.hpp>
#include <string>
#include <vector>
#include <optional>
#include <deque>
#include <chrono>
#include <cstdint>

namespace sancak {
/// IFF (Dost-Düşman) durumu
enum class Affiliation : uint8_t {
    Friend = 0,
    Foe = 1,
    Unknown = 2
};

// ============================================================================
// SABITLER
// ============================================================================

/// Varsayılan kamera çözünürlüğü
inline constexpr int kDefaultFrameWidth  = 640;
inline constexpr int kDefaultFrameHeight = 480;

/// YOLO model giriş boyutu
inline constexpr int kModelInputSize = 640;

/// Yerçekimi ivmesi (m/s²)
inline constexpr double kGravity = 9.80665;

// ============================================================================
// ENUMLAR
// ============================================================================

/// Hedef sınıflandırma kategorisi
enum class TargetClass : uint8_t {
    kDrone      = 0,    ///< Düşman drone
    kPlane      = 1,    ///< Düşman uçak
    kHelicopter = 2,    ///< Düşman helikopter
    kJet        = 3,    ///< Düşman jet
    kRocket     = 4,    ///< Düşman roket/füze
    kFriendly   = 5,    ///< Dost hedef (ateş etme!)
    kUnknown    = 255   ///< Tanımlanamayan
};

/// Hedef sınıf adını string olarak döndürür
inline const char* TargetClassName(TargetClass cls) {
    switch (cls) {
        case TargetClass::kDrone:      return "DRONE";
        case TargetClass::kPlane:      return "UCAK";
        case TargetClass::kHelicopter: return "HELIKOPTER";
        case TargetClass::kJet:        return "JET";
        case TargetClass::kRocket:     return "ROKET";
        case TargetClass::kFriendly:   return "DOST";
        default:                       return "BILINMEYEN";
    }
}

/// Hedef düşman mı?
inline bool IsEnemy(TargetClass cls) {
    return cls != TargetClass::kFriendly && cls != TargetClass::kUnknown;
}

/// Pipeline durumu
enum class PipelineState : uint8_t {
    kIdle,       ///< Bekleme - hedef yok
    kDetecting,  ///< YOLO çalışıyor, hedef aranıyor
    kTracking,   ///< Hedef takipte
    kLocked,     ///< Balon kilitlendi, nişan alındı
    kEngaging    ///< Ateş emri verildi
};

inline const char* PipelineStateName(PipelineState state) {
    switch (state) {
        case PipelineState::kIdle:      return "BEKLEME";
        case PipelineState::kDetecting: return "TESPIT";
        case PipelineState::kTracking:  return "TAKIP";
        case PipelineState::kLocked:    return "KILITLI";
        case PipelineState::kEngaging:  return "ATES";
        default:                        return "?";
    }
}

// ============================================================================
// YAPILAR (STRUCTS)
// ============================================================================

/// YOLO tarafından tespit edilen ham hedef
struct Detection {
    cv::Rect2f  bbox;            ///< Bounding box (piksel)
    float       confidence;      ///< Güven skoru [0, 1]
    TargetClass target_class;    ///< Hedef sınıfı
    int         class_id;        ///< Ham sınıf ID (model çıkışı)
    Affiliation affiliation = Affiliation::Unknown; ///< IFF sonucu
};

/// Balon segmentasyon sonucu
struct BalloonResult {
    bool        found = false;       ///< Balon bulundu mu?
    cv::Point2f center;              ///< Balonun merkezi (frame koordinatları)
    float       radius = 0.0f;       ///< Balon yarıçapı (piksel)
    float       confidence = 0.0f;   ///< Tespit güveni
};

/// Mesafe tahmini sonucu
struct DistanceEstimate {
    float distance_m    = 0.0F;  ///< Tahmini mesafe (metre)
    float confidence    = 0.0F;  ///< Tahmin güveni
};

/// Balistik düzeltme vektörü
struct BallisticCorrection {
    float dx_px = 0.0F;   ///< Yatay düzeltme (piksel)
    float dy_px = 0.0F;   ///< Dikey düzeltme (piksel)
    float parallax_deg = 0.0F;   ///< Paralaks açısı (derece)
    float drop_m       = 0.0F;   ///< Balistik düşüş (metre)
    float lead_m       = 0.0F;   ///< Önleme mesafesi (metre)
};

/// Nişan noktası (tüm düzeltmeler uygulanmış)
struct AimPoint {
    cv::Point2f raw_center;      ///< Balonun ham merkezi
    cv::Point2f corrected;       ///< Düzeltilmiş nişan noktası
    BallisticCorrection correction;  ///< Uygulanan düzeltmeler
    float distance_m = 0.0F;    ///< Tahmini mesafe
    bool  valid = false;         ///< Geçerli nişan mı?
};

/// Ağ/telemetri için sadeleştirilmiş nişan sonucu
///
/// Not: CombatPipeline içindeki detaylı AimPoint yapısından türetilir.
struct AimResult {
    cv::Point2f raw_xy;       ///< Ham (x,y)
    cv::Point2f corrected_xy; ///< Düzeltilmiş (x,y)
    int32_t     class_id = -1;///< Modelin ham class ID'si
    bool        valid = false;
};

/// Gelişmiş telemetri (PC'ye gönderilecek)
struct AimTelemetry {
    AimResult aim;
    double inference_ms = 0.0;   ///< YOLO çıkarım süresi (ms)
    double aim_solve_ms = 0.0;   ///< AimSolver hesap süresi (ms)
    uint32_t frame_id = 0;
    uint64_t monotonic_us = 0;   ///< (isteğe bağlı) pipeline timestamp (us)
};

/// PC'ye telemetri göndermek için arayüz
struct INetworkSender {
    virtual ~INetworkSender() = default;
    virtual void publishAimTelemetry(const AimTelemetry& telem) = 0;
};

/// Takip edilen tek hedef
struct TrackedTarget {
    int            track_id = -1;     ///< Takip kimliği
    Detection      detection;         ///< Güncel tespit
    std::deque<Affiliation> affiliation_history; ///< IFF majority voting tamponu (son N frame)
    BalloonResult  balloon;           ///< Balon bulgusu
    AimPoint       aim;               ///< Nişan noktası
    cv::Point2f    velocity;          ///< Hız vektörü (px/frame)
    int            age = 0;           ///< Kaç frame boyunca takip edildi
    int            lost_frames = 0;   ///< Kaç frame boyunca kayboldu
    bool           is_priority = false; ///< Öncelikli hedef mi?
};

/// Tek frame'lik pipeline çıkışı
struct PipelineOutput {
    cv::Mat                      display_frame;  ///< Görselleştirilmiş frame
    PipelineState                state;           ///< Pipeline durumu
    std::vector<TrackedTarget>   targets;         ///< Tüm takip edilen hedefler
    std::optional<AimPoint>      primary_aim;     ///< Birincil nişan noktası
    double                       fps = 0.0;       ///< Mevcut FPS
    double                       inference_ms = 0.0; ///< YOLO çıkarım süresi
};

/// Steady clock kısayolu
using SteadyClock = std::chrono::steady_clock;
using TimePoint   = std::chrono::steady_clock::time_point;
using Duration    = std::chrono::steady_clock::duration;

} // namespace sancak
