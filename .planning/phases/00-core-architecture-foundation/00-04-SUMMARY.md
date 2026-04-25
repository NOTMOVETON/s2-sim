---
phase: "00-core-architecture-foundation"
plan: "04"
subsystem: "s2_core"
tags: ["tick-lifecycle", "8-phases", "command-queue", "mutex", "tdd", "refactoring"]

dependency_graph:
  requires:
    - "KernelCommand variant (Plan 03) — kernel_command.hpp с 16 командами"
    - "PluginContext (Plan 02) — структура с WorldQuery, EventBus, KernelCommandQueue"
    - "PluginRole enum (Plan 02) — SENSOR, INTERACTION, RESOURCE, ACTUATION, UTILITY"
    - "WorldQuery базовый класс (Plan 02) — для NullWorldQuery"
  provides:
    - "SimEngine::tick() → phase0..phase8 именованные методы (D-18)"
    - "command_queue_ = std::vector<KernelCommand> + std::mutex (D-05)"
    - "push_command(KernelCommand) — публичный потокобезопасный метод"
    - "NullWorldQuery — вложенный private класс в SimEngine"
    - "phase3_agents(): SENSOR и INTERACTION плагины пропускаются (D-19)"
    - "phase4_sensors(): только PluginRole::SENSOR"
    - "phase5_interactions(): только PluginRole::INTERACTION"
    - "phase8_cleanup(): единственное место clear_contributions() (D-20)"
    - "test_tick_lifecycle.cpp: 3 теста порядка фаз"
  affects:
    - "workspace/s2_core/include/s2/sim_engine.hpp"
    - "workspace/s2_core/tests/test_tick_lifecycle.cpp"
    - "workspace/s2_core/CMakeLists.txt"

tech_stack:
  added:
    - "std::mutex + std::lock_guard для thread-safe command_queue_"
    - "KernelCommandQueue local_queue; queue.swap() под mutex — lock-free drain pattern"
    - "NullWorldQuery — вложенный private класс наследующий WorldQuery (все методы — заглушки)"
    - "PluginContext создаётся локально в каждой фазе (не хранится как поле)"
    - "apply_kernel_command<T> — шаблонный метод для std::visit dispatch"
  patterns:
    - "Фазовая декомпозиция tick(): 9 именованных методов phase0..phase8"
    - "Lock-swap drain: swap command_queue_ под mutex, обрабатывать local_queue без блокировки"
    - "Role-based dispatch: if role == SENSOR → skip in phase3, call in phase4"

key_files:
  created:
    - "workspace/s2_core/tests/test_tick_lifecycle.cpp"
  modified:
    - "workspace/s2_core/include/s2/sim_engine.hpp"
    - "workspace/s2_core/CMakeLists.txt"

decisions:
  - "NullWorldQuery — вложенный private класс в SimEngine (не отдельный файл): минимальная поверхность, не загрязняет namespace s2"
  - "PluginContext создаётся локально в каждой фазе (не хранится как поле): позволяет передавать разные tick_cmds буферы в phase3, phase4, phase5"
  - "Lock-swap drain в phase0: swap под mutex, потом обработка без блокировки — минимальное время удержания мьютекса"
  - "plugin_ctx_ как поле убран: было избыточным после введения per-phase локальных PluginContext"
  - "apply_kernel_command<SetPose> молча игнорирует неизвестный id (нет panic) — T-00-11"
  - "phase5_interactions() реализован для INTERACTION роли (план требовал только phase4 и phase8, но INTERACTION симметрично SENSOR)"

metrics:
  duration: "15m"
  completed_date: "2026-04-25"
  tasks_completed: 1
  tasks_total: 1
  files_created: 1
  files_modified: 2
---

# Phase 00 Plan 04: SimEngine 8-Phase Tick Lifecycle Summary

**One-liner:** Рефакторинг SimEngine::tick() в 9 явных именованных фаз с command_queue_ под mutex, NullWorldQuery и гарантированным порядком: сенсоры строго в Phase 4, clear_contributions только в Phase 8

## Tasks Completed

| Task | Name | Commit | Files |
|------|------|--------|-------|
| 1 (RED) | Тесты 8-фазного lifecycle | 53f00e4 | test_tick_lifecycle.cpp, CMakeLists.txt |
| 1 (GREEN) | SimEngine tick рефакторинг — 8 фаз + command_queue_ | 9c20dde | sim_engine.hpp |

## What Was Built

### Task 1: SimEngine tick рефакторинг

Изменён `workspace/s2_core/include/s2/sim_engine.hpp`:

**Новый публичный метод:**
```cpp
void push_command(KernelCommand cmd)
{
  std::lock_guard<std::mutex> lock(command_queue_mutex_);
  command_queue_.push_back(std::move(cmd));
}
```

**Новые private поля:**
- `KernelCommandQueue command_queue_` — очередь команд (HTTP-тред + плагины)
- `std::mutex command_queue_mutex_` — защита от data race (T-00-09)
- `NullWorldQuery null_world_query_` — заглушка WorldQuery (Plan 05 заменит)

**Убраны поля:**
- `plugin_ctx_` (PluginContext) — теперь создаётся локально в каждой фазе
- `plugin_cmds_` (KernelCommandQueue) — теперь локальные `tick_cmds` в каждой фазе
- `plugin_bus_` остался как `EventBus plugin_bus_` но перемещён к другим полям

**Вложенный класс:**
```cpp
class NullWorldQuery : public WorldQuery {};  // все методы WorldQuery — заглушки
```

**tick() теперь:**
```cpp
void tick()
{
  sim_time_ += dt_;
  phase0_kernel_commands();
  phase1_transport_input();
  phase2_actors();
  phase3_agents();
  phase4_sensors();
  phase5_interactions();
  phase6_attachments();
  phase7_snapshot_publish();
  phase8_cleanup();
}
```

**Ключевые гарантии:**

- Phase 0 drain использует lock-swap pattern: swap под mutex → обработка без блокировки
- Phase 3 пропускает SENSOR и INTERACTION плагины (только ACTUATION/RESOURCE/UTILITY)
- Phase 4 вызывает ТОЛЬКО PluginRole::SENSOR
- Phase 5 вызывает ТОЛЬКО PluginRole::INTERACTION
- Phase 8: единственное место вызова `agent.state.clear_contributions()`

**Безопасность угроз (Threat Model):**
- T-00-09: mutex защищает push_command() vs phase0 drain
- T-00-11: apply_kernel_command<SetPose> молча игнорирует неизвестный id

Создан `workspace/s2_core/tests/test_tick_lifecycle.cpp`:
- `SensorCalledInPhase4`: проверяет что SENSOR плагин вызывается в Phase 4
- `PushCommandSetPoseApplied`: SetPose применяется после push_command + step(1)
- `ClearContributionsOnlyInPhase8`: SENSOR видит contributions из Phase 3 (не очищены до Phase 4)

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 2 - Missing critical] phase5_interactions() добавлен не только в заглушку**

- **Found during:** Task 1, анализ плана
- **Issue:** План упоминал phase5 как заглушку ("пусто"), но INTERACTION роль уже существует в PluginRole. Симметрично phase4_sensors(), phase5 должен вызывать update() для INTERACTION плагинов.
- **Fix:** phase5_interactions() реализован симметрично phase4_sensors() — вызывает INTERACTION плагины с PluginContext.
- **Files modified:** workspace/s2_core/include/s2/sim_engine.hpp
- **Commit:** 9c20dde

**2. [Rule 1 - Bug] plugin_ctx_ поле убрано — создаётся локально в каждой фазе**

- **Found during:** Task 1, реализация
- **Issue:** Старый код хранил `PluginContext plugin_ctx_` как поле SimEngine, инициализированное при конструировании. При новой архитектуре каждая фаза создаёт свой `tick_cmds` буфер и передаёт его в PluginContext. Общий plugin_ctx_ не мог иметь разные tick_cmds для разных фаз.
- **Fix:** plugin_ctx_ удалён как поле; PluginContext создаётся локально в каждой из phase3/phase4/phase5 с локальным tick_cmds буфером.
- **Files modified:** workspace/s2_core/include/s2/sim_engine.hpp
- **Commit:** 9c20dde

## Verification Results

```
100% tests passed, 0 tests failed out of 2

  s2_core_tests:  Passed 0.96 sec
  s2_editor_tests: Passed 0.01 sec

Total Test time: 0.97 sec
```

Все done-критерии выполнены:
- `phase0_kernel_commands` найден в sim_engine.hpp (строки 499, 521)
- `phase4_sensors` найден (строки 503, 760)
- `phase8_cleanup` найден (строки 507, 848)
- `command_queue_` найден как поле (строки 156-157)
- `push_command` найден как публичный метод (строка 154)
- `clear_contributions` только в phase8_cleanup (строка 851)
- test_tick_lifecycle.cpp: 3 теста, все зелёные
- Docker: все тесты проходят

## TDD Gate Compliance

- RED: `test_tick_lifecycle.cpp` создан с 3 тестами; `PushCommandSetPoseApplied` не компилировался (push_command отсутствовал) — RED gate подтверждён компиляционной ошибкой
- GREEN: sim_engine.hpp рефакторинг, все тесты прошли
- REFACTOR: не потребовался (код чистый с первой итерации)

Коммиты:
- `53f00e4 test(00-04)`: RED gate
- `9c20dde feat(00-04)`: GREEN gate

## Known Stubs

- `phase1_transport_input()` — пусто (TODO Phase 5)
- `phase2_actors()` — пусто (TODO Phase 2 Actor Foundation)
- `phase6_attachments()` — пусто (TODO Phase 2 Prop Foundation)
- `NullWorldQuery` — все методы WorldQuery возвращают пустые результаты (Plan 05 добавит WorldQueryImpl)
- `apply_kernel_command<SpawnEntity>` — только лог + TODO (Plan 05+)
- `apply_kernel_command<DespawnEntity>` — только лог + TODO (Phase 6)
- `apply_kernel_command<AddPlugin/RemovePlugin/ConfigPlugin>` — DEFERRED Phase 5 (TRAN-07)

## Threat Flags

None. command_queue_ mutex и apply_kernel_command заглушки покрыты планом (T-00-09, T-00-11).

## Self-Check: PASSED
