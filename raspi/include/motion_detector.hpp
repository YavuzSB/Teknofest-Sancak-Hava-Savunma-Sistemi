/**
 * @file motion_detector.hpp
 * @brief Motion detection module - frame difference based detection
 *
 * Analyzes the difference between consecutive frames to detect motion and
 * returns an approximate motion region.
 */
#pragma once

#include "common_defs.hpp"

#include <opencv2/opencv.hpp>
#include <optional>

namespace sancak {

/// Motion threshold
constexpr int MOTION_THRESHOLD = 2500;

/**
 * @class MotionDetector
 * @brief Frame-based motion detector
 *
 * Computes absdiff against previous frame and extracts the biggest moving blob.
 */
class MotionDetector {
public:
    MotionDetector();

    /**
        * @brief Resets internal state (useful when starting a new stream)
     */
    void reset();

    /**
        * @brief Returns true if the frame differs enough from previous
        * @param currentGray Current grayscale image
        * @param threshold Motion threshold (sum of thresholded pixel values)
        * @return Motion detected?
     */
    bool hasMotion(const cv::Mat& currentGray,
                   int threshold = MOTION_THRESHOLD);

    /**
        * @brief Returns bounding box of the biggest motion region
        * @param currentGray Current grayscale image
        * @param minArea Minimum contour area
        * @return Motion region or nullopt
     */
    std::optional<cv::Rect> detectMotionRegion(const cv::Mat& currentGray,
                                               int minArea = 300);

    /**
        * @brief Last diff image (debug)
        * @return Binary thresholded diff
     */
    cv::Mat getLastDiffImage() const { return lastDiff_; }

private:
    cv::Mat previousGray_;   // Previous frame (grayscale)
    cv::Mat lastDiff_;       // Last diff (debug)
};

} // namespace sancak
