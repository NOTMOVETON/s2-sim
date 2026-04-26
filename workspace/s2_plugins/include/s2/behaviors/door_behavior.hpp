#pragma once

/**
 * @file door_behavior.hpp
 * DoorBehavior — FSM-поведение двери (CLOSED/OPENING/OPEN/CLOSING).
 *
 * Первый конкретный IActorBehavior в S2.
 * Демонстрирует паттерн: behaviors_registry, ActorFSM, императивное
 * управление collision, wire-реакции через on_signal().
 *
 * Триггеры открытия:
 *  - on_interact(source, "open", {}) — proximity (через DoorOpenerPlugin)
 *  - on_signal(SignalEvent{signal_id: "wire_open"}) — wire-триггер
 *
 * Триггеры закрытия:
 *  - on_interact(source, "close", {})
 *  - on_signal(SignalEvent{signal_id: "wire_close"})
 *  - auto_close_secs > 0 — таймер после OPEN
 *
 * FSM переходы:
 *  CLOSED -[open]-> OPENING -[_auto]-> OPEN -[close]-> CLOSING -[_auto]-> CLOSED
 *
 * Императивное управление:
 *  - actor.collision_enabled = false при OPEN
 *  - actor.collision_enabled = true  при CLOSED
 *  - actor.current_state синхронизируется каждый тик
 */

#include <s2/actor_behavior.hpp>
#include <s2/actor_fsm.hpp>

namespace s2
{

class DoorBehavior : public IActorBehavior
{
public:
    std::string type() const override { return "door"; }

    void on_init(const YAML::Node& config) override;
    void on_spawn(ActorId actor_id) override;
    void on_reset() override;

    void update(double dt, Actor& actor, const WorldContext& ctx) override;

    void on_signal(const SignalEvent& event) override;
    void on_interact(EntityId source, const std::string& action,
                     const nlohmann::json& params) override;

    std::string current_state() const override;
    std::string to_json() const override;

private:
    void setup_fsm();

    ActorFSM fsm_;
    ActorId  actor_id_{0};

    // Конфигурация из YAML
    double open_duration_{1.0};    ///< Секунды на открытие (OPENING)
    double close_duration_{1.0};   ///< Секунды на закрытие (CLOSING)
    double auto_close_secs_{0.0};  ///< 0 = не закрывается автоматически

    // Таймер текущего состояния
    double state_timer_{0.0};

    // Для on_enter коллбеков FSM (actor не доступен в коллбеке)
    Actor* current_actor_{nullptr};

    // Для публикации ActorStateChanged в on_interact/on_signal
    EventBus* bus_{nullptr};
};

} // namespace s2
