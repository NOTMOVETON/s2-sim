#pragma once

/**
 * @file path_display.hpp
 * Плагин отображения планируемого пути от nav stack.
 *
 * Подписывается на ROS2-топик nav_msgs/Path и передаёт последний полученный
 * путь во фронтенд через to_json() → plugins_data → SSE.
 * В stub-режиме (без ROS2) — не отображает ничего.
 *
 * Путь сбрасывается только при reset симуляции (re-init плагина) или
 * при отключении через кнопку в UI.
 *
 * Конфиг YAML:
 *   - type: "path_display"
 *     topic: "/plan"        # ROS2-топик nav_msgs/Path
 *     max_points: 500       # максимум точек пути
 *     color: "#00FF88"      # цвет линии
 */

#include <s2/plugins/plugin_base.hpp>
#include <array>
#include <mutex>
#include <string>
#include <vector>

namespace s2
{
namespace plugins
{

class PathDisplayPlugin : public IAgentPlugin
{
public:
    std::string type() const override { return "path_display"; }

    void from_config(const YAML::Node& node) override
    {
        if (node["topic"])
            topic_ = node["topic"].as<std::string>();
        if (node["max_points"])
            max_points_ = node["max_points"].as<int>();
        if (node["color"])
            color_ = node["color"].as<std::string>();
    }

    // Ничего не делает в update — данные приходят через handle_subscription
    void update(double /*dt*/, Agent& /*agent*/) override {}

    std::string to_json() const override
    {
        std::lock_guard<std::mutex> lock(mutex_);
        std::string json = "{\"type\":\"path\",\"points\":[";
        if (visible_)
        {
            for (std::size_t i = 0; i < points_.size(); ++i)
            {
                if (i > 0) json += ",";
                json += "[" + std::to_string(points_[i][0]) + ","
                           + std::to_string(points_[i][1]) + ","
                           + std::to_string(points_[i][2]) + "]";
            }
        }
        json += "],\"color\":\"" + color_ + "\",\"visible\":"
             + (visible_ ? "true" : "false") + "}";
        return json;
    }

    // ─── Подписка на топик пути ────────────────────────────────────────────

    std::vector<std::string> subscribe_topics() const override
    {
        return {topic_};
    }

    /**
     * @brief Обрабатывает входящий nav_msgs/Path в JSON.
     *
     * Ожидаемый формат JSON (конвертируется в ros2_transport_adapter.cpp):
     * {
     *   "poses": [
     *     {"position": {"x": 1.0, "y": 2.0, "z": 0.0}},
     *     ...
     *   ]
     * }
     */
    void handle_subscription(const std::string& /*topic*/,
                              const std::string& msg_json) override
    {
        try
        {
            YAML::Node data = YAML::Load(msg_json);
            std::vector<std::array<double, 3>> new_points;

            if (data["poses"])
            {
                for (const auto& pose_stamped : data["poses"])
                {
                    auto pos = pose_stamped["position"];
                    if (!pos) continue;
                    double x = pos["x"] ? pos["x"].as<double>() : 0.0;
                    double y = pos["y"] ? pos["y"].as<double>() : 0.0;
                    double z = pos["z"] ? pos["z"].as<double>() : 0.0;
                    new_points.push_back({x, y, z});
                    if (static_cast<int>(new_points.size()) >= max_points_) break;
                }
            }

            std::lock_guard<std::mutex> lock(mutex_);
            points_ = std::move(new_points);
        }
        catch (const std::exception&) {}
    }

    // ─── Управление через UI ───────────────────────────────────────────────

    bool has_inputs() const override { return true; }

    std::string inputs_schema() const override
    {
        return R"({"visible": {"type": "boolean", "default": true, "label": "Show path"}})";
    }

    void handle_input(const std::string& json_input) override
    {
        try
        {
            YAML::Node data = YAML::Load(json_input);
            if (data["visible"])
                visible_ = data["visible"].as<bool>();
        }
        catch (const std::exception&) {}
    }

private:
    std::string topic_{"/plan"};
    int         max_points_{500};
    std::string color_{"#00FF88"};
    bool        visible_{true};

    mutable std::mutex mutex_;
    std::vector<std::array<double, 3>> points_;
};

} // namespace plugins
} // namespace s2
