#include <catch2/catch_test_macros.hpp>

#include "balloon_detector.hpp"

#include <opencv2/opencv.hpp>

using namespace sancak;

static cv::Mat makeBlackFrame(int w, int h)
{
    return cv::Mat(h, w, CV_8UC3, cv::Scalar(0, 0, 0));
}

TEST_CASE("BalloonDetector detects a red balloon as enemy", "[detector]")
{
    BalloonDetector detector(/*cameraIndex=*/0, /*headless=*/true);

    cv::Mat frame = makeBlackFrame(320, 240);
    cv::circle(frame, cv::Point(160, 120), 30, cv::Scalar(0, 0, 255), cv::FILLED); // red

    const auto results = detector.processFrame(frame);

    REQUIRE(results.enemies.size() >= 1);
}

TEST_CASE("BalloonDetector detects a blue balloon as friend", "[detector]")
{
    BalloonDetector detector(/*cameraIndex=*/0, /*headless=*/true);

    cv::Mat frame = makeBlackFrame(320, 240);
    cv::circle(frame, cv::Point(100, 120), 30, cv::Scalar(255, 0, 0), cv::FILLED); // blue

    const auto results = detector.processFrame(frame);

    REQUIRE(results.friends.size() >= 1);
}
