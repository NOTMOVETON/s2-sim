#pragma once

/**
 * @file door_opener_plugin.hpp
 * DoorOpenerPlugin — interaction-плагин агента для открытия дверей.
 *
 * Ищет ближайший актор-дверь в interaction_distance_ и отправляет
 * cmd::Interact{action: "open"} через KernelCommandQueue.
 *
 * Роль: INTERACTION — не двигает агента, а взаимодействует с миром.
 *
 * YAML конфигурация:
 *   - type: door_opener
 *     interaction_distance: 2.0   # метры (по умолчанию 2.0)
 */

#include <s2/plugins/plugin_base.hpp>

namespace s2
{
namespace plugins
{

class DoorOpenerPlugin : public IAgentPlugin
{
public:
    std::string type() const override { return "door_opener"; }
    PluginRole  role() const override { return PluginRole::INTERACTION; }
    std::string display_label() const override { return "Door Opener"; }

    void update(double dt, Agent& agent, const PluginContext& ctx) override;
    void from_config(const YAML::Node& node) override;
    std::string to_json() const override;

    nlohmann::json config_schema() const override;

private:
    double interaction_distance_{2.0};  ///< Дальность поиска двери (метры)
};

} // namespace plugins
} // namespace s2
