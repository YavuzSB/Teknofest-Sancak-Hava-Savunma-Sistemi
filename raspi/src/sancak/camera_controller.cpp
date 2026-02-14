/**
 * @file camera_controller.cpp
 * @brief Kamera Yönetimi - Implementasyon
 */
#include "sancak/camera_controller.hpp"
#include "sancak/logger.hpp"

#include <opencv2/imgproc.hpp>

namespace sancak {

CameraController::~CameraController() {
    release();
}

bool CameraController::open(const CameraConfig& config) {
    config_ = config;

    cap_.open(config_.device_index, cv::CAP_V4L2);
    if (!cap_.isOpened()) {
        // V4L2 başarısızsa genel backend dene
        cap_.open(config_.device_index);
    }

    if (!cap_.isOpened()) {
        SANCAK_LOG_ERROR("Kamera açılamadı: index={}", config_.device_index);
        opened_ = false;
        return false;
    }

    cap_.set(cv::CAP_PROP_FRAME_WIDTH,  config_.frame_width);
    cap_.set(cv::CAP_PROP_FRAME_HEIGHT, config_.frame_height);
    cap_.set(cv::CAP_PROP_FPS,          config_.fps_target);

    // Buffer boyutunu minimize et (gecikme azaltma)
    cap_.set(cv::CAP_PROP_BUFFERSIZE, 1);

    opened_ = true;

    int actual_w = static_cast<int>(cap_.get(cv::CAP_PROP_FRAME_WIDTH));
    int actual_h = static_cast<int>(cap_.get(cv::CAP_PROP_FRAME_HEIGHT));
    double actual_fps = cap_.get(cv::CAP_PROP_FPS);

    SANCAK_LOG_INFO("Kamera açıldı: index={} | {}x{} @ {:.0f} FPS",
                    config_.device_index, actual_w, actual_h, actual_fps);
    return true;
}

bool CameraController::openFile(const std::string& video_path) {
    cap_.open(video_path);
    if (!cap_.isOpened()) {
        SANCAK_LOG_ERROR("Video dosyası açılamadı: {}", video_path);
        opened_ = false;
        return false;
    }

    opened_ = true;
    int w = static_cast<int>(cap_.get(cv::CAP_PROP_FRAME_WIDTH));
    int h = static_cast<int>(cap_.get(cv::CAP_PROP_FRAME_HEIGHT));
    SANCAK_LOG_INFO("Video dosyası açıldı: {} | {}x{}", video_path, w, h);
    return true;
}

bool CameraController::read(cv::Mat& frame) {
    if (!opened_ || !cap_.isOpened()) return false;

    if (!cap_.read(frame)) return false;
    if (frame.empty()) return false;

    // Çözünürlük ayarı
    if (frame.cols != config_.frame_width || frame.rows != config_.frame_height) {
        cv::resize(frame, frame, cv::Size(config_.frame_width, config_.frame_height));
    }

    // Flip ayarları
    if (config_.flip_horizontal && config_.flip_vertical) {
        cv::flip(frame, frame, -1);
    } else if (config_.flip_horizontal) {
        cv::flip(frame, frame, 1);
    } else if (config_.flip_vertical) {
        cv::flip(frame, frame, 0);
    }

    return true;
}

void CameraController::release() {
    if (opened_ && cap_.isOpened()) {
        cap_.release();
        SANCAK_LOG_INFO("Kamera kapatıldı");
    }
    opened_ = false;
}

bool CameraController::isOpened() const {
    return opened_ && cap_.isOpened();
}

double CameraController::fps() const {
    if (!cap_.isOpened()) return 0.0;
    return cap_.get(cv::CAP_PROP_FPS);
}

} // namespace sancak
