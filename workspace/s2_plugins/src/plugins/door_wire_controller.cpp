/**
 * @file door_wire_controller.cpp
 * DoorWireController -- реализация wire-контроллера двери.
 *
 * В update():
 *   1. subscribe_once(ctx.bus) -- подписка при первом вызове
 *   2. Итерация pending_signals() и реакций
 *   3. Для совпавших сигналов -- cmd::Interact на актора (agent.id)
 *   4. flush_signals() -- очистка обработанных сигналов
 *
 * cmd::Interact маршрутизируется ядром к actor.behavior.on_interact() (D-10).
 * DoorBehavior реагирует на action: close_and_lock / force_open / unlock / open / close.
 */

#include <s2/plugins/door_wire_controller.hpp>
#include <s2/agent.hpp>
#include <s2/kernel_command.hpp>

#include <nlohmann/json.hpp>

namespace s2
{
namespace plugins
{

void DoorWireController::from_config(const YAML::Node& node)
{
    if (!node["reactions"]) return;

    for (const auto& r : node["reactions"]) {
        Reaction reaction;
        reaction.signal_id     = r["signal_id"].as<std::string>("");
        reaction.source_entity = r["source_entity"].as<EntityId>(0);
        reaction.on_active     = r["on_active"].as<std::string>("");
        reaction.on_inactive   = r["on_inactive"].as<std::string>("");
        reactions_.push_back(std::move(reaction));
    }
}

void DoorWireController::update(double /*dt*/, Agent& agent, const PluginContext& ctx)
{
    subscribe_once(ctx.bus);

    // Итерировать pending_signals и проверить каждую реакцию
    for (const auto& reaction : reactions_) {
        for (const auto& sig : pending_signals()) {
            // Фильтр по signal_id
            if (sig.signal_id != reaction.signal_id) continue;
            // Фильтр по source_entity (0 = любой источник)
            if (reaction.source_entity != 0 &&
                sig.source_entity != reaction.source_entity) continue;

            // Выбрать действие в зависимости от active/inactive
            const std::string& action = sig.active
                ? reaction.on_active
                : reaction.on_inactive;
            if (action.empty()) continue;

            // Отправить cmd::Interact на самого себя (актор -> behavior.on_interact)
            ctx.commands.push_back(cmd::Interact{
                .source_id    = agent.id,
                .target_id    = agent.id,
                .action       = action,
                .params       = nlohmann::json::object(),
                .max_distance = 0.0
            });
        }
    }

    flush_signals();
}

std::string DoorWireController::to_json() const
{
    nlohmann::json j;
    j["type"]      = "door_wire_controller";
    j["reactions"]  = static_cast<int>(reactions_.size());
    return j.dump();
}

} // namespace plugins
} // namespace s2
