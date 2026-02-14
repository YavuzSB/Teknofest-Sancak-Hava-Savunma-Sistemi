#include "balloon_detector.hpp"
#include <opencv2/opencv.hpp>
#include <stdexcept>

namespace sancak {

static void deleteCapture(void* p)
{
    delete static_cast<cv::VideoCapture*>(p);
}

void BalloonDetector::initializeCamera()
{
    cap_.reset();

    auto* cap = new cv::VideoCapture();
    cap_ = std::unique_ptr<void, void (*)(void*)>(cap, &deleteCapture);

    if (!videoPath_.empty())
    {
        cap->open(videoPath_);
    }
    else
    {
        cap->open(cameraIndex_);
        if (cap->isOpened())
        {
            cap->set(cv::CAP_PROP_FRAME_WIDTH, CAMERA_WIDTH);
            cap->set(cv::CAP_PROP_FRAME_HEIGHT, CAMERA_HEIGHT);
        }
    }

    if (!cap->isOpened())
    {
        throw std::runtime_error("Camera/video source could not be opened");
    }

    motionDetector_.reset();
    lastTime_ = std::chrono::steady_clock::now();
}

void BalloonDetector::run()
{
    initializeCamera();

    auto* cap = static_cast<cv::VideoCapture*>(cap_.get());
    if (!cap)
    {
        throw std::runtime_error("Internal error: capture not initialized");
    }

    if (!headless_)
    {
        cv::namedWindow("Sancak - pc_webcam_test", cv::WINDOW_NORMAL);
    }

    while (true)
    {
        cv::Mat frame;
        if (!cap->read(frame) || frame.empty())
        {
            break;
        }

        frameCount_++;

        cv::Mat gray;
        cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);
        const bool motion = motionDetector_.hasMotion(gray);
        isActiveMode_ = motion;

        const int skip = isActiveMode_ ? FRAME_SKIP_ACTIVE : FRAME_SKIP_IDLE;
        const bool shouldProcess = (skip <= 1) || (frameCount_ % skip == 0);

        if (shouldProcess)
        {
            lastDetections_ = processFrame(frame);
            processCount_++;
        }

        if (!headless_)
        {
            cv::Mat display = frame.clone();
            drawDetections(display, lastDetections_.enemies, true);
            drawDetections(display, lastDetections_.friends, false);

            const double fps = updateFps();
            cv::putText(display,
                        "FPS:" + std::to_string(static_cast<int>(fps)),
                        cv::Point(10, 25),
                        cv::FONT_HERSHEY_SIMPLEX,
                        0.7,
                        COLOR_WHITE_BGR,
                        2);

            cv::imshow("Sancak - pc_webcam_test", display);
            const int key = cv::waitKey(1);
            if (key == 'q' || key == 27)
            {
                break;
            }
        }
        else
        {
            updateFps();
        }
    }

    cleanup();
}

void BalloonDetector::cleanup()
{
    cap_.reset();
    if (!headless_)
    {
        cv::destroyAllWindows();
    }
}

} // namespace sancak
