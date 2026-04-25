#pragma once
/**
 * @file event_bus.hpp
 * EventBus — типизированная шина событий (переименован из SimBus).
 *
 * Содержит все стандартные event-типы S2.
 * Принцип работы идентичен SimBus — synchronous dispatch через std::any.
 *
 * Архитектурная мотивация:
 *  - Модули не знают друг о друге напрямую
 *  - Зональная система публикует «entity вошла в зону»
 *  - Interaction модуль публикует «захват удался/не удался»
 *  - Без шины пришлось бы делать прямые вызовы — сильная связность модулей
 *
 * Ограничения (по дизайну):
 *  - НЕ потокобезопасен (вызывается только в симуляционном потоке)
 *  - НЕ поддерживает отписку во время dispatch
 *  - НЕ поддерживает приоритеты подписчиков
 *  - Если подписчик бросает исключение — оно пробрасывается наружу
 */

#include <s2/types.hpp>

#include <any>
#include <functional>
#include <string>
#include <typeindex>
#include <unordered_map>
#include <vector>

namespace s2
{

// ============================================================================
// Стандартные события
// ============================================================================

/**
 * @brief Пространство имён стандартных событий S2.
 *
 * Каждое событие — POD-структура, легко копируемая.
 * Модули могут добавлять свои события — достаточно создать структуру
 * и подписаться на неё через EventBus.
 */
namespace event
{

// ─── Зоны ────────────────────────────────────────────────────────────────────

/** @brief Агент вошёл в зону (legacy — backward compat с AgentEnteredZone). */
struct AgentEnteredZone { AgentId agent; ZoneId zone; };

/** @brief Агент вышел из зоны (legacy — backward compat с AgentExitedZone). */
struct AgentExitedZone  { AgentId agent; ZoneId zone; };

/** @brief Entity вошла в зону. Публикуется ZoneSystem. */
struct ZoneEntered { ZoneId zone_id; EntityId entity_id; };

/** @brief Entity вышла из зоны. Публикуется ZoneSystem. */
struct ZoneExited  { ZoneId zone_id; EntityId entity_id; };

// ─── Сущности ────────────────────────────────────────────────────────────────

/** @brief Entity создана. Публикуется ядром при SpawnEntity. */
struct EntitySpawned { EntityId id; std::string entity_type; };

/** @brief Entity удалена. Публикуется ядром при DespawnEntity. */
struct EntityDespawned { EntityId id; };

// ─── Акторы ──────────────────────────────────────────────────────────────────

/** @brief Состояние актора изменилось (FSM-переход). */
struct ActorStateChanged { ActorId actor; ActorState old_state; ActorState new_state; };

// ─── Сигналы ─────────────────────────────────────────────────────────────────

/** @brief Сигнал активирован (wire triggered, button pressed и т.п.). */
struct SignalActivated   { std::string signal_id; EntityId source_entity; };

/** @brief Сигнал деактивирован. */
struct SignalDeactivated { std::string signal_id; EntityId source_entity; };

// ─── Взаимодействия ──────────────────────────────────────────────────────────

/** @brief Попытка захватить объект. Публикуется GrabberPlugin перед командой. */
struct GrabAttempt   { EntityId agent; EntityId target; };

/** @brief Захват успешен. Публикуется ядром после AttachObject. */
struct GrabSucceeded { EntityId agent; EntityId target; };

/** @brief Захват не удался. Публикуется ядром при отказе AttachObject. */
struct GrabFailed    { EntityId agent; EntityId target; std::string reason; };

// ─── Урон ────────────────────────────────────────────────────────────────────

/** @brief Нанесён урон сущности. */
struct DamageDealt { EntityId source; EntityId target; double amount; std::string damage_type; };

// ─── Объекты (legacy) ────────────────────────────────────────────────────────

/** @brief Объект привязан к агенту (legacy). */
struct ObjectAttached  { ObjectId obj; AgentId agent; std::string link; };

/** @brief Объект отпущен (legacy). */
struct ObjectReleased  { ObjectId obj; AgentId agent; };

/** @brief Столкновение агента (legacy). */
struct AgentCollision  { AgentId agent; Vec3 point; };

/** @brief Команда телепортации агента (legacy). */
struct TeleportAgentCommand { AgentId agent_id; Vec3 destination; double yaw{0.0}; };

/** @brief Установка runtime-цели телепорт-зоны (legacy). */
struct SetZoneTeleportTargetCommand { ZoneId zone_id; Vec3 destination; double yaw{0.0}; };

}  // namespace event

// ============================================================================
// EventBus
// ============================================================================

/**
 * @brief Типизированная шина событий.
 *
 * Механизм publish/subscribe для синхронной коммуникации между модулями.
 * Dispatch синхронный — подписчики вызываются прямо в publish().
 *
 * Не потокобезопасен (только симуляционный поток).
 *
 * Пример использования:
 * @code
 * EventBus bus;
 *
 * // Подписка
 * bus.subscribe<event::ZoneEntered>(
 *     [](const event::ZoneEntered& e) {
 *         std::cout << "Entity " << e.entity_id << " entered zone " << e.zone_id;
 *     });
 *
 * // Публикация
 * bus.publish(event::ZoneEntered{.zone_id = "ice_zone", .entity_id = 42});
 * @endcode
 */
class EventBus
{
public:
  EventBus() = default;

  /**
   * @brief Подписаться на событие типа EventT.
   * @tparam EventT Тип события
   * @param handler void(const EventT&)
   */
  template <typename EventT>
  void subscribe(std::function<void(const EventT&)> handler)
  {
    auto wrapper = [handler](const std::any& event) {
      handler(std::any_cast<const EventT&>(event));
    };
    handlers_[typeid(EventT)].push_back(std::move(wrapper));
  }

  /**
   * @brief Опубликовать событие — синхронно вызвать всех подписчиков.
   * @tparam EventT Тип события (выводится автоматически)
   */
  template <typename EventT>
  void publish(const EventT& event)
  {
    auto it = handlers_.find(typeid(EventT));
    if (it == handlers_.end())
      return;
    for (const auto& handler : it->second)
      handler(event);
  }

  /** @brief Количество подписчиков на тип EventT. */
  template <typename EventT>
  std::size_t subscriber_count() const
  {
    auto it = handlers_.find(typeid(EventT));
    if (it == handlers_.end()) return 0;
    return it->second.size();
  }

  /** @brief Количество зарегистрированных типов событий. */
  std::size_t event_type_count() const { return handlers_.size(); }

private:
  // type_index → список обработчиков (обёрнутых в std::function для any)
  std::unordered_map<std::type_index,
                     std::vector<std::function<void(const std::any&)>>>
      handlers_;
};

}  // namespace s2
