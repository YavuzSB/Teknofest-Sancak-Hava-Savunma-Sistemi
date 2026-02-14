/**
 * @file balloon_detector.hpp
 * @brief Multi-balloon detection system optimized with frame skipping
 *
 * Detects RED (enemy) and BLUE/YELLOW (friend) balloons using
 * shape analysis and color classification.
 */
#pragma once

#include "common_defs.hpp"

#include <opencv2/opencv.hpp>
#include <chrono>
#include "shape_analyzer.hpp"
#include "color_filter.hpp"
#include "motion_detector.hpp"

namespace sancak {

/// Kamera parametreleri
constexpr int CAMERA_INDEX  = 0;
constexpr int CAMERA_WIDTH  = 640;
constexpr int CAMERA_HEIGHT = 480;

/// Frame skipping
constexpr int FRAME_SKIP_IDLE   = 10;  // Uyku modunda
constexpr int FRAME_SKIP_ACTIVE = 1;   // Aktif modda

/// Tek bir balon tespiti
struct BalloonDetection {
    cv::Rect         bbox;        // Bounding box
    double           confidence;  // Gfcven skoru
    ShapeMetrics     metrics;     // 5fekil analizi metrikleri
    BalloonColor     color;       // Renk kategorisi
};

/// Detection results (enemy vs friend)
struct DetectionResults {
    std::vector<BalloonDetection> enemies;  // Red balloons
    std::vector<BalloonDetection> friends;  // Blue/yellow balloons
};

/**
 * @class BalloonDetector
 * @brief Balloon detection system (optimized)
 *
 * Features:
 *   - Frame skipping (anti-lag)
 *   - Single combined mask processing
 *   - Renk + 5fekil bazl31 filtreleme
 *   - Enemy/friend classification
 *   - Headless mode support
 */
class BalloonDetector {
public:
    /**
        * @param cameraIndex Camera device index
        * @param headless If true, UI is disabled
     */
    explicit BalloonDetector(int cameraIndex = CAMERA_INDEX,
                            bool headless = true);

    /**
        * @param videoPath Video file path (instead of camera)
        * @param headless If true, UI is disabled
     */
    explicit BalloonDetector(const std::string& videoPath,
                             bool headless = true);

    /**
        * @brief Initializes camera/video source
     */
    void initializeCamera();

    /**
        * @brief Processes a single frame
        * @param frame Input BGR frame
        * @return DetectionResults Enemy and friend detections
     */
    DetectionResults processFrame(const cv::Mat& frame);

    /**
        * @brief Draws detections on a frame
        * @param frame Frame to draw on (modified)
        * @param detections Detections list
        * @param isEnemy True if enemy
     */
    void drawDetections(cv::Mat& frame,
                       const std::vector<BalloonDetection>& detections,
                       bool isEnemy) const;

    /**
        * @brief Updates FPS estimation
        * @return Current FPS
     */
    double updateFps();

    /**
        * @brief Main loop (frame skipping + motion-based mode switch)
        * Stops on EOF or on user request (UI mode).
     */
    void run();

    /**
        * @brief Releases resources (camera, windows)
     */
    void cleanup();

    /**
        * @brief Returns headless mode
     */
    bool isHeadless() const { return headless_; }

private:
    // Kamera ve ayarlar
    int  cameraIndex_;
    bool headless_;
    std::string videoPath_;
    cv::VideoCapture cap_;

    // Hareket alg31lay31c31
    MotionDetector motionDetector_;

    // Durum
    bool isActiveMode_ = false;

    // Frame saya1fc31lar31
    int frameCount_   = 0;
    int processCount_ = 0;

    // FPS
    std::chrono::steady_clock::time_point lastTime_;
    double fps_ = 0.0;

    // Son tespit sonue7lar31 (decoupled display)
    DetectionResults lastDetections_;
};

} // namespace sancak
