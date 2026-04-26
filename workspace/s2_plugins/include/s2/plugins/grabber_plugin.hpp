#pragma once

/**
 * @file grabber_plugin.hpp
 * GrabberPlugin -- interaction-плагин агента для захвата пропов.
 *
 * Grab:    ищет ближайший movable prop в interaction_distance через WorldQuery,
 *          шлёт AttachObject{parent=agent, link=grab_link, child=prop}.
 * Release: шлёт DetachObject{child=held_prop_id}.
 * Contribution: manipulation_locked=true когда prop захвачен.
 *
 * Events:
 *  - GrabAttempt   -- перед попыткой захвата
 *  - GrabSucceeded -- захват удался
 *  - GrabFailed    -- захват не удался (нет пропа в радиусе)
 */

#include <s2/plugin_base.hpp>

namespace s2::plugins
{

class GrabberPlugin : public IAgentPlugin
{
public:
    std::string type() const override { return "grabber"; }
    PluginRole  role() const override { return PluginRole::INTERACTION; }

    void update(double dt, Agent& agent, const PluginContext& ctx) override;
    void from_config(const YAML::Node& node) override;
    void on_reset(Agent& agent) override;
    std::string to_json() const override;
    bool has_inputs() const override { return true; }
    std::string inputs_schema() const override;
    void handle_input(const std::string& json_input) override;

private:
    double      interaction_distance_{1.5};  ///< метры
    std::string grab_link_;                  ///< link агента для привязки ("gripper", "")

    ObjectId    held_prop_id_{0};  ///< 0 = ничего не держим
    bool        grab_requested_{false};
    bool        release_requested_{false};
};

} // namespace s2::plugins
