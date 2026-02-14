/**
 * @file detect_balloons.hpp
 * @brief Gelişmiş Balon Tespit Sistemi (Raspberry Pi 5 Optimized) - Header
 *
 * Özellikler:
 *   - Renk (HSV) + Şekil analizi (Circularity, Convexity, Inertia Ratio)
 *   - Frame Skipping ile Anti-Lag Buffer Yönetimi
 *   - Tekil Maske İşleme (%60 CPU Tasarrufu)
 *   - Ön Eleme ile Optimizasyon
 *   - Düşman (Kırmızı) ve Dost (Sarı, Mavi) balon ayrımı
 *   - HEADLESS Mode (SSH/Monitörsüz çalışma desteği)
 */
#pragma once

#include "common_defs.hpp"

#include <opencv2/opencv.hpp>
#include <chrono>

// ============================================================================
// YAPILANDIRMA PARAMETRELERİ
// ============================================================================

// Renk Tanımları (BGR Format)
inline const cv::Scalar COLOR_RED_BGR   {0, 0, 255};
inline const cv::Scalar COLOR_GREEN_BGR {0, 255, 0};
inline const cv::Scalar COLOR_WHITE_BGR {255, 255, 255};
inline const cv::Scalar COLOR_YELLOW_BGR{0, 255, 255};

// Şekil Analiz Eşikleri
constexpr double CIRCULARITY_THRESHOLD = 0.6;
constexpr double CONVEXITY_THRESHOLD   = 0.85;
constexpr double INERTIA_RATIO_MIN     = 0.3;
constexpr double INERTIA_RATIO_MAX     = 1.5;

// Minimum kontur alanı (piksel²)
constexpr int MIN_CONTOUR_AREA = 500;

// Hareket algılama eşiği
constexpr int MOTION_THRESHOLD = 2500;

// Frame Skipping Ayarları
constexpr int FRAME_SKIP_IDLE   = 10;   // Uyku: her 10. kare
constexpr int FRAME_SKIP_ACTIVE = 1;    // Aktif: her kare

// Kamera Ayarları
constexpr int CAMERA_INDEX  = 0;
constexpr int CAMERA_WIDTH  = 640;
constexpr int CAMERA_HEIGHT = 480;

// Morfoloji parametreleri
constexpr int MORPH_KERNEL_SIZE = 5;
constexpr int MORPH_ITERATIONS  = 2;


// ============================================================================
// YAPI TANIMLARI
// ============================================================================

/// Şekil analizi debug bilgisi
struct ShapeDebugInfo {
    int    area         = 0;
    double circularity  = 0.0;
    double convexity    = 0.0;
    double inertia      = 0.0;
};

/// is_balloon_shape sonucu
struct BalloonShapeResult {
    bool           isBalloon  = false;
    double         confidence = 0.0;
    ShapeDebugInfo debug;
};

/// Tek bir balon tespiti
struct BalloonDetection {
    cv::Rect       bbox;           // Bounding box
    double         confidence;
    ShapeDebugInfo debug;
    std::string    colorName;      // "red", "blue", "yellow"
};

/// Tespit sonuçları
struct DetectionResult {
    std::vector<BalloonDetection> enemy;   // Düşman (kırmızı)
    std::vector<BalloonDetection> friendd; // Dost   (mavi, sarı)  — friend C++ ayrılmış
};


// ============================================================================
// YARDIMCI FONKSİYONLAR - ŞEKİL ANALİZİ
// ============================================================================

/// Dairesellik (Circularity): 4πA / P²
double calculateCircularity(const std::vector<cv::Point>& contour,
                            double area, double perimeter);

/// Dışbükeylik (Convexity): A / ConvexHullArea
double calculateConvexity(const std::vector<cv::Point>& contour, double area);

/// Inertia Ratio (Atalet Oranı) — elipsin eksen oranı
double calculateInertiaRatio(const std::vector<cv::Point>& contour);

/// Konturun balon şekline uyup uymadığını kontrol eder
BalloonShapeResult isBalloonShape(const std::vector<cv::Point>& contour);


// ============================================================================
// YARDIMCI FONKSİYONLAR - GÖRÜNTÜ İŞLEME
// ============================================================================

/// Maskeye morfolojik işlemler uygular (gürültü temizleme)
cv::Mat applyMorphology(const cv::Mat& mask,
                        int kernelSize = MORPH_KERNEL_SIZE,
                        int iterations = MORPH_ITERATIONS);

/// İki frame arasındaki hareketi algılar
bool detectMotion(const cv::Mat& currentGray,
                  const cv::Mat& previousGray,
                  int threshold = MOTION_THRESHOLD);


// ============================================================================
// ANA BALON TESPİT SINIFI
// ============================================================================

/**
 * @class AdvancedBalloonDetector
 * @brief Gelişmiş Balon Tespit Sistemi (Pi 5 Optimized)
 *
 *   - Renk + Şekil bazlı filtreleme
 *   - Frame Skipping (Anti-Lag)
 *   - Tekil Maske İşleme
 *   - Ön Eleme Optimizasyonu
 */
class AdvancedBalloonDetector {
public:
    /**
     * @param cameraIndex  Kamera indeksi.
     * @param headless     true ise cv::imshow kullanılmaz (SSH/monitörsüz).
     */
    explicit AdvancedBalloonDetector(int cameraIndex = CAMERA_INDEX,
                                    bool headless = true);

    /// Kamerayı başlat ve ayarla
    void initializeCamera();

    /**
     * @brief Tek seferde tüm renkleri işler (optimize).
     *
     * 1 HSV dönüşümü + birleşik mask + 1 findContours.
     *
     * @param frame  BGR görüntü.
     * @return DetectionResult  Düşman ve dost tespitler.
     */
    DetectionResult processAllColors(const cv::Mat& frame);

    /// Tespit sonuçlarını frame üzerine çizer
    void drawDetections(cv::Mat& frame,
                        const std::vector<BalloonDetection>& detections,
                        bool isEnemy);

    /// FPS hesapla
    double updateFps();

    /**
     * @brief Ana tespit döngüsü (frame skipping ile anti-lag).
     *        Ctrl+C veya 'q' tuşu ile durdurulur.
     */
    void run();

    /// Kaynakları temizle
    void cleanup();

private:
    int  cameraIndex_;
    bool headless_;
    cv::VideoCapture cap_;

    // Hareket durumu
    cv::Mat previousGray_;
    bool    isActiveMode_ = false;

    // Frame skipping
    int frameCount_   = 0;
    int processCount_ = 0;

    // FPS
    std::chrono::steady_clock::time_point lastTime_;
    double fps_ = 0.0;

    // Son tespit sonuçları
    DetectionResult lastDetections_;
};
