---
phase: 01-zone-visual-control-layer
plan: 05
subsystem: s2_core
tags: [zone, scene-loader, yaml-parsing, lifecycle, detection-mode, owned-zones, zone-templates, build-snapshot, visual-hints]
dependency_graph:
  requires:
    - "01-01: Zone struct fields (DetectionMode, ZoneLifecycle, SelfDestructPolicy, strength, attached_to_entity_id)"
    - "01-03: ZoneSystem remove_zone(), toggle_zone_with_events(), update_owned_zones_positions()"
    - "01-04: ZoneSpawnSystem class, SpawnZone/DespawnZone/ToggleZone handlers in SimEngine"
  provides:
    - "SceneLoader парсит lifecycle, detection_mode enum, self_destruct, owned_zones, zone_templates из YAML"
    - "SimEngine build_snapshot() заполняет ZoneSnapshot.strength и visual_hints"
    - "ZoneSpawnSystem интегрирован в SimEngine: init в load_world, tick в phase0"
    - "load_zone_templates() метод SimEngine для передачи шаблонов из SceneData"
  affects:
    - "workspace/s2_core/include/s2/scene_loader.hpp"
    - "workspace/s2_core/include/s2/sim_engine.hpp"
    - "workspace/s2_visualizer/src/main.cpp"
tech_stack:
  added: []
  patterns:
    - "zone_templates в SceneData: отдельный вектор ZoneTemplate, передаётся через load_zone_templates()"
    - "build_snapshot visual_hints: итерация zone.effects → plugin->visual_hint() → ZoneSnapshot::Hint"
key_files:
  created: []
  modified:
    - workspace/s2_core/include/s2/scene_loader.hpp
    - workspace/s2_core/include/s2/sim_engine.hpp
    - workspace/s2_visualizer/src/main.cpp
key_decisions:
  - "load_zone_templates() — отдельный метод SimEngine (не параметр load_world) для backward compat"
  - "zone_spawn_system_.init() вызывается в load_world() автоматически, шаблоны добавляются через отдельный вызов"
  - "state_change trigger парсит actor_id как uint32_t (соответствует ActorId typedef)"
patterns_established:
  - "SceneData.zone_templates: шаблоны передаются отдельно от SimWorld через load_zone_templates()"
  - "build_snapshot visual_hints: собираются из effect.plugin->visual_hint() для каждой зоны"
requirements_completed: [ZONE-04, ZONE-08, ZONE-09]
metrics:
  duration: "5 мин"
  completed_date: "2026-04-26"
  tasks_completed: 2
  tasks_total: 2
  files_modified: 3
  files_created: 0
---

# Phase 01 Plan 05: SceneLoader YAML Extensions + SimEngine Snapshot/ZoneSpawnSystem Integration Summary

**SceneLoader парсит lifecycle/detection_mode/self_destruct/owned_zones/zone_templates из YAML; SimEngine заполняет ZoneSnapshot.strength/visual_hints, интегрирует ZoneSpawnSystem с init/tick/load_zone_templates.**

## Performance

- **Duration:** 5 мин
- **Started:** 2026-04-26T15:57:54Z
- **Completed:** 2026-04-26T16:03:06Z
- **Tasks:** 2/2
- **Files modified:** 3

## Accomplishments
- SceneLoader расширен: 5 новых секций YAML парсинга (detection_mode enum, lifecycle, self_destruct, owned_zones, zone_templates)
- SimEngine build_snapshot() заполняет ZoneSnapshot.strength и собирает visual_hints из эффектов зон
- ZoneSpawnSystem полностью интегрирован в SimEngine: init в load_world(), tick в phase0, load_zone_templates() для передачи шаблонов
- main.cpp обновлён: вызывает load_zone_templates() при старте и при reload сцены

## Task Commits

1. **Задача 1: SceneLoader — lifecycle, detection_mode enum, self_destruct, owned_zones, zone_templates** — `2e1a53d` (feat)
2. **Задача 2: SimEngine — ZoneSpawnSystem интеграция, build_snapshot strength/visual_hints** — `b12a66e` (feat)

## Files Created/Modified
- `workspace/s2_core/include/s2/scene_loader.hpp` — парсинг detection_mode в enum, lifecycle секция, self_destruct_policy, owned_zones у агентов, zone_templates секция; добавлен include zone_spawn_system.hpp, SceneData.zone_templates вектор
- `workspace/s2_core/include/s2/sim_engine.hpp` — поле zone_spawn_system_, init в load_world(), load_zone_templates() метод, tick в phase0, build_snapshot strength/visual_hints
- `workspace/s2_visualizer/src/main.cpp` — вызов load_zone_templates() после load_world() при старте и reload

## Decisions Made
- load_zone_templates() реализован как отдельный метод SimEngine вместо расширения load_world() — сохраняет backward compat с существующими тестами которые вызывают load_world(SimWorld)
- zone_spawn_system_.init() вызывается автоматически в load_world() — гарантирует правильную подписку на EventBus
- state_change trigger парсит actor_id как uint32_t (через as<uint32_t>()) — соответствует ActorId typedef и event::ActorStateChanged.actor

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 2 - Missing Critical] load_zone_templates() как отдельный метод**
- **Found during:** Task 2
- **Issue:** План указывал добавить zone_templates в load_world(), но load_world() принимает SimWorld, а не SceneData. zone_templates не являются частью SimWorld.
- **Fix:** Создан отдельный публичный метод load_zone_templates(vector<ZoneTemplate>) для передачи шаблонов после load_world()
- **Files modified:** sim_engine.hpp, main.cpp
- **Verification:** Docker build tests проходит, main.cpp вызывает load_zone_templates() при старте и reload

---

**Total deviations:** 1 auto-fixed (Rule 2 — missing integration path)
**Impact on plan:** API чище чем в плане — load_world не требует изменения сигнатуры, backward compat полный.

## Issues Encountered

None.

## User Setup Required

None — нет конфигурации внешних сервисов.

## Known Stubs

None. Все парсеры полноценные с правильными default-значениями. visual_hints собираются из реальных plugin->visual_hint() вызовов.

## Threat Flags

| Flag | File | Description |
|------|------|-------------|
| threat_flag: T-01-11 mitigated | scene_loader.hpp | owned_zones.id auto-генерируется если пустой (agent.name + "_zone_" + index) |

## Next Phase Readiness
- Backend pipeline ZONE-04/08/09 полностью замкнут: YAML -> SceneLoader -> ZoneSystem + ZoneSpawnSystem -> build_snapshot -> JSON SSE
- Все закомментированные зоны в test_phase1.yaml теперь можно раскомментировать (lifecycle, detection_mode, self_destruct_policy)
- Готово к Plan 06 (UI Zone Inspector, VisualHint рендеринг)

## Self-Check

- workspace/s2_core/include/s2/scene_loader.hpp -- FOUND
- workspace/s2_core/include/s2/sim_engine.hpp -- FOUND
- workspace/s2_visualizer/src/main.cpp -- FOUND
- Commit 2e1a53d (Task 1) -- FOUND
- Commit b12a66e (Task 2) -- FOUND

## Self-Check: PASSED

---
*Phase: 01-zone-visual-control-layer*
*Completed: 2026-04-26*
