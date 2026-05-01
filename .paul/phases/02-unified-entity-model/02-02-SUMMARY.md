---
phase: 02-unified-entity-model
plan: 02
subsystem: core
tags: [entity-model, scene-loader, yaml, role-enforcement, testing]

requires:
  - phase: 02-01
    provides: Agent/Actor/Prop flat structs, SimWorld Variant D, EntityType enum

provides:
  - SceneData использует AgentData/PropData/ActorData
  - YAML-парсинг transport/tags/immune_to_effects для всех entity типов
  - Backward compat: старые сцены грузятся через domain_id fallback
  - role() enforcement в load_world() — max 1 actuation-плагин на агента
  - test_unified_entity.cpp — 6 тестов entity model

affects: [phase-08-transport, phase-06-effects]

tech-stack:
  added: []
  patterns:
    - "SceneData использует typed aliases (AgentData/PropData/ActorData) — одно изменение в SceneLoader"
    - "transport_type/transport_domain_id живут в agent.tags до Phase 8 (TransportPool)"

key-files:
  modified:
    - workspace/s2_core/include/s2/scene_loader.hpp
    - workspace/s2_core/include/s2/sim_engine.hpp
    - workspace/s2_core/CMakeLists.txt
  created:
    - workspace/s2_core/tests/test_unified_entity.cpp

key-decisions:
  - "scene_loader.cpp не существует — реализация inline в hpp; изменения внесены в header"
  - "transport поля хранятся в agent.tags[transport_type/transport_domain_id] до Phase 8"

patterns-established:
  - "Backward compat через fallback: если transport: отсутствует — читаем domain_id с верхнего уровня"

duration: ~30min
started: 2026-05-02T00:00:00Z
completed: 2026-05-02T00:00:00Z
---

# Phase 2 Plan 02: SceneLoader + role() enforcement + Entity тесты

**SceneData переведён на AgentData/PropData/ActorData; YAML-парсинг transport/tags/immune_to_effects добавлен для всех entity типов; role() enforcement в load_world() и 6 green тестов entity model.**

## Performance

| Метрика | Значение |
|---------|----------|
| Duration | ~30 min |
| Tasks | 2 completed |
| Files modified | 3 |
| Files created | 1 |

## Acceptance Criteria Results

| Criterion | Status | Notes |
|-----------|--------|-------|
| AC-1: SceneData uses AgentData/PropData/ActorData | Pass | Aliases backward-compatible |
| AC-2: transport/tags/immune_to_effects парсятся | Pass | Все три entity типа |
| AC-3: Backward compat — старые YAML сцены | Pass | domain_id fallback при отсутствии transport: |
| AC-4: role() enforcement бросает исключение | Pass | std::runtime_error при >1 actuation |
| AC-5: test_unified_entity.cpp green | Pass | 6 тестов, 100% passed |

## Accomplishments

- `SceneData.agents/props/actors` → `vector<AgentData/PropData/ActorData>` (semantic rename, backward-compatible)
- Полный парсинг `transport/ros2/tags/immune_to_effects/enabled` из YAML для Agent/Actor/Prop
- Backward compat: `domain_id` на верхнем уровне агента работает без `transport:` секции
- `load_world()` enforcement: `std::runtime_error` при двух actuation-плагинах на агенте
- 6 тестов в `test_unified_entity.cpp` покрывают: tags, PropData fields, O(1) lookup, remove+reindex, role enforcement, map semantics

## Files Created/Modified

| File | Change | Purpose |
|------|--------|---------|
| `workspace/s2_core/include/s2/scene_loader.hpp` | Modified | SceneData types + YAML parsing новых полей |
| `workspace/s2_core/include/s2/sim_engine.hpp` | Modified | role() enforcement в load_world() |
| `workspace/s2_core/tests/test_unified_entity.cpp` | Created | 6 тестов entity model |
| `workspace/s2_core/CMakeLists.txt` | Modified | Добавлен test_unified_entity.cpp |

## Decisions Made

| Decision | Rationale | Impact |
|----------|-----------|--------|
| scene_loader.cpp не создавался | SceneLoader реализован как inline header; .cpp в plan был ошибочен | Нет impact — все изменения в .hpp |
| transport_type/domain_id → tags | Phase 8 реализует TransportPool; пока tags — временное хранение | Phase 8 будет читать из tags["transport_type"] |

## Deviations from Plan

### Summary

| Type | Count | Impact |
|------|-------|--------|
| Scope addtion | 1 | Minimal |

**Total impact:** Минимальный — файл scene_loader.cpp отсутствует, все изменения внесены в scene_loader.hpp.

### Auto-fixed Issues

**1. Deviation: scene_loader.cpp не существует**
- **Found during:** Task 1 (SceneData + YAML parsing)
- **Issue:** PLAN.md указывал `scene_loader.cpp` в `files_modified`, но файл не существует — вся реализация SceneLoader inline в `scene_loader.hpp`
- **Fix:** Изменения внесены в `scene_loader.hpp`
- **Files:** `workspace/s2_core/include/s2/scene_loader.hpp`
- **Verification:** Build 0 errors

## Issues Encountered

| Issue | Resolution |
|-------|------------|
| None | — |

## Next Phase Readiness

**Ready:**
- Entity model полностью реализован (Phase 02-01 + 02-02)
- SceneLoader загружает transport/tags/immune_to_effects из YAML
- role() enforcement активен — защита от неправильной конфигурации
- 100% тестов green

**Concerns:**
- `agent.domain_id` (legacy поле) ещё присутствует в struct — удаляется в Phase 8
- `signals` и `owned_zones` — пустые заглушки (Phase 12/14)

**Blockers:**
- None

---
*Phase: 02-unified-entity-model, Plan: 02*
*Completed: 2026-05-02*
