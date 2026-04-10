#pragma once

/**
 * @file gnss.hpp
 * Плагин GNSS (GPS) сенсора с GeographicLib.
 */

#include <s2/agent.hpp>
#include <s2/geo_origin.hpp>
#include <s2/sensor_data.hpp>
#include <GeographicLib/LocalCartesian.hpp>

#include <cmath>
#include <random>

namespace s2
{
namespace plugins
{

/**
 * @brief Плагин GNSS-сенсора.
 */
class GnssPlugin : public IAgentPlugin
{
public:
    GnssPlugin() = default;

    std::string type() const override { return "gnss"; }
    double default_publish_rate_hz() const override { return publish_rate_hz_; }

    void set_geo_origin(const GeoOrigin& origin)
    {
        geo_origin_ = origin;
        converter_.Reset(origin.lat, origin.lon, origin.alt);
    }

    void update(double dt, Agent& agent) override
    {
        // Управляем собственным таймером публикации
        publish_timer_ += dt;
        double interval = (publish_rate_hz_ > 0.0) ? (1.0 / publish_rate_hz_) : 0.0;
        // Используем небольшой эпсилон для устойчивости к ошибкам накопления float
        if (interval > 0.0 && publish_timer_ < interval - 1e-9)
        {
            return;
        }
        // -= interval вместо = 0.0 исключает накопление дрейфа таймера
        publish_timer_ -= interval;

        double x = agent.world_pose.x;
        double y = agent.world_pose.y;
        double z = agent.world_pose.z;

        double lat, lon, alt;
        converter_.Reverse(y, x, z, lat, lon, alt);

        std::normal_distribution<double> lat_noise(0.0, noise_std_ / 111320.0);
        std::normal_distribution<double> lon_noise(0.0, noise_std_ / (111320.0 * std::cos(geo_origin_.lat * M_PI / 180.0)));
        std::normal_distribution<double> alt_noise(0.0, noise_std_);

        current_lat_ = lat + lat_noise(rng_);
        current_lon_ = lon + lon_noise(rng_);
        current_alt_ = alt + alt_noise(rng_);

        double yaw = agent.world_pose.yaw;
        current_azimuth_ = std::fmod(yaw, 2.0 * M_PI);
        if (current_azimuth_ < 0.0) current_azimuth_ += 2.0 * M_PI;

        // Публикуем в SharedState: инкрементируем seq, чтобы мост знал о новых данных
        GnssData data;
        data.seq      = ++seq_;
        data.lat      = current_lat_;
        data.lon      = current_lon_;
        data.alt      = current_alt_;
        data.azimuth  = current_azimuth_;
        data.accuracy = noise_std_;
        agent.state.emplace<GnssData>(data);
    }

    void from_config(const YAML::Node& node) override
    {
        if (node["noise_std"])       noise_std_        = node["noise_std"].as<double>();
        if (node["publish_rate_hz"]) publish_rate_hz_  = node["publish_rate_hz"].as<double>();
    }

    std::string to_json() const override
    {
        return "{\"plugin\":\"gnss\","
               "\"lat\":" + std::to_string(current_lat_) + ","
               "\"lon\":" + std::to_string(current_lon_) + ","
               "\"alt\":" + std::to_string(current_alt_) + ","
               "\"azimuth\":" + std::to_string(current_azimuth_) + ","
               "\"accuracy\":" + std::to_string(noise_std_) + "}";
    }

private:
    GeoOrigin geo_origin_;
    GeographicLib::LocalCartesian converter_;
    double noise_std_{0.5};
    double publish_rate_hz_{10.0};   ///< По умолчанию 10 Гц (типично для GNSS)
    double publish_timer_{0.0};
    uint64_t seq_{0};
    double current_lat_{0.0};
    double current_lon_{0.0};
    double current_alt_{0.0};
    double current_azimuth_{0.0};
    std::mt19937 rng_{42};
};

} // namespace plugins
} // namespace s2