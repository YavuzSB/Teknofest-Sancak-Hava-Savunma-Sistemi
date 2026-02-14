#include <catch2/catch_test_macros.hpp>

#include "shape_analyzer.hpp"

#include <opencv2/opencv.hpp>

using namespace sancak;

static std::vector<cv::Point> makeCircleContour(int cx, int cy, int r, int points)
{
    std::vector<cv::Point> contour;
    contour.reserve(static_cast<size_t>(points));
    for (int i = 0; i < points; ++i)
    {
        const double t = (2.0 * CV_PI * i) / static_cast<double>(points);
        const int x = static_cast<int>(std::round(cx + r * std::cos(t)));
        const int y = static_cast<int>(std::round(cy + r * std::sin(t)));
        contour.emplace_back(x, y);
    }
    return contour;
}

TEST_CASE("Circular contour is classified as balloon-ish", "[shape]")
{
    // Buyukce bir daire konturu uretelim ki MIN_CONTOUR_AREA engeline takilmasin.
    const auto contour = makeCircleContour(0, 0, 30, 120);

    const auto result = analyzeBalloonShape(contour);

    // Tam deterministik bir esik beklemiyoruz; ama daire benzeri bir kontur icin
    // circularity ve convexity yuksek olmali.
    REQUIRE(result.metrics.circularity > 0.7);
    REQUIRE(result.metrics.convexity > 0.8);
}

TEST_CASE("Too small contour is rejected", "[shape]")
{
    const auto contour = makeCircleContour(0, 0, 5, 40);
    const auto result = analyzeBalloonShape(contour);

    REQUIRE(result.isBalloon == false);
    REQUIRE(result.confidence == 0.0);
}
