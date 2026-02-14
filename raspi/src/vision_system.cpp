/**
 * @file vision_system.cpp
 * @brief DetectionPipeline implementasyonu - Hibrit Durum Makinesi
 */
#include "vision_system.hpp"

using namespace cv;
using namespace std;
using Clock = chrono::steady_clock;

// ─────────────────────────────────────────────
// Constructor
// ─────────────────────────────────────────────
DetectionPipeline::DetectionPipeline(HsvRangeProvider hsvRangeProvider,
                                     int idleMotionThreshold,
                                     int minMotionArea,
                                     int roiExpansion,
                                     int trackReconfirmInterval)
    : hsvRangeProvider_(std::move(hsvRangeProvider))
    , idleMotionThreshold_(idleMotionThreshold)
    , minMotionArea_(minMotionArea)
    , roiExpansion_(roiExpansion)
    , trackReconfirmInterval_(trackReconfirmInterval)
    , state_(PipelineState::IDLE)
    , roi_(nullopt)
    , lastTarget_(nullopt)
    , roiTimestamp_()
    , frameCount_(0)
{
}

// ─────────────────────────────────────────────
// Reset
// ─────────────────────────────────────────────
void DetectionPipeline::reset()
{
    state_ = PipelineState::IDLE;
    roi_ = nullopt;
    lastTarget_ = nullopt;
    roiTimestamp_ = Clock::time_point{};
}

// ─────────────────────────────────────────────
// Hareket Algılama
// ─────────────────────────────────────────────
optional<Rect> DetectionPipeline::motionDetect(const Mat& gray)
{
    if (lastFrameGray_.empty()) {
        lastFrameGray_ = gray.clone();
        return nullopt;
    }

    Mat diff, blurred, thresh, opened;
    absdiff(lastFrameGray_, gray, diff);
    lastFrameGray_ = gray.clone();

    GaussianBlur(diff, blurred, Size(5, 5), 0);
    threshold(blurred, thresh, idleMotionThreshold_, 255, THRESH_BINARY);

    Mat kernel = getStructuringElement(MORPH_RECT, Size(3, 3));
    morphologyEx(thresh, opened, MORPH_OPEN, kernel);

    vector<vector<Point>> contours;
    findContours(opened, contours, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);

    if (contours.empty())
        return nullopt;

    // En büyük konturu bul
    auto biggest = max_element(contours.begin(), contours.end(),
                               [](const vector<Point>& a, const vector<Point>& b) {
                                   return contourArea(a) < contourArea(b);
                               });

    double area = contourArea(*biggest);
    if (area < minMotionArea_)
        return nullopt;

    return boundingRect(*biggest);
}

// ─────────────────────────────────────────────
// HSV Arama
// ─────────────────────────────────────────────
optional<Point> DetectionPipeline::hsvSearch(const Mat& frame, const Rect& region)
{
    int hFrame = frame.rows;
    int wFrame = frame.cols;

    int x0 = max(0, region.x);
    int y0 = max(0, region.y);
    int x1 = min(wFrame, region.x + region.width);
    int y1 = min(hFrame, region.y + region.height);

    if (x1 <= x0 || y1 <= y0)
        return nullopt;

    Mat roiImg = frame(Rect(x0, y0, x1 - x0, y1 - y0));

    if (roiImg.empty())
        return nullopt;

    Mat hsv;
    cvtColor(roiImg, hsv, COLOR_BGR2HSV);

    auto [low, high] = hsvRangeProvider_();
    Mat mask;
    inRange(hsv, low, high, mask);

    GaussianBlur(mask, mask, Size(5, 5), 0);
    Mat kernel = getStructuringElement(MORPH_RECT, Size(5, 5));
    morphologyEx(mask, mask, MORPH_OPEN, kernel);

    vector<vector<Point>> contours;
    findContours(mask, contours, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);

    if (contours.empty())
        return nullopt;

    auto biggest = max_element(contours.begin(), contours.end(),
                               [](const vector<Point>& a, const vector<Point>& b) {
                                   return contourArea(a) < contourArea(b);
                               });

    double area = contourArea(*biggest);
    if (area < 150.0)
        return nullopt;

    Rect bRect = boundingRect(*biggest);
    int cx = x0 + bRect.x + bRect.width / 2;
    int cy = y0 + bRect.y + bRect.height / 2;

    return Point(cx, cy);
}

// ─────────────────────────────────────────────
// Ana İşleme
// ─────────────────────────────────────────────
ProcessResult DetectionPipeline::process(const Mat& frame, bool autonomous)
{
    frameCount_++;
    optional<Point> target = nullopt;
    Mat displayFrame = frame.clone();
    Mat gray;
    cvtColor(frame, gray, COLOR_BGR2GRAY);

    // Manuel mod: sadece ham frame
    if (!autonomous) {
        reset();
        return { displayFrame, nullopt };
    }

    // =========== DURUM MAKİNESİ ===========
    switch (state_) {
    case PipelineState::IDLE: {
        auto motionRoi = motionDetect(gray);
        if (motionRoi) {
            Rect r = *motionRoi;
            roi_ = Rect(r.x - roiExpansion_,
                        r.y - roiExpansion_,
                        r.width + 2 * roiExpansion_,
                        r.height + 2 * roiExpansion_);
            state_ = PipelineState::ROI;
            roiTimestamp_ = Clock::now();
        }
        break;
    }

    case PipelineState::ROI: {
        if (!roi_) {
            state_ = PipelineState::IDLE;
            break;
        }
        target = hsvSearch(frame, *roi_);
        if (target) {
            lastTarget_ = target;
            state_ = PipelineState::TRACK;
        } else {
            // 2 saniyelik zaman aşımı
            auto elapsed = chrono::duration_cast<chrono::milliseconds>(
                Clock::now() - roiTimestamp_).count();
            if (elapsed > 2000) {
                state_ = PipelineState::IDLE;
                roi_ = nullopt;
            }
        }
        break;
    }

    case PipelineState::TRACK: {
        if (lastTarget_ && (frameCount_ % trackReconfirmInterval_ == 0)) {
            int tx = lastTarget_->x;
            int ty = lastTarget_->y;
            Rect localRoi(tx - 60, ty - 60, 120, 120);
            auto newTarget = hsvSearch(frame, localRoi);
            if (newTarget) {
                lastTarget_ = newTarget;
            } else {
                // Kayboldu -> ROI moduna geri dön
                state_ = PipelineState::ROI;
                roi_ = localRoi;
                roiTimestamp_ = Clock::now();
            }
        }
        target = lastTarget_;
        if (!target) {
            state_ = PipelineState::IDLE;
        }
        break;
    }
    } // switch

    // =========== GÖRSEL İŞARETLEMELER ===========
    if (roi_) {
        rectangle(displayFrame, *roi_, Scalar(255, 255, 0), 1);
    }
    if (target) {
        circle(displayFrame, *target, 5, Scalar(0, 0, 255), -1);
        string tText = "TARGET (" + to_string(target->x) + "," + to_string(target->y) + ")";
        putText(displayFrame, tText, Point(target->x + 5, target->y - 5),
                FONT_HERSHEY_SIMPLEX, 0.5, Scalar(0, 0, 255), 1);
    }

    // Durum bilgisi
    string stateStr;
    switch (state_) {
        case PipelineState::IDLE:  stateStr = "IDLE";  break;
        case PipelineState::ROI:   stateStr = "ROI";   break;
        case PipelineState::TRACK: stateStr = "TRACK"; break;
    }
    putText(displayFrame, "STATE:" + stateStr, Point(10, 20),
            FONT_HERSHEY_SIMPLEX, 0.6, Scalar(0, 255, 0), 2);

    return { displayFrame, target };
}
