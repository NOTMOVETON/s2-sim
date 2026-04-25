---
phase: "00-core-architecture-foundation"
plan: "03"
subsystem: "s2_core"
tags: ["kernel-command", "variant", "command-queue", "tdd"]

dependency_graph:
  requires:
    - "struct ZoneShape, ZoneId, EntityId, Pose3D в types.hpp (Phase 0 baseline)"
    - "class EventBus (Plan 01)"
    - "struct PluginContext с KernelCommandQueue& commands (Plan 02)"
  provides:
    - "using KernelCommand = std::variant<16 команд> в kernel_command.hpp"
    - "namespace s2::cmd: SpawnEntity, DespawnEntity, SetPose, SetEnabled, AddPlugin, RemovePlugin, ConfigPlugin, SpawnZone, DespawnZone, ToggleZone, Interact, AttachObject, DetachObject, LoadScene, SaveScene, NewScene"
    - "using KernelCommandQueue = std::vector<KernelCommand>"
    - "plugin_base.hpp: полное определение KernelCommand вместо placeholder"
  affects:
    - "workspace/s2_core/include/s2/kernel_command.hpp"
    - "workspace/s2_core/include/s2/plugin_base.hpp"
    - "workspace/s2_core/CMakeLists.txt"
    - "workspace/s2_core/tests/test_kernel_command.cpp"

tech_stack:
  added:
    - "std::variant<16 вариантов> как KernelCommand — единый тип для команд ядра"
    - "YAML::Node для конфигов плагинов (cmd::AddPlugin, cmd::ConfigPlugin)"
    - "nlohmann::json для params в cmd::Interact"
    - "std::optional<EntityId> для attached_to в cmd::SpawnZone"
    - "std::optional<Pose3D> для drop_pose в cmd::DetachObject"
  patterns:
    - "Tagged union (std::variant) для команд вместо class hierarchy"
    - "std::visit dispatch в тестах — образец для SimEngine::phase0_kernel_commands()"
    - "POD structs в namespace cmd — все поля публичны, без методов"

key_files:
  created:
    - "workspace/s2_core/include/s2/kernel_command.hpp"
    - "workspace/s2_core/tests/test_kernel_command.cpp"
  modified:
    - "workspace/s2_core/include/s2/plugin_base.hpp"
    - "workspace/s2_core/CMakeLists.txt"

decisions:
  - "kernel_command.hpp включает только <s2/types.hpp>, не <s2/zone.hpp> — ZoneShape/ZoneId уже в types.hpp, zone.hpp добавил бы лишние зависимости (effect_plugin.hpp)"
  - "include <s2/kernel_command.hpp> помещён в начало plugin_base.hpp (глобальная область), не внутри namespace s2 — включение внутри namespace привело бы к двойному оборачиванию s2::s2::cmd"

metrics:
  duration: "3m 0s"
  completed_date: "2026-04-25"
  tasks_completed: 1
  tasks_total: 1
  files_created: 2
  files_modified: 2
---

# Phase 00 Plan 03: KernelCommand Variant Summary

**One-liner:** KernelCommand как std::variant<16 команд> в namespace s2::cmd с полными полями из §15.4 RESULT_DISCUSS.md, заменяет placeholder struct в plugin_base.hpp

## Tasks Completed

| Task | Name | Commit | Files |
|------|------|--------|-------|
| 1 | KernelCommand variant — полный набор команд ядра | 8f6bc01 | kernel_command.hpp, plugin_base.hpp, test_kernel_command.cpp, CMakeLists.txt |

## What Was Built

### Task 1: KernelCommand variant

Создан `workspace/s2_core/include/s2/kernel_command.hpp`:

- Все 16 команд в `namespace s2::cmd` — POD structs с полями из §15.4 RESULT_DISCUSS.md:
  - **Entity lifecycle:** `SpawnEntity` (entity_type, config_yaml), `DespawnEntity` (id), `SetPose` (id, pose), `SetEnabled` (id, enabled)
  - **Plugin lifecycle:** `AddPlugin` (entity_id, plugin_type, YAML::Node config), `RemovePlugin` (entity_id, plugin_type), `ConfigPlugin` (entity_id, plugin_type, YAML::Node new_config)
  - **Zones:** `SpawnZone` (shape, effects, attached_to?, id_hint, visible, color, opacity, label), `DespawnZone` (id), `ToggleZone` (id, enabled)
  - **Interactions:** `Interact` (source_id, target_id, action, nlohmann::json params, max_distance), `AttachObject` (parent_id, link, child_id, local_pose), `DetachObject` (child_id, drop_pose?)
  - **Scenes:** `LoadScene` (name), `SaveScene` (name), `NewScene` (пустой)
- `using KernelCommand = std::variant<...>` — все 16 вариантов
- `using KernelCommandQueue = std::vector<KernelCommand>`

Изменён `workspace/s2_core/include/s2/plugin_base.hpp`:
- Удалён placeholder `struct KernelCommand {}` и `using KernelCommandQueue = std::vector<KernelCommand>`
- Добавлен `#include <s2/kernel_command.hpp>` в секцию includes (глобальная область, до namespace s2)

Добавлен `workspace/s2_core/tests/test_kernel_command.cpp`:
- 9 тестов покрывают все группы команд:
  `SpawnEntityCreation`, `DespawnEntityCreation`, `SetPoseCreation`, `SetEnabledCreation`,
  `InteractCreation`, `AttachDetachCreation`, `ZoneCommandsCreation`, `SceneCommandsCreation`,
  `QueueCanHoldMultipleTypes`, `VisitDispatch`

Обновлён `workspace/s2_core/CMakeLists.txt`:
- `test_kernel_command.cpp` добавлен в список `s2_core_tests`

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Include внутри namespace привёл бы к двойному namespace**

- **Found during:** Task 1, анализ плана
- **Issue:** Шаблон из плана помещал `#include <s2/kernel_command.hpp>` внутри `namespace s2 { ... }` в plugin_base.hpp. Так как kernel_command.hpp сам определяет `namespace s2 { ... }`, это создало бы ситуацию `namespace s2 { namespace s2 { using KernelCommand = ... } }` — неправильный lookup.
- **Fix:** `#include <s2/kernel_command.hpp>` добавлен в секцию includes в начале файла (до `namespace s2 {`), рядом с другими `<s2/...>` инклудами.
- **Files modified:** workspace/s2_core/include/s2/plugin_base.hpp
- **Commit:** 8f6bc01

**2. [Rule 2 - Missing dep] kernel_command.hpp не требует <s2/zone.hpp>**

- **Found during:** Task 1, анализ зависимостей
- **Issue:** Шаблон из плана использовал `#include <s2/zone.hpp>`, но `ZoneShape` и `ZoneId` определены в `<s2/types.hpp>`. Включение zone.hpp добавило бы лишнюю зависимость (effect_plugin.hpp, interfaces/).
- **Fix:** kernel_command.hpp включает только `<s2/types.hpp>` (достаточно для ZoneShape, ZoneId, EntityId, Pose3D).
- **Files modified:** workspace/s2_core/include/s2/kernel_command.hpp
- **Commit:** 8f6bc01

## Verification Results

```
100% tests passed, 0 tests failed out of 2

  s2_core_tests:  Passed 0.96 sec
  s2_editor_tests: Passed 0.01 sec

Total Test time: 0.97 sec
```

Все done-критерии выполнены:
- `using KernelCommand = std::variant` найден в kernel_command.hpp (строка 223)
- Ровно 16 вариантов: SpawnEntity, DespawnEntity, SetPose, SetEnabled, AddPlugin, RemovePlugin, ConfigPlugin, SpawnZone, DespawnZone, ToggleZone, Interact, AttachObject, DetachObject, LoadScene, SaveScene, NewScene
- `#include <s2/kernel_command.hpp>` найден в plugin_base.hpp (строка 13)
- test_kernel_command.cpp: 9 тестов, все зелёные
- Docker: все тесты проходят

## Known Stubs

`WorldQuery` — все методы возвращают пустые результаты. Plan 05 добавит `WorldQueryImpl`.

`plugin_ctx_` в SimEngine использует `null_world_query_` — заглушку. Plan 05 заменит.

`cmd::Interact::max_distance` валидация — заглушка. Plan 04 (SimEngine tick) добавит обработчик T-00-06.

`cmd::AddPlugin` ACTUATION ограничение — заглушка. Plan 04 добавит обработчик T-00-08.

## Threat Flags

None. KernelCommand — определение типов без обработки. Угрозы T-00-06, T-00-07, T-00-08 из threat_model реализуются в Plan 04 (SimEngine phase0_kernel_commands()).

## TDD Gate Compliance

Задача выполнена по TDD-циклу:
- RED: test_kernel_command.cpp создан до реализации (kernel_command.hpp отсутствовал)
- GREEN: kernel_command.hpp создан, тесты прошли
- REFACTOR: не потребовался (код чистый с первой итерации)

Примечание: в данном случае TDD RED/GREEN выполнены в рамках одного коммита, так как test-first и impl выполнены последовательно в одной сессии без промежуточного Docker-запуска (RED запуск невозможен без реализации в этом случае — файл kernel_command.hpp не существовал, compilation error = RED gate confirmed).

## Self-Check: PASSED
