/**
 * @file color.cpp
 * Реализация ColorPlugin — смена цвета агента по ROS2-сервису (триггер).
 */

#include <s2/plugins/color.hpp>

#include <nlohmann/json.hpp>

namespace s2
{
namespace plugins
{

void ColorPlugin::initialize(Agent& agent)
{
    if (agent.kinematic_tree)
    {
        is_urdf_ = true;
        for (const auto& link : agent.kinematic_tree->links())
        {
            if (!link.visual.type.empty())
            {
                default_link_colors_[link.name] = link.visual.color;
            }
        }
    }
    else
    {
        is_urdf_ = false;
        default_color_ = agent.visual.color;
    }
}

void ColorPlugin::from_config(const YAML::Node& node)
{
    if (node["service"])
        service_name_ = node["service"].as<std::string>();
    if (node["color"])
        configured_color_ = node["color"].as<std::string>();
    if (node["duration"])
        duration_ = node["duration"].as<double>();
}

std::string ColorPlugin::handle_service(const std::string& /*service_name*/,
                                        const std::string& /*request_json*/)
{
    timer_ = duration_;
    return R"({"success":true})";
}

void ColorPlugin::update(double dt, Agent& agent)
{
    if (timer_ > 0.0)
    {
        if (is_urdf_ && agent.kinematic_tree)
        {
            for (const auto& [name, orig] : default_link_colors_)
            {
                agent.kinematic_tree->set_link_color(name, configured_color_);
            }
        }
        else
        {
            agent.visual.color = configured_color_;
        }
        timer_ -= dt;
    }
    else
    {
        if (is_urdf_ && agent.kinematic_tree)
        {
            for (const auto& [name, orig_color] : default_link_colors_)
            {
                agent.kinematic_tree->set_link_color(name, orig_color);
            }
        }
        else
        {
            agent.visual.color = default_color_;
        }
    }
}

std::string ColorPlugin::to_json() const
{
    nlohmann::json j;
    j["plugin"]       = "color";
    j["active_color"] = configured_color_;
    j["remaining"]    = (timer_ > 0.0) ? timer_ : 0.0;
    return j.dump();
}

} // namespace plugins
} // namespace s2
