#include "sancak/inference_worker.hpp"
#include <utility>

namespace sancak {

InferenceWorker::InferenceWorker(YoloDetector* detector)
    : detector_(detector) {}

InferenceWorker::~InferenceWorker() {
    stop();
}

void InferenceWorker::start() {
    if (running_.exchange(true)) return;
    stop_flag_ = false;
    worker_ = std::thread(&InferenceWorker::workerLoop, this);
}

void InferenceWorker::stop() {
    if (!running_.exchange(false)) return;
    {
        std::lock_guard<std::mutex> lock(mtx_);
        stop_flag_ = true;
        cv_.notify_all();
    }
    if (worker_.joinable()) worker_.join();
}

void InferenceWorker::pushFrame(const cv::Mat& frame) {
    std::lock_guard<std::mutex> lock(mtx_);
    frame.copyTo(latest_frame_);
    new_frame_ = true;
    cv_.notify_all();
}

std::optional<std::vector<Detection>> InferenceWorker::getLatestResult() {
    std::lock_guard<std::mutex> lock(mtx_);
    return latest_result_;
}

void InferenceWorker::workerLoop() {
    while (true) {
        cv::Mat frame;
        {
            std::unique_lock<std::mutex> lock(mtx_);
            cv_.wait(lock, [&] { return new_frame_ || stop_flag_; });
            if (stop_flag_) break;
            if (latest_frame_.empty()) continue;
            latest_frame_.copyTo(frame);
            new_frame_ = false;
        }
        if (!frame.empty() && detector_) {
            auto result = detector_->detect(frame);
            {
                std::lock_guard<std::mutex> lock(mtx_);
                latest_result_ = std::move(result);
            }
        }
    }
}

} // namespace sancak
