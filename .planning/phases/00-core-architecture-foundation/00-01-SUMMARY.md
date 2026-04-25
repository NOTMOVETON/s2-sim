---
phase: "00-core-architecture-foundation"
plan: "01"
subsystem: "s2_core"
tags: ["eventbus", "signal", "types", "backward-compat", "tdd"]

dependency_graph:
  requires: []
  provides:
    - "struct Signal в namespace s2 (types.hpp)"
    - "class EventBus с полным набором event:: типов (event_bus.hpp)"
    - "using SimBus = EventBus backward-compat (sim_bus.hpp)"
    - "Agent::signals поле (agent.hpp)"
  affects:
    - "workspace/s2_core/include/s2/types.hpp"
    - "workspace/s2_core/include/s2/agent.hpp"
    - "workspace/s2_core/include/s2/event_bus.hpp"
    - "workspace/s2_core/include/s2/sim_bus.hpp"
    - "workspace/s2_core/tests/test_signal.cpp"
    - "workspace/s2_core/tests/test_event_bus.cpp"

tech_stack:
  added:
    - "nlohmann/json в types.hpp (для Signal::params)"
  patterns:
    - "TDD RED/GREEN/REFACTOR цикл для обеих задач"
    - "Backward-compat через using-алиас (SimBus = EventBus)"
    - "Typed event dispatch через std::type_index + std::any"

key_files:
  created:
    - "workspace/s2_core/include/s2/event_bus.hpp"
    - "workspace/s2_core/tests/test_signal.cpp"
    - "workspace/s2_core/tests/test_event_bus.cpp"
  modified:
    - "workspace/s2_core/include/s2/types.hpp"
    - "workspace/s2_core/include/s2/agent.hpp"
    - "workspace/s2_core/include/s2/sim_bus.hpp"
    - "workspace/s2_core/CMakeLists.txt"

decisions:
  - "SimBus сохранён как using-алиас в sim_bus.hpp для нулевой миграции существующего кода"
  - "EventBus живёт в отдельном event_bus.hpp — новый канонический заголовок"
  - "Signal::params = nlohmann::json (произвольные параметры без типизации)"
  - "test_event_bus.cpp включает sim_bus.hpp для теста SimBusAliasWorksAsEventBus"

metrics:
  duration: "7m 40s"
  completed_date: "2026-04-25"
  tasks_completed: 2
  tasks_total: 2
  files_created: 3
  files_modified: 4
---

# Phase 00 Plan 01: Signal struct + EventBus Foundation Summary

**One-liner:** Signal struct (7 полей из D-15) + EventBus с 11 новыми event-типами ARCH-04 через backward-compat SimBus alias

## Tasks Completed

| Task | Name | Commit | Files |
|------|------|--------|-------|
| 1 | Signal struct в types.hpp + Agent::signals | a3ca214 | types.hpp, agent.hpp, test_signal.cpp, CMakeLists.txt |
| 2 | EventBus + backward-compat + test_event_bus | d2b710b | event_bus.hpp, sim_bus.hpp, test_event_bus.cpp, CMakeLists.txt |

## What Was Built

### Task 1: Signal struct + Agent::signals (TDD GREEN)

Добавлен `struct Signal` в `workspace/s2_core/include/s2/types.hpp`:
- Поля из D-15: `signal_type`, `signal_id`, `local_pose`, `params`, `range`, `requires_los`, `enabled`
- `#include <nlohmann/json.hpp>` добавлен для `Signal::params`
- wire-конвенция документирована: `range = infinity, requires_los = false`
- Поле `std::vector<Signal> signals` добавлено в `struct Agent` (после `visual`)

6 тестов в `test_signal.cpp`: DefaultValues, AllFieldsPresent, WireConvention, EmptyByDefault, CanAddSignal, MultipleSignals.

### Task 2: EventBus + backward-compat (TDD GREEN)

Создан `workspace/s2_core/include/s2/event_bus.hpp` — canonical header:
- `class EventBus` с методами `subscribe/publish/subscriber_count/event_type_count`
- Новые event-типы (ARCH-04 D-08): `EntitySpawned`, `EntityDespawned`, `ZoneEntered`, `ZoneExited`, `SignalActivated`, `SignalDeactivated`, `GrabAttempt`, `GrabSucceeded`, `GrabFailed`, `DamageDealt`
- Legacy-типы сохранены: `AgentEnteredZone`, `AgentExitedZone`, `ActorStateChanged`, `ObjectAttached`, `ObjectReleased`, `AgentCollision`, `TeleportAgentCommand`, `SetZoneTeleportTargetCommand`

Переписан `sim_bus.hpp` как backward-compat wrapper:
```cpp
#include <s2/event_bus.hpp>
namespace s2 { using SimBus = EventBus; }
```

9 тестов в `test_event_bus.cpp`: все новые event-типы + SimBusAliasWorksAsEventBus.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Добавлен #include <s2/sim_bus.hpp> в test_event_bus.cpp**
- **Found during:** Task 2, TDD GREEN
- **Issue:** test_event_bus.cpp включал только `<s2/event_bus.hpp>`, но тест `SimBusAliasWorksAsEventBus` использует `SimBus`, который определён только в `sim_bus.hpp`. Компилятор: `'SimBus' was not declared in this scope`.
- **Fix:** Добавлен `#include <s2/sim_bus.hpp>` в test_event_bus.cpp
- **Files modified:** `workspace/s2_core/tests/test_event_bus.cpp`
- **Commit:** d2b710b (часть Task 2 commit)

## Verification Results

```
100% tests passed, 0 tests failed out of 2
369 tests from 52 test suites (s2_core_tests) — Passed
9 tests from 3 test suites (s2_editor_tests) — Passed
Total Test time: 1.10 sec
```

Все done-критерии выполнены:
- `struct Signal` в types.hpp с полями из D-15
- `std::vector<Signal> signals` в agent.hpp
- `class EventBus` в event_bus.hpp с 11+ event-типами
- `using SimBus = EventBus` в sim_bus.hpp
- `test_sim_bus.cpp` не изменён (backward compat работает)
- Docker: все 369 тестов зелёные

## Known Stubs

None.

## Threat Flags

None. Новая поверхность не добавлена. EventBus — internal-only API без внешнего входа (T-00-01, T-00-02 в threat_model плана).

## Self-Check: PASSED
