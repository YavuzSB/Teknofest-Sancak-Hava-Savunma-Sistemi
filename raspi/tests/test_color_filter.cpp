#include <catch2/catch_test_macros.hpp>

#include "color_filter.hpp"

#include <opencv2/opencv.hpp>

namespace {
cv::Mat makeSolidBgrFromHsv(const cv::Scalar& hsv, int width = 160, int height = 120)
{
    cv::Mat hsv1(1, 1, CV_8UC3);
    hsv1.at<cv::Vec3b>(0, 0) = cv::Vec3b(
        static_cast<unsigned char>(hsv[0]),
        static_cast<unsigned char>(hsv[1]),
        static_cast<unsigned char>(hsv[2])
    );

    cv::Mat bgr1;
    cv::cvtColor(hsv1, bgr1, cv::COLOR_HSV2BGR);
    const cv::Vec3b bgr = bgr1.at<cv::Vec3b>(0, 0);

    cv::Mat img(height, width, CV_8UC3, cv::Scalar(bgr[0], bgr[1], bgr[2]));
    return img;
}
}

TEST_CASE("ColorFilter creates red mask for red pixels")
{
    const auto& cf = sancak::ColorFilter::instance();

    // Pure red in HSV is around H=0 (OpenCV H range: 0-179)
    const cv::Mat bgr = makeSolidBgrFromHsv(cv::Scalar(0, 255, 255));
    const cv::Mat hsv = cf.prepareHsv(bgr);
    const cv::Mat redMask = cf.createRedMask(hsv);

    REQUIRE(cv::countNonZero(redMask) > 0);
}

TEST_CASE("ColorFilter creates blue and yellow masks for their HSV ranges")
{
    const auto& cf = sancak::ColorFilter::instance();

    const cv::Mat bgrBlue = makeSolidBgrFromHsv(cv::Scalar(110, 255, 255));
    const cv::Mat hsvBlue = cf.prepareHsv(bgrBlue);
    const cv::Mat blueMask = cf.createBlueMask(hsvBlue);
    REQUIRE(cv::countNonZero(blueMask) > 0);

    const cv::Mat bgrYellow = makeSolidBgrFromHsv(cv::Scalar(25, 255, 255));
    const cv::Mat hsvYellow = cf.prepareHsv(bgrYellow);
    const cv::Mat yellowMask = cf.createYellowMask(hsvYellow);
    REQUIRE(cv::countNonZero(yellowMask) > 0);
}

TEST_CASE("ColorFilter identifyColor returns dominant color inside ROI")
{
    const auto& cf = sancak::ColorFilter::instance();

    const int w = 80;
    const int h = 60;
    cv::Mat redMask(h, w, CV_8UC1, cv::Scalar(0));
    cv::Mat blueMask(h, w, CV_8UC1, cv::Scalar(0));
    cv::Mat yellowMask(h, w, CV_8UC1, cv::Scalar(0));

    const cv::Rect roi(10, 10, 20, 20);
    redMask(roi).setTo(255);

    REQUIRE(cf.identifyColor(redMask, blueMask, yellowMask, roi) == sancak::BalloonColor::RED);

    redMask.setTo(0);
    blueMask(roi).setTo(255);
    REQUIRE(cf.identifyColor(redMask, blueMask, yellowMask, roi) == sancak::BalloonColor::BLUE);

    blueMask.setTo(0);
    yellowMask(roi).setTo(255);
    REQUIRE(cf.identifyColor(redMask, blueMask, yellowMask, roi) == sancak::BalloonColor::YELLOW);
}
