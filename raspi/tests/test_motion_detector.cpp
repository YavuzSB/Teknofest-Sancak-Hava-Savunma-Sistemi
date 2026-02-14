#include <catch2/catch_test_macros.hpp>

#include "motion_detector.hpp"

#include <opencv2/opencv.hpp>

TEST_CASE("MotionDetector detects motion between frames")
{
    sancak::MotionDetector md;

    cv::Mat a(120, 160, CV_8UC1, cv::Scalar(0));
    cv::Mat b = a.clone();
    cv::rectangle(b, cv::Rect(40, 30, 20, 20), cv::Scalar(255), -1);

    REQUIRE(md.hasMotion(a, /*threshold*/ 1) == false);
    REQUIRE(md.hasMotion(b, /*threshold*/ 1) == true);
}

TEST_CASE("MotionDetector returns a motion bounding box")
{
    sancak::MotionDetector md;

    cv::Mat a(120, 160, CV_8UC1, cv::Scalar(0));
    cv::Mat b = a.clone();
    const cv::Rect blob(50, 40, 30, 25);
    cv::rectangle(b, blob, cv::Scalar(255), -1);

    // Prime previous frame
    (void)md.detectMotionRegion(a);

    auto roi = md.detectMotionRegion(b, /*minArea*/ 50);
    REQUIRE(roi.has_value());

    // Bounding box should overlap the blob reasonably
    const cv::Rect detected = *roi;
    REQUIRE((detected & blob).area() > 0);
}
