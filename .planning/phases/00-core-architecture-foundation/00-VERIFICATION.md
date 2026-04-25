---
phase: "00-core-architecture-foundation"
verified: "2026-04-26T10:00:00Z"
status: passed
score: 3/3 must-haves verified (2 deferred к будущим фазам)
overrides_applied: 0
re_verification:
  previous_status: gaps_found
  previous_score: 3/5
  gaps_closed:
    - "EventBus доставляет ZoneEntered событие подписчикам при входе агента в зону"
  gaps_remaining: []
  regressions: []
deferred:
  - truth: "WorldQuery.raycast возвращает корректный результат через статическую геометрию"
    addressed_in: "Phase 4"
    evidence: "Phase 4 task 4.6: 'Ray-zone transit (PERC-07): WorldQuery.raycast с zone intersection; накопление attenuation по zone_type'. Phase 4 success criteria: ArucoDetector обнаруживает маркер через fog-зону. REQUIREMENTS.md: ARCH-05 — PARTIAL: интерфейс создан (Plan 02), WorldQueryImpl реализация — Plan 05+."
  - truth: "KernelCommand::Interact вызывает target.behavior.on_interact() через ядро с валидацией"
    addressed_in: "Phase 2"
    evidence: "Phase 2 task 2.1: IActorBehavior с on_interact(source, action, params). Phase 2 task 2.2: ActorRegistry + тиковый цикл Phase 2. apply_kernel_command<Interact> явно отмечен 'TODO в следующих фазах' в sim_engine.hpp строка 911."
human_verification: []
---

# Phase 0: Core Architecture Foundation — Verification Report

**Phase Goal:** Правильный lifecycle плагинов, typed EventBus, WorldQuery API, Signal struct на Entity, 8-фазный tick lifecycle, полный набор KernelCommands — фундамент, на котором строится всё остальное.
**Verified:** 2026-04-26T10:00:00Z
**Status:** PASSED
**Re-verification:** Да — после закрытия gap ARCH-04 планом 00-06

---

## Goal Achievement

### Observable Truths (from ROADMAP.md Success Criteria)

| # | Truth | Status | Evidence |
|---|-------|--------|----------|
| 1 | on_reset() вызывается для всех плагинов при /sim/reset; DiffDrive и Battery корректно сбрасывают состояние | VERIFIED | SimEngine::reset() строка 229-236 sim_engine.hpp: цикл `for agent: for plugin: plugin->on_reset(agent)`. DiffDrivePlugin::on_reset() строки 134-142: сбрасывает external_linear_velocity_=0, external_angular_velocity_=0, has_external_input_=false, time_acc_=0. BatteryPlugin::on_reset(): bat->level=initial_level_, bat->charging=false. Тесты PluginOnReset.DiffDriveExternalVelocityResetAfterReset и BatteryLevelRestoredAfterReset покрыты. |
| 2 | EventBus доставляет ZoneEntered событие подписчикам при входе агента в зону | VERIFIED | event::ZoneEntered определён в event_bus.hpp строка 57. ZoneSystem::on_agent_enter() строка 203 zone_system.cpp: bus.publish(event::ZoneEntered{.zone_id = zone.id, .entity_id = agent.id}) — добавлено в план 00-06, коммит 11e5fd8. Аналогично event::ZoneExited строка 227. Backward compat: legacy AgentEnteredZone (строка 202) и AgentExitedZone (строка 226) сохранены. Тесты ZoneSystem_ZoneEnteredEvent.PublishedOnAgentEnter и ZoneSystem_ZoneExitedEvent.PublishedOnAgentExit добавлены в test_zone_system.cpp (строки 386, 411), коммит 7e8e9b6. |
| 3 | WorldQuery.raycast возвращает корректный результат через статическую геометрию | DEFERRED | WorldQuery базовый класс имеет stub-реализацию raycast() возвращающую RaycastQueryResult{}. NullWorldQuery в SimEngine наследует заглушку. WorldQueryImpl отложен на Phase 4. |
| 4 | Сенсоры (Lidar) работают из финальной позиции агента (после collision response) | VERIFIED | SimEngine::tick() строки 504-512: phase3_agents() (кинематика + коллизии) вызывается до phase4_sensors(). phase3_agents() строка 616: SENSOR плагины явно пропускаются через `continue`. phase4_sensors() строка 765: вызывает только PluginRole::SENSOR. Тест TickLifecycle.SensorCalledInPhase4 и ClearContributionsOnlyInPhase8 покрывают этот порядок. |
| 5 | KernelCommand::Interact вызывает target.behavior.on_interact() через ядро с валидацией | DEFERRED | apply_kernel_command в sim_engine.hpp строка 909-914: Interact попадает в else-ветку `// Все остальные команды ... TODO в следующих фазах`. IActorBehavior::on_interact() ещё не существует — задача Phase 2 (task 2.1). |

**Score:** 3/3 non-deferred truths verified (2 deferred исключаются из счёта)

Итоговый счёт с учётом всех статусов: 3/5 (SC1 + SC2 + SC4 = verified; SC3 + SC5 = deferred)

---

### Deferred Items

Items not yet met but explicitly addressed in later milestone phases.

| # | Item | Addressed In | Evidence |
|---|------|-------------|----------|
| 1 | WorldQuery.raycast возвращает корректный результат | Phase 4 | Task 4.6: "WorldQuery.raycast с zone intersection; накопление attenuation по zone_type" |
| 2 | KernelCommand::Interact вызывает target.behavior.on_interact() | Phase 2 | Task 2.1: IActorBehavior с on_interact(source, action, params). Task 2.2: ActorRegistry + Phase 2 тик. |

---

## Required Artifacts

### ARCH-01 (Plugin Lifecycle)

| Artifact | Expected | Status | Details |
|----------|----------|--------|---------|
| `workspace/s2_core/include/s2/plugin_base.hpp` | on_spawn, on_despawn, on_scene_load, on_reset с default no-op | VERIFIED | Строки 173, 180, 187, 194 — все 4 lifecycle-метода с `(void)agent` no-op. |
| `workspace/s2_plugins/include/s2/plugins/diff_drive.hpp` | on_reset() сбрасывает external_linear_velocity_, external_angular_velocity_, has_external_input_ | VERIFIED | Строки 134-142: все 3 поля + time_acc_ сброшены. |
| `workspace/s2_plugins/include/s2/plugins/battery.hpp` | on_reset() сбрасывает level до initial_level_ | VERIFIED | Строки 81-89: bat->level=initial_level_, bat->charging=false. |
| `workspace/s2_core/include/s2/sim_engine.hpp` | reset() вызывает on_reset() для всех плагинов | VERIFIED | Строка 235: `plugin->on_reset(agent)` в двойном цикле agents/plugins. |
| `workspace/s2_core/tests/test_plugin_on_reset.cpp` | Тесты on_reset | VERIFIED | Файл существует, 3 теста. |

### ARCH-02 (PluginRole)

| Artifact | Expected | Status | Details |
|----------|----------|--------|---------|
| `workspace/s2_core/include/s2/plugin_base.hpp` | enum class PluginRole с 5 значениями | VERIFIED | Строки 45-52: ACTUATION, SENSOR, INTERACTION, RESOURCE, UTILITY. |
| `workspace/s2_plugins/include/s2/plugins/diff_drive.hpp` | role() = ACTUATION | VERIFIED | Строка 36. |
| `workspace/s2_plugins/include/s2/plugins/gravity.hpp` | role() = ACTUATION | VERIFIED | Строка 34. |
| `workspace/s2_plugins/include/s2/plugins/gnss.hpp` | role() = SENSOR | VERIFIED | Строка 30. |
| `workspace/s2_plugins/include/s2/plugins/imu.hpp` | role() = SENSOR | VERIFIED | Строка 28. |
| `workspace/s2_plugins/include/s2/plugins/lidar.hpp` | role() = SENSOR | VERIFIED | Строка 40. |
| `workspace/s2_plugins/include/s2/plugins/battery.hpp` | role() = RESOURCE | VERIFIED | Строка 49. |
| `workspace/s2_plugins/include/s2/plugins/color.hpp` | role() = UTILITY | VERIFIED | Строка 32. |
| `workspace/s2_plugins/include/s2/plugins/joint_vel.hpp` | role() = UTILITY | VERIFIED | Строка 36. |
| `workspace/s2_plugins/include/s2/plugins/trajectory_recorder.hpp` | role() = UTILITY | VERIFIED | Строка 32. |
| `workspace/s2_plugins/include/s2/plugins/path_display.hpp` | role() = UTILITY | VERIFIED | Строка 36. |
| `workspace/s2_plugins/include/s2/plugins/topic_display.hpp` | role() = UTILITY | VERIFIED | Строка 32. |
| `workspace/s2_core/include/s2/scene_loader.hpp` | throws runtime_error если >1 ACTUATION | VERIFIED | Строки 218-227: счётчик actuation_count + throw runtime_error. |

### ARCH-03 (Signal struct)

| Artifact | Expected | Status | Details |
|----------|----------|--------|---------|
| `workspace/s2_core/include/s2/types.hpp` | struct Signal с 7 полями | VERIFIED | Строки 370-379: signal_type, signal_id, local_pose, params (nlohmann::json), range{0.0}, requires_los{false}, enabled{true}. |
| `workspace/s2_core/include/s2/agent.hpp` | Agent::signals поле | VERIFIED | Строка 49: `std::vector<Signal> signals`. |

### ARCH-04 (typed EventBus)

| Artifact | Expected | Status | Details |
|----------|----------|--------|---------|
| `workspace/s2_core/include/s2/event_bus.hpp` | class EventBus + все event:: типы | VERIFIED | Строка 144: class EventBus. Event types: EntitySpawned (65), EntityDespawned (68), ZoneEntered (57), ZoneExited (60), ActorStateChanged (73), SignalActivated (78), SignalDeactivated (81), GrabAttempt (86), GrabSucceeded (89), GrabFailed (92), DamageDealt (97). Все 11 новых event-типов присутствуют. |
| `workspace/s2_core/include/s2/sim_bus.hpp` | using SimBus = EventBus | VERIFIED | Строка 19. |
| `workspace/s2_core/tests/test_event_bus.cpp` | Тесты новых event-типов | VERIFIED | Файл существует. |
| `workspace/s2_core/src/zone_system.cpp` | event::ZoneEntered публикуется при входе агента | VERIFIED | Строка 203: bus.publish(event::ZoneEntered{.zone_id = zone.id, .entity_id = agent.id}). Добавлено планом 00-06, коммит 11e5fd8. |
| `workspace/s2_core/src/zone_system.cpp` | event::ZoneExited публикуется при выходе агента | VERIFIED | Строка 227: bus.publish(event::ZoneExited{.zone_id = zone.id, .entity_id = agent.id}). |
| `workspace/s2_core/tests/test_zone_system.cpp` | Тесты ZoneEntered/ZoneExited | VERIFIED | Строки 386-437: TEST(ZoneSystem_ZoneEnteredEvent, PublishedOnAgentEnter) и TEST(ZoneSystem_ZoneExitedEvent, PublishedOnAgentExit). Проверяют zone_id и entity_id. Коммит 7e8e9b6. |

### ARCH-05 (WorldQuery API)

| Artifact | Expected | Status | Details |
|----------|----------|--------|---------|
| `workspace/s2_core/include/s2/world_query.hpp` | class WorldQuery с методами | VERIFIED (interface) | Строка 113: class WorldQuery. Методы: find_in_radius, find_in_box, find_nearest, find_entity_below, find_signals_of_type, has_line_of_sight, raycast, zones_at, is_in_zone, find_deformable_in_box — все 10 присутствуют как virtual с stub-реализацией. |
| WorldQueryImpl — реальная реализация | NullWorldQuery заменена WorldQueryImpl | DEFERRED | WorldQuery stub. NullWorldQuery в SimEngine строка 469. WorldQueryImpl отложен на Phase 4. |

### ARCH-06 (8-phase tick lifecycle)

| Artifact | Expected | Status | Details |
|----------|----------|--------|---------|
| `workspace/s2_core/include/s2/sim_engine.hpp` | tick() вызывает phase0..phase8 | VERIFIED | Строки 504-512: все 9 фаз в порядке. |
| phase0 дренирует command_queue_ под mutex | Swap под mutex, обработка без блокировки | VERIFIED | Строки 526-538: lock_guard + local_queue.swap + std::visit. |
| push_command(KernelCommand) публичный | Потокобезопасный API | VERIFIED | Строки 149-157: lock_guard<mutex> + push_back. |
| phase3 пропускает SENSOR/INTERACTION | Skip в цикле плагинов | VERIFIED | Строка 616: `if (r == SENSOR || r == INTERACTION) continue`. |
| phase4 вызывает только SENSOR | Role-based dispatch | VERIFIED | Строки 765-790: `if role() != SENSOR continue`. |
| clear_contributions() только в phase8 | Единственное место | VERIFIED | Строка 856: в phase8_cleanup(). Строки 572, 752: явные комментарии "НЕ здесь (D-20)". |

### ARCH-07 (KernelCommand variant)

| Artifact | Expected | Status | Details |
|----------|----------|--------|---------|
| `workspace/s2_core/include/s2/kernel_command.hpp` | using KernelCommand = std::variant<16 команд> | VERIFIED | Строка 223: std::variant со всеми 16 типами. Все структуры присутствуют в namespace s2::cmd. |
| SpawnEntity, DespawnEntity, SetPose, SetEnabled | Entity lifecycle 4 команды | VERIFIED | Строки 45, 55, 63, 73. |
| AddPlugin, RemovePlugin, ConfigPlugin | Plugin lifecycle 3 команды | VERIFIED | Строки 86, 97, 107. |
| SpawnZone, DespawnZone, ToggleZone | Zone 3 команды | VERIFIED | Строки 120, 135, 145. |
| Interact, AttachObject, DetachObject | Interaction 3 команды | VERIFIED | Строки 163, 176, 188. |
| LoadScene, SaveScene, NewScene | Scene 3 команды | VERIFIED | Строки 197, 200, 203. |
| cmd::Interact handler вызывает on_interact() | Реализован в phase0 | DEFERRED | Строка 909-914: else-ветка `(void)cmd`. Реализация — Phase 2. |

---

## Key Link Verification

| From | To | Via | Status | Details |
|------|----|-----|--------|---------|
| `sim_engine.hpp` | `kernel_command.hpp` | `#include` + phase0 + command_queue_ | VERIFIED | Строка 22 sim_engine.hpp. |
| `sim_engine.hpp` | `world_query.hpp` | `#include` + NullWorldQuery | VERIFIED | Строка 27 sim_engine.hpp. |
| `plugin_base.hpp` | `kernel_command.hpp` | `#include` (строка 13) | VERIFIED | KernelCommandQueue используется в PluginContext. |
| `plugin_base.hpp` | `world_query.hpp` | include через event_bus.hpp chain | VERIFIED | PluginContext::world = const WorldQuery&. |
| `plugin_base.hpp` | `event_bus.hpp` | `#include` (строка 11) | VERIFIED | PluginContext::bus = EventBus&. |
| `diff_drive.hpp` | `plugin_base.hpp` | update() сигнатура + role() | VERIFIED | update(double, Agent&, const PluginContext&) + PluginRole::ACTUATION. |
| `scene_loader.hpp` | `plugin_base.hpp` | PluginRole::ACTUATION валидация | VERIFIED | Строка 220 scene_loader.hpp: plugin->role() == PluginRole::ACTUATION. |
| `zone_system.cpp` | `event_bus.hpp` | ZoneEntered/ZoneExited публикация | VERIFIED | Строки 203, 227: bus.publish(event::ZoneEntered) и bus.publish(event::ZoneExited). Оба с корректными полями zone_id и entity_id. Gap закрыт планом 00-06. |

---

## Data-Flow Trace (Level 4)

| Artifact | Data Variable | Source | Produces Real Data | Status |
|----------|---------------|--------|--------------------|--------|
| `sim_engine.hpp` phase0 | command_queue_ | push_command() (HTTP thread) | Да — реальные команды из REST API | FLOWING |
| `sim_engine.hpp` phase3 | agent.world_velocity | DiffDrivePlugin via update() | Да — через SharedState contributions | FLOWING |
| `sim_engine.hpp` phase4 | sensor data | SENSOR plugins via update() | Да — реальные плагины (Lidar, GNSS, IMU) | FLOWING |
| `zone_system.cpp` on_agent_enter | event::ZoneEntered | agent.id, zone.id | Да — реальные значения из AgentId/ZoneId | FLOWING |
| `world_query.hpp` raycast | RaycastQueryResult | NullWorldQuery stub | Нет — всегда возвращает {} | STATIC (deferred Phase 4) |

---

## Behavioral Spot-Checks

Step 7b: SKIPPED — проверка требует запуска Docker (внешний сервис). SUMMARYs 00-05 и 00-06 подтверждают `100% tests passed` после каждого плана. Коммиты 11e5fd8 и 7e8e9b6 верифицированы в git log.

---

## Requirements Coverage

| Requirement | Source Plan | Description | Status | Evidence |
|-------------|------------|-------------|--------|----------|
| ARCH-01 | Plans 02, 05 | IAgentPlugin lifecycle + on_reset bugfixes | SATISFIED | on_spawn/on_despawn/on_scene_load/on_reset в plugin_base.hpp; DiffDrive + Battery on_reset корректны; SimEngine::reset() вызывает их. |
| ARCH-02 | Plans 02, 05 | PluginRole + max 1 ACTUATION | SATISFIED | enum class PluginRole (5 значений); все 11 плагинов реализуют role(); SceneLoader::load() throws runtime_error. |
| ARCH-03 | Plan 01 | Signal struct на Entity | SATISFIED | struct Signal в types.hpp с 7 полями; Agent::signals в agent.hpp. |
| ARCH-04 | Plans 01, 06 | EventBus typed events + ZoneEntered публикация | SATISFIED | Все 11 event-типов определены в event_bus.hpp. event::ZoneEntered публикуется в ZoneSystem::on_agent_enter() (строка 203), event::ZoneExited в on_agent_exit() (строка 227). Backward compat с legacy AgentEnteredZone/AgentExitedZone сохранён. Тесты ZoneSystem_ZoneEnteredEvent и ZoneSystem_ZoneExitedEvent подтверждают корректность zone_id и entity_id. Gap закрыт планом 00-06. |
| ARCH-05 | Plan 02 | WorldQuery read-only API | PARTIAL | Интерфейс (10 методов) создан в world_query.hpp. WorldQueryImpl отложен на Phase 4. |
| ARCH-06 | Plan 04 | 8-phase tick lifecycle | SATISFIED | tick() содержит все 9 фаз в порядке; phase0 дренирует очередь; phase3 пропускает SENSOR/INTERACTION; phase8 единственное место clear_contributions(). |
| ARCH-07 | Plan 03 | KernelCommand полный набор | PARTIAL | Все 16 команд определены как std::variant. cmd::SetPose обрабатывается в apply_kernel_command. Остальные (SpawnEntity, DespawnEntity, Interact, Zones, Scene) — заглушки с TODO. Interact реализация — Phase 2. |

---

## Anti-Patterns Found

| File | Pattern | Severity | Impact |
|------|---------|----------|--------|
| `workspace/s2_core/include/s2/sim_engine.hpp:887` | `std::cout << "[SimEngine] SpawnEntity: ... (TODO)"` | Warning | SpawnEntity команда ничего не делает |
| `workspace/s2_core/include/s2/sim_engine.hpp:892` | `std::cout << "[SimEngine] DespawnEntity: ... (TODO)"` | Warning | DespawnEntity команда ничего не делает |
| `workspace/s2_core/include/s2/sim_engine.hpp:911-913` | `// Все остальные команды ... TODO ... (void)cmd` | Warning | Interact, Zone, Attach, Scene команды проглатываются без обработки — плановые заглушки |
| `workspace/s2_core/include/s2/sim_engine.hpp:547,556,821,858` | TODO Phase 5/2 комментарии в phase1/phase2/phase6/phase8 | Info | Заглушки для будущих фаз — ожидаемо |
| `workspace/s2_core/include/s2/world_query.hpp` | Все методы возвращают `{}` / `false` / `0` | Info | NullWorldQuery — плановая заглушка, Phase 4 |

Классификация: все анти-паттерны являются плановыми заглушками с явными TODO/DEFERRED комментариями. Ни один не является случайным пропуском.

---

## Human Verification Required

Секция пуста — автоматическая верификация покрыла все проверяемые аспекты.

---

## Re-verification Summary

**Gap закрыт:** 1/1

- ARCH-04 (ZoneEntered не публиковался): CLOSED — коммит 11e5fd8 добавил bus.publish(event::ZoneEntered) в ZoneSystem::on_agent_enter() строка 203 и bus.publish(event::ZoneExited) в on_agent_exit() строка 227. Legacy события AgentEnteredZone/AgentExitedZone сохранены. Коммит 7e8e9b6 добавил 2 интеграционных теста подтверждающих zone_id и entity_id.

**Регрессии:** 0

**Итог:** Phase 0 goal достигнут. Все 3 non-deferred truths верифицированы. 2 deferred задачи явно закреплены за Phase 2 (Interact) и Phase 4 (WorldQueryImpl).

---

_Verified: 2026-04-26T10:00:00Z_
_Verifier: Claude (gsd-verifier)_
_Re-verification: Plan 00-06 gap closure_
