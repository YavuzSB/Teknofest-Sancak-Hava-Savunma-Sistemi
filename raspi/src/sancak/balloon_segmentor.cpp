/**
 * @file balloon_segmentor.cpp
 * @brief Turuncu Balon Segmentasyonu - Implementasyon
 *
 * YOLO bounding box içinde HSV filtre + kontur analizi ile
 * turuncu balonun merkezini bulur.
 */
#include "sancak/balloon_segmentor.hpp"
#include "sancak/logger.hpp"

#include <opencv2/imgproc.hpp>
#include <algorithm>

namespace sancak {

void BalloonSegmentor::initialize(const BalloonConfig& config) {
    config_ = config;
    hsv_lower_ = cv::Scalar(config_.h_min, config_.s_min, config_.v_min);
    hsv_upper_ = cv::Scalar(config_.h_max, config_.s_max, config_.v_max);
    morph_kernel_ = cv::getStructuringElement(
        cv::MORPH_ELLIPSE,
        cv::Size(config_.morph_kernel_size, config_.morph_kernel_size)
    );
    SANCAK_LOG_INFO("Balloon Segmentor başlatıldı | HSV: [{},{},{}]-[{},{},{}]",
                    config_.h_min, config_.s_min, config_.v_min,
                    config_.h_max, config_.s_max, config_.v_max);
}

void BalloonSegmentor::updateHsvRange(int h_min, int h_max, int s_min, int s_max,
                                       int v_min, int v_max) {
    config_.h_min = h_min; config_.h_max = h_max;
    config_.s_min = s_min; config_.s_max = s_max;
    config_.v_min = v_min; config_.v_max = v_max;
    hsv_lower_ = cv::Scalar(h_min, s_min, v_min);
    hsv_upper_ = cv::Scalar(h_max, s_max, v_max);
}

BalloonResult BalloonSegmentor::segment(const cv::Mat& frame, const cv::Rect2f& bbox) {
    BalloonResult result;

    // Bbox'ı integer'a çevir ve frame sınırlarına kırp
    int x1 = std::max(0, static_cast<int>(bbox.x));
    int y1 = std::max(0, static_cast<int>(bbox.y));
    int x2 = std::min(frame.cols, static_cast<int>(bbox.x + bbox.width));
    int y2 = std::min(frame.rows, static_cast<int>(bbox.y + bbox.height));

    if (x2 <= x1 || y2 <= y1) return result;

    cv::Rect roi(x1, y1, x2 - x1, y2 - y1);
    cv::Mat crop = frame(roi);

    if (crop.empty()) return result;

    // HSV'ye dönüştür
    cv::Mat hsv;
    cv::cvtColor(crop, hsv, cv::COLOR_BGR2HSV);

    // Turuncu maske oluştur
    cv::Mat mask = createOrangeMask(hsv);

    // Morfoloji uygula
    mask = applyMorphology(mask);

    // Kontur bul
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(mask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    if (contours.empty()) return result;

    // En büyük konturu bul
    auto best_it = std::max_element(contours.begin(), contours.end(),
        [](const auto& a, const auto& b) {
            return cv::contourArea(a) < cv::contourArea(b);
        }
    );

    double area = cv::contourArea(*best_it);
    if (area < 50.0) return result;  // Çok küçük → gürültü

    // Minimum çevreleyen daire
    cv::Point2f center;
    float radius;
    cv::minEnclosingCircle(*best_it, center, radius);

    // Yarıçap kontrolü
    if (radius < config_.min_radius_px || radius > config_.max_radius_px) return result;

    // Dairesellik kontrolü
    double perimeter = cv::arcLength(*best_it, true);
    double circularity = (perimeter > 0.0)
        ? (4.0 * CV_PI * area) / (perimeter * perimeter)
        : 0.0;

    if (circularity < config_.min_circularity) return result;

    // Global koordinatlara dönüştür
    result.found  = true;
    result.center = cv::Point2f(static_cast<float>(x1) + center.x, static_cast<float>(y1) + center.y);
    result.radius = radius;

    // Güven skoru: dairesellik + alan oranı
    double area_ratio = area / (CV_PI * radius * radius);
    result.confidence = static_cast<float>(
        circularity * 0.5 + std::min(area_ratio, 1.0) * 0.5
    );

    return result;
}

cv::Mat BalloonSegmentor::createOrangeMask(const cv::Mat& hsv) const {
    cv::Mat mask;
    cv::inRange(hsv, hsv_lower_, hsv_upper_, mask);
    return mask;
}

cv::Mat BalloonSegmentor::applyMorphology(const cv::Mat& mask) const {
    cv::Mat result;
    // Opening: küçük gürültüleri temizle
    cv::morphologyEx(mask, result, cv::MORPH_OPEN, morph_kernel_,
                      cv::Point(-1, -1), 1);
    // Closing: boşlukları kapat
    cv::morphologyEx(result, result, cv::MORPH_CLOSE, morph_kernel_,
                      cv::Point(-1, -1), config_.morph_iterations);
    return result;
}

} // namespace sancak
