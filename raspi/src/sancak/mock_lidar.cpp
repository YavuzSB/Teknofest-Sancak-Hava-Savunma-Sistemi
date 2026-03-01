#include "sancak/mock_lidar.hpp"

#include <algorithm>
#include <chrono>

namespace sancak {

MockLidar::MockLidar(const LidarConfig& cfg)
    : cfg_(cfg), rng_(static_cast<std::uint32_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count())) {
}

void MockLidar::update() {
    float nextDistance = 0.0f;

    if (cfg_.mock_fixed_distance_m > 0.0f) {
        nextDistance = cfg_.mock_fixed_distance_m;
    } else {
        float lo = std::min(cfg_.mock_min_distance_m, cfg_.mock_max_distance_m);
        float hi = std::max(cfg_.mock_min_distance_m, cfg_.mock_max_distance_m);
        std::uniform_real_distribution<float> dist(lo, hi);
        nextDistance = dist(rng_);
    }

    LidarPointCloud cloud;
    cloud.points.reserve(1);
    cloud.points.emplace_back(0.0f, 0.0f, nextDistance);

    std::lock_guard<std::mutex> lock(mutex_);
    last_distance_m_ = nextDistance;
    last_cloud_ = std::move(cloud);
}

std::optional<float> MockLidar::getDistanceMeters() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return last_distance_m_;
}

LidarPointCloud MockLidar::getPointCloud() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return last_cloud_;
}

} // namespace sancak
