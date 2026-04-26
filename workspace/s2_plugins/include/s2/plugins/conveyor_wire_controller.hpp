#pragma once

/**
 * @file conveyor_wire_controller.hpp
 * ConveyorWireController -- плагин актора/агента для wire-управления конвейером.
 *
 * Слушает wire-сигналы через SignalListenerBase (EventBus) и доставляет
 * их в behavior.on_interact() актора через cmd::Interact.
 *
 * Конфигурация YAML:
 *   - type: conveyor_wire_controller
 *     reactions:
 *       - signal_id: emergency_stop
 *         on_active: stop
 *         on_inactive: start
 *
 * Реакции: stop, reverse, start
 * Роль: INTERACTION -- не двигает сущность, а взаимодействует через EventBus/KernelCommand.
 */

#include <s2/signal_listener_base.hpp>

#include <string>
#include <vector>

namespace s2
{
namespace plugins
{

class ConveyorWireController : public SignalListenerBase
{
public:
    std::string type() const override { return "conveyor_wire_controller"; }
    PluginRole  role() const override { return PluginRole::INTERACTION; }

    void update(double dt, Agent& agent, const PluginContext& ctx) override;
    void from_config(const YAML::Node& node) override;
    std::string to_json() const override;

private:
    /**
     * @brief Декларативная реакция на wire-сигнал.
     */
    struct Reaction
    {
        std::string signal_id;         ///< Идентификатор сигнала
        EntityId    source_entity{0};  ///< 0 = любой источник
        std::string on_active;         ///< Действие при активации: "stop", "reverse", "start"
        std::string on_inactive;       ///< Действие при деактивации ("" = нет действия)
    };

    std::vector<Reaction> reactions_;
};

} // namespace plugins
} // namespace s2
