
#include "balloon_detector.hpp"

#include "color_filter.hpp"

#include <stdexcept>

namespace sancak {

BalloonDetector::BalloonDetector(int cameraIndex, bool headless)
	: cameraIndex_(cameraIndex)
	, headless_(headless)
	, videoPath_()
	, cap_(nullptr, &BalloonDetector::noopDeleter)
	, motionDetector_()
	, lastTime_(std::chrono::steady_clock::now())
{
}

BalloonDetector::BalloonDetector(const std::string& videoPath, bool headless)
	: cameraIndex_(CAMERA_INDEX)
	, headless_(headless)
	, videoPath_(videoPath)
	, cap_(nullptr, &BalloonDetector::noopDeleter)
	, motionDetector_()
	, lastTime_(std::chrono::steady_clock::now())
{
}

DetectionResults BalloonDetector::processFrame(const cv::Mat& frame)
{
	DetectionResults results;
	if (frame.empty())
		return results;

	const auto& cf = ColorFilter::instance();
	cv::Mat hsv = cf.prepareHsv(frame);

	cv::Mat redMask = cf.createRedMask(hsv);
	cv::Mat blueMask = cf.createBlueMask(hsv);
	cv::Mat yellowMask = cf.createYellowMask(hsv);
	cv::Mat combinedMask = cf.createCombinedMask(hsv);
	combinedMask = cf.applyMorphology(combinedMask);

	std::vector<std::vector<cv::Point>> contours;
	cv::findContours(combinedMask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

	for (const auto& contour : contours) {
		ShapeAnalysisResult shape = analyzeBalloonShape(contour);
		if (!shape.isBalloon)
			continue;

		cv::Rect bbox = cv::boundingRect(contour);
		bbox &= cv::Rect(0, 0, frame.cols, frame.rows);
		if (bbox.width <= 0 || bbox.height <= 0)
			continue;

		BalloonColor color = cf.identifyColor(redMask, blueMask, yellowMask, bbox);
		BalloonDetection det;
		det.bbox = bbox;
		det.confidence = shape.confidence;
		det.metrics = shape.metrics;
		det.color = color;

		if (color == BalloonColor::RED) {
			results.enemies.push_back(det);
		} else if (color == BalloonColor::BLUE || color == BalloonColor::YELLOW) {
			results.friends.push_back(det);
		}
	}

	return results;
}

void BalloonDetector::drawDetections(cv::Mat& frame,
								   const std::vector<BalloonDetection>& detections,
								   bool isEnemy) const
{
	const cv::Scalar boxColor = isEnemy ? COLOR_RED_BGR : COLOR_GREEN_BGR;
	for (const auto& d : detections) {
		cv::rectangle(frame, d.bbox, boxColor, 2);
		const std::string label = isEnemy ? "ENEMY" : "FRIEND";
		cv::putText(frame, label, cv::Point(d.bbox.x, std::max(0, d.bbox.y - 5)),
					cv::FONT_HERSHEY_SIMPLEX, 0.6, boxColor, 2);
	}
}

double BalloonDetector::updateFps()
{
	auto now = std::chrono::steady_clock::now();
	const double dt = std::chrono::duration_cast<std::chrono::duration<double>>(now - lastTime_).count();
	lastTime_ = now;
	if (dt > 0.0)
		fps_ = 1.0 / dt;
	return fps_;
}

} // namespace sancak

