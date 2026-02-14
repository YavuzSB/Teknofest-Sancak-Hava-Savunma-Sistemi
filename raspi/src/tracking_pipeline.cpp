/**
 * @file tracking_pipeline.cpp
 * @brief Otonom Takip Hatt31 - Implementasyon
 */
#include "tracking_pipeline.hpp"
#include "color_filter.hpp"

namespace sancak {

using Clock = std::chrono::steady_clock;

TrackingPipeline::TrackingPipeline(HsvRangeProvider hsvRangeProvider,
                                   int roiExpansion,
                                   int trackReconfirmInterval)
    : hsvRangeProvider_(std::move(hsvRangeProvider))
    , roiExpansion_(roiExpansion)
    , trackReconfirmInterval_(trackReconfirmInterval)
    , state_(TrackingState::IDLE)
    , roi_(std::nullopt)
    , lastTarget_(std::nullopt)
    , roiTimestamp_()
    , frameCount_(0)
{
}

void TrackingPipeline::reset()
{
    state_ = TrackingState::IDLE;
    roi_ = std::nullopt;
    lastTarget_ = std::nullopt;
    roiTimestamp_ = Clock::time_point{};
    motionDetector_.reset();
}

std::optional<cv::Point> TrackingPipeline::searchTarget(const cv::Mat& frame,
                                                       const cv::Rect& region)
{
    int hFrame = frame.rows;
    int wFrame = frame.cols;

    // Bf6lgeyi frame s31n31rlar31na k31rp
    int x0 = std::max(0, region.x);
    int y0 = std::max(0, region.y);
    int x1 = std::min(wFrame, region.x + region.width);
    int y1 = std::min(hFrame, region.y + region.height);

    if (x1 <= x0 || y1 <= y0)
        return std::nullopt;

    cv::Mat roiImg = frame(cv::Rect(x0, y0, x1 - x0, y1 - y0));
    if (roiImg.empty())
        return std::nullopt;

    // HSV'ye df6nfcs5ftfcr
    auto& cf = ColorFilter::instance();
    cv::Mat hsv = cf.prepareHsv(roiImg);

    // HSV aral315f31n31 al
    auto [low, high] = hsvRangeProvider_();

    // Maske olu5ftur
    cv::Mat mask;
    cv::inRange(hsv, low, high, mask);

    // Morfoloji uygula
    mask = cf.applyMorphology(mask);

    // Kontur bul
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(mask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    if (contours.empty())
        return std::nullopt;

    // En bfcyfck konturu bul
    auto biggest = std::max_element(
        contours.begin(),
        contours.end(),
        [](const std::vector<cv::Point>& a, const std::vector<cv::Point>& b) {
            return cv::contourArea(a) < cv::contourArea(b);
        }
    );

    double area = cv::contourArea(*biggest);
    if (area < 150.0)
        return std::nullopt;

    // Merkez hesapla (global koordinatlara df6nfcs5ftfcr)
    cv::Rect bRect = cv::boundingRect(*biggest);
    int cx = x0 + bRect.x + bRect.width / 2;
    int cy = y0 + bRect.y + bRect.height / 2;

    return cv::Point(cx, cy);
}

TrackingResult TrackingPipeline::process(const cv::Mat& frame, bool autonomous)
{
    frameCount_++;
    std::optional<cv::Point> target = std::nullopt;
    cv::Mat displayFrame = frame.clone();

    // Manuel mod: s31f31rla ve e731k
    if (!autonomous) {
        reset();
        return { displayFrame, std::nullopt, TrackingState::IDLE };
    }

    // Gri tonlama (hareket alg31lama ie7in)
    cv::Mat gray;
    cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);

    // ========== DURUM MAK30NES30 ==========
    switch (state_) {
    case TrackingState::IDLE: {
        // Hareket bf6lgesi tespit et
        auto motionRoi = motionDetector_.detectMotionRegion(gray);
        if (motionRoi) {
            cv::Rect r = *motionRoi;
            // ROI'yi geni5flet
            roi_ = cv::Rect(
                r.x - roiExpansion_,
                r.y - roiExpansion_,
                r.width  + 2 * roiExpansion_,
                r.height + 2 * roiExpansion_
            );
            state_ = TrackingState::ROI;
            roiTimestamp_ = Clock::now();
        }
        break;
    }

    case TrackingState::ROI: {
        if (!roi_) {
            state_ = TrackingState::IDLE;
            break;
        }

        // ROI ie7inde hedef ara
        target = searchTarget(frame, *roi_);
        if (target) {
            lastTarget_ = target;
            state_ = TrackingState::TRACK;
        } else {
            // 2 saniye timeout
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                Clock::now() - roiTimestamp_
            ).count();

            if (elapsed > 2000) {
                state_ = TrackingState::IDLE;
                roi_ = std::nullopt;
            }
        }
        break;
    }

    case TrackingState::TRACK: {
        // Periyodik yeniden do1frulama
        if (lastTarget_ && (frameCount_ % trackReconfirmInterval_ == 0)) {
            int tx = lastTarget_->x;
            int ty = lastTarget_->y;
            cv::Rect localRoi(tx - 60, ty - 60, 120, 120);

            auto newTarget = searchTarget(frame, localRoi);
            if (newTarget) {
                lastTarget_ = newTarget;
            } else {
                // Kayboldu -> ROI'ye geri df6n
                state_ = TrackingState::ROI;
                roi_ = localRoi;
                roiTimestamp_ = Clock::now();
            }
        }

        target = lastTarget_;
        if (!target) {
            state_ = TrackingState::IDLE;
        }
        break;
    }
    } // switch

    // ========== Gf6RSEL d0ARETL30LEMELER ==========
    // ROI e7iz
    if (roi_) {
        cv::rectangle(displayFrame, *roi_, cv::Scalar(255, 255, 0), 1);
    }

    // Hedef e7iz
    if (target) {
        cv::circle(displayFrame, *target, 5, cv::Scalar(0, 0, 255), -1);
        std::string tText = "TARGET (" + std::to_string(target->x) + "," +
                           std::to_string(target->y) + ")";
        cv::putText(displayFrame, tText, cv::Point(target->x + 5, target->y - 5),
                   cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 0, 255), 1);
    }

    // Durum bilgisi
    std::string stateStr;
    switch (state_) {
        case TrackingState::IDLE:  stateStr = "IDLE";  break;
        case TrackingState::ROI:   stateStr = "ROI";   break;
        case TrackingState::TRACK: stateStr = "TRACK"; break;
    }
    cv::putText(displayFrame, "STATE:" + stateStr, cv::Point(10, 20),
               cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(0, 255, 0), 2);

    return { displayFrame, target, state_ };
}

} // namespace sancak
