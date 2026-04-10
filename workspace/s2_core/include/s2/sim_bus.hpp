#pragma once

/**
 * @file sim_bus.hpp
 * SimBus — типизированная шина событий для синхронной коммуникации внутри тика.
 *
 * Архитектурная мотивация:
 *  - Модули не знают друг о друге напрямую
 *  - Зональная система хочет сообщить «агент вошёл в зону»
 *  - Interaction модуль хочет знать «дверь открылась»
 *  - Без шины пришлось бы делать прямые вызовы —耦合 между модулями
 *
 * Принцип работы:
 *  - Подписка: модуль регистрирует handler для конкретного типа события
 *  - Публикация: модуль публикует событие → все подписчики вызываются синхронно
 *  - Dispatch синхронный — вызов в момент publish, без очередей
 *  - Без аллокаций на горячем пути — вектор подписчиков заранее собран
 *
 * Примеры событий:
 *  - AgentEnteredZone / AgentExitedZone
 *  - ObjectAttached / ObjectReleased
 *  - ActorStateChanged
 *  - AgentCollision
 *
 * Чего SimBus НЕ делает:
 *  - Не буферизует события (синхронный dispatch)
 *  - Не гарантирует доставку (если подписчик не зарегистрирован — событие теряется)
 *  - Не поддерживает приоритеты подписчиков (порядок = порядок подписки)
 *  - Не поддерживает отписку во время dispatch (это undefined behavior)
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
 * Каждое событие — POD-структура (plain old data), легко копируемая.
 * Модули могут добавлять свои события — достаточно создать структуру
 * и подписаться на неё через SimBus.
 */
namespace event
{

/**
 * @brief Агент вошёл в зону.
 * Публикуется ZoneSystem при обнаружении входа.
 * Подписчики: зональные эффекты, interaction модули.
 */
struct AgentEnteredZone
{
  AgentId agent;   ///< Идентификатор агента
  ZoneId zone;     ///< Идентификатор зоны
};

/**
 * @brief Агент вышел из зоны.
 * Публикуется ZoneSystem при обнаружении выхода.
 * Подписчики: зональные эффекты (для снятия MODIFIER/CONTINUOUS).
 */
struct AgentExitedZone
{
  AgentId agent;   ///< Идентификатор агента
  ZoneId zone;     ///< Идентификатор зоны
};

/**
 * @brief Объект привязан к агенту (захвачен).
 * Публикуется ядром после успешного AttachObject.
 * Подписчики: Grabber, визуализатор, resource модули.
 */
struct ObjectAttached
{
  ObjectId obj;          ///< Идентификатор объекта
  AgentId agent;         ///< Идентификатор агента-захватчика
  std::string link;      ///< Имя link-а, к которому привязан (например, "gripper")
};

/**
 * @brief Объект отпущен агентом.
 * Публикуется ядром после DetachObject.
 * Подписчики: Grabber, визуализатор, resource модули.
 */
struct ObjectReleased
{
  ObjectId obj;    ///< Идентификатор объекта
  AgentId agent;   ///< Идентификатор агента, который отпустил
};

/**
 * @brief Состояние актора изменилось.
 * Публикуется актором при переходе FSM.
 * Подписчики: Interaction модули (DoorOpener ждёт "open"), визуализатор.
 */
struct ActorStateChanged
{
  ActorId actor;          ///< Идентификатор актора
  ActorState old_state;   ///< Предыдущее состояние
  ActorState new_state;   ///< Новое состояние
};

/**
 * @brief Столкновение агента с препятствием.
 * Публикуется ядром при обнаружении коллизии.
 * Подписчики: визуализатор (подсветка), safety модули.
 */
struct AgentCollision
{
  AgentId agent;     ///< Идентификатор агента
  Vec3 point;        ///< Точка столкновения в мировых координатах
};

}  // namespace event

// ============================================================================
// SimBus
// ============================================================================

/**
 * @brief Типизированная шина событий (Event Bus).
 *
 * Механизм publish/subscribe для синхронной коммуникации между модулями.
 *
 * Как работает:
 *  1. Модуль подписывается на тип события: subscribe<EventT>(handler)
 *  2. Модуль публикует событие: publish<EventT>(event)
 *  3. Все подписчики вызываются синхронно в порядке подписки
 *
 * Типобезопасность:
 *  - Подписчик получает событие правильного типа (через std::any_cast)
 *  - Разные типы событий не пересекаются
 *  - Компилятор проверяет типы handler-ов
 *
 * Производительность:
 *  - Синхронный dispatch — нет очередей, нет аллокаций
 *  - Вектор подписчиков хранится в unordered_map по type_index
 *  - Для 100 агентов × 10 событий = ~1000 вызовов за тик — наносекунды
 *
 * Ограничения:
 *  - НЕ потокобезопасен (вызывается только в симуляционном потоке)
 *  - НЕ поддерживает отписку во время dispatch
 *  - НЕ поддерживает приоритеты подписчиков
 *  - Если подписчик бросает исключение — оно пробрасывается наружу
 *
 * Пример использования:
 * @code
 * SimBus bus;
 *
 * // Подписка
 * bus.subscribe<event::AgentEnteredZone>(
 *     [](const event::AgentEnteredZone& e) {
 *         std::cout << "Agent " << e.agent << " entered zone " << e.zone;
 *     });
 *
 * // Публикация
 * bus.publish(event::AgentEnteredZone{.agent = 1, .zone = "ice_zone"});
 * @endcode
 */
class SimBus
{
public:
  SimBus() = default;

  /**
   * @brief Подписаться на событие типа EventT.
   *
   * Handler вызывается синхронно при каждом publish<EventT>.
   * Порядок вызова = порядок подписки.
   *
   * @tparam EventT Тип события (должен быть копируемым)
   * @param handler Функция-обработчик: void(const EventT&)
   *
   * Пример:
   * @code
   * bus.subscribe<event::AgentEnteredZone>(
   *     [this](const event::AgentEnteredZone& e) { on_enter(e); });
   * @endcode
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
   * @brief Опубликовать событие.
   *
   * Все подписчики на тип EventT вызываются синхронно.
   * Если подписчиков нет — событие тихо теряется (не ошибка).
   *
   * @tparam EventT Тип события (определяется из аргумента)
   * @param event Событие для публикации
   *
   * Пример:
   * @code
   * bus.publish(event::AgentEnteredZone{.agent = 1, .zone = "ice_zone"});
   * @endcode
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

  /**
   * @brief Количество подписчиков на тип события.
   * Полезно для отладки и тестирования.
   *
   * @tparam EventT Тип события
   * @return Количество подписчиков
   */
  template <typename EventT>
  std::size_t subscriber_count() const
  {
    auto it = handlers_.find(typeid(EventT));
    if (it == handlers_.end())
      return 0;
    return it->second.size();
  }

  /**
   * @brief Общее количество зарегистрированных типов событий.
   */
  std::size_t event_type_count() const { return handlers_.size(); }

private:
  // type_index → список обработчиков (обёрнутых в std::function для any)
  std::unordered_map<std::type_index,
                     std::vector<std::function<void(const std::any&)>>>
      handlers_;
};

}  // namespace s2