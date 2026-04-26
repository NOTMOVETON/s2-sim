#pragma once

/**
 * @file event_reactor.hpp
 * EventReactor -- декларативный плагин для трансляции wire-сигналов в EventBus события.
 *
 * Слушает wire-сигнал (через SignalListenerBase) и публикует другое событие.
 * Предназначен для простых случаев, когда не нужен полноценный контроллер.
 *
 * Конфигурация YAML:
 *   - type: event_reactor
 *     listen:
 *       signal_id: factory_power
 *       source_entity: 0            # 0 = любой
 *     on_active:
 *       fire_event: signal_activated
 *       params: { signal_id: "power_on", source_entity: 5 }
 *     on_inactive:
 *       fire_event: signal_activated
 *       params: { signal_id: "power_off", source_entity: 5 }
 *
 * Threat mitigation (T-02-11): только "signal_activated" обрабатывается;
 * неизвестные event_type логируются и пропускаются.
 *
 * Роль: INTERACTION
 */

#include <s2/signal_listener_base.hpp>

#include <nlohmann/json.hpp>
#include <string>

namespace s2
{
namespace plugins
{

class EventReactor : public SignalListenerBase
{
public:
    std::string type() const override { return "event_reactor"; }
    PluginRole  role() const override { return PluginRole::INTERACTION; }

    void update(double dt, Agent& agent, const PluginContext& ctx) override;
    void from_config(const YAML::Node& node) override;
    std::string to_json() const override;

private:
    std::string    listen_signal_id_;     ///< Идентификатор отслеживаемого сигнала
    EntityId       listen_source_{0};     ///< 0 = любой источник

    // Действие при активации
    std::string    on_active_event_;      ///< Тип события: "signal_activated"
    nlohmann::json on_active_params_;     ///< Параметры события (signal_id, source_entity)

    // Действие при деактивации
    std::string    on_inactive_event_;
    nlohmann::json on_inactive_params_;
};

} // namespace plugins
} // namespace s2
