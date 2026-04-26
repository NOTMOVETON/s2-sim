#pragma once

/**
 * @file signal_listener_base.hpp
 * SignalListenerBase -- общая база для wire-контроллеров.
 *
 * Подписывается на EventBus события SignalActivated / SignalDeactivated
 * при первом вызове update() (через subscribe_once()).
 * Накапливает pending_signals_ между flush-ами.
 *
 * Унаследованные контроллеры (DoorWireController, ConveyorWireController, EventReactor):
 *   1. Вызывают subscribe_once(ctx.bus) в начале update()
 *   2. Итерируют pending_signals() для реакций
 *   3. Вызывают flush_signals() в конце update()
 *
 * Threat mitigation (T-02-13): при накоплении > 100 событий за тик
 * лишние отбрасываются (защита от DoS через спам сигналами).
 */

#include <s2/plugin_base.hpp>
#include <s2/event_bus.hpp>

#include <algorithm>
#include <string>
#include <vector>

namespace s2
{

class SignalListenerBase : public plugins::IAgentPlugin
{
public:
    // ---- Управление подпиской и накоплением сигналов -------------------------

    /**
     * @brief Подписаться на EventBus при первом вызове.
     * Вызывается в начале update() каждого наследника.
     * Повторные вызовы -- no-op.
     */
    void subscribe_once(EventBus& bus)
    {
        if (subscribed_) return;

        bus.subscribe<event::SignalActivated>(
            [this](const event::SignalActivated& e) {
                if (pending_signals_.size() < kMaxPendingSignals) {
                    pending_signals_.push_back({e.signal_id, e.source_entity, true});
                }
            });

        bus.subscribe<event::SignalDeactivated>(
            [this](const event::SignalDeactivated& e) {
                if (pending_signals_.size() < kMaxPendingSignals) {
                    pending_signals_.push_back({e.signal_id, e.source_entity, false});
                }
            });

        bus_ = &bus;
        subscribed_ = true;
    }

    /**
     * @brief Очистить накопленные сигналы после обработки.
     * Вызывается в конце update() каждого наследника.
     */
    void flush_signals() { pending_signals_.clear(); }

    // ---- Доступ к накопленным сигналам --------------------------------------

    /**
     * @brief Накопленный сигнал из EventBus (SignalActivated/Deactivated).
     */
    struct PendingSignal
    {
        std::string signal_id;
        EntityId    source_entity{0};
        bool        active{true};
    };

    const std::vector<PendingSignal>& pending_signals() const { return pending_signals_; }

protected:
    /**
     * @brief Фильтрация по signal_id.
     */
    std::vector<PendingSignal> filter_by_id(const std::string& signal_id) const
    {
        std::vector<PendingSignal> result;
        for (const auto& sig : pending_signals_) {
            if (sig.signal_id == signal_id) {
                result.push_back(sig);
            }
        }
        return result;
    }

    /**
     * @brief Фильтрация по source_entity.
     */
    std::vector<PendingSignal> filter_by_source(EntityId source_entity) const
    {
        std::vector<PendingSignal> result;
        for (const auto& sig : pending_signals_) {
            if (sig.source_entity == source_entity) {
                result.push_back(sig);
            }
        }
        return result;
    }

    EventBus* bus_{nullptr};  ///< Указатель на EventBus (устанавливается в subscribe_once)

private:
    static constexpr std::size_t kMaxPendingSignals = 100;  ///< T-02-13: лимит на один тик
    std::vector<PendingSignal> pending_signals_;
    bool subscribed_{false};
};

} // namespace s2
