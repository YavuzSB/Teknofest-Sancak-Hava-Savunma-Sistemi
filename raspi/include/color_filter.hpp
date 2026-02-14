/**
 * @file color_filter.hpp
 * @brief Color filtering module - HSV-based segmentation
 *
 * Segments RED (enemy) and BLUE/YELLOW (friend) balloons using HSV ranges
 * plus simple morphology operations.
 */
#pragma once

#include "common_defs.hpp"

#include <opencv2/opencv.hpp>

namespace sancak {

/// Renk kategorileri
enum class BalloonColor {
    RED,      // Enemy
    BLUE,     // Friend
    YELLOW,   // Friend
    UNKNOWN
};

/// Gf6rselle5ftirme renkleri (BGR)
inline const cv::Scalar COLOR_RED_BGR    {0, 0, 255};
inline const cv::Scalar COLOR_GREEN_BGR  {0, 255, 0};
inline const cv::Scalar COLOR_WHITE_BGR  {255, 255, 255};
inline const cv::Scalar COLOR_YELLOW_BGR {0, 255, 255};

/// Morfoloji parametreleri
constexpr int MORPH_KERNEL_SIZE = 5;
constexpr int MORPH_ITERATIONS  = 2;

/**
 * @class ColorFilter
 * @brief HSV color filtering and mask operations
 *
 * Singleton that owns HSV ranges and performs color segmentation.
 */
class ColorFilter {
public:
    /**
     * @brief Singleton instance al31r
     */
    static ColorFilter& instance();

    /**
        * @brief Converts BGR image to HSV and applies blur
        * @param bgrImage Input image (BGR)
        * @return Blurred HSV image
     */
    cv::Mat prepareHsv(const cv::Mat& bgrImage) const;

    /**
        * @brief Creates red mask (two ranges: 0-10 and 170-180)
        * @param hsv HSV image
     * @return Binary mask
     */
    cv::Mat createRedMask(const cv::Mat& hsv) const;

    /**
        * @brief Creates blue mask
        * @param hsv HSV image
     * @return Binary mask
     */
    cv::Mat createBlueMask(const cv::Mat& hsv) const;

    /**
        * @brief Creates yellow mask
        * @param hsv HSV image
     * @return Binary mask
     */
    cv::Mat createYellowMask(const cv::Mat& hsv) const;

    /**
        * @brief Creates friend mask (blue + yellow)
        * @param hsv HSV image
     * @return Binary mask
     */
    cv::Mat createFriendMask(const cv::Mat& hsv) const;

    /**
        * @brief Creates combined mask (red + blue + yellow)
        * @param hsv HSV image
        * @return Binary mask
     */
    cv::Mat createCombinedMask(const cv::Mat& hsv) const;

    /**
        * @brief Applies morphology (noise cleanup)
        * @param mask Input mask
        * @param kernelSize Kernel size (odd)
        * @param iterations Iteration count
        * @return Improved mask
     */
    cv::Mat applyMorphology(const cv::Mat& mask,
                           int kernelSize = MORPH_KERNEL_SIZE,
                           int iterations = MORPH_ITERATIONS) const;

    /**
        * @brief Finds dominant color inside ROI
        * @param redMask Red mask
        * @param blueMask Blue mask
        * @param yellowMask Yellow mask
        * @param roi Region of interest
        * @return BalloonColor Dominant color
     */
    BalloonColor identifyColor(const cv::Mat& redMask,
                              const cv::Mat& blueMask,
                              const cv::Mat& yellowMask,
                              const cv::Rect& roi) const;

    // HSV ranges (public access)
    struct HsvRange {
        cv::Scalar lower;
        cv::Scalar upper;
    };

    // Returns color ranges
    HsvRange getRedLowRange()   const { return redLowRange_; }
    HsvRange getRedHighRange()  const { return redHighRange_; }
    HsvRange getBlueRange()     const { return blueRange_; }
    HsvRange getYellowRange()   const { return yellowRange_; }

private:
    ColorFilter();  // Singleton
    ColorFilter(const ColorFilter&) = delete;
    ColorFilter& operator=(const ColorFilter&) = delete;

    // HSV color ranges
    HsvRange redLowRange_;
    HsvRange redHighRange_;
    HsvRange blueRange_;
    HsvRange yellowRange_;
};

} // namespace sancak
