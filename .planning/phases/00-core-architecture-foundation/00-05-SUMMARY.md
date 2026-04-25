---
phase: "00-core-architecture-foundation"
plan: "05"
subsystem: "s2_plugins + s2_core"
tags: ["plugin-lifecycle", "on_reset", "diff_drive", "battery", "scene_loader", "sim_engine", "validation", "tdd"]

dependency_graph:
  requires:
    - "DiffDrivePlugin, BatteryPlugin с новой update() сигнатурой (Plan 02)"
    - "PluginRole enum в plugin_base.hpp (Plan 02)"
    - "SimEngine::reset() + restore_initial_states() (Plan 04)"
    - "IAgentPlugin::on_reset() с default-реализацией (Plan 02)"
  provides:
    - "DiffDrivePlugin::on_reset(): сброс external_linear_velocity_, external_angular_velocity_, has_external_input_, time_acc_ (D-13)"
    - "BatteryPlugin::on_reset(): восстановление BatteryComponent::level до initial_level_ (D-13)"
    - "SimEngine::reset(): вызов on_reset() для всех плагинов всех агентов (D-12)"
    - "SceneLoader::load(): валидация — throw runtime_error если ACTUATION плагинов > 1 (D-11)"
    - "test_plugin_on_reset.cpp: 3 теста, все зелёные"
  affects:
    - "workspace/s2_plugins/include/s2/plugins/diff_drive.hpp"
    - "workspace/s2_plugins/include/s2/plugins/battery.hpp"
    - "workspace/s2_core/include/s2/sim_engine.hpp"
    - "workspace/s2_core/include/s2/scene_loader.hpp"
    - "workspace/s2_core/tests/test_plugin_on_reset.cpp"

tech_stack:
  added: []
  patterns:
    - "on_reset() вызывается из SimEngine::reset() в цикле агент/плагин"
    - "SceneLoader считает ACTUATION плагины после создания всего списка плагинов агента"

key_files:
  created:
    - "workspace/s2_core/tests/test_plugin_on_reset.cpp"
  modified:
    - "workspace/s2_plugins/include/s2/plugins/diff_drive.hpp"
    - "workspace/s2_plugins/include/s2/plugins/battery.hpp"
    - "workspace/s2_core/include/s2/sim_engine.hpp"
    - "workspace/s2_core/include/s2/scene_loader.hpp"
    - "workspace/s2_core/CMakeLists.txt"

decisions:
  - "Тест DiffDriveExternalVelocityResetAfterReset требует engine.resume() до step(1) после reset() — иначе tick() возвращает раньше из-за paused_=true и тест проходит по неверной причине"
  - "SceneLoader валидирует ACTUATION после создания всего списка плагинов агента — позволяет собрать все плагины и посчитать их разом"
  - "BatteryPlugin::on_reset() сбрасывает charging=false — инвариант: после reset батарея не должна находиться в состоянии зарядки без зоны зарядки"

metrics:
  duration: "20m"
  completed_date: "2026-04-25"
  tasks_completed: 2
  tasks_total: 2
  files_created: 1
  files_modified: 5
---

# Phase 00 Plan 05: Plugin on_reset + SceneLoader ACTUATION Validation Summary

**One-liner:** DiffDrive и Battery on_reset() с фиксом багов D-13, SimEngine::reset() вызывает on_reset() для всех плагинов, SceneLoader бросает runtime_error при >1 ACTUATION плагина на агента

## Tasks Completed

| Task | Name | Commit | Files |
|------|------|--------|-------|
| 1 | on_reset() для DiffDrive и Battery | 327f9e6 | diff_drive.hpp, battery.hpp |
| 2 (RED) | Тесты on_reset и валидации ACTUATION | 303fdbf | test_plugin_on_reset.cpp, CMakeLists.txt |
| 2 (GREEN) | SimEngine::reset() + SceneLoader валидация | 57cbc5d | sim_engine.hpp, scene_loader.hpp |

## What Was Built

### Task 1: on_reset() для DiffDrivePlugin и BatteryPlugin

**DiffDrivePlugin** (`workspace/s2_plugins/include/s2/plugins/diff_drive.hpp`):
```cpp
void on_reset(Agent& agent) override
{
  (void)agent;
  // Сброс external команд — агент не должен продолжать движение после reset (D-13)
  external_linear_velocity_  = 0.0;
  external_angular_velocity_ = 0.0;
  has_external_input_        = false;
  time_acc_                  = 0.0;
}
```

**BatteryPlugin** (`workspace/s2_plugins/include/s2/plugins/battery.hpp`):
```cpp
void on_reset(Agent& agent) override
{
    auto* bat = agent.state.get<BatteryComponent>();
    if (bat) {
        bat->level    = initial_level_;
        bat->charging = false;
    }
}
```

### Task 2: SimEngine::reset() + SceneLoader ACTUATION validation (TDD)

**SimEngine::reset()** (`workspace/s2_core/include/s2/sim_engine.hpp`):
```cpp
void reset()
{
  restore_initial_states();
  // Вызвать on_reset() для всех плагинов всех агентов (D-12)
  for (auto& agent : world_.agents())
    for (auto& plugin : agent.plugins)
      plugin->on_reset(agent);
  sim_time_ = 0.0;
  paused_   = true;
}
```

**SceneLoader::load()** (`workspace/s2_core/include/s2/scene_loader.hpp`):
```cpp
// Валидация: не более одного ACTUATION-плагина на агента (D-11)
{
    int actuation_count = 0;
    for (const auto& plugin : agent.plugins)
        if (plugin->role() == PluginRole::ACTUATION)
            actuation_count++;
    if (actuation_count > 1) {
        throw std::runtime_error(
            "Агент '" + agent.name + "' имеет " +
            std::to_string(actuation_count) +
            " ACTUATION-плагина. Допустим только один.");
    }
}
```

**Тесты** (`workspace/s2_core/tests/test_plugin_on_reset.cpp`):
- `PluginOnReset.DiffDriveExternalVelocityResetAfterReset`: после reset + resume + step(1) агент не двигается
- `PluginOnReset.BatteryLevelRestoredAfterReset`: после reset уровень батареи = initial_level_ = 0.8
- `SceneLoaderValidation.SingleActuationPluginCountedCorrectly`: подсчёт ACTUATION плагинов корректен

## Verification Results

```
100% tests passed, 0 tests failed out of 2

  s2_core_tests:  Passed 0.96 sec (396 тестов из 58 наборов)
  s2_editor_tests: Passed 0.01 sec

Total Test time: 0.97 sec
```

Все done-критерии выполнены:
1. `void update(double dt, Agent& agent)` — 0 результатов (все обновлены в Plan 02)
2. `const PluginContext` — 13+ строк в s2_plugins/
3. `PluginRole::` — 11 записей (все 11 плагинов имеют role())
4. `on_reset` в diff_drive.hpp — строка 134
5. `external_linear_velocity_` в on_reset — строка 138
6. `on_reset` в battery.hpp — строка 81
7. `initial_level_` в on_reset battery — строка 86
8. `on_reset` в sim_engine.hpp reset() — строки 232, 235
9. `runtime_error` в scene_loader.hpp — строки 223, 243

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Тест DiffDriveExternalVelocityResetAfterReset требует engine.resume()**

- **Found during:** Task 2 (RED gate)
- **Issue:** После `engine.reset()`, `paused_ = true`. Вызов `engine.step(1)` ничего не делает — `tick()` возвращает рано из-за паузы. Тест проходил по неверной причине (velocity=0 от restore_initial_states, а не от on_reset).
- **Fix:** Добавлен `engine.resume()` до `step(1)` в тесте. Также добавлен `engine.resume()` перед первым `step(1)` в обоих тестах для единообразия.
- **Files modified:** workspace/s2_core/tests/test_plugin_on_reset.cpp
- **Commit:** 303fdbf

**2. [Rule 2 - Missing] BatteryPlugin::on_reset() сбрасывает charging=false**

- **Found during:** Task 1
- **Issue:** План (D-13) указывал только сброс level. Но состояние charging также должно сброситься — после reset батарея не находится в зоне зарядки, поэтому charging=false является корректным инвариантом.
- **Fix:** Добавлен `bat->charging = false;` в BatteryPlugin::on_reset().
- **Files modified:** workspace/s2_plugins/include/s2/plugins/battery.hpp
- **Commit:** 327f9e6

## TDD Gate Compliance

- RED: `test_plugin_on_reset.cpp` создан с 3 тестами; `DiffDriveExternalVelocityResetAfterReset` и `BatteryLevelRestoredAfterReset` FAIL — RED gate подтверждён
- GREEN: `SimEngine::reset()` обновлён + `SceneLoader` валидация добавлена; все тесты прошли — GREEN gate подтверждён
- REFACTOR: не потребовался

Коммиты:
- `303fdbf test(00-05)`: RED gate
- `57cbc5d feat(00-05)`: GREEN gate

## Known Stubs

Нет. Все реализации функциональны.

## Threat Flags

None. Threat T-00-13 (SceneLoader ACTUATION validation) закрыт в этом плане.

## Self-Check: PASSED
