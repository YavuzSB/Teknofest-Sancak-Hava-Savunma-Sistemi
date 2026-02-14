/**
 * @file camera_controller.hpp
 * @brief Sancak Hava Savunma Sistemi - Kamera Yönetimi
 *
 * USB/CSI kamera yakalama, çözünürlük ayarı, frame atma.
 *
 * @author Sancak Takımı
 * @date 2026
 */
#pragma once

#include "sancak/types.hpp"
#include "sancak/config_manager.hpp"

#include <opencv2/videoio.hpp>
#include <string>

namespace sancak {

/**
 * @class CameraController
 * @brief Kamera açma, frame okuma, kaynak yönetimi
 */
class CameraController {
public:
    CameraController() = default;
    ~CameraController();

    // Kopyalama yasak
    CameraController(const CameraController&) = delete;
    CameraController& operator=(const CameraController&) = delete;

    /**
     * @brief Kamerayı aç
     * @param config Kamera ayarları
     * @return Başarılı mı?
     */
    bool open(const CameraConfig& config);

    /**
     * @brief Video dosyasından aç (test için)
     * @param video_path Dosya yolu
     * @return Başarılı mı?
     */
    bool openFile(const std::string& video_path);

    /**
     * @brief Bir frame oku
     * @param frame Çıkış frame'i
     * @return Frame okundu mu?
     */
    bool read(cv::Mat& frame);

    /**
     * @brief Kamerayı kapat
     */
    void release();

    /// Kamera açık mı?
    bool isOpened() const;

    /// Kamera FPS'i
    double fps() const;

private:
    cv::VideoCapture cap_;
    CameraConfig config_;
    bool opened_ = false;
};

} // namespace sancak
