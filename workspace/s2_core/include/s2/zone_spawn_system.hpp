#pragma once

/**
 * @file zone_spawn_system.hpp
 * ZoneSpawnSystem — декларативный спавн зон по триггерам (per D-14, D-15, D-16).
 *
 * Типы триггеров:
 *  - timer:        зона спавнится через N секунд sim_time
 *  - event:        EventBus событие с фильтром по типу + source_entity
 *  - state_change: ActorStateChanged с фильтром actor_id + target_state
 *  - command:      прямой SpawnZone KernelCommand (уже есть, не требует ZoneSpawnSystem)
 *
 * ZoneSpawnSystem — отдельная система в SimEngine.
 * Работает только в sim_thread — не использует std::thread.
 */

#include <s2/kernel_command.hpp>
#include <s2/sim_bus.hpp>
#include <s2/zone.hpp>
#include <string>
#include <vector>

namespace s2 {

class ZoneSpawnSystem
{
public:
    // ── Структуры триггеров ──────────────────────────────────────────────────

    struct TimerTrigger {
        double delay_seconds{0.0};  ///< Секунд от момента регистрации
        double trigger_time{-1.0};  ///< Абсолютное sim_time срабатывания (вычисляется в add_template)
        bool   fired{false};
    };

    struct EventTrigger {
        std::string event_type;    ///< Имя типа события: "ZoneEntered", "ZoneExited", ...
        std::string source_entity; ///< Фильтр по entity_id (пусто = любой)
    };

    struct StateChangeTrigger {
        ActorId     actor_id{0};    ///< Фильтр по id актора
        std::string target_state;   ///< Ожидаемое новое состояние
    };

    struct ZoneTemplate {
        std::string name;
        cmd::SpawnZone spawn_cmd;   ///< Команда создания зоны (shape, effects, color, ...)

        /// Ровно один из триггеров задан (остальные — дефолт / не проверяются)
        enum class TriggerType { TIMER, EVENT, STATE_CHANGE } trigger_type{TriggerType::TIMER};
        TimerTrigger      timer;
        EventTrigger      event;
        StateChangeTrigger state_change;
    };

    // ── API ──────────────────────────────────────────────────────────────────

    /// Инициализация: подписаться на EventBus (для event и state_change триггеров).
    void init(SimBus& bus, KernelCommandQueue& out_queue, double current_sim_time);

    /// Зарегистрировать шаблон зоны с триггером.
    void add_template(ZoneTemplate tmpl, double current_sim_time);

    /// Проверить timer-триггеры. Вызывается в Phase 0 тика.
    void tick(double sim_time);

    /// Очистить все шаблоны (при смене сцены).
    void clear();

private:
    /// Проверить event-триггеры при получении EventBus события.
    void check_event_triggers(const std::string& event_type,
                              const std::string& source_entity);

    /// Проверить state_change триггеры при ActorStateChanged.
    void check_state_change_triggers(ActorId actor_id,
                                     const std::string& new_state);

    std::vector<ZoneTemplate> templates_;
    KernelCommandQueue*       out_queue_{nullptr};
};

} // namespace s2
