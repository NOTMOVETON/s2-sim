/**
 * @file door_behavior.cpp
 * DoorBehavior — реализация FSM двери.
 *
 * FSM: CLOSED -[open]-> OPENING -[_auto]-> OPEN -[close]-> CLOSING -[_auto]-> CLOSED
 *
 * Таймерные переходы:
 *  - OPENING -> OPEN после open_duration_ секунд
 *  - CLOSING -> CLOSED после close_duration_ секунд
 *  - OPEN -> CLOSING после auto_close_secs_ (если > 0)
 *
 * Императивное управление:
 *  - actor.collision_enabled = false при входе в OPEN
 *  - actor.collision_enabled = true  при входе в CLOSED
 *  - actor.current_state синхронизируется каждый тик
 *
 * Публикация ActorStateChanged через EventBus при каждом FSM-переходе.
 */

#include <s2/behaviors/door_behavior.hpp>
#include <s2/actor.hpp>

#include <nlohmann/json.hpp>

namespace s2
{

// ============================================================================
// Lifecycle
// ============================================================================

void DoorBehavior::on_init(const YAML::Node& config)
{
    // Прочитать конфигурацию с дефолтами
    open_duration_   = config["open_duration"].as<double>(1.0);
    close_duration_  = config["close_duration"].as<double>(1.0);
    auto_close_secs_ = config["auto_close_secs"].as<double>(0.0);

    setup_fsm();
}

void DoorBehavior::on_spawn(ActorId actor_id)
{
    actor_id_ = actor_id;
}

void DoorBehavior::on_reset()
{
    // Сброс таймера
    state_timer_ = 0.0;

    // Принудительный возврат в CLOSED через последовательные fire
    // (FSM может быть в любом состоянии)
    const auto& state = fsm_.current_state();
    if (state == "OPENING") {
        fsm_.fire("_auto");    // OPENING -> OPEN
        fsm_.fire("close");    // OPEN -> CLOSING
        fsm_.fire("_auto");    // CLOSING -> CLOSED
    } else if (state == "OPEN") {
        fsm_.fire("close");    // OPEN -> CLOSING
        fsm_.fire("_auto");    // CLOSING -> CLOSED
    } else if (state == "CLOSING") {
        fsm_.fire("_auto");    // CLOSING -> CLOSED
    }
    // Если CLOSED — ничего не делать

    // Сбросить указатели
    current_actor_ = nullptr;
    bus_ = nullptr;
}

// ============================================================================
// Настройка FSM
// ============================================================================

void DoorBehavior::setup_fsm()
{
    // Состояния: CLOSED, OPENING, OPEN, CLOSING
    // Первое добавленное — начальное (CLOSED)

    fsm_.add_state("CLOSED",
        // on_enter: включить коллизию
        [this]() {
            if (current_actor_) {
                current_actor_->collision_enabled = true;
            }
        },
        nullptr, // on_update
        nullptr  // on_exit
    );

    fsm_.add_state("OPENING",
        nullptr, // on_enter
        nullptr, // on_update
        nullptr  // on_exit
    );

    fsm_.add_state("OPEN",
        // on_enter: выключить коллизию
        [this]() {
            if (current_actor_) {
                current_actor_->collision_enabled = false;
            }
        },
        nullptr, // on_update
        nullptr  // on_exit
    );

    fsm_.add_state("CLOSING",
        nullptr, // on_enter
        nullptr, // on_update
        nullptr  // on_exit
    );

    // Переходы
    fsm_.add_transition("CLOSED",  "OPENING", "open");
    fsm_.add_transition("OPENING", "OPEN",    "_auto");
    fsm_.add_transition("OPEN",    "CLOSING", "close");
    fsm_.add_transition("CLOSING", "CLOSED",  "_auto");
}

// ============================================================================
// Основной тик
// ============================================================================

void DoorBehavior::update(double dt, Actor& actor, const WorldContext& ctx)
{
    // Установить указатели для on_enter коллбеков
    current_actor_ = &actor;
    bus_ = &ctx.bus;

    state_timer_ += dt;

    const auto& state = fsm_.current_state();

    // Таймерные автопереходы: OPENING -> OPEN
    if (state == "OPENING" && state_timer_ >= open_duration_) {
        std::string old_state = state;
        state_timer_ = 0.0;
        fsm_.fire("_auto");
        ctx.bus.publish(event::ActorStateChanged{
            .actor = actor.id,
            .old_state = old_state,
            .new_state = fsm_.current_state()
        });
    }
    // Таймерные автопереходы: CLOSING -> CLOSED
    else if (state == "CLOSING" && state_timer_ >= close_duration_) {
        std::string old_state = state;
        state_timer_ = 0.0;
        fsm_.fire("_auto");
        ctx.bus.publish(event::ActorStateChanged{
            .actor = actor.id,
            .old_state = old_state,
            .new_state = fsm_.current_state()
        });
    }
    // auto_close: OPEN -> CLOSING после auto_close_secs_
    else if (state == "OPEN" && auto_close_secs_ > 0.0 && state_timer_ >= auto_close_secs_) {
        std::string old_state = state;
        state_timer_ = 0.0;
        fsm_.fire("close");
        ctx.bus.publish(event::ActorStateChanged{
            .actor = actor.id,
            .old_state = old_state,
            .new_state = fsm_.current_state()
        });
    }

    // Вызвать on_update текущего состояния
    fsm_.update(dt);

    // Синхронизировать actor.current_state с FSM
    actor.current_state = fsm_.current_state();

    // Очистить временный указатель
    current_actor_ = nullptr;
}

// ============================================================================
// Взаимодействия
// ============================================================================

void DoorBehavior::on_interact(EntityId /*source*/, const std::string& action,
                               const nlohmann::json& /*params*/)
{
    std::string old_state = fsm_.current_state();

    if (action == "open") {
        state_timer_ = 0.0;
        if (fsm_.fire("open") && bus_) {
            bus_->publish(event::ActorStateChanged{
                .actor = actor_id_,
                .old_state = old_state,
                .new_state = fsm_.current_state()
            });
        }
    }
    else if (action == "close") {
        state_timer_ = 0.0;
        if (fsm_.fire("close") && bus_) {
            bus_->publish(event::ActorStateChanged{
                .actor = actor_id_,
                .old_state = old_state,
                .new_state = fsm_.current_state()
            });
        }
    }
}

void DoorBehavior::on_signal(const SignalEvent& event)
{
    // Whitelist: только wire_open и wire_close (T-02-09)
    if (!event.active) return;

    std::string old_state = fsm_.current_state();

    if (event.signal_id == "wire_open") {
        state_timer_ = 0.0;
        if (fsm_.fire("open") && bus_) {
            bus_->publish(event::ActorStateChanged{
                .actor = actor_id_,
                .old_state = old_state,
                .new_state = fsm_.current_state()
            });
        }
    }
    else if (event.signal_id == "wire_close") {
        state_timer_ = 0.0;
        if (fsm_.fire("close") && bus_) {
            bus_->publish(event::ActorStateChanged{
                .actor = actor_id_,
                .old_state = old_state,
                .new_state = fsm_.current_state()
            });
        }
    }
    // Остальные signal_id игнорируются (whitelist по T-02-09)
}

// ============================================================================
// Состояние и сериализация
// ============================================================================

std::string DoorBehavior::current_state() const
{
    return fsm_.current_state();
}

std::string DoorBehavior::to_json() const
{
    nlohmann::json j;
    j["type"]  = "door";
    j["state"] = fsm_.current_state();
    j["timer"] = state_timer_;
    j["open_duration"]   = open_duration_;
    j["close_duration"]  = close_duration_;
    j["auto_close_secs"] = auto_close_secs_;
    return j.dump();
}

} // namespace s2
