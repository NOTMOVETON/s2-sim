---
phase: 02-actor-prop-foundation
plan: 05
subsystem: plugins
tags: [grabber, prop, attach, detach, interaction, eventbus, tdd, integration-test, c++17]

# Dependency graph
requires:
  - phase: 02-actor-prop-foundation
    plan: 01
    provides: "IActorBehavior, Actor struct (behavior+plugins), Prop struct (attachment), WorldQuery (find_nearest_movable_prop)"
  - phase: 02-actor-prop-foundation
    plan: 02
    provides: "SimEngine phase2_actors, phase6_attachments, cmd::Interact/AttachObject/DetachObject handlers"
  - phase: 02-actor-prop-foundation
    plan: 03
    provides: "DoorBehavior FSM, behaviors_registry"
  - phase: 02-actor-prop-foundation
    plan: 04
    provides: "SignalListenerBase, wire controllers, plugins_registry pattern"
provides:
  - "GrabberPlugin: INTERACTION плагин агента для захвата пропов (grab/release через handle_input)"
  - "GrabberPlugin: EventBus события GrabAttempt/GrabSucceeded/GrabFailed"
  - "GrabberPlugin: manipulation_locked contribution в SharedState агента"
  - "Интеграционные тесты DoorBehavior через SimEngine (3 теста)"
  - "Интеграционные тесты AttachObject/DetachObject через SimEngine (6 тестов)"
affects: [03-actor-ecosystem, 04-perception-system]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "GrabberPlugin: proximity-based grab через WorldQuery.find_nearest_movable_prop -> cmd::AttachObject"
    - "Интеграционный тест через SimEngine: push_command -> step -> проверка state/events"
    - "TestWorldQuery: мок WorldQuery с настраиваемым find_nearest_movable_prop для unit-тестов"

key-files:
  created:
    - workspace/s2_plugins/include/s2/plugins/grabber_plugin.hpp
    - workspace/s2_plugins/src/plugins/grabber_plugin.cpp
    - workspace/s2_plugins/tests/test_grabber_plugin.cpp
    - workspace/s2_core/tests/test_door_behavior.cpp
    - workspace/s2_core/tests/test_grab_attach.cpp
  modified:
    - workspace/s2_plugins/src/plugins_registry.cpp
    - workspace/s2_plugins/CMakeLists.txt
    - workspace/s2_core/CMakeLists.txt

key-decisions:
  - "manipulation_locked через add_lock() в SharedState — использует существующий LockContribution паттерн (не отдельный struct)"
  - "DoorBehavior тест: первый step() перед Interact — bus_ устанавливается в update(), on_interact до первого update() не публикует события"
  - "SimEngine в тестах: unique_ptr<SimEngine> для DoorBehavior тестов (mutex/atomic не movable), прямая переменная для GrabAttach (не возвращается из функции)"

patterns-established:
  - "GrabberPlugin: handle_input(json) -> grab_requested_ flag -> update() checks flag -> WorldQuery -> AttachObject/DetachObject"
  - "TestWorldQuery mock: наследник WorldQuery с настраиваемым результатом find_nearest_movable_prop"
  - "Integration test pattern: SimEngine + push_command + step + bus().subscribe + проверка мира"

requirements-completed: [PROP-03, ACTR-01, ACTR-02, ACTR-06, PROP-01, PROP-02]

# Metrics
duration: 10min
completed: 2026-04-26
---

# Phase 2 Plan 05: GrabberPlugin + Integration Tests Summary

**GrabberPlugin (grab/release через WorldQuery + AttachObject/DetachObject) с TDD + интеграционные тесты DoorBehavior FSM и attachment механики через SimEngine**

## Performance

- **Duration:** 10 min
- **Started:** 2026-04-26T19:46:11Z
- **Completed:** 2026-04-26T19:56:31Z
- **Tasks:** 2 (Task 1 TDD: RED + GREEN)
- **Files modified:** 8

## Accomplishments
- GrabberPlugin: grab через find_nearest_movable_prop -> AttachObject + GrabSucceeded/GrabFailed события
- GrabberPlugin: release -> DetachObject, manipulation_locked contribution через add_lock()
- GrabberPlugin: from_config (interaction_distance, grab_link), to_json, inputs_schema, T-02-14 JSON protection
- 13 unit-тестов GrabberPlugin (TDD RED 7 failures -> GREEN 0 failures)
- 3 интеграционных теста DoorBehavior (OpenViaInteract, ActorStateChanged, CloseAfterOpen)
- 6 интеграционных тестов AttachObject/DetachObject (attach/detach/events/drop_pose/phase6_follow)
- Phase 2 полностью завершена: все 5 планов выполнены

## Task Commits

Each task was committed atomically:

1. **Task 1 RED: GrabberPlugin failing tests** - `330b1df` (test)
2. **Task 1 GREEN: GrabberPlugin implementation** - `f524555` (feat)
3. **Task 2: Integration tests DoorBehavior + GrabAttach** - `77ca9a1` (feat)

## Files Created/Modified
- `workspace/s2_plugins/include/s2/plugins/grabber_plugin.hpp` - GrabberPlugin declaration: INTERACTION role, grab/release/manipulation_locked
- `workspace/s2_plugins/src/plugins/grabber_plugin.cpp` - GrabberPlugin: update (grab/release), from_config, handle_input, to_json, inputs_schema
- `workspace/s2_plugins/tests/test_grabber_plugin.cpp` - 13 unit-тестов: grab/release/events/lock/reset/json/schema
- `workspace/s2_plugins/src/plugins_registry.cpp` - Добавлена регистрация grabber
- `workspace/s2_plugins/CMakeLists.txt` - Добавлены grabber_plugin.cpp и test target
- `workspace/s2_core/tests/test_door_behavior.cpp` - 3 интеграционных теста через SimEngine
- `workspace/s2_core/tests/test_grab_attach.cpp` - 6 интеграционных тестов через SimEngine
- `workspace/s2_core/CMakeLists.txt` - Добавлены test_door_behavior.cpp и test_grab_attach.cpp

## Decisions Made
- manipulation_locked через add_lock(true, "grabber") в SharedState агента — использует существующий LockContribution паттерн. Не нужен отдельный struct — LockContribution уже содержит locked+source, resolve() вычисляет OR всех lock contributions.
- DoorBehavior integration test: первый step() перед cmd::Interact для инициализации bus_ в DoorBehavior. По дизайну (D-07 из Plan 02-03), bus_ устанавливается в update(), on_interact до первого update() не публикует ActorStateChanged.
- SimEngine в тестах DoorBehavior: unique_ptr<SimEngine> вместо прямого возврата — SimEngine содержит std::mutex и std::atomic<bool>, которые запрещают copy и move.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] DoorBehavior тест: bus_ nullptr при on_interact до первого update**
- **Found during:** Task 2 (test_door_behavior.cpp PublishesActorStateChanged)
- **Issue:** cmd::Interact обрабатывается в phase0 (до phase2), on_interact вызывается до первого update(), bus_ = nullptr, ActorStateChanged не публикуется
- **Fix:** Добавлен step(1) перед push_command(Interact) для инициализации bus_ в DoorBehavior
- **Files modified:** workspace/s2_core/tests/test_door_behavior.cpp
- **Verification:** Тест проходит, событие публикуется
- **Committed in:** 77ca9a1

**2. [Rule 3 - Blocking] SimEngine non-movable в тестах DoorBehavior**
- **Found during:** Task 2 (test_door_behavior.cpp компиляция)
- **Issue:** SimEngine содержит std::mutex и std::atomic<bool> — не является movable, хелпер make_engine_with_door не может вернуть по значению
- **Fix:** Заменено на unique_ptr<SimEngine> в хелпере и вызовах
- **Files modified:** workspace/s2_core/tests/test_door_behavior.cpp
- **Verification:** Компиляция и все тесты проходят
- **Committed in:** 77ca9a1

---

**Total deviations:** 2 auto-fixed (1 bug, 1 blocking)
**Impact on plan:** Тестовый код скорректирован для совместимости с SimEngine API. Продакшн-код реализован по плану без отклонений.

## TDD Gate Compliance

- [x] RED gate: `330b1df` (test commit — 7/13 failures)
- [x] GREEN gate: `f524555` (feat commit — 0 failures)
- [ ] REFACTOR gate: не требовался (код чистый, дублирования нет)

## Issues Encountered
None — сборка и тесты прошли без проблем (после исправления deviation 1 и 2).

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- Phase 2 полностью завершена: IActorBehavior, DoorBehavior, wire-контроллеры, GrabberPlugin, все тесты
- Phase 3 (Actor Ecosystem) может строить PedestrianBehavior, ConveyorActor, ElevatorBehavior на инфраструктуре Phase 2
- GrabberPlugin демонстрирует паттерн INTERACTION плагина с WorldQuery + AttachObject
- Интеграционные тесты дают образец для будущих поведений акторов

## Self-Check: PASSED

- [x] workspace/s2_plugins/include/s2/plugins/grabber_plugin.hpp -- FOUND
- [x] workspace/s2_plugins/src/plugins/grabber_plugin.cpp -- FOUND
- [x] workspace/s2_plugins/tests/test_grabber_plugin.cpp -- FOUND
- [x] workspace/s2_core/tests/test_door_behavior.cpp -- FOUND
- [x] workspace/s2_core/tests/test_grab_attach.cpp -- FOUND
- [x] 02-05-SUMMARY.md -- FOUND
- [x] Commit 330b1df -- FOUND
- [x] Commit f524555 -- FOUND
- [x] Commit 77ca9a1 -- FOUND

---
*Phase: 02-actor-prop-foundation*
*Completed: 2026-04-26*
