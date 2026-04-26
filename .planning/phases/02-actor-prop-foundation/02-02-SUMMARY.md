---
phase: 02-actor-prop-foundation
plan: 02
subsystem: core
tags: [actor, prop, sim-engine, attachment, kernel-command, scene-loader, c++17]

# Dependency graph
requires:
  - phase: 02-actor-prop-foundation
    plan: 01
    provides: "IActorBehavior, ActorFSM, Actor struct (behavior+plugins+SharedState), Prop struct (signals+capabilities+attachment)"
provides:
  - "SimEngine phase2_actors(): pre_resolve -> resolve -> behavior.update -> plugins.update"
  - "SimEngine phase6_attachments(): attached prop position update из позы агента/актора"
  - "apply_kernel_command(cmd::Interact): маршрутизация к actor.behavior.on_interact()"
  - "apply_kernel_command(cmd::AttachObject): запись attached_to_agent/link/offset в Prop"
  - "apply_kernel_command(cmd::DetachObject): сброс attachment + drop_pose"
  - "SceneLoader: парсинг actors с type/collision/behavior/plugins"
  - "SceneLoader: парсинг props с has_collision/capabilities/tags/signals"
  - "SceneLoader::BehaviorFactory тип для фабрики поведений акторов"
affects: [02-03, 02-04, 02-05, 03-actor-ecosystem]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Agent-прокси для плагинов актора — IAgentPlugin::update(Agent&) с proxy из Actor"
    - "Attachment: phase6 обновляет world_pose по parent + offset"
    - "BehaviorFactory в SceneLoader для динамического создания IActorBehavior"

key-files:
  created:
    - workspace/s2_core/tests/test_actor_prop_loading.cpp
  modified:
    - workspace/s2_core/include/s2/sim_engine.hpp
    - workspace/s2_core/include/s2/scene_loader.hpp
    - workspace/s2_core/CMakeLists.txt

key-decisions:
  - "Agent-прокси в phase2_actors() — IAgentPlugin::update() принимает Agent&, для акторов создаётся proxy (id, world_pose). В Phase 6 ENTY будет унифицировано через EntityBase."
  - "Упрощённое сложение поз в phase6_attachments() — prop.pose = parent.pose + offset. В Phase 6 ENTY будет через Transform3D."
  - "BehaviorFactory — третий параметр SceneLoader::load() с default {} — обратная совместимость нулевая."

patterns-established:
  - "phase2_actors: plugins.pre_resolve -> state.resolve -> behavior.update -> plugins.update"
  - "phase6_attachments: iterate props with attached_to_agent -> update world_pose"
  - "cmd::Interact dispatch: world_.get_actor(target_id) -> behavior->on_interact()"
  - "cmd::AttachObject/DetachObject: modify Prop fields + publish ObjectAttached/ObjectReleased"

requirements-completed: [ACTR-01, PROP-01, PROP-02]

# Metrics
duration: 9min
completed: 2026-04-26
---

# Phase 2 Plan 02: Engine Integration Summary

**SimEngine tick cycle: phase2 actors (behavior.update), phase6 attachments (prop follow parent), Interact/Attach/Detach KernelCommands, SceneLoader actors/props YAML parsing**

## Performance

- **Duration:** 9 min
- **Started:** 2026-04-26T19:08:16Z
- **Completed:** 2026-04-26T19:17:48Z
- **Tasks:** 2
- **Files modified:** 4

## Accomplishments
- SimEngine phase2_actors() реализован: pre_resolve -> resolve -> behavior.update -> plugins.update для каждого актора с Agent-прокси для плагинов
- SimEngine phase6_attachments() реализован: обновление world_pose attached пропов из позы родителя (агент с kinematic_tree или актор)
- apply_kernel_command: обработчики Interact (-> actor.behavior.on_interact), AttachObject (-> prop.attached_to_agent + ObjectAttached event), DetachObject (-> сброс attachment + ObjectReleased event)
- phase8_cleanup: очистка SharedState contributions акторов
- PropSnapshot: добавлен attached_to_agent для визуализации
- SceneLoader: расширенный парсинг actors (type, collision, collision_enabled, behavior через BehaviorFactory, plugins)
- SceneLoader: расширенный парсинг props (has_collision, capabilities, tags, signals)
- 5 тестов в test_actor_prop_loading.cpp — все проходят

## Task Commits

Each task was committed atomically:

1. **Task 1: Phase 2 (actors tick) + Phase 6 (attachments) + KernelCommand handlers** - `4b06438` (feat)
2. **Task 2: SceneLoader парсинг actors и props + тест** - `740eea7` (feat)

## Files Created/Modified
- `workspace/s2_core/include/s2/sim_engine.hpp` - phase2_actors, phase6_attachments, Interact/Attach/Detach handlers, phase8 cleanup
- `workspace/s2_core/include/s2/scene_loader.hpp` - расширенный парсинг actors/props, BehaviorFactory type
- `workspace/s2_core/tests/test_actor_prop_loading.cpp` - 5 тестов загрузки акторов и пропов из YAML
- `workspace/s2_core/CMakeLists.txt` - добавлен test_actor_prop_loading.cpp

## Decisions Made
- Agent-прокси в phase2_actors(): IAgentPlugin::update() принимает Agent& — для controller-плагинов актора создаётся минимальный Agent proxy с id и world_pose. Это намеренный паттерн Phase 2, не технический долг. В Phase 6 ENTY будет EntityBase.
- Упрощённое сложение поз в phase6_attachments(): prop.pose = parent.pose + offset (без матрицы вращения). Достаточно для Phase 2 — полное преобразование будет через Transform3D в Phase 6 ENTY.
- BehaviorFactory как третий параметр SceneLoader::load() с default {} — обратная совместимость: все существующие вызовы load(path, plugin_factory) работают без изменений.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Signal.type -> Signal.signal_type**
- **Found during:** Task 2 (SceneLoader prop signals parsing)
- **Issue:** План использовал `sig.type` но в struct Signal поле называется `signal_type`
- **Fix:** Заменено на `sig.signal_type` в парсинге
- **Files modified:** workspace/s2_core/include/s2/scene_loader.hpp
- **Verification:** Docker build tests проходит
- **Committed in:** 740eea7

---

**Total deviations:** 1 auto-fixed (1 bug)
**Impact on plan:** Тривиальный фикс — имя поля в структуре. Без влияния на scope.

## Issues Encountered
- Docker networking error при первом запуске финальной верификации — решено повторным запуском.

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- Phase 2 actors tick + phase 6 attachments + command handlers полностью реализованы
- SceneLoader готов загружать акторов с behavior (через BehaviorFactory) и пропы с signals/capabilities/tags
- Plan 02-03 может реализовать DoorBehavior используя behavior.update() + BehaviorFactory
- Plan 02-04 может реализовать SignalListenerBase + controller-плагины
- Plan 02-05 может реализовать GrabberPlugin используя AttachObject/DetachObject commands

## Self-Check: PASSED

- [x] workspace/s2_core/include/s2/sim_engine.hpp — FOUND
- [x] workspace/s2_core/include/s2/scene_loader.hpp — FOUND
- [x] workspace/s2_core/tests/test_actor_prop_loading.cpp — FOUND
- [x] 02-02-SUMMARY.md — FOUND
- [x] Commit 4b06438 — FOUND
- [x] Commit 740eea7 — FOUND

---
*Phase: 02-actor-prop-foundation*
*Completed: 2026-04-26*
