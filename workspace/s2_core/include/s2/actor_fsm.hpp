#pragma once

/**
 * @file actor_fsm.hpp
 * ActorFSM — утилитарный класс конечного автомата для IActorBehavior.
 *
 * Используется behavior'ами, которым удобна FSM-модель:
 *  - DoorBehavior: CLOSED → OPENING → OPEN → CLOSING → CLOSED
 *  - ElevatorBehavior: IDLE → MOVING_UP → MOVING_DOWN → ...
 *
 * Behavior'ы без FSM (DirtPile и т.п.) не обязаны использовать ActorFSM.
 *
 * Реализация полностью inline — FSM небольшой, нет смысла выносить в .cpp.
 * Состояния хранятся как строки, переходы — в таблице.
 */

#include <algorithm>
#include <functional>
#include <string>
#include <vector>

namespace s2
{

/**
 * @brief Утилитарный конечный автомат для behavior'ов акторов.
 *
 * API:
 *  - add_state(name, on_enter, on_update, on_exit) — добавить состояние
 *  - add_transition(from, to, trigger, guard) — добавить переход
 *  - fire(trigger) — попытаться выполнить переход
 *  - update(dt) — вызвать on_update текущего состояния
 *  - current_state() — текущее состояние
 *
 * Первое добавленное состояние становится начальным.
 *
 * Пример:
 * @code
 * ActorFSM fsm;
 * fsm.add_state("closed", nullptr, nullptr, nullptr);
 * fsm.add_state("opening", nullptr, [this](double dt){ animate(dt); }, nullptr);
 * fsm.add_state("open", [this]{ on_fully_open(); }, nullptr, nullptr);
 *
 * fsm.add_transition("closed", "opening", "open_cmd");
 * fsm.add_transition("opening", "open", "animation_done");
 *
 * fsm.fire("open_cmd");   // closed → opening
 * fsm.update(dt);         // вызывает animate(dt)
 * fsm.fire("animation_done"); // opening → open, вызывает on_fully_open()
 * @endcode
 */
class ActorFSM
{
public:
    /// Тип callback при входе в состояние
    using OnEnter  = std::function<void()>;
    /// Тип callback при тиковом обновлении состояния
    using OnUpdate = std::function<void(double dt)>;
    /// Тип callback при выходе из состояния
    using OnExit   = std::function<void()>;
    /// Тип guard-условия для перехода
    using Guard    = std::function<bool()>;

    /**
     * @brief Описание состояния FSM.
     */
    struct State
    {
        std::string name;       ///< Имя состояния ("closed", "opening", ...)
        OnEnter     on_enter;   ///< Вызывается при входе (nullptr = no-op)
        OnUpdate    on_update;  ///< Вызывается каждый тик (nullptr = no-op)
        OnExit      on_exit;    ///< Вызывается при выходе (nullptr = no-op)
    };

    /**
     * @brief Описание перехода между состояниями.
     */
    struct Transition
    {
        std::string from;       ///< Исходное состояние
        std::string to;         ///< Целевое состояние
        std::string trigger;    ///< Триггер (имя события)
        Guard       guard;      ///< Условие (nullptr = всегда разрешён)
    };

    // ─── API ─────────────────────────────────────────────────────────────────

    /**
     * @brief Добавить состояние в FSM.
     *
     * Если текущее состояние ещё не установлено (FSM пуст),
     * первое добавленное состояние становится начальным.
     *
     * @param name      Имя состояния (уникальное)
     * @param on_enter  Callback при входе (nullptr = no-op)
     * @param on_update Callback при тиковом обновлении (nullptr = no-op)
     * @param on_exit   Callback при выходе (nullptr = no-op)
     */
    inline void add_state(std::string name,
                          OnEnter on_enter = nullptr,
                          OnUpdate on_update = nullptr,
                          OnExit on_exit = nullptr)
    {
        states_.push_back(State{
            std::move(name),
            std::move(on_enter),
            std::move(on_update),
            std::move(on_exit)
        });

        // Первое состояние — начальное
        if (current_.empty())
        {
            current_ = states_.back().name;
        }
    }

    /**
     * @brief Добавить переход между состояниями.
     *
     * @param from    Исходное состояние
     * @param to      Целевое состояние
     * @param trigger Триггер (имя события для fire())
     * @param guard   Условие перехода (nullptr = всегда разрешён)
     */
    inline void add_transition(std::string from,
                               std::string to,
                               std::string trigger,
                               Guard guard = nullptr)
    {
        transitions_.push_back(Transition{
            std::move(from),
            std::move(to),
            std::move(trigger),
            std::move(guard)
        });
    }

    /**
     * @brief Попытаться выполнить переход по триггеру.
     *
     * Ищет первый переход с from == current_ и trigger == trigger.
     * Если guard задан и возвращает false — переход не выполняется.
     *
     * При успешном переходе:
     *  1. Вызывает on_exit текущего состояния
     *  2. Устанавливает current_ = to
     *  3. Вызывает on_enter нового состояния
     *
     * @param trigger Имя триггера
     * @return true если переход выполнен, false если нет подходящего перехода
     */
    inline bool fire(const std::string& trigger)
    {
        // Найти подходящий переход
        for (const auto& t : transitions_)
        {
            if (t.from == current_ && t.trigger == trigger)
            {
                // Проверить guard
                if (t.guard && !t.guard())
                    return false;

                // Вызвать on_exit текущего состояния
                auto* old_state = find_state(current_);
                if (old_state && old_state->on_exit)
                    old_state->on_exit();

                // Перейти в новое состояние
                current_ = t.to;

                // Вызвать on_enter нового состояния
                auto* new_state = find_state(current_);
                if (new_state && new_state->on_enter)
                    new_state->on_enter();

                return true;
            }
        }
        return false;
    }

    /**
     * @brief Вызвать on_update текущего состояния.
     *
     * Вызывается каждый тик из behavior.update().
     * Если on_update == nullptr — ничего не делает.
     *
     * @param dt Шаг тика (секунды)
     */
    inline void update(double dt)
    {
        auto* state = find_state(current_);
        if (state && state->on_update)
            state->on_update(dt);
    }

    /**
     * @brief Текущее состояние FSM.
     * @return Имя текущего состояния (пустая строка если FSM пуст)
     */
    const std::string& current_state() const { return current_; }

private:
    /**
     * @brief Найти состояние по имени.
     * @return Указатель на State или nullptr если не найдено
     */
    State* find_state(const std::string& name)
    {
        auto it = std::find_if(states_.begin(), states_.end(),
                               [&name](const State& s) { return s.name == name; });
        return (it != states_.end()) ? &(*it) : nullptr;
    }

    std::vector<State>      states_;       ///< Зарегистрированные состояния
    std::vector<Transition> transitions_;  ///< Зарегистрированные переходы
    std::string             current_;      ///< Текущее состояние
};

}  // namespace s2
