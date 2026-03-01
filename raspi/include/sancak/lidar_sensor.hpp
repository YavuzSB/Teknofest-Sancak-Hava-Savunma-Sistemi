/**
 * @file lidar_sensor.hpp
 * @brief Lidar sensör arayüzü (modüler entegrasyon)
 */
#pragma once

#include <optional>
#include <vector>

#include <opencv2/core.hpp>

namespace sancak {

struct LidarPointCloud {
    std::vector<cv::Point3f> points;
};

class ILidarSensor {
public:
    virtual ~ILidarSensor() = default;

    virtual void update() = 0;

    [[nodiscard]] virtual std::optional<float> getDistanceMeters() const = 0;

    [[nodiscard]] virtual LidarPointCloud getPointCloud() const = 0;
};

} // namespace sancak
