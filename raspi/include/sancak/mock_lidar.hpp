/**
 * @file mock_lidar.hpp
 * @brief Donanım yokken test amaçlı Mock Lidar
 */
#pragma once

#include "sancak/config_manager.hpp"
#include "sancak/lidar_sensor.hpp"

#include <mutex>
#include <optional>
#include <random>

namespace sancak {

class MockLidar final : public ILidarSensor {
public:
    explicit MockLidar(const LidarConfig& cfg);

    void update() override;

    [[nodiscard]] std::optional<float> getDistanceMeters() const override;

    [[nodiscard]] LidarPointCloud getPointCloud() const override;

private:
    LidarConfig cfg_;

    mutable std::mutex mutex_;
    std::optional<float> last_distance_m_;
    LidarPointCloud last_cloud_;

    std::mt19937 rng_;
};

} // namespace sancak
