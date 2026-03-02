#pragma once
#include "sancak/yolo_detector.hpp"
#include <opencv2/core.hpp>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <optional>

namespace sancak {

class InferenceWorker {
public:
    explicit InferenceWorker(YoloDetector* detector);
    ~InferenceWorker();

    // Yeni bir frame gönder (en güncel frame her zaman işlenir)
    void pushFrame(const cv::Mat& frame);

    // Son inference sonucu (varsa) thread-safe şekilde alınır
    std::optional<std::vector<Detection>> getLatestResult();

    // Worker thread'i başlat/durdur
    void start();
    void stop();

    // Çalışıyor mu?
    bool isRunning() const { return running_.load(); }

private:
    void workerLoop();

    YoloDetector* detector_;
    std::thread worker_;
    std::mutex mtx_;
    std::condition_variable cv_;
    std::atomic<bool> running_{false};
    bool stop_flag_ = false;

    // En güncel frame ve sonucu
    cv::Mat latest_frame_;
    bool new_frame_ = false;
    std::optional<std::vector<Detection>> latest_result_;
};

} // namespace sancak
