---
phase: 02-actor-prop-foundation
plan: 03
subsystem: plugins
tags: [actor, behavior, fsm, door, interaction, wire-trigger, c++17]

# Dependency graph
requires:
  - phase: 02-actor-prop-foundation
    plan: 01
    provides: "IActorBehavior, ActorFSM, Actor struct (behavior+plugins+collision_enabled)"
  - phase: 02-actor-prop-foundation
    plan: 02
    provides: "SimEngine phase2_actors (behavior.update), cmd::Interact dispatch, SceneLoader BehaviorFactory"
provides:
  - "DoorBehavior: FSM CLOSED/OPENING/OPEN/CLOSING с таймерными переходами и wire-триггерами"
  - "DoorOpenerPlugin: INTERACTION плагин агента — proximity open через cmd::Interact"
  - "behaviors_registry: фабрика behavior по типу (create_behavior)"
affects: [02-04, 03-actor-ecosystem]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "IActorBehavior FSM: ActorFSM + таймерные автопереходы + ActorStateChanged публикация"
    - "behaviors_registry: статическая регистрация behavior-типов (BehaviorRegistrar)"
    - "Interaction pipeline: Plugin -> cmd::Interact -> Engine -> behavior.on_interact()"
    - "Wire-триггер: on_signal() с whitelist signal_id (wire_open/wire_close)"

key-files:
  created:
    - workspace/s2_plugins/include/s2/behaviors/door_behavior.hpp
    - workspace/s2_plugins/src/behaviors/door_behavior.cpp
    - workspace/s2_plugins/include/s2/plugins/door_opener_plugin.hpp
    - workspace/s2_plugins/src/plugins/door_opener_plugin.cpp
    - workspace/s2_plugins/src/behaviors_registry.cpp
    - workspace/s2_plugins/tests/test_door_behavior.cpp
  modified:
    - workspace/s2_plugins/src/plugins_registry.cpp
    - workspace/s2_plugins/CMakeLists.txt

key-decisions:
  - "bus_ устанавливается в update(), on_interact/on_signal публикуют ActorStateChanged только если bus_ != nullptr (после первого update)"
  - "on_reset: принудительный возврат в CLOSED через цепочку fire() — безопасно из любого состояния FSM"
  - "behaviors_registry отделён от plugins_registry — behavior и plugin фабрики независимы"

patterns-established:
  - "DoorBehavior FSM: on_init -> setup_fsm -> add_state/add_transition -> on_interact fires trigger -> update checks timers"
  - "current_actor_ pattern: временный указатель на актора для on_enter коллбеков FSM (устанавливается/сбрасывается в update)"
  - "Wire whitelist: on_signal проверяет signal_id по whitelistу, игнорирует неизвестные (T-02-09)"
  - "Interaction plugin: find_in_radius(actors_only) -> cmd::Interact -> engine dispatch"

requirements-completed: [ACTR-02]

# Metrics
duration: 7min
completed: 2026-04-26
---

# Phase 2 Plan 03: DoorBehavior + DoorOpenerPlugin Summary

**DoorBehavior FSM (CLOSED/OPENING/OPEN/CLOSING) с proximity + wire-триггерами, ActorStateChanged событиями и DoorOpenerPlugin interaction-плагином агента**

## Performance

- **Duration:** 7 min
- **Started:** 2026-04-26T19:23:19Z
- **Completed:** 2026-04-26T19:30:57Z
- **Tasks:** 2 (Task 1 TDD: RED + GREEN)
- **Files modified:** 8

## Accomplishments
- DoorBehavior: полный FSM с 4 состояниями, таймерными автопереходами (open_duration, close_duration, auto_close_secs)
- Триггеры открытия/закрытия: on_interact("open"/"close"), on_signal("wire_open"/"wire_close")
- Императивное управление: collision_enabled = false при OPEN, true при CLOSED
- ActorStateChanged публикуется через EventBus при каждом FSM-переходе (6 точек публикации)
- DoorOpenerPlugin: INTERACTION плагин, ищет двери через find_in_radius -> cmd::Interact
- behaviors_registry: фабрика create_behavior() для SceneLoader BehaviorFactory
- 19 тестов DoorBehavior — все проходят (TDD: RED 18 failures -> GREEN 0 failures)

## Task Commits

Each task was committed atomically:

1. **Task 1 RED: DoorBehavior failing tests** - `47dae26` (test)
2. **Task 1 GREEN: DoorBehavior FSM implementation** - `d1f54f9` (feat)
3. **Task 2: DoorOpenerPlugin interaction plugin** - `592eb0d` (feat)

## Files Created/Modified
- `workspace/s2_plugins/include/s2/behaviors/door_behavior.hpp` - DoorBehavior declaration: FSM, таймеры, wire whitelist
- `workspace/s2_plugins/src/behaviors/door_behavior.cpp` - DoorBehavior реализация: FSM setup, update с таймерами, on_interact/on_signal
- `workspace/s2_plugins/include/s2/plugins/door_opener_plugin.hpp` - DoorOpenerPlugin declaration: INTERACTION role
- `workspace/s2_plugins/src/plugins/door_opener_plugin.cpp` - DoorOpenerPlugin: find_in_radius -> cmd::Interact
- `workspace/s2_plugins/src/behaviors_registry.cpp` - Реестр behavior-типов: create_behavior() + BehaviorRegistrar
- `workspace/s2_plugins/tests/test_door_behavior.cpp` - 19 тестов: FSM, collision, events, wire, auto_close, reset
- `workspace/s2_plugins/src/plugins_registry.cpp` - Добавлена регистрация door_opener
- `workspace/s2_plugins/CMakeLists.txt` - Добавлены новые source файлы и test target

## Decisions Made
- bus_ устанавливается в update(): on_interact и on_signal могут публиковать ActorStateChanged только после первого вызова update(). Это корректно — behavior не получает ctx в on_interact/on_signal, а bus_ сохраняется из последнего update().
- on_reset() через цепочку fire(): из любого состояния FSM вызывается серия fire() для возврата в CLOSED. Проще и безопаснее чем прямая манипуляция current_ (которая бы пропустила on_enter/on_exit коллбеки).
- behaviors_registry отделён от plugins_registry: behavior-типы акторов (door, conveyor, elevator) и plugin-типы агентов (diff_drive, lidar) — разные реестры с разными фабричными сигнатурами.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Тест PublishesActorStateChangedOnTransitions — bus_ nullptr**
- **Found during:** Task 1 GREEN
- **Issue:** Тест вызывал on_interact() до первого update(), bus_ был nullptr, событие не публиковалось
- **Fix:** Тест исправлен: вызов update() перед on_interact() для установки bus_
- **Files modified:** workspace/s2_plugins/tests/test_door_behavior.cpp
- **Verification:** Тест проходит, событие корректно публикуется
- **Committed in:** d1f54f9

**2. [Rule 1 - Bug] Тест AutoCloseAfterTimeout — таймеры перекрывались**
- **Found during:** Task 1 GREEN
- **Issue:** close_duration (0.2s) завершалось во время advance после auto_close, состояние проскакивало CLOSING -> CLOSED
- **Fix:** Увеличено close_duration до 0.5 в тесте, чтобы проверить именно CLOSING состояние
- **Files modified:** workspace/s2_plugins/tests/test_door_behavior.cpp
- **Verification:** Тест корректно проверяет переход OPEN -> CLOSING
- **Committed in:** d1f54f9

---

**Total deviations:** 2 auto-fixed (2 bugs in tests)
**Impact on plan:** Тестовые таймеры скорректированы. Продакшн-код реализован по плану без отклонений.

## TDD Gate Compliance

- [x] RED gate: `47dae26` (test commit — 18/19 failures)
- [x] GREEN gate: `d1f54f9` (feat commit — 0 failures)
- [ ] REFACTOR gate: не требовался (код чистый, дублирования нет)

## Issues Encountered
None — сборка и тесты прошли без проблем.

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- DoorBehavior полностью функционален — первый конкретный IActorBehavior в S2
- behaviors_registry готов для регистрации новых behavior-типов (conveyor, elevator, pedestrian)
- DoorOpenerPlugin демонстрирует паттерн interaction-плагина (find_in_radius -> cmd::Interact)
- Plan 02-04 может реализовать SignalListenerBase + DoorWireController (контроллеры на акторе)
- Plan 02-05 может реализовать GrabberPlugin по аналогии с DoorOpenerPlugin

## Self-Check: PASSED

- [x] workspace/s2_plugins/include/s2/behaviors/door_behavior.hpp -- FOUND
- [x] workspace/s2_plugins/src/behaviors/door_behavior.cpp -- FOUND
- [x] workspace/s2_plugins/include/s2/plugins/door_opener_plugin.hpp -- FOUND
- [x] workspace/s2_plugins/src/plugins/door_opener_plugin.cpp -- FOUND
- [x] workspace/s2_plugins/src/behaviors_registry.cpp -- FOUND
- [x] workspace/s2_plugins/tests/test_door_behavior.cpp -- FOUND
- [x] 02-03-SUMMARY.md -- FOUND
- [x] Commit 47dae26 -- FOUND
- [x] Commit d1f54f9 -- FOUND
- [x] Commit 592eb0d -- FOUND

---
*Phase: 02-actor-prop-foundation*
*Completed: 2026-04-26*
