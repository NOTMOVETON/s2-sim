/**
 * @file event_reactor.cpp
 * EventReactor -- реализация декларативного плагина событий.
 *
 * Слушает wire-сигнал через SignalListenerBase (EventBus) и при совпадении
 * публикует указанное событие обратно в EventBus.
 *
 * В Phase 5 (API) EventReactor может быть расширен на кастомные типы событий.
 * Сейчас поддерживается только "signal_activated" (T-02-11).
 */

#include <s2/plugins/event_reactor.hpp>
#include <s2/agent.hpp>

namespace s2
{
namespace plugins
{

void EventReactor::from_config(const YAML::Node& node)
{
    if (node["listen"]) {
        listen_signal_id_ = node["listen"]["signal_id"].as<std::string>("");
        listen_source_    = node["listen"]["source_entity"].as<EntityId>(0);
    }

    if (node["on_active"]) {
        on_active_event_ = node["on_active"]["fire_event"].as<std::string>("");
        if (node["on_active"]["params"]) {
            // Парсим параметры как JSON (YAML -> string -> json)
            const auto& p = node["on_active"]["params"];
            on_active_params_ = nlohmann::json::object();
            for (auto it = p.begin(); it != p.end(); ++it) {
                const auto key = it->first.as<std::string>();
                // Попытка как число, потом как строка
                try {
                    on_active_params_[key] = it->second.as<int>();
                } catch (...) {
                    on_active_params_[key] = it->second.as<std::string>("");
                }
            }
        }
    }

    if (node["on_inactive"]) {
        on_inactive_event_ = node["on_inactive"]["fire_event"].as<std::string>("");
        if (node["on_inactive"]["params"]) {
            const auto& p = node["on_inactive"]["params"];
            on_inactive_params_ = nlohmann::json::object();
            for (auto it = p.begin(); it != p.end(); ++it) {
                const auto key = it->first.as<std::string>();
                try {
                    on_inactive_params_[key] = it->second.as<int>();
                } catch (...) {
                    on_inactive_params_[key] = it->second.as<std::string>("");
                }
            }
        }
    }
}

void EventReactor::update(double /*dt*/, Agent& /*agent*/, const PluginContext& ctx)
{
    subscribe_once(ctx.bus);

    for (const auto& sig : pending_signals()) {
        // Фильтр по listen_signal_id_
        if (sig.signal_id != listen_signal_id_) continue;
        // Фильтр по source (0 = любой)
        if (listen_source_ != 0 && sig.source_entity != listen_source_) continue;

        const std::string& event_type = sig.active
            ? on_active_event_
            : on_inactive_event_;
        const nlohmann::json& params = sig.active
            ? on_active_params_
            : on_inactive_params_;

        if (event_type.empty()) continue;

        // T-02-11: только "signal_activated" обрабатывается
        if (event_type == "signal_activated") {
            ctx.bus.publish(event::SignalActivated{
                .signal_id     = params.value("signal_id", event_type),
                .source_entity = static_cast<EntityId>(params.value("source_entity", 0))
            });
        }
        // Неизвестные event_type -- пропускаем (T-02-11)
        // В Phase 5 можно расширить на кастомные типы
    }

    flush_signals();
}

std::string EventReactor::to_json() const
{
    nlohmann::json j;
    j["type"]            = "event_reactor";
    j["listen"]          = listen_signal_id_;
    j["on_active_event"] = on_active_event_;
    return j.dump();
}

} // namespace plugins
} // namespace s2
