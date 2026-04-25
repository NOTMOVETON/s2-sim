---
phase: 00-core-architecture-foundation
plan: "06"
subsystem: zone-system
tags: [cpp17, zone-system, event-bus, events, gap-closure]

# Dependency graph
requires:
  - phase: 00-05
    provides: SimEngine::reset(), SceneLoader ACTUATION validation
  - phase: 00-01
    provides: EventBus с ZoneEntered/ZoneExited struct-ами

provides:
  - ZoneSystem публикует event::ZoneEntered при входе агента в зону
  - ZoneSystem публикует event::ZoneExited при выходе агента из зоны
  - Интеграционные тесты ZoneEntered/ZoneExited (2 новых теста)
affects:
  - phase-1-zone-visual-control
  - phase-2-actor-prop-foundation

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Двойная публикация событий: legacy (AgentEnteredZone) + новый (ZoneEntered) — нулевой разрыв совместимости"

key-files:
  created: []
  modified:
    - workspace/s2_core/src/zone_system.cpp
    - workspace/s2_core/tests/test_zone_system.cpp

key-decisions:
  - "Gap ARCH-04 закрыт: ZoneEntered/ZoneExited публикуются сразу после legacy-событий без удаления последних"
  - "EntityId и AgentId оба uint32_t — agent.id передаётся в entity_id без приведения типов"

patterns-established:
  - "Backward-compat паттерн: новое событие публикуется следующей строкой после legacy-события"

requirements-completed: [ARCH-04]

# Metrics
duration: 5min
completed: 2026-04-26
---

# Phase 0 Plan 06: Gap ARCH-04 Closure Summary

**ZoneSystem теперь публикует event::ZoneEntered и event::ZoneExited при переходах агентов через границы зон, с полной backward-compat через legacy AgentEnteredZone/AgentExitedZone**

## Performance

- **Duration:** ~5 min
- **Started:** 2026-04-26T00:12:00Z
- **Completed:** 2026-04-26T00:16:00Z
- **Tasks:** 2
- **Files modified:** 2

## Accomplishments

- Добавлена публикация event::ZoneEntered в ZoneSystem::on_agent_enter() после legacy-события
- Добавлена публикация event::ZoneExited в ZoneSystem::on_agent_exit() после legacy-события
- Написаны 2 интеграционных теста, подтверждающих zone_id и entity_id в событиях
- Все 11+2 = 13 тестов ZoneSystem зелёные

## Task Commits

1. **Задача 1: публикация ZoneEntered/ZoneExited** — `11e5fd8` (feat)
2. **Задача 2: интеграционные тесты** — `7e8e9b6` (test)

## Files Created/Modified

- `workspace/s2_core/src/zone_system.cpp` — добавлены 2 строки bus.publish(event::ZoneEntered/ZoneExited)
- `workspace/s2_core/tests/test_zone_system.cpp` — добавлены тесты ZoneSystem_ZoneEnteredEvent и ZoneSystem_ZoneExitedEvent

## Decisions Made

- Legacy-события AgentEnteredZone/AgentExitedZone оставлены без изменений — нулевая миграция для существующих подписчиков
- Новые события публикуются сразу после legacy (в той же функции) — атомарный порядок гарантирован

## Deviations from Plan

None — план выполнен точно по спецификации.

## Issues Encountered

None.

## Known Stubs

None.

## Threat Flags

Нет новых сетевых точек входа, auth-путей или внешних доверительных границ.

## Next Phase Readiness

- ARCH-04 полностью закрыт: Phase 1 (ToggleZone ON_ENTER/ON_EXIT) и Phase 2 (EventReactor) могут подписываться на event::ZoneEntered
- Phase 0 все gap-ы устранены; переход к Phase 1 разблокирован

## Self-Check: PASSED

- workspace/s2_core/src/zone_system.cpp содержит bus.publish(event::ZoneEntered) — FOUND
- workspace/s2_core/src/zone_system.cpp содержит bus.publish(event::ZoneExited) — FOUND
- workspace/s2_core/tests/test_zone_system.cpp содержит ZoneSystem_ZoneEnteredEvent — FOUND
- workspace/s2_core/tests/test_zone_system.cpp содержит ZoneSystem_ZoneExitedEvent — FOUND
- Коммит 11e5fd8 существует — FOUND
- Коммит 7e8e9b6 существует — FOUND
- Docker tests: 100% passed, code 0 — PASSED

---
*Phase: 00-core-architecture-foundation*
*Completed: 2026-04-26*
