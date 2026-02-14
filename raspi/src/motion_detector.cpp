/**
 * @file motion_detector.cpp
 * @brief Hareket Alg31lama Modfclfc - Implementasyon
 */
#include "motion_detector.hpp"

namespace sancak {

MotionDetector::MotionDetector()
{
}

void MotionDetector::reset()
{
    previousGray_ = cv::Mat();
    lastDiff_     = cv::Mat();
}

bool MotionDetector::hasMotion(const cv::Mat& currentGray, int threshold)
{
    if (previousGray_.empty()) {
        previousGray_ = currentGray.clone();
        return false;
    }

    cv::Mat frameDiff, thresh;
    cv::absdiff(currentGray, previousGray_, frameDiff);
    previousGray_ = currentGray.clone();

    cv::threshold(frameDiff, thresh, 25, 255, cv::THRESH_BINARY);
    lastDiff_ = thresh.clone();  // Debug ie7in sakla

    double totalDiff = cv::sum(thresh)[0];
    return totalDiff > threshold;
}

std::optional<cv::Rect> MotionDetector::detectMotionRegion(const cv::Mat& currentGray,
                                                           int minArea)
{
    if (previousGray_.empty()) {
        previousGray_ = currentGray.clone();
        return std::nullopt;
    }

    // Frame fark31
    cv::Mat diff, blurred, thresh, opened;
    cv::absdiff(previousGray_, currentGray, diff);
    previousGray_ = currentGray.clone();

    // Gaussian blur + threshold
    cv::GaussianBlur(diff, blurred, cv::Size(5, 5), 0);
    cv::threshold(blurred, thresh, 25, 255, cv::THRESH_BINARY);

    // Morfolojik opening (gfcrfcltfc temizleme)
    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(3, 3));
    cv::morphologyEx(thresh, opened, cv::MORPH_OPEN, kernel);

    lastDiff_ = opened.clone();  // Debug ie7in sakla

    // Konturlar31 bul
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(opened, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    if (contours.empty())
        return std::nullopt;

    // En bfcyfck konturu bul
    auto biggest = std::max_element(
        contours.begin(),
        contours.end(),
        [](const std::vector<cv::Point>& a, const std::vector<cv::Point>& b) {
            return cv::contourArea(a) < cv::contourArea(b);
        }
    );

    double area = cv::contourArea(*biggest);
    if (area < minArea)
        return std::nullopt;

    return cv::boundingRect(*biggest);
}

} // namespace sancak
