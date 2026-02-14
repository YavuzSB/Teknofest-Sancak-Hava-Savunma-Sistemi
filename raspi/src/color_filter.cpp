/**
 * @file color_filter.cpp
 * @brief Renk Filtreleme Modfclfc - Implementasyon
 */
#include "color_filter.hpp"

namespace sancak {

ColorFilter::ColorFilter()
{
    // K31rm31z31 (HSV'de k31rm31z31 0 ve 180 civar31ndad31r)
    redLowRange_  = { cv::Scalar(0,   100, 80), cv::Scalar(10,  255, 255) };
    redHighRange_ = { cv::Scalar(170, 100, 80), cv::Scalar(180, 255, 255) };

    // Mavi
    blueRange_   = { cv::Scalar(100, 100, 70), cv::Scalar(130, 255, 255) };

    // Sar31
    yellowRange_ = { cv::Scalar(20,  100, 70), cv::Scalar(35,  255, 255) };
}

ColorFilter& ColorFilter::instance()
{
    static ColorFilter inst;
    return inst;
}

cv::Mat ColorFilter::prepareHsv(const cv::Mat& bgrImage) const
{
    cv::Mat hsv, hsvBlur;
    cv::cvtColor(bgrImage, hsv, cv::COLOR_BGR2HSV);
    cv::GaussianBlur(hsv, hsvBlur, cv::Size(5, 5), 0);
    return hsvBlur;
}

cv::Mat ColorFilter::createRedMask(const cv::Mat& hsv) const
{
    cv::Mat mask1, mask2, combined;
    cv::inRange(hsv, redLowRange_.lower,  redLowRange_.upper,  mask1);
    cv::inRange(hsv, redHighRange_.lower, redHighRange_.upper, mask2);
    cv::bitwise_or(mask1, mask2, combined);
    return combined;
}

cv::Mat ColorFilter::createBlueMask(const cv::Mat& hsv) const
{
    cv::Mat mask;
    cv::inRange(hsv, blueRange_.lower, blueRange_.upper, mask);
    return mask;
}

cv::Mat ColorFilter::createYellowMask(const cv::Mat& hsv) const
{
    cv::Mat mask;
    cv::inRange(hsv, yellowRange_.lower, yellowRange_.upper, mask);
    return mask;
}

cv::Mat ColorFilter::createFriendMask(const cv::Mat& hsv) const
{
    cv::Mat maskBlue   = createBlueMask(hsv);
    cv::Mat maskYellow = createYellowMask(hsv);
    
    cv::Mat combined;
    cv::bitwise_or(maskBlue, maskYellow, combined);
    return combined;
}

cv::Mat ColorFilter::createCombinedMask(const cv::Mat& hsv) const
{
    cv::Mat maskRed    = createRedMask(hsv);
    cv::Mat maskFriend = createFriendMask(hsv);
    
    cv::Mat combined;
    cv::bitwise_or(maskRed, maskFriend, combined);
    return combined;
}

cv::Mat ColorFilter::applyMorphology(const cv::Mat& mask,
                                    int kernelSize,
                                    int iterations) const
{
    cv::Mat kernel = cv::getStructuringElement(
        cv::MORPH_ELLIPSE,
        cv::Size(kernelSize, kernelSize)
    );

    cv::Mat result;
    // Opening: kfce7fck gfcrfcltfcleri temizler
    cv::morphologyEx(mask, result, cv::MORPH_OPEN, kernel, cv::Point(-1, -1), 1);
    // Closing: delikleri kapat31r
    cv::morphologyEx(result, result, cv::MORPH_CLOSE, kernel, cv::Point(-1, -1), iterations);

    return result;
}

BalloonColor ColorFilter::identifyColor(const cv::Mat& redMask,
                                       const cv::Mat& blueMask,
                                       const cv::Mat& yellowMask,
                                       const cv::Rect& roi) const
{
    // ROI s31n31rlar31n31 kontrol et
    if (roi.x < 0 || roi.y < 0 ||
        roi.x + roi.width > redMask.cols ||
        roi.y + roi.height > redMask.rows) {
        return BalloonColor::UNKNOWN;
    }

    // ROI ie7indeki pikselleri say
    cv::Mat roiRed    = redMask(roi);
    cv::Mat roiBlue   = blueMask(roi);
    cv::Mat roiYellow = yellowMask(roi);

    int redPixels    = cv::countNonZero(roiRed);
    int bluePixels   = cv::countNonZero(roiBlue);
    int yellowPixels = cv::countNonZero(roiYellow);

    // En fazla piksele sahip renk kazan31r
    if (redPixels > bluePixels && redPixels > yellowPixels) {
        return BalloonColor::RED;
    } else if (bluePixels > yellowPixels) {
        return BalloonColor::BLUE;
    } else if (yellowPixels > 0) {
        return BalloonColor::YELLOW;
    }

    return BalloonColor::UNKNOWN;
}

} // namespace sancak
