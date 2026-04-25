#pragma once
/**
 * @file sim_bus.hpp
 * Backward-compatibility header: SimBus = EventBus.
 *
 * Весь код, включающий sim_bus.hpp, продолжает работать без изменений.
 * Новый код должен использовать event_bus.hpp напрямую.
 *
 * Сохраняет полный доступ к namespace event:: (AgentEnteredZone,
 * AgentExitedZone, ObjectAttached, ObjectReleased, ActorStateChanged,
 * AgentCollision, TeleportAgentCommand, SetZoneTeleportTargetCommand
 * и все новые типы из ARCH-04).
 */
#include <s2/event_bus.hpp>

namespace s2
{
  /// @brief Алиас для backward compatibility: SimBus = EventBus.
  using SimBus = EventBus;
}  // namespace s2
