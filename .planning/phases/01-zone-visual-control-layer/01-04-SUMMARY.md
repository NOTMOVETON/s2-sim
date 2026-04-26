---
phase: 01-zone-visual-control-layer
plan: 04
subsystem: s2_core
tags: [zone, kernel-command, spawn-zone, despawn-zone, toggle-zone, spawn-triggers, timer, event, state-change, tdd]
dependency_graph:
  requires:
    - "01-01: Zone struct fields (DetectionMode, ZoneLifecycle, SelfDestructPolicy, strength, attached_to_entity_id)"
    - "01-03: ZoneSystem remove_zone(), toggle_zone_with_events(), add_zone()"
  provides:
    - "SimEngine обработчики SpawnZone/DespawnZone/ToggleZone в apply_kernel_command()"
    - "ZoneSpawnSystem с тремя типами триггеров: timer, event, state_change"
    - "next_zone_id_ счётчик для auto-generated ZoneId"
  affects:
    - "workspace/s2_core/include/s2/sim_engine.hpp"
    - "workspace/s2_core/include/s2/zone_spawn_system.hpp"
    - "workspace/s2_core/src/zone_spawn_system.cpp"
tech_stack:
  added: []
  patterns:
    - "TDD RED/GREEN: тесты пишутся до реализации, реализация проходит тесты"
    - "ZoneSpawnSystem подписывается на EventBus в init() — event dispatch в sim_thread"
    - "SpawnZone создаёт Zone из cmd полей, attached_to конвертирует EntityId в string"
key_files:
  created:
    - workspace/s2_core/include/s2/zone_spawn_system.hpp
    - workspace/s2_core/src/zone_spawn_system.cpp
    - workspace/s2_core/tests/test_zone_commands.cpp
    - workspace/s2_core/tests/test_zone_spawn_triggers.cpp
  modified:
    - workspace/s2_core/include/s2/sim_engine.hpp
    - workspace/s2_core/CMakeLists.txt
key_decisions:
  - "SpawnZone.attached_to (EntityId uint32_t) конвертируется в string через std::to_string для attached_to_entity_id"
  - "ZoneSpawnSystem подписывается на 6 типов EventBus событий: ZoneEntered, ZoneExited, SignalActivated, GrabSucceeded, GrabFailed, ActorStateChanged"
  - "StateChangeTrigger использует ActorId (uint32_t) вместо string entity_id — соответствует event::ActorStateChanged.actor"
  - "Timer trigger одноразовый (fired=true после срабатывания) — без cooldown в Phase 1"
patterns_established:
  - "ZoneSpawnSystem: отдельный класс-система, init(bus, queue, time) + tick(time) + add_template(tmpl, time)"
  - "Event trigger: подписка на конкретные типы событий с string-matching по event_type + source_entity"
requirements_completed: [ZONE-05, ZONE-08, ZONE-10]
metrics:
  duration: "6 мин"
  completed_date: "2026-04-26"
  tasks_completed: 2
  tasks_total: 2
  files_modified: 2
  files_created: 4
---

# Phase 01 Plan 04: Zone KernelCommands + ZoneSpawnSystem Summary

**SpawnZone/DespawnZone/ToggleZone обработчики в SimEngine + ZoneSpawnSystem с тремя типами триггеров (timer/event/state_change), 15 TDD-тестов, все 440 тестов проходят.**

## Performance

- **Duration:** 6 мин
- **Started:** 2026-04-26T15:46:45Z
- **Completed:** 2026-04-26T15:53:33Z
- **Tasks:** 2/2
- **Files modified:** 6

## Accomplishments
- SimEngine обрабатывает SpawnZone (создание зоны из cmd), DespawnZone (удаление с ZoneExited), ToggleZone (включение/выключение с enter/exit событиями)
- ZoneSpawnSystem реализован как отдельный класс с init/tick/add_template/clear API
- Три типа триггеров: timer (одноразовый по sim_time), event (ZoneEntered/ZoneExited/SignalActivated/Grab*), state_change (ActorStateChanged)
- 15 новых тестов (7 ZoneCommands + 8 ZoneSpawnTriggers) все проходят
- ZONE-10 покрыт через SpawnZone{attached_to: prop_id} — никакого специального кода

## Task Commits

1. **Задача 1: SpawnZone/DespawnZone/ToggleZone обработчики** — `8918c45` (feat)
2. **Задача 2 RED: Тесты ZoneSpawnSystem** — `426cbba` (test)
3. **Задача 2 GREEN: Реализация ZoneSpawnSystem** — `0a8a114` (feat)

## Files Created/Modified
- `workspace/s2_core/include/s2/sim_engine.hpp` — обработчики SpawnZone/DespawnZone/ToggleZone в apply_kernel_command(), поле next_zone_id_
- `workspace/s2_core/include/s2/zone_spawn_system.hpp` — класс ZoneSpawnSystem с ZoneTemplate, TimerTrigger, EventTrigger, StateChangeTrigger
- `workspace/s2_core/src/zone_spawn_system.cpp` — реализация init() с EventBus подписками, tick() для timer, check_event_triggers(), check_state_change_triggers()
- `workspace/s2_core/tests/test_zone_commands.cpp` — 7 тестов: SpawnZone (id_hint, auto-id, attached_to, unknown effect), DespawnZone, ToggleZone (disable, exit events)
- `workspace/s2_core/tests/test_zone_spawn_triggers.cpp` — 8 тестов: timer (2), event (4), state_change (1), clear (1)
- `workspace/s2_core/CMakeLists.txt` — добавлены zone_spawn_system.cpp, test_zone_commands.cpp, test_zone_spawn_triggers.cpp

## Decisions Made
- SpawnZone.attached_to (EntityId = uint32_t) конвертируется в string через std::to_string() для Zone.attached_to_entity_id — соответствует паттерну update_owned_zones_positions() из Plan 03
- StateChangeTrigger использует ActorId (uint32_t) вместо string — точно соответствует event::ActorStateChanged.actor
- ZoneSpawnSystem подписывается на 6 типов событий (ZoneEntered, ZoneExited, SignalActivated, GrabSucceeded, GrabFailed, ActorStateChanged) для максимального покрытия event-триггеров
- Timer trigger одноразовый (fired=true) — T-01-09 документирует отсутствие cooldown как ограничение Phase 1

## Deviations from Plan

None — план выполнен точно как написан.

## Issues Encountered

None.

## User Setup Required

None — нет конфигурации внешних сервисов.

## Known Stubs

None. Все методы полностью реализованы.

## Threat Flags

None. Митигации из threat model:
- T-01-08 (attached_to несуществующий id): zone.attached_to_entity_id заполняется, но если агент не найден в update_owned_zones_positions — молча игнорируется (реализовано в Plan 03)
- T-01-09 (event trigger бесконечный цикл): документировано как ограничение Phase 1 — нет cooldown/max_spawns
- T-01-10 (ZoneId injection через id_hint): ZoneId — std::string, используется как ключ в map, не исполняется

## TDD Gate Compliance

Git log подтверждает TDD gate sequence для Task 2:
1. `426cbba` — test(01-04): RED gate (failing tests) — 7 из 8 тестов падают
2. `0a8a114` — feat(01-04): GREEN gate (implementation) — все 8 тестов проходят

Task 1 также следует TDD паттерну (тесты + реализация в одном коммите, т.к. план описывает их вместе).

## Next Phase Readiness
- Zone KernelCommands полностью работают — SpawnZone/DespawnZone/ToggleZone обрабатываются SimEngine
- ZoneSpawnSystem готов к интеграции в SimEngine (zone_spawn_system_ поле + init + tick в phase0)
- Интеграция ZoneSpawnSystem в SimEngine (YAML парсинг zone_templates, вызов tick) — будущие планы
- ZONE-10 (zone movement через invisible prop) покрыт: SpawnZone{attached_to: prop_id} работает через ZONE-09 механизм

---
*Phase: 01-zone-visual-control-layer*
*Completed: 2026-04-26*
