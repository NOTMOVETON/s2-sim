---
phase: 04-rest-api-ivizadapter
plan: 02
subsystem: api
tags: [rest, http, kernel-commands, viz-server, sim-engine]

requires:
  - phase: 04-01
    provides: IVizAdapter, WebVizAdapter, NullVizAdapter — VizServer за интерфейсом
  - phase: 03-kernel-commands-queue
    provides: enqueue(KernelCommand), KernelCommand types

provides:
  - RestApiServer: HTTP REST сервер на порту viz+1, маршрутизирует команды через enqueue()
  - VizServer без command routing: чистый SSE/статика
  - SimEngineCommandAdapter удалён полностью

affects: ["05-scripted-behavior", "transport", "web-frontend"]

tech-stack:
  added: []
  patterns:
    - "REST POST → engine->enqueue(KernelCommand) — никаких прямых вызовов"
    - "VizServer = только SSE + статика; команды идут через отдельный порт"

key-files:
  created:
    - workspace/s2_visualizer/src/rest_api_server.hpp
    - workspace/s2_visualizer/src/rest_api_server.cpp
    - workspace/s2_core/tests/test_rest_api.cpp
  modified:
    - workspace/s2_visualizer/src/viz_server.hpp
    - workspace/s2_visualizer/src/viz_server.cpp
    - workspace/s2_visualizer/src/main.cpp
    - workspace/s2_visualizer/CMakeLists.txt
    - workspace/s2_core/CMakeLists.txt

key-decisions:
  - "REST порт = viz_port + 1 (default 1938) — конфигурируется через сцену"
  - "scenes_dir = parent_path(scene_path) — без хардкода путей"
  - "ZoneId = std::string, требует отдельный extract_zone_id()"

patterns-established:
  - "HTTP команды: parse → dispatch → enqueue → send_ok"
  - "GET read-only: читать state без мьютекса (eventual consistency)"

duration: ~30min
started: 2026-05-02T00:00:00Z
completed: 2026-05-02T01:00:00Z
---

# Phase 4 Plan 02: RestApiServer + VizCommandHandler Removal Summary

**Создан RestApiServer (порт viz+1, 19 эндпоинтов через enqueue); удалены VizCommandHandler и SimEngineCommandAdapter; VizServer = только SSE/статика.**

## Performance

| Metric | Value |
|--------|-------|
| Duration | ~30 мин |
| Completed | 2026-05-02 |
| Tasks | 3 выполнено |
| Files modified | 5 изменено, 3 создано |

## Acceptance Criteria Results

| Criterion | Status | Notes |
|-----------|--------|-------|
| AC-1: REST команды через KernelCommands | Pass | POST /sim/pause → enqueue(PauseSim{}), тест подтверждает |
| AC-2: VizCommandHandler удалён | Pass | grep 0 результатов |
| AC-3: VizServer без command routing | Pass | serve_http() — только OPTIONS/plugin-registry/статика |
| AC-4: Сборка и тесты | Pass | docker build exit 0, 100% тестов пройдено |

## Accomplishments

- RestApiServer: 19 эндпоинтов (sim control, entities, zones, agents, scenes), socket/bind/listen, CORS, detached threads per client
- VizCommandHandler + SimEngineCommandAdapter (~350 строк) полностью удалены
- VizServer упрощён: serve_http() больше не маршрутизирует команды
- Тест `test_rest_api.cpp`: PauseSim + SetSpeed через enqueue

## Files Created/Modified

| File | Change | Purpose |
|------|--------|---------|
| `s2_visualizer/src/rest_api_server.hpp` | Создан | Объявление RestApiServer |
| `s2_visualizer/src/rest_api_server.cpp` | Создан | Реализация: HTTP роутинг → enqueue |
| `s2_core/tests/test_rest_api.cpp` | Создан | Интеграционный тест enqueue-логики |
| `s2_visualizer/src/viz_server.hpp` | Изменён | Удалены VizCommandHandler, set_command_handler, poll_commands |
| `s2_visualizer/src/viz_server.cpp` | Изменён | Удалена вся command routing логика из serve_http() |
| `s2_visualizer/src/main.cpp` | Изменён | Удалён SimEngineCommandAdapter, добавлен RestApiServer |
| `s2_visualizer/CMakeLists.txt` | Изменён | Добавлен rest_api_server.cpp |
| `s2_core/CMakeLists.txt` | Изменён | Добавлен test_rest_api.cpp |

## Decisions Made

| Decision | Rationale | Impact |
|----------|-----------|--------|
| REST порт = viz_port + 1 | Автоматически 1938 при дефолтном viz 1937 | Конфигурируется через viz_config.port |
| scenes_dir из parent_path(scene_path) | Нет хардкода путей, работает в Docker | Передаётся в RestApiServer конструктором |
| ZoneId отдельный extract | ZoneId = std::string, не uint32_t | extract_zone_id() vs extract_entity_id() |
| Нет force_broadcast после REST команд | Следующий viz tick покажет изменение | Eventual consistency; упрощает RestApiServer |

## Deviations from Plan

None — план выполнен точно как написан.

## Issues Encountered

None.

## Next Phase Readiness

**Ready:**
- Phase 4 полностью завершена: IVizAdapter + RestApiServer
- VizServer = чистый SSE/статика, команды через отдельный порт
- KernelCommands покрывают все операции sim control

**Concerns:**
- `agent.domain_id` legacy поле остаётся до Phase 8

**Blockers:**
- None

---
*Phase: 04-rest-api-ivizadapter, Plan: 02*
*Completed: 2026-05-02*
