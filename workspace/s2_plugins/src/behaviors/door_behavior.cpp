#include <s2/behaviors/door_behavior.hpp>
#include <s2/actor.hpp>

#include <nlohmann/json.hpp>

namespace s2
{

// Стаб для TDD RED — все методы минимальные, тесты должны падать

void DoorBehavior::on_init(const YAML::Node& /*config*/)
{
    // TODO: прочитать конфигурацию, настроить FSM
}

void DoorBehavior::on_spawn(ActorId actor_id)
{
    actor_id_ = actor_id;
}

void DoorBehavior::on_reset()
{
    // TODO: сброс FSM
}

void DoorBehavior::update(double /*dt*/, Actor& /*actor*/, const WorldContext& /*ctx*/)
{
    // TODO: таймерные переходы, синхронизация actor.current_state
}

void DoorBehavior::on_signal(const SignalEvent& /*event*/)
{
    // TODO: wire-триггеры
}

void DoorBehavior::on_interact(EntityId /*source*/, const std::string& /*action*/,
                               const nlohmann::json& /*params*/)
{
    // TODO: fire("open") / fire("close")
}

std::string DoorBehavior::current_state() const
{
    return fsm_.current_state();
}

std::string DoorBehavior::to_json() const
{
    return "{}";
}

void DoorBehavior::setup_fsm()
{
    // TODO: настроить состояния и переходы
}

} // namespace s2
