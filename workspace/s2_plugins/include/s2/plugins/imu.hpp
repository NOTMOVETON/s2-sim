#pragma once

/**
 * @file imu.hpp
 * Плагин IMU (Inertial Measurement Unit) сенсора.
 */

#include <s2/agent.hpp>
#include <s2/sensor_data.hpp>
#include <yaml-cpp/yaml.h>

#include <cmath>

namespace s2
{
namespace plugins
{

/**
 * @brief Плагин IMU-сенсора.
 */
class ImuPlugin : public IAgentPlugin
{
public:
    ImuPlugin() = default;

    std::string type() const override { return "imu"; }
    double publish_rate_hz() const override { return publish_rate_hz_; }

    void update(double dt, Agent& agent) override
    {
        publish_timer_ += dt;
        double interval = (publish_rate_hz_ > 0.0) ? (1.0 / publish_rate_hz_) : 0.0;
        if (interval > 0.0 && publish_timer_ < interval - 1e-9)
        {
            return;
        }
        publish_timer_ -= interval;

        ImuData data;
        data.seq    = ++seq_;
        data.gyro_x = agent.world_velocity.angular.x();
        data.gyro_y = agent.world_velocity.angular.y();
        data.gyro_z = agent.world_velocity.angular.z();
        data.accel_z = 9.81;
        data.yaw = agent.world_pose.yaw;

        current_gyro_z_ = data.gyro_z;
        current_yaw_    = data.yaw;

        agent.state.emplace<ImuData>(data);
    }

    void from_config(const YAML::Node& node) override
    {
        if (node["publish_rate_hz"]) publish_rate_hz_ = node["publish_rate_hz"].as<double>();
    }

    std::string to_json() const override
    {
        return "{\"plugin\":\"imu\","
               "\"gyro_z\":" + std::to_string(current_gyro_z_) + ","
               "\"yaw\":" + std::to_string(current_yaw_) + "}";
    }

private:
    double publish_rate_hz_{100.0};  ///< По умолчанию 100 Гц (совпадает с sim rate)
    double publish_timer_{0.0};
    uint64_t seq_{0};
    double current_gyro_z_{0.0};
    double current_yaw_{0.0};
};

} // namespace plugins
} // namespace s2