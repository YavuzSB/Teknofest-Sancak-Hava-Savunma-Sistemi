#include <catch2/catch_test_macros.hpp>

#include "tracking_pipeline.hpp"

#include <opencv2/opencv.hpp>

namespace {
std::pair<cv::Scalar, cv::Scalar> redRange()
{
    // Matches ColorFilter redLowRange_ roughly
    return {cv::Scalar(0, 100, 80), cv::Scalar(10, 255, 255)};
}

cv::Mat makeFrameWithRedCircle(int w, int h, cv::Point center, int radius)
{
    cv::Mat hsv(h, w, CV_8UC3, cv::Scalar(0, 0, 0));
    cv::circle(hsv, center, radius, cv::Scalar(0, 255, 255), -1);

    cv::Mat bgr;
    cv::cvtColor(hsv, bgr, cv::COLOR_HSV2BGR);
    return bgr;
}
}

TEST_CASE("TrackingPipeline goes IDLE -> ROI -> TRACK on motion+color")
{
    sancak::TrackingPipeline pipeline(redRange);

    const int w = 220;
    const int h = 180;

    const cv::Mat frame1(h, w, CV_8UC3, cv::Scalar(0, 0, 0));
    const cv::Mat frame2 = makeFrameWithRedCircle(w, h, cv::Point(110, 90), 25);
    const cv::Mat frame3 = frame2.clone();

    auto r1 = pipeline.process(frame1, true);
    REQUIRE(r1.state == sancak::TrackingState::IDLE);
    REQUIRE(!r1.target.has_value());

    auto r2 = pipeline.process(frame2, true);
    REQUIRE(r2.state == sancak::TrackingState::ROI);

    auto r3 = pipeline.process(frame3, true);
    REQUIRE(r3.state == sancak::TrackingState::TRACK);
    REQUIRE(r3.target.has_value());

    const auto t = *r3.target;
    REQUIRE(t.x > 0);
    REQUIRE(t.y > 0);
    REQUIRE(t.x < w);
    REQUIRE(t.y < h);
}
