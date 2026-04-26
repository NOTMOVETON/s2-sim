#pragma once

/**
 * @file door_wire_controller.hpp
 * DoorWireController -- плагин актора/агента для wire-управления дверью.
 *
 * Слушает wire-сигналы через SignalListenerBase (EventBus) и доставляет
 * их в behavior.on_interact() актора через cmd::Interact.
 *
 * Конфигурация YAML:
 *   - type: door_wire_controller
 *     reactions:
 *       - signal_id: factory_power
 *         source_entity: 0         # 0 = любой источник
 *         on_active: force_open    # действие при активации
 *         on_inactive: close       # действие при деактивации (пустое = нет действия)
 *
 * Реакции: close_and_lock, force_open, unlock, open, close
 * Роль: INTERACTION -- не двигает сущность, а взаимодействует через EventBus/KernelCommand.
 */

#include <s2/signal_listener_base.hpp>

#include <string>
#include <vector>

namespace s2
{
namespace plugins
{

class DoorWireController : public SignalListenerBase
{
public:
    std::string type() const override { return "door_wire_controller"; }
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
        std::string on_active;         ///< Действие при активации: "close_and_lock", "force_open", "unlock", "open", "close"
        std::string on_inactive;       ///< Действие при деактивации ("" = нет действия)
    };

    std::vector<Reaction> reactions_;
};

} // namespace plugins
} // namespace s2
