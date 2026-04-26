---
phase: 02-actor-prop-foundation
plan: 01
subsystem: core
tags: [actor, prop, fsm, behavior, world-query, c++17]

# Dependency graph
requires:
  - phase: 00-core-architecture-foundation
    provides: "EventBus, KernelCommand, WorldQuery, Signal struct, PluginContext, IAgentPlugin"
provides:
  - "IActorBehavior интерфейс (9 виртуальных методов + стабы Phase 7)"
  - "WorldContext struct для behavior.update()"
  - "SignalEvent struct для on_signal()"
  - "ActorFSM утилитарный класс (add_state/add_transition/fire/update)"
  - "Actor struct расширенный (behavior, plugins, SharedState, collision_enabled, type)"
  - "Prop struct расширенный (signals, capabilities, tags, attachment)"
  - "WorldQuery::find_nearest_movable_prop() заглушка"
affects: [02-02, 02-03, 02-04, 02-05, 03-actor-ecosystem]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "IActorBehavior — lifecycle интерфейс для акторов (аналог IAgentPlugin)"
    - "ActorFSM — утилитарный конечный автомат с guard-условиями"
    - "WorldContext — контекст ядра для behavior (аналог PluginContext)"
    - "Prop attachment — optional EntityId + link + offset"

key-files:
  created:
    - workspace/s2_core/include/s2/actor_behavior.hpp
    - workspace/s2_core/include/s2/actor_fsm.hpp
  modified:
    - workspace/s2_core/include/s2/actor.hpp
    - workspace/s2_core/include/s2/prop.hpp
    - workspace/s2_core/include/s2/world_query.hpp

key-decisions:
  - "Actor остается aggregate struct (без user-declared конструкторов) — сохраняет designated initializer синтаксис в тестах"
  - "ActorFSM реализован inline в header — FSM небольшой, нет смысла выносить в .cpp"
  - "Prop::capabilities через std::set, tags через std::map — упорядоченный доступ"

patterns-established:
  - "IActorBehavior lifecycle: on_init(YAML) -> on_spawn(ActorId) -> update(dt, Actor&, WorldContext&) -> on_signal/on_interact"
  - "ActorFSM: состояния как строки, guard-условия, callbacks (on_enter/on_update/on_exit)"
  - "Prop attachment: attached_to_agent + attach_link + attach_offset"

requirements-completed: [ACTR-01, PROP-01]

# Metrics
duration: 6min
completed: 2026-04-26
---

# Phase 2 Plan 01: Actor & Prop Foundation Summary

**IActorBehavior интерфейс с FSM утилитой, расширенные Actor (behavior+plugins+SharedState) и Prop (signals+capabilities+attachment) структуры**

## Performance

- **Duration:** 6 min
- **Started:** 2026-04-26T18:56:58Z
- **Completed:** 2026-04-26T19:03:26Z
- **Tasks:** 2
- **Files modified:** 5

## Accomplishments
- IActorBehavior: полный интерфейс с 9 виртуальными методами (on_init/on_spawn/on_reset/update/on_signal/on_interact/current_state/to_json) + 3 стаба Phase 7 (can_release/accept_material, is_deformable)
- ActorFSM: inline утилита с add_state/add_transition/fire/update/current_state и guard-условиями
- Actor struct расширен: behavior (unique_ptr), plugins vector, SharedState, collision_enabled, type
- Prop struct расширен: signals, capabilities (set), tags (map), attachment (attached_to_agent/link/offset), has_collision
- WorldQuery: добавлен find_nearest_movable_prop() заглушка для GrabberPlugin
- Все 100% существующих тестов проходят (обратная совместимость)

## Task Commits

Each task was committed atomically:

1. **Task 1: IActorBehavior + WorldContext + ActorFSM** - `245bdca` (feat)
2. **Task 2: Расширить Actor + Prop + WorldQuery** - `e6968df` (feat)

## Files Created/Modified
- `workspace/s2_core/include/s2/actor_behavior.hpp` - IActorBehavior интерфейс + WorldContext + SignalEvent
- `workspace/s2_core/include/s2/actor_fsm.hpp` - ActorFSM утилитарный класс (inline реализация)
- `workspace/s2_core/include/s2/actor.hpp` - Actor struct расширен: behavior, plugins, SharedState, collision_enabled, type
- `workspace/s2_core/include/s2/prop.hpp` - Prop struct расширен: signals, capabilities, tags, attachment, has_collision
- `workspace/s2_core/include/s2/world_query.hpp` - WorldQuery: find_nearest_movable_prop()

## Decisions Made
- Actor остается aggregate struct (без user-declared конструкторов) — это сохраняет designated initializer синтаксис (`Actor{.id = 1, .name = "door_1"}`) в существующих тестах. Implicit move constructor/assignment корректно работают с unique_ptr.
- ActorFSM полностью inline — класс небольшой (5 методов, ~40 строк логики), нет смысла создавать .cpp.
- Prop::capabilities через std::set, tags через std::map (а не unordered_*) — упорядоченный доступ, предсказуемый порядок итерации при сериализации.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Не добавлены explicit конструкторы в Actor**
- **Found during:** Task 2
- **Issue:** План требовал explicit `Actor() = default; Actor(Actor&&) = default; ...` — но это нарушило бы aggregate-инициализацию в C++17 (`Actor{.id=1, .name="door"}`), ломая тесты test_sim_engine.cpp и test_snapshot_viz.cpp
- **Fix:** Оставлен implicit конструктор (compiler-generated) — move-only семантика автоматически обеспечена unique_ptr полями
- **Files modified:** workspace/s2_core/include/s2/actor.hpp
- **Verification:** Все тесты проходят, designated initializers работают

---

**Total deviations:** 1 auto-fixed (1 bug prevention)
**Impact on plan:** Фикс необходим для обратной совместимости. Семантика move-only сохранена.

## Issues Encountered
- Docker networking error при первом запуске тестов — решено перезапуском `docker compose down && up`

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- Все контракты Phase 2 определены: IActorBehavior, ActorFSM, расширенные Actor/Prop, WorldQuery
- Plan 02-02 может реализовать SimEngine phase2_actors() и phase6_attachments()
- Plan 02-03 может реализовать DoorBehavior используя IActorBehavior + ActorFSM
- Plan 02-05 может реализовать GrabberPlugin используя find_nearest_movable_prop()

---
*Phase: 02-actor-prop-foundation*
*Completed: 2026-04-26*
