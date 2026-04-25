---
phase: "00-core-architecture-foundation"
plan: "02"
subsystem: "s2_core"
tags: ["world-query", "plugin-lifecycle", "plugin-role", "plugin-context", "tdd"]

dependency_graph:
  requires:
    - "struct Signal в namespace s2 (Plan 01)"
    - "class EventBus (Plan 01)"
  provides:
    - "class WorldQuery с 10 методами read-only API (world_query.hpp)"
    - "EntityFilter, DetectionVolume, Box, RaycastQueryResult вспомогательные типы"
    - "enum class PluginRole { ACTUATION, SENSOR, INTERACTION, RESOURCE, UTILITY }"
    - "struct PluginContext { const WorldQuery& world; EventBus& bus; KernelCommandQueue& commands }"
    - "struct KernelCommand placeholder + using KernelCommandQueue = std::vector<KernelCommand>"
    - "IAgentPlugin::on_spawn/on_despawn/on_scene_load/on_reset lifecycle-методы"
    - "IAgentPlugin::role() чисто виртуальный"
    - "IAgentPlugin::provided_capabilities() -> vector<string>"
    - "IAgentPlugin::update(double, Agent&, const PluginContext&) новая сигнатура"
    - "IAgentPlugin::config_schema() -> nlohmann::json"
    - "SimEngine::plugin_ctx_ (null-контекст до Plan 05)"
  affects:
    - "workspace/s2_core/include/s2/world_query.hpp"
    - "workspace/s2_core/include/s2/plugin_base.hpp"
    - "workspace/s2_core/include/s2/sim_engine.hpp"
    - "workspace/s2_core/CMakeLists.txt"
    - "workspace/s2_core/tests/test_plugin_lifecycle.cpp"
    - "workspace/s2_core/tests/test_plugin_roles.cpp"
    - "workspace/s2_plugins/include/s2/plugins/* (все плагины)"
    - "workspace/s2_plugins/src/color.cpp"
    - "workspace/s2_plugins/src/joint_vel.cpp"
    - "workspace/s2_plugins/src/plugins_registry.cpp"

tech_stack:
  added:
    - "struct KernelCommand placeholder (полный вариант в Plan 04)"
    - "nlohmann::json как возвращаемый тип config_schema()"
  patterns:
    - "Null-object pattern: WorldQuery-заглушка в SimEngine до Plan 05"
    - "Context-struct pattern (аналог EffectContext): PluginContext передаётся в update()"
    - "Forward-declare + placeholder struct: KernelCommand для compile-time completeness"
    - "ASCII-only labels в config_schema() для компилятор-совместимости в Docker"

key_files:
  created:
    - "workspace/s2_core/include/s2/world_query.hpp"
    - "workspace/s2_core/tests/test_plugin_lifecycle.cpp"
    - "workspace/s2_core/tests/test_plugin_roles.cpp"
  modified:
    - "workspace/s2_core/include/s2/plugin_base.hpp"
    - "workspace/s2_core/include/s2/sim_engine.hpp"
    - "workspace/s2_core/CMakeLists.txt"
    - "workspace/s2_plugins/include/s2/plugins/diff_drive.hpp"
    - "workspace/s2_plugins/include/s2/plugins/gnss.hpp"
    - "workspace/s2_plugins/include/s2/plugins/imu.hpp"
    - "workspace/s2_plugins/include/s2/plugins/battery.hpp"
    - "workspace/s2_plugins/include/s2/plugins/gravity.hpp"
    - "workspace/s2_plugins/include/s2/plugins/lidar.hpp"
    - "workspace/s2_plugins/include/s2/plugins/color.hpp"
    - "workspace/s2_plugins/include/s2/plugins/joint_vel.hpp"
    - "workspace/s2_plugins/include/s2/plugins/path_display.hpp"
    - "workspace/s2_plugins/include/s2/plugins/topic_display.hpp"
    - "workspace/s2_plugins/include/s2/plugins/trajectory_recorder.hpp"
    - "workspace/s2_plugins/src/color.cpp"
    - "workspace/s2_plugins/src/joint_vel.cpp"
    - "workspace/s2_plugins/src/plugins_registry.cpp"
    - "workspace/s2_core/tests/test_gravity_plugin.cpp"
    - "workspace/s2_core/tests/test_battery_plugin.cpp"
    - "workspace/s2_core/tests/test_color_plugin.cpp"
    - "workspace/s2_core/tests/test_effect_modifier.cpp"
    - "workspace/s2_core/tests/test_effect_mutation.cpp"
    - "workspace/s2_core/tests/test_joint_vel_plugin.cpp"
    - "workspace/s2_core/tests/test_lidar_plugin.cpp"

decisions:
  - "KernelCommand — полный struct placeholder (не forward-declare) чтобы std::vector<KernelCommand> компилировался без complete-type ошибки"
  - "config_schema() labels переведены в ASCII-only из-за encoding-проблем с Cyrillic в R-string literals в Docker gcc"
  - "Все плагины мигрированы за один проход (D-03) вместе с Task 2 — иначе Docker build не компилируется"
  - "SimEngine добавляет null_world_query_/plugin_bus_/plugin_cmds_/plugin_ctx_ члены для передачи в update()"
  - "nlohmann::json::array({...}) вместо json::parse(R\"(...)\"): избегаем encoding-проблем с UTF-8 в raw string literals"

metrics:
  duration: "20m 15s"
  completed_date: "2026-04-25"
  tasks_completed: 2
  tasks_total: 2
  files_created: 3
  files_modified: 24
---

# Phase 00 Plan 02: WorldQuery + Extended IAgentPlugin Summary

**One-liner:** WorldQuery read-only API (10 методов, заглушки) + IAgentPlugin lifecycle (on_spawn/on_despawn/on_scene_load/on_reset) + PluginRole enum (5 значений) + PluginContext struct + миграция всех 11 плагинов на новую сигнатуру update()

## Tasks Completed

| Task | Name | Commit | Files |
|------|------|--------|-------|
| 1 | WorldQuery header — read-only API | d856689 | world_query.hpp |
| 2 | IAgentPlugin lifecycle + PluginRole + PluginContext + миграция | 883e04d | plugin_base.hpp, sim_engine.hpp, 11 плагинов, 7 тестов, 2 новых теста |

## What Was Built

### Task 1: WorldQuery read-only API

Создан `workspace/s2_core/include/s2/world_query.hpp`:

- `class WorldQuery` с 10 виртуальными методами (заглушки):
  - `find_in_radius`, `find_in_box`, `find_nearest`, `find_entity_below` — поиск Entity
  - `find_signals_of_type` — поиск сигналов в DetectionVolume
  - `has_line_of_sight`, `raycast` — геометрия
  - `zones_at`, `is_in_zone` — зоны
  - `find_deformable_in_box` — деформируемые объекты
- Вспомогательные типы: `EntityFilter` (agents_only/actors_only/all), `DetectionVolume` (SPHERE/CONE/BOX), `Box` (AABB), `RaycastQueryResult` (с entity_id в отличие от RaycastResult)

### Task 2: Расширенный IAgentPlugin + полная миграция

Изменён `workspace/s2_core/include/s2/plugin_base.hpp`:

- `struct KernelCommand` — placeholder (полный вариант в Plan 04)
- `using KernelCommandQueue = std::vector<KernelCommand>`
- `enum class PluginRole { ACTUATION, SENSOR, INTERACTION, RESOURCE, UTILITY }`
- `struct PluginContext { const WorldQuery& world; EventBus& bus; KernelCommandQueue& commands }`
- `virtual PluginRole role() const = 0` — чисто виртуальный
- `virtual std::vector<std::string> provided_capabilities() const { return {}; }`
- Lifecycle-методы с default no-op: `on_spawn`, `on_despawn`, `on_scene_load`, `on_reset`
- `update()` сигнатура: `virtual void update(double dt, Agent& agent, const PluginContext& ctx) = 0`
- `config_schema()` возвращает `nlohmann::json` вместо `std::string`

Изменён `workspace/s2_core/include/s2/sim_engine.hpp`:
- Добавлены `null_world_query_`, `plugin_bus_`, `plugin_cmds_`, `plugin_ctx_` члены
- `plugin->update()` вызывается с 3 аргументами через `plugin_ctx_`

Мигрированы все 11 плагинов (D-03 one-pass):
- Добавлен `role() const override` в каждый
- Изменена `update()` сигнатура (ctx игнорируется — Plan 06 добавит использование)
- `config_schema()` возвращает `nlohmann::json::array({...})`

Обновлены 7 тестовых файлов:
- Добавлен `g_null_world / g_null_bus / g_null_cmds / g_ctx` null-контекст
- Все `plugin.update(dt, agent)` заменены на `plugin.update(dt, agent, g_ctx)`

Созданы 2 новых тестовых файла:
- `test_plugin_lifecycle.cpp` — 5 тестов lifecycle-методов
- `test_plugin_roles.cpp` — 5 тестов системы ролей

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] Миграция всех плагинов выполнена в Plan 02 вместо Plan 06**

- **Found during:** Task 2
- **Issue:** `s2_core_tests` линкуется с `s2_plugins`. Изменение `update()` сигнатуры на pure virtual в `IAgentPlugin` немедленно ломает все 11 плагинов, делая Docker build невозможным. Plan 06 не может исправить то, что мешает компилировать Plan 02.
- **Fix:** Выполнена полная миграция всех плагинов в рамках Task 2 (D-03: "за один проход")
- **Files modified:** 11 plugin hpp/cpp файлов, 7 тестовых файлов
- **Commit:** 883e04d

**2. [Rule 1 - Bug] KernelCommand требует complete type для std::vector**

- **Found during:** Task 2, первый Docker build
- **Issue:** `using KernelCommandQueue = std::vector<KernelCommand>` с только forward-declared `struct KernelCommand` вызывает `error: static assertion failed: template argument must be a complete class` в destructor std::vector
- **Fix:** Добавлено placeholder-определение `struct KernelCommand {}` в plugin_base.hpp. Plan 04 расширит это до полного `std::variant<...>`
- **Files modified:** workspace/s2_core/include/s2/plugin_base.hpp
- **Commit:** 883e04d

**3. [Rule 1 - Bug] Encoding проблема с Cyrillic в R-string literals в Docker gcc**

- **Found during:** Task 2, второй Docker build
- **Issue:** `R"([{"label":"Частота (Гц)"}])"` — Cyrillic байты в raw string literal вызывают parse error компилятора: `expected ')' before ':' token`
- **Fix:** `config_schema()` переведена с `nlohmann::json::parse(R"([...])")` на `nlohmann::json::array({...})` с ASCII-only labels
- **Files modified:** все plugin hpp файлы
- **Commit:** 883e04d

## Verification Results

```
100% tests passed, 0 tests failed out of 2

  s2_core_tests:  Passed 0.96 sec
  s2_editor_tests: Passed 0.01 sec

Total Test time: 0.97 sec
```

Все done-критерии выполнены:
- `class WorldQuery` в world_query.hpp с 10 методами из §15.3 RESULT_DISCUSS.md
- `find_in_radius`, `find_signals_of_type`, `has_line_of_sight` присутствуют
- `enum class PluginRole` с 5 значениями в plugin_base.hpp
- `struct PluginContext` с world/bus/commands в plugin_base.hpp
- `virtual void on_spawn(Agent&)` и другие lifecycle-методы добавлены
- `virtual PluginRole role() const = 0` добавлен
- `update(double, Agent&, const PluginContext&)` — новая сигнатура
- `provided_capabilities()` возвращает `std::vector<std::string>`
- test_plugin_lifecycle.cpp и test_plugin_roles.cpp — все тесты зелёные
- Docker: все тесты компилируются и проходят

## Known Stubs

`KernelCommand` — пустой struct-placeholder. Plan 04 добавит:
```cpp
using KernelCommand = std::variant<cmd::SpawnEntity, cmd::DespawnEntity, ...>;
```

`WorldQuery` — все методы возвращают пустые результаты. Plan 05 добавит `WorldQueryImpl` в `SimEngine`.

`plugin_ctx_` в SimEngine использует `null_world_query_` — заглушку WorldQuery. Plan 05 заменит на `WorldQueryImpl`.

## Threat Flags

None. WorldQuery — read-only API без внешнего входа. T-00-03 (Tampering) — WorldQuery только const refs; нет методов записи по дизайну.

## Self-Check: PASSED
