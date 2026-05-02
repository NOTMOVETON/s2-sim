---
phase: 03-kernel-commands-queue
plan: 01
subsystem: core
tags: [kernel-commands, command-queue, thread-safety, sim-engine, sim-bus]

requires:
  - phase: 02-unified-entity-model
    provides: EntityId, EntityType, Pose3D, Agent/Prop/Actor structs, SimWorld O(1) lookup

provides:
  - KernelCommand std::variant с 24 типами команд (namespace s2::cmd)
  - CommandQueue — потокобезопасная очередь (mutex + swap-drain)
  - SimEngine.enqueue() — публичный API постановки команд из любого потока
  - SimEngine.tick() PHASE 0 — атомарное применение команд в начале каждого тика
  - SimBus расширен 10 формальными event types

affects: [04-rest-api, 07-shared-state-revision, 11-zone-lifecycle]

tech-stack:
  added: [std::variant, std::mutex, std::queue]
  patterns: [KernelCommand visitor с if constexpr, swap-drain потокобезопасная очередь]

key-files:
  created:
    - workspace/s2_core/include/s2/kernel_command.hpp
    - workspace/s2_core/include/s2/command_queue.hpp
    - workspace/s2_core/tests/test_kernel_commands.cpp
  modified:
    - workspace/s2_core/include/s2/sim_bus.hpp
    - workspace/s2_core/include/s2/sim_engine.hpp
    - workspace/s2_core/CMakeLists.txt

key-decisions:
  - "PHASE 0 вставлена до guard if(paused_) — SetPose и ResumeSim работают даже когда симуляция на паузе"
  - "cmd-типы в namespace s2::cmd — в тестах с using namespace s2 пишется cmd::SetPose{}"
  - "drain() swap под мьютексом — мьютекс не держится во время обработки команд"
  - "apply_commands реализует только 5 из 24 команд — остальные заглушки по scope limit Phase 3"

patterns-established:
  - "KernelCommand visitor: std::visit с lambda + if constexpr std::is_same_v<T, cmd::X>"
  - "CommandQueue: enqueue lock+push, drain lock+swap+unlock+convert — минимальное время под мьютексом"

duration: ~1 сессия
started: 2026-05-02T00:00:00Z
completed: 2026-05-02T00:00:00Z
---

# Phase 3 Plan 1: KernelCommands Queue — Summary

**Thread-safety фундамент: все изменения мира через буфер команд — `CommandQueue` с mutex + `SimEngine.tick()` PHASE 0.**

## Performance

| Метрика | Значение |
|---------|----------|
| Duration | ~1 сессия |
| Tasks | 3/3 completed |
| Files modified | 6 |
| Tests | 4 new PASSED, 22 regression PASSED |

## Acceptance Criteria Results

| Criterion | Status | Notes |
|-----------|--------|-------|
| AC-1: KernelCommand variant компилируется со всеми типами | Pass | 24 cmd-типа, сборка без ошибок |
| AC-2: CommandQueue потокобезопасен | Pass | Test 4: 1000 enqueue + drain concurrent → 0 потерь |
| AC-3: Команды применяются в начале тика N+1, не в середине тика N | Pass | SetPose через enqueue + step(1) → поза обновлена |
| AC-4: SimBus имеет полный набор формальных event types | Pass | 10 новых структур, компилируется |

## Accomplishments

- `kernel_command.hpp`: `std::variant<...>` из 24 typed command structs в `namespace s2::cmd`
- `command_queue.hpp`: `CommandQueue` с mutex + swap-based drain — мьютекс не держится во время обработки
- `sim_engine.hpp`: PHASE 0 в начале `tick()` до `if (paused_)` guard; `enqueue()` публичный; `apply_commands()` с `std::visit`
- `sim_bus.hpp`: +10 event types — EntitySpawned/Despawned, ZoneEntered/Exited, GrabAttempt/Succeeded/Failed, DamageDealt, SignalActivated/Deactivated

## Task Commits

Все изменения в одном APPLY прогоне. Commit создаётся в transition.

## Files Created/Modified

| File | Change | Purpose |
|------|--------|---------|
| `include/s2/kernel_command.hpp` | Created | 24 command types + KernelCommand variant |
| `include/s2/command_queue.hpp` | Created | Thread-safe queue (mutex + swap drain) |
| `tests/test_kernel_commands.cpp` | Created | 4 tests: SetPose, PauseResume, MultiCmd, ThreadSafe |
| `include/s2/sim_bus.hpp` | Modified | +10 event structs в namespace s2::event |
| `include/s2/sim_engine.hpp` | Modified | +includes, +enqueue(), +command_queue_, PHASE 0, apply_commands() |
| `CMakeLists.txt` | Modified | +test_kernel_commands.cpp в s2_core_tests |

## Decisions Made

| Decision | Rationale | Impact |
|----------|-----------|--------|
| PHASE 0 до `if (paused_)` guard | ResumeSim и SetPose должны работать даже когда sim на паузе | Тест 2 (PauseResume) проходит; Phase 4 REST может ставить команды в паузе |
| `namespace s2::cmd` для команд | `cmd::SetPose{}` читабельнее `s2::KernelCmd::SetPose` в тестах и будущем REST коде | Phase 4 будет писать `cmd::SpawnEntity{}` |
| swap-drain pattern в CommandQueue | Mutex только для swap() — обработка команд вне мьютекса, нет блокировки продюсера | Минимальная contention при высокой частоте тиков |
| apply_commands() — только 5 команд | Scope limit: SpawnEntity/DespawnEntity/AddPlugin/RemovePlugin реализуются в Phase 4 | 19 команд — no-op заглушки с комментарием |

## Deviations from Plan

None — план выполнен точно как написан.

## Issues Encountered

None

## Next Phase Readiness

**Ready:**
- `SimEngine.enqueue(KernelCommand)` готов принимать REST команды (Phase 4)
- `CommandQueue` потокобезопасен — REST поток пишет, sim поток читает
- `RemoveOwnEffect` команда зарегистрирована в variant (Phase 7 реализует)
- SimBus event types готовы для подписчиков (Phase 4 публикует EntitySpawned)

**Concerns:**
- `apply_commands()` сейчас ищет entity в `get_agent()` → `get_prop()` → `get_actor()` последовательно. При будущем едином Entity registry (если Phase 2 перейдёт к `unordered_map<EntityId, Entity>`) — нужно обновить lookup

**Blockers:**
- None

---
*Phase: 03-kernel-commands-queue, Plan: 01*
*Completed: 2026-05-02*
