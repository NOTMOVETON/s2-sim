/**
 * @file door_opener_plugin.cpp
 * DoorOpenerPlugin — реализация interaction-плагина для открытия дверей.
 *
 * Каждый тик ищет ближайший актор в радиусе interaction_distance_.
 * Если найден — отправляет cmd::Interact{action: "open"} в command queue.
 *
 * Ядро маршрутизирует Interact к actor.behavior.on_interact() (D-09).
 * DoorBehavior реагирует на action="open" — fire("open") в FSM.
 *
 * Идемпотентность: повторный Interact при уже OPENING/OPEN не ломает FSM,
 * fire("open") просто не находит подходящего перехода (T-02-10).
 */

#include <s2/plugins/door_opener_plugin.hpp>
#include <s2/agent.hpp>
#include <s2/kernel_command.hpp>
#include <s2/world_query.hpp>

#include <nlohmann/json.hpp>

namespace s2
{
namespace plugins
{

void DoorOpenerPlugin::from_config(const YAML::Node& node)
{
    interaction_distance_ = node["interaction_distance"].as<double>(2.0);
}

void DoorOpenerPlugin::update(double /*dt*/, Agent& agent, const PluginContext& ctx)
{
    // Найти акторов в радиусе interaction_distance_
    auto nearby = ctx.world.find_in_radius(
        agent.world_pose.position(),
        interaction_distance_,
        EntityFilter::actors_only()
    );

    if (nearby.empty()) return;

    // Взять ближайшего (первого) — WorldQueryImpl вернёт отсортированный результат (Phase 5)
    EntityId target_id = nearby.front();

    // Отправить команду открытия двери
    ctx.commands.push_back(cmd::Interact{
        .source_id    = agent.id,
        .target_id    = target_id,
        .action       = "open",
        .params       = nlohmann::json::object(),
        .max_distance = interaction_distance_
    });
}

std::string DoorOpenerPlugin::to_json() const
{
    nlohmann::json j;
    j["interaction_distance"] = interaction_distance_;
    return j.dump();
}

nlohmann::json DoorOpenerPlugin::config_schema() const
{
    return nlohmann::json::array({
        {{"key", "interaction_distance"}, {"label", "Interaction Distance (m)"},
         {"type", "number"}, {"default", 2.0}, {"min", 0.1}, {"max", 20.0}}
    });
}

} // namespace plugins
} // namespace s2
