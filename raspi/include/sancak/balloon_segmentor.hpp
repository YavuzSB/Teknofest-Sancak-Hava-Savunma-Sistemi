/**
 * @file balloon_segmentor.hpp
 * @brief Sancak Hava Savunma Sistemi - Turuncu Balon Segmentasyonu
 *
 * YOLO'nun tespit ettiği bounding box içinde turuncu balonun
 * merkezini HSV filtre + kontur analizi ile bulur.
 *
 * @author Sancak Takımı
 * @date 2026
 */
#pragma once

#include "sancak/types.hpp"
#include "sancak/config_manager.hpp"

#include <opencv2/opencv.hpp>

namespace sancak {

/**
 * @class BalloonSegmentor
 * @brief Bounding box içinde turuncu balon tespiti
 *
 * Algoritma:
 *  1. YOLO bounding box'ı kırp
 *  2. HSV'ye dönüştür
 *  3. Turuncu renk maskesi oluştur
 *  4. Morfoloji uygula (gürültü temizle)
 *  5. Kontur bul ve en büyüğünü seç
 *  6. minEnclosingCircle ile merkez ve yarıçap hesapla
 *  7. Dairesellik kontrolü
 *  8. Global koordinatlara dönüştür
 */
class BalloonSegmentor {
public:
    BalloonSegmentor() = default;

    /**
     * @brief Konfigürasyon ile başlat
     * @param config Balon ayarları
     */
    void initialize(const BalloonConfig& config);

    /**
     * @brief Verilen bounding box içinde balonu bul
     * @param frame Tam frame (BGR)
     * @param bbox  YOLO tespit bounding box'ı
     * @return BalloonResult (found, center, radius, confidence)
     */
    BalloonResult segment(const cv::Mat& frame, const cv::Rect2f& bbox);

    /**
     * @brief Verilen bounding box içinde balonu bul (HSV frame hazırsa)
     * @param hsvFrame Tam frame (HSV, CV_8UC3)
     * @param bbox     YOLO tespit bounding box'ı
     * @return BalloonResult (found, center, radius, confidence)
     */
    BalloonResult segmentHsv(const cv::Mat& hsvFrame, const cv::Rect2f& bbox);

    /**
     * @brief HSV aralığını güncelle (çalışma zamanında)
     */
    void updateHsvRange(int h_min, int h_max, int s_min, int s_max,
                        int v_min, int v_max);

private:
    /// Turuncu maske oluştur
    cv::Mat createOrangeMask(const cv::Mat& hsv) const;

    /// Morfoloji uygula
    cv::Mat applyMorphology(const cv::Mat& mask) const;

    BalloonConfig config_;
    cv::Scalar hsv_lower_;
    cv::Scalar hsv_upper_;
    cv::Mat morph_kernel_;
};

} // namespace sancak
