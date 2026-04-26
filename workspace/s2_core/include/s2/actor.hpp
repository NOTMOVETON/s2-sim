#pragma once

/**
 * @file actor.hpp
 * Структура актора — активного неагентного объекта.
 *
 * Actor — это дверь, лифт, пешеход и т.д.
 * Имеет один behavior (IActorBehavior), который определяет логику.
 * Опционально имеет controller-плагины (DoorWireController и т.п.).
 *
 * Actor не является copyable (содержит unique_ptr).
 * Используйте std::move() при добавлении в контейнеры.
 */

#include <s2/actor_behavior.hpp>
#include <s2/plugin_base.hpp>
#include <s2/shared_state.hpp>
#include <s2/types.hpp>

#include <memory>
#include <string>
#include <vector>

namespace s2
{

/**
 * @brief Активный неагентный объект — дверь, лифт, конвейер.
 *
 * Расширенная структура Phase 2:
 *  - behavior:          единственный IActorBehavior (логика актора)
 *  - plugins:           опциональные controller-плагины (DoorWireController и т.п.)
 *  - state:             SharedState для contribution/resolver (lock contributions)
 *  - type:              строковый тип актора ("door", "conveyor", "elevator")
 *  - collision_enabled: false при OPEN двери — CollisionSystem пропускает
 *
 * Actor — move-only (содержит unique_ptr<IActorBehavior> и vector<unique_ptr>).
 * Implicit move constructor/assignment сгенерированы компилятором.
 * Implicit copy constructor/assignment удалены.
 */
struct Actor
{
  ActorId id{0};                           ///< Уникальный идентификатор актора
  std::string name;                        ///< Человекочитаемое имя
  std::string type;                        ///< Тип актора: "door", "conveyor", "elevator"
  Pose3D world_pose;                       ///< Поза в мировых координатах
  ActorState current_state;                ///< Текущее состояние FSM (строка)
  bool collision_enabled{true};            ///< false при OPEN двери — пропускается CollisionSystem

  CollisionShape collision;               ///< Коллизионное описание
  VisualDesc visual;                       ///< Визуальное описание

  // ─── Поведение актора ──────────────────────────────────────────────────────

  /// Единственный behavior актора (D-04)
  /// nullptr допустимо для акторов без логики (статические объекты)
  std::unique_ptr<IActorBehavior> behavior;

  // ─── Опциональные плагины ──────────────────────────────────────────────────

  /// Controller-плагины актора (DoorWireController и т.п.) (D-05)
  /// Те же IAgentPlugin что у агентов, но живут на акторе
  std::vector<std::unique_ptr<plugins::IAgentPlugin>> plugins;

  // ─── SharedState ───────────────────────────────────────────────────────────

  /// SharedState актора — участвует в contribution/resolver (D-05)
  /// DoorWireController пишет lock contributions
  SharedState state;
};

} // namespace s2
