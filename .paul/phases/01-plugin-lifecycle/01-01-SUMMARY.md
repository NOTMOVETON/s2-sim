---
phase: 01-plugin-lifecycle
plan: 01
subsystem: plugin-system
tags: [lifecycle, on_reset, role, capabilities, IAgentPlugin]

requires: []
provides:
  - PluginRole enum + role() метод во всех плагинах
  - on_reset() в интерфейсе и во всех 11 плагинах
  - on_spawn/on_despawn/on_scene_load в интерфейсе (no-op дефолты)
  - provided_capabilities() — автодобавление в agent.capabilities при init
  - SimEngine::reset() вызывает plugin->on_reset()
affects: [02-unified-entity-model, 06-effect-plugin, 08-per-agent-transport]

tech-stack:
  added: []
  patterns:
    - PluginRole enum для классификации плагинов
    - provided_capabilities() — декларативная регистрация capabilities без YAML дублирования
    - Agent* захват в initialize() для доступа в on_reset() (Battery паттерн)

key-files:
  created:
    - workspace/s2_core/tests/test_plugin_lifecycle.cpp
  modified:
    - workspace/s2_core/include/s2/plugin_base.hpp
    - workspace/s2_core/include/s2/sim_engine.hpp
    - workspace/s2_transport/src/sim_transport_bridge.cpp
    - workspace/s2_plugins/include/s2/plugins/diff_drive.hpp
    - workspace/s2_plugins/include/s2/plugins/battery.hpp
    - workspace/s2_plugins/include/s2/plugins/gnss.hpp
    - workspace/s2_plugins/include/s2/plugins/imu.hpp
    - workspace/s2_plugins/include/s2/plugins/lidar.hpp
    - workspace/s2_plugins/include/s2/plugins/color.hpp
    - workspace/s2_plugins/include/s2/plugins/gravity.hpp
    - workspace/s2_plugins/include/s2/plugins/joint_vel.hpp
    - workspace/s2_plugins/include/s2/plugins/path_display.hpp
    - workspace/s2_plugins/include/s2/plugins/topic_display.hpp
    - workspace/s2_plugins/include/s2/plugins/trajectory_recorder.hpp

key-decisions:
  - "on_spawn/on_despawn принимают Agent& (не Entity&) — Phase 2 обновит сигнатуру"
  - "Battery захватывает Agent* в initialize() для доступа к BatteryComponent в on_reset()"
  - "GNSS сбрасывает rng_.seed(42) — детерминизм после reset"

patterns-established:
  - "Плагины сбрасывают только runtime-поля (seq_, timer_, кеши). Config-поля не трогают."
  - "on_reset() не принимает аргументов — если нужен агент, захватить в initialize()"

duration: ~2ч
started: 2026-04-27T00:00:00Z
completed: 2026-04-28T00:00:00Z
---

# Phase 1 Plan 01: Plugin Lifecycle Summary

**Добавлен полный lifecycle-контракт IAgentPlugin: PluginRole, on_reset(), on_spawn/despawn/scene_load, provided_capabilities() — исправлены баги сброса состояния в DiffDrive и Battery.**

## Performance

| Metric | Value |
|--------|-------|
| Duration | ~2ч |
| Tasks | 3/3 completed |
| Files modified | 15 |
| Tests added | 4 новых теста |

## Acceptance Criteria Results

| Criterion | Status | Notes |
|-----------|--------|-------|
| AC-1: Полный lifecycle в интерфейсе | Pass | PluginRole enum + 6 новых методов в plugin_base.hpp |
| AC-2: on_reset() сбрасывает DiffDrive и Battery | Pass | Подтверждено тестами PluginLifecycle |
| AC-3: SimEngine::reset() сбрасывает плагины | Pass | reset() итерирует agent.plugins → plugin->on_reset() |
| AC-4: provided_capabilities() → agent.capabilities | Pass | SimTransportBridge после initialize() вставляет caps |
| AC-5: role() правильная роль | Pass | DiffDrive→actuation, Battery→resource, Lidar/GNSS/IMU→sensor |

## Accomplishments

- Интерфейс IAgentPlugin получил полный lifecycle без поломки существующих плагинов (все методы с default no-op)
- Баг на reset: DiffDrive больше не держит external_velocity_ после сброса симуляции
- Баг на reset: Battery восстанавливает BatteryComponent.level до initial_level_ после сброса
- provided_capabilities() устраняет необходимость дублировать `diff_drive` capability в YAML конфигах

## Files Created/Modified

| File | Change | Purpose |
|------|--------|---------|
| `plugin_base.hpp` | Modified | PluginRole enum + on_reset/on_spawn/on_despawn/on_scene_load/role/provided_capabilities |
| `sim_engine.hpp` | Modified | reset() вызывает plugin->on_reset() для всех агентов |
| `sim_transport_bridge.cpp` | Modified | После initialize() добавляет provided_capabilities() в agent.capabilities |
| `diff_drive.hpp` | Modified | role=actuation, provided_capabilities={"diff_drive"}, on_reset |
| `battery.hpp` | Modified | role=resource, on_reset (с BatteryComponent reset через Agent*) |
| `gnss.hpp` | Modified | role=sensor, on_reset (seq/timer/coords/rng reset) |
| `imu.hpp` | Modified | role=sensor, on_reset (seq/timer/gyro/yaw) |
| `lidar.hpp` | Modified | role=sensor, on_reset (seq/timer/scan_points clear) |
| `color.hpp` | Modified | on_reset (timer=0, update() автовосстанавливает цвет) |
| `gravity.hpp` | Modified | on_reset (fall_velocity_=0, slide_velocity_=Zero) |
| `joint_vel.hpp` | Modified | on_reset (target_vel=0 для всех маппингов) |
| `path_display.hpp` | Modified | on_reset (points_.clear() под mutex) |
| `topic_display.hpp` | Modified | on_reset (raw_.clear() под mutex) |
| `trajectory_recorder.hpp` | Modified | on_reset (timer=0, points_.clear()) |
| `test_plugin_lifecycle.cpp` | Created | 4 теста: DiffDrive reset, Battery reset, role(), provided_caps |

## Decisions Made

| Decision | Rationale | Impact |
|----------|-----------|--------|
| on_spawn/on_despawn принимают Agent& | Entity ещё не существует (Phase 2). Минимальный diff сейчас. | Phase 2 обновит сигнатуры при введении Entity |
| Battery хранит Agent* из initialize() | on_reset() не принимает аргументов, но нужен доступ к BatteryComponent в SharedState | Паттерн для других плагинов с resource в SharedState |
| GNSS сбрасывает rng_.seed(42) | После reset симуляция должна быть детерминирована | GNSS даёт одинаковые шумы при повторном прогоне |
| Color: только timer_=0 в on_reset() | update() автовосстанавливает цвет когда timer≤0 — дублирования нет | Чище чем хранить Agent* только ради восстановления цвета |

## Deviations from Plan

**None** — план выполнен точно. Единственная адаптация: в Battery использован `Agent*` захват (предусмотрен планом как опция).

## Issues Encountered

| Issue | Resolution |
|-------|------------|
| docker compose --project-directory не работал | Использован `docker compose -f docker/docker-compose.yml` и `docker_retry.sh` |

## Next Phase Readiness

**Ready:**
- Все плагины имеют корректный lifecycle — Phase 2 (Unified Entity) может безопасно менять сигнатуры методов
- PluginRole enum готов для enforcement в Phase 2 (максимум один actuation-плагин)
- provided_capabilities() механизм готов — Phase 2 использует его при инициализации Entity

**Concerns:**
- on_spawn/on_despawn принимают Agent& — Phase 2 ДОЛЖНА обновить сигнатуры на Entity& при введении нового типа

**Blockers:** None

---
*Phase: 01-plugin-lifecycle, Plan: 01*
*Completed: 2026-04-28*
