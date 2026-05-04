#pragma once

/**
 * @file actor_fsm.hpp
 * ActorFSM — утилитарный класс конечного автомата для реализаций IActorBehavior (Phase 5).
 *
 * Используется внутри конкретных поведений (DoorBehavior, ConveyorBehavior и др.).
 * Не является наследником IActorBehavior — это вспомогательный инструмент.
 */

#include <functional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace s2 {

using StateId = std::string;
using EventId = std::string;
using Condition = std::function<bool()>;

struct Transition {
    StateId   to;
    Condition condition; // пустая = безусловный переход
};

class ActorFSM {
public:
    void add_state(const StateId& s) {
        states_.insert(s);
    }

    void set_initial(const StateId& s) {
        current_state_ = s;
    }

    void add_transition(const StateId& from,
                        const EventId& event,
                        const StateId& to,
                        Condition cond = {}) {
        transitions_[from][event].push_back({to, std::move(cond)});
    }

    // Ищет первый подходящий переход из current_state_ по event.
    // Проверяет condition (если есть). Меняет состояние и возвращает true при успехе.
    bool fire(const EventId& event) {
        auto it_state = transitions_.find(current_state_);
        if (it_state == transitions_.end()) return false;

        auto it_event = it_state->second.find(event);
        if (it_event == it_state->second.end()) return false;

        for (const auto& tr : it_event->second) {
            if (!tr.condition || tr.condition()) {
                current_state_ = tr.to;
                return true;
            }
        }
        return false;
    }

    // Хук для state-based логики — реализации дописывают свои обработчики.
    void update(double /*dt*/) {}

    const StateId& current_state() const { return current_state_; }

private:
    StateId current_state_;
    std::unordered_set<StateId> states_;
    std::unordered_map<StateId,
        std::unordered_map<EventId, std::vector<Transition>>> transitions_;
};

} // namespace s2
