---
phase: 05-iactorbehavior-behaviorregistry
plan: 01
subsystem: simulation-core
tags: [actor, behavior, fsm, registry, scene-loader, c++17]

requires:
  - phase: 02-unified-entity-model
    provides: Actor struct с BehaviorSlot (Phase 2 добавил поля, Phase 5 добавил behavior)

provides:
  - IActorBehavior интерфейс с полным lifecycle
  - ActorFSM утилитарный класс переходов состояний
  - BehaviorRegistry фабрика типов по строке
  - Actor::behavior (BehaviorSlot) в entity.hpp
  - SceneLoader::load() с BehaviorRegistry* параметром

affects:
  - 09-worldquery-formalization (WorldContext получит WorldQuery* в Phase 9)
  - 10-tick-lifecycle-revision (SimEngine вызывает behavior.update() в Phase 10)
  - 15-first-actors (DoorBehavior, ConveyorBehavior реализуют IActorBehavior)

tech-stack:
  added: []
  patterns:
    - "IActorBehavior + ActorFSM: интерфейс поведения + утилитарный FSM"
    - "BehaviorRegistry: фабрика по строке, явный параметр (не singleton)"
    - "Forward-declare Actor в actor_behavior.hpp для разрыва циклической зависимости"

key-files:
  created:
    - workspace/s2_core/include/s2/actor_behavior.hpp
    - workspace/s2_core/include/s2/world_context.hpp
    - workspace/s2_core/include/s2/actor_fsm.hpp
    - workspace/s2_core/include/s2/behavior_registry.hpp
    - workspace/s2_core/tests/test_actor_behavior.cpp
  modified:
    - workspace/s2_core/include/s2/entity.hpp
    - workspace/s2_core/include/s2/scene_loader.hpp
    - workspace/s2_core/CMakeLists.txt

key-decisions:
  - "Forward-declare Actor в actor_behavior.hpp: entity.hpp включает actor_behavior.hpp, не наоборот"
  - "WorldContext stub (sim_time+dt): Phase 9 добавит WorldQuery*"
  - "BehaviorRegistry через явный параметр SceneLoader::load(): не singleton, тестируемо"
  - "SignalEvent пустая структура: Phase 14 заполнит"

patterns-established:
  - "Конкретные поведения (DoorBehavior и др.) наследуют IActorBehavior + используют ActorFSM внутри"
  - "BehaviorRegistry::register_type() вызывается до SceneLoader::load()"

duration: ~20min
started: 2026-05-02T00:00:00Z
completed: 2026-05-02T00:20:00Z
---

# Phase 5 Plan 01: IActorBehavior + BehaviorRegistry Summary

**IActorBehavior интерфейс + ActorFSM утилита + BehaviorRegistry фабрика добавлены; Actor получил BehaviorSlot; SceneLoader загружает behavior из YAML.**

## Performance

| Metric | Value |
|--------|-------|
| Duration | ~20 min |
| Tasks | 3/3 completed |
| Files created | 5 |
| Files modified | 3 |

## Acceptance Criteria Results

| Criterion | Status | Notes |
|-----------|--------|-------|
| AC-1: IActorBehavior компилируется, Actor имеет BehaviorSlot | Pass | Сборка чистая, `Actor::behavior` в entity.hpp |
| AC-2: ActorFSM переключает состояния | Pass | Тесты BasicTransitions + ConditionalTransition |
| AC-3: BehaviorRegistry создаёт поведение по имени | Pass | Тесты CreateByName + HasType + UnknownTypeThrows |
| AC-4: SceneLoader загружает behavior из YAML | Pass | Тесты LoadBehaviorFromYAML + NoBehaviorRegistryKeepsNullptr |

## Accomplishments

- `IActorBehavior` — полный lifecycle (on_init, on_spawn, on_reset, update, on_signal, on_interact, current_state, to_json) + material stubs Phase 20
- `ActorFSM` — add_state/set_initial/add_transition/fire с условиями (Condition fn); header-only C++17
- `BehaviorRegistry` — register_type/create/has_type; бросает runtime_error на неизвестный тип
- `WorldContext` — stub (sim_time, dt); готов к расширению в Phase 9
- `Actor::behavior` добавлен в конец struct; Agent и Prop не тронуты
- `SceneLoader::load()` принимает `BehaviorRegistry* = nullptr`; backward-compatible

## Files Created/Modified

| File | Change | Purpose |
|------|--------|---------|
| `include/s2/world_context.hpp` | Created | Stub (sim_time, dt) для behavior.update(); Phase 9 добавит WorldQuery* |
| `include/s2/actor_behavior.hpp` | Created | Интерфейс IActorBehavior + SignalEvent stub |
| `include/s2/actor_fsm.hpp` | Created | Утилитарный FSM для реализаций поведений |
| `include/s2/behavior_registry.hpp` | Created | Фабрика типов по строке |
| `tests/test_actor_behavior.cpp` | Created | 7 тестов: FSM, Registry, SceneLoader с behavior |
| `include/s2/entity.hpp` | Modified | `#include actor_behavior.hpp` + `unique_ptr<IActorBehavior> behavior` в Actor |
| `include/s2/scene_loader.hpp` | Modified | `#include behavior_registry.hpp` + `BehaviorRegistry*` параметр + YAML-парсинг behavior |
| `CMakeLists.txt` | Modified | `test_actor_behavior.cpp` добавлен в s2_core_tests |

## Decisions Made

| Decision | Rationale | Impact |
|----------|-----------|--------|
| Forward-declare `struct Actor` в actor_behavior.hpp | entity.hpp включает actor_behavior.hpp → circular dep без forward-declare | Стандартный C++ паттерн, без цены |
| WorldContext — stub | Phase 9 сделает его полным (WorldQuery*); добавление сейчас — преждевременно | Phase 9 просто добавит поле, не сломав API |
| BehaviorRegistry через параметр, не singleton | Тестируемость, множественные registry в тестах | SceneLoader::load() получил backward-compatible дефолт nullptr |
| SignalEvent пустая структура | Phase 14; нужна для компиляции on_signal сигнатуры | Phase 14 расширит без ломания API |

## Deviations from Plan

None — план выполнен точно как написан.

## Issues Encountered

None.

## Next Phase Readiness

**Ready:**
- IActorBehavior интерфейс определён — Phase 15 (DoorBehavior, ConveyorBehavior) может реализовывать
- ActorFSM готов к использованию в реализациях
- BehaviorRegistry API стабилен — регистрация типов в main.cpp или тестах

**Concerns:**
- WorldContext — stub; поведения в Phase 15 получат только sim_time+dt до Phase 9
- behavior.update() не вызывается SimEngine — интеграция в Phase 10

**Blockers:**
- None

---
*Phase: 05-iactorbehavior-behaviorregistry, Plan: 01*
*Completed: 2026-05-02*
