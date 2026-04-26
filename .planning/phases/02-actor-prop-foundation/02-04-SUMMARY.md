---
phase: 02-actor-prop-foundation
plan: 04
subsystem: plugins
tags: [wire-controller, signal-listener, event-reactor, interaction, eventbus, c++17]

# Dependency graph
requires:
  - phase: 02-actor-prop-foundation
    plan: 01
    provides: "IActorBehavior, SignalEvent, Actor struct (behavior+plugins)"
  - phase: 02-actor-prop-foundation
    plan: 03
    provides: "DoorBehavior FSM, DoorOpenerPlugin, behaviors_registry, plugins_registry pattern"
provides:
  - "SignalListenerBase -- общая база для wire-контроллеров (EventBus подписка, pending_signals_, filter, flush)"
  - "DoorWireController -- wire-реакции close_and_lock/force_open/unlock через cmd::Interact"
  - "ConveyorWireController -- wire-реакции stop/reverse/start через cmd::Interact"
  - "EventReactor -- декларативная трансляция wire-сигналов в EventBus события"
affects: [02-05, 03-actor-ecosystem]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "SignalListenerBase: EventBus subscribe_once() + pending_signals_ + flush_signals() паттерн"
    - "Wire controller: декларативные реакции (reactions YAML) -> cmd::Interact на актора"
    - "EventReactor: signal -> event трансляция через EventBus (расширяемо в Phase 5)"

key-files:
  created:
    - workspace/s2_plugins/include/s2/signal_listener_base.hpp
    - workspace/s2_plugins/include/s2/plugins/door_wire_controller.hpp
    - workspace/s2_plugins/src/plugins/door_wire_controller.cpp
    - workspace/s2_plugins/include/s2/plugins/conveyor_wire_controller.hpp
    - workspace/s2_plugins/src/plugins/conveyor_wire_controller.cpp
    - workspace/s2_plugins/include/s2/plugins/event_reactor.hpp
    - workspace/s2_plugins/src/plugins/event_reactor.cpp
  modified:
    - workspace/s2_plugins/src/plugins_registry.cpp
    - workspace/s2_plugins/CMakeLists.txt

key-decisions:
  - "SignalListenerBase подписывается на EventBus при первом update() (subscribe_once) -- on_spawn() не имеет доступа к EventBus"
  - "Лимит 100 pending_signals_ за тик (T-02-13) -- защита от DoS через спам сигналами"
  - "EventReactor поддерживает только signal_activated (T-02-11) -- расширение в Phase 5"

patterns-established:
  - "subscribe_once(ctx.bus) + iterate pending_signals() + flush_signals() в update()"
  - "Wire controller: from_config парсит reactions[], update() матчит signal_id/source_entity"
  - "cmd::Interact с target_id = agent.id для self-dispatch на behavior.on_interact()"

requirements-completed: [ACTR-06]

# Metrics
duration: 5min
completed: 2026-04-26
---

# Phase 2 Plan 04: Wire Controllers Summary

**SignalListenerBase + три wire-контроллера (DoorWireController, ConveyorWireController, EventReactor) для декларативного управления акторами через EventBus wire-сигналы**

## Performance

- **Duration:** 5 min
- **Started:** 2026-04-26T19:35:56Z
- **Completed:** 2026-04-26T19:41:07Z
- **Tasks:** 2
- **Files modified:** 9

## Accomplishments
- SignalListenerBase: общая база с EventBus подпиской (subscribe_once), pending_signals_, filter_by_id/filter_by_source, flush_signals(), лимит 100 событий (T-02-13)
- DoorWireController: декларативные реакции на wire-сигналы (close_and_lock, force_open, unlock, open, close) через cmd::Interact
- ConveyorWireController: аналогичный контроллер с реакциями stop/reverse/start
- EventReactor: декларативная трансляция signal_id -> fire_event через EventBus (только signal_activated, T-02-11)
- Все три плагина зарегистрированы в plugins_registry, роль INTERACTION
- Все 4/4 тестовых таргета проходят (100%)

## Task Commits

Each task was committed atomically:

1. **Task 1: SignalListenerBase + DoorWireController + ConveyorWireController** - `816aaf4` (feat)
2. **Task 2: EventReactor декларативный плагин** - `f725f8d` (feat)

## Files Created/Modified
- `workspace/s2_plugins/include/s2/signal_listener_base.hpp` - SignalListenerBase: EventBus subscribe_once, pending_signals_, filter, flush, лимит 100
- `workspace/s2_plugins/include/s2/plugins/door_wire_controller.hpp` - DoorWireController declaration: Reaction struct с signal_id/source_entity/on_active/on_inactive
- `workspace/s2_plugins/src/plugins/door_wire_controller.cpp` - DoorWireController: from_config реакций, update -> cmd::Interact
- `workspace/s2_plugins/include/s2/plugins/conveyor_wire_controller.hpp` - ConveyorWireController declaration
- `workspace/s2_plugins/src/plugins/conveyor_wire_controller.cpp` - ConveyorWireController: аналогичная логика для конвейера
- `workspace/s2_plugins/include/s2/plugins/event_reactor.hpp` - EventReactor declaration: listen signal_id, on_active/on_inactive fire_event
- `workspace/s2_plugins/src/plugins/event_reactor.cpp` - EventReactor: from_config YAML -> params, update -> bus.publish
- `workspace/s2_plugins/src/plugins_registry.cpp` - Добавлена регистрация door_wire_controller, conveyor_wire_controller, event_reactor
- `workspace/s2_plugins/CMakeLists.txt` - Добавлены три новых source файла

## Decisions Made
- SignalListenerBase подписывается на EventBus при первом update() через subscribe_once() -- on_spawn() не имеет доступа к ctx.bus. Паттерн consistent для всех наследников.
- Лимит 100 pending_signals_ за тик (T-02-13): при превышении новые сигналы отбрасываются -- защита от бесконечного накопления.
- EventReactor поддерживает только "signal_activated" тип события (T-02-11): неизвестные event_type пропускаются без ошибки. Расширение на кастомные типы -- Phase 5.
- Wire-контроллеры доставляют реакции через cmd::Interact с target_id = agent.id (self-dispatch): ядро маршрутизирует к behavior.on_interact(), что обеспечивает единый pipeline.

## Deviations from Plan

None -- план выполнен точно как написано.

## Issues Encountered
None -- сборка и тесты прошли без проблем.

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- SignalListenerBase готова для использования Phase 3 контроллерами (ElevatorWireController и т.п.)
- Wire-контроллеры демонстрируют паттерн interaction-плагина актора (subscribe_once -> pending_signals -> cmd::Interact)
- Plan 02-05 может реализовать GrabberPlugin по аналогии с DoorOpenerPlugin
- Phase 3 может реализовать ConveyorActor + ElevatorBehavior с привязанными wire-контроллерами

## Self-Check: PASSED

- [x] workspace/s2_plugins/include/s2/signal_listener_base.hpp -- FOUND
- [x] workspace/s2_plugins/include/s2/plugins/door_wire_controller.hpp -- FOUND
- [x] workspace/s2_plugins/src/plugins/door_wire_controller.cpp -- FOUND
- [x] workspace/s2_plugins/include/s2/plugins/conveyor_wire_controller.hpp -- FOUND
- [x] workspace/s2_plugins/src/plugins/conveyor_wire_controller.cpp -- FOUND
- [x] workspace/s2_plugins/include/s2/plugins/event_reactor.hpp -- FOUND
- [x] workspace/s2_plugins/src/plugins/event_reactor.cpp -- FOUND
- [x] 02-04-SUMMARY.md -- FOUND
- [x] Commit 816aaf4 -- FOUND
- [x] Commit f725f8d -- FOUND

---
*Phase: 02-actor-prop-foundation*
*Completed: 2026-04-26*
