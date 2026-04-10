#pragma once

/**
 * @file trajectory_recorder.hpp
 * Плагин записи и отображения траектории робота.
 *
 * Записывает историю поз агента с заданным интервалом.
 * Передаёт данные во фронтенд через to_json() → plugins_data → SSE.
 * Не требует ROS2.
 *
 * Конфиг YAML:
 *   - type: "trajectory_recorder"
 *     record_interval_s: 0.5   # записывать позу каждые 0.5 секунды
 *     max_points: 200           # максимум точек в буфере
 *     color: "#FFAA00"          # цвет линии в визуализаторе
 */

#include <s2/plugins/plugin_base.hpp>
#include <array>
#include <string>
#include <vector>

namespace s2
{
namespace plugins
{

class TrajectoryRecorderPlugin : public IAgentPlugin
{
public:
    std::string type() const override { return "trajectory_recorder"; }

    void from_config(const YAML::Node& node) override
    {
        if (node["record_interval_s"])
            record_interval_s_ = node["record_interval_s"].as<double>();
        if (node["max_points"])
            max_points_ = node["max_points"].as<int>();
        if (node["color"])
            color_ = node["color"].as<std::string>();
    }

    void update(double dt, Agent& agent) override
    {
        if (!enabled_) return;

        timer_ += dt;
        if (timer_ >= record_interval_s_)
        {
            timer_ = 0.0;
            points_.push_back({
                agent.world_pose.x,
                agent.world_pose.y,
                agent.world_pose.z
            });
            if (static_cast<int>(points_.size()) > max_points_)
                points_.erase(points_.begin());
        }
    }

    std::string to_json() const override
    {
        std::string json = "{\"type\":\"trajectory\",\"points\":[";
        for (std::size_t i = 0; i < points_.size(); ++i)
        {
            if (i > 0) json += ",";
            json += "[" + std::to_string(points_[i][0]) + ","
                       + std::to_string(points_[i][1]) + ","
                       + std::to_string(points_[i][2]) + "]";
        }
        json += "],\"color\":\"" + color_ + "\",\"enabled\":"
             + (enabled_ ? "true" : "false") + "}";
        return json;
    }

    // ─── Управление через UI ───────────────────────────────────────────────

    bool has_inputs() const override { return true; }

    std::string inputs_schema() const override
    {
        return R"({"enabled": {"type": "boolean", "default": true, "label": "Record"}})";
    }

    void handle_input(const std::string& json_input) override
    {
        try
        {
            YAML::Node data = YAML::Load(json_input);
            if (data["enabled"])
            {
                // Принимаем и boolean, и число (0/1)
                enabled_ = data["enabled"].as<bool>();
                if (!enabled_)
                    points_.clear();
            }
        }
        catch (const std::exception&) {}
    }

private:
    double record_interval_s_{0.5};
    int    max_points_{200};
    std::string color_{"#FFAA00"};
    double timer_{0.0};
    bool   enabled_{true};
    std::vector<std::array<double, 3>> points_;
};

} // namespace plugins
} // namespace s2
