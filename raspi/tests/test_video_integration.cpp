#include <catch2/catch_test_macros.hpp>
#include "balloon_detector.hpp"

#include <opencv2/opencv.hpp>

#include <cstdlib>
#include <string>

using namespace sancak;

static std::string getVideoPath()
{
#ifdef SANCAK_TEST_VIDEO_PATH
    return std::string(SANCAK_TEST_VIDEO_PATH);
#else
    const char* env = std::getenv("SANCAK_TEST_VIDEO");
    return env ? std::string(env) : std::string();
#endif
}

TEST_CASE("Video integration: BalloonDetector processes frames without crashing", "[integration][video]")
{
    const std::string videoPath = getVideoPath();
    if (videoPath.empty())
    {
        SKIP("No video provided. Set SANCAK_TEST_VIDEO env var or -DSANCAK_TEST_VIDEO_PATH=... at configure time.");
    }

    cv::VideoCapture cap(videoPath);
    if (!cap.isOpened())
    {
        SKIP("Video could not be opened: " + videoPath);
    }

    BalloonDetector detector(videoPath, /*headless=*/true);

    cv::Mat frame;
    int processed = 0;

    // Keep it short and deterministic for CI/dev machines.
    while (processed < 30 && cap.read(frame))
    {
        if (frame.empty())
        {
            continue;
        }

        const auto results = detector.processFrame(frame);
        REQUIRE(results.enemies.size() < 10000);
        REQUIRE(results.friends.size() < 10000);
        processed++;
    }

    REQUIRE(processed > 0);
}
