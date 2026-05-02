---
phase: 04-rest-api-ivizadapter
plan: 01
subsystem: core
tags: [viz, adapter, interface, cpp, cmake]

requires:
  - phase: 03-kernel-commands-queue
    provides: SimEngine with KernelCommands, SimBus event types

provides:
  - IVizAdapter interface in s2_core (no VizServer dependency)
  - NullVizAdapter for headless/test use
  - VizRegistry composite adapter
  - WebVizAdapter wrapping VizServer in s2_visualizer
  - SimEngine decoupled from VizServer

affects: 04-02-PLAN (REST API uses IVizAdapter pattern), future transport phases

tech-stack:
  added: []
  patterns: [Adapter pattern for visualization, Composite via VizRegistry]

key-files:
  created:
    - workspace/s2_core/include/s2/viz_adapter.hpp
    - workspace/s2_core/include/s2/null_viz_adapter.hpp
    - workspace/s2_core/include/s2/viz_registry.hpp
    - workspace/s2_visualizer/src/web_viz_adapter.hpp
    - workspace/s2_visualizer/src/web_viz_adapter.cpp
  modified:
    - workspace/s2_core/include/s2/sim_engine.hpp
    - workspace/s2_core/CMakeLists.txt
    - workspace/s2_visualizer/CMakeLists.txt
    - workspace/s2_visualizer/src/main.cpp
  deleted:
    - workspace/s2_core/src/sim_engine_viz.cpp
    - workspace/s2_visualizer/src/sim_engine_viz_impl.cpp

key-decisions:
  - "publish_viz() inlined in sim_engine.hpp — no more ODR stub/override split"
  - "WebVizAdapter::server() exposes VizServer& for force_broadcast_* calls in main.cpp"

patterns-established:
  - "VizServer access via WebVizAdapter::server() for non-adapter methods"
  - "IVizAdapter* in SimEngine — set_viz_adapter(nullptr) = headless mode"

duration: ~30min
started: 2026-05-02T00:00:00Z
completed: 2026-05-02T00:00:00Z
---

# Phase 4 Plan 01: IVizAdapter Abstraction Summary

**IVizAdapter interface introduced in s2_core, SimEngine decoupled from VizServer; WebVizAdapter wraps VizServer in s2_visualizer; ODR stub/override pattern eliminated.**

## Performance

| Metric | Value |
|--------|-------|
| Duration | ~30 min |
| Tasks | 4 completed |
| Files created | 5 |
| Files modified | 4 |
| Files deleted | 2 |

## Acceptance Criteria Results

| Criterion | Status | Notes |
|-----------|--------|-------|
| AC-1: s2_core не зависит от VizServer | Pass | `grep -r "VizServer" workspace/s2_core/` → 0 results |
| AC-2: SimEngine работает с NullVizAdapter | Pass | Compiles cleanly, no HTTP deps in s2_core headers |
| AC-3: WebVizAdapter делегирует в VizServer | Pass | Delegation verified via code review; build passes |
| AC-4: Сборка и тесты проходят | Pass | Build exit 0, 2/2 tests passed |

## Accomplishments

- Broke ODR stub/override split: `sim_engine_viz.cpp` (s2_core) + `sim_engine_viz_impl.cpp` (s2_visualizer) → single inline `publish_viz()` in header
- SimEngine now testable without any HTTP server via `NullVizAdapter`
- VizRegistry enables multiple simultaneous viz adapters (future: WebViz + REST)

## Files Created/Modified

| File | Change | Purpose |
|------|--------|---------|
| `s2_core/include/s2/viz_adapter.hpp` | Created | IVizAdapter interface |
| `s2_core/include/s2/null_viz_adapter.hpp` | Created | No-op adapter for tests/headless |
| `s2_core/include/s2/viz_registry.hpp` | Created | Composite: fans out to N adapters |
| `s2_visualizer/src/web_viz_adapter.hpp` | Created | WebVizAdapter declaration |
| `s2_visualizer/src/web_viz_adapter.cpp` | Created | WebVizAdapter wrapping VizServer |
| `s2_core/include/s2/sim_engine.hpp` | Modified | IVizAdapter*, inline publish_viz() |
| `s2_core/CMakeLists.txt` | Modified | Removed sim_engine_viz.cpp |
| `s2_visualizer/CMakeLists.txt` | Modified | Removed sim_engine_viz_impl.cpp, added web_viz_adapter.cpp |
| `s2_visualizer/src/main.cpp` | Modified | WebVizAdapter instead of VizServer |
| `s2_core/src/sim_engine_viz.cpp` | Deleted | ODR stub obsolete |
| `s2_visualizer/src/sim_engine_viz_impl.cpp` | Deleted | ODR override obsolete |

## Decisions Made

| Decision | Rationale | Impact |
|----------|-----------|--------|
| `publish_viz()` inline in header | ODR split was a fragile workaround; inline cleaner | s2_core self-contained |
| `WebVizAdapter::server()` exposes `VizServer&` | `force_broadcast_*` и `set_command_handler` не входят в IVizAdapter — удаляются в 04-02 | main.cpp работает без изменений архитектуры до 04-02 |

## Deviations from Plan

### Summary

| Type | Count | Impact |
|------|-------|--------|
| Auto-fixed | 1 | Essential — без этого сборка не проходила |

### Auto-fixed Issues

**1. `set_command_handler` not covered in plan**
- **Found during:** Task 4 (main.cpp update)
- **Issue:** `g_viz->set_command_handler(...)` — plan описал `force_broadcast_*` паттерн но не упомянул `set_command_handler`
- **Fix:** `g_viz->server().set_command_handler(...)` — consistent с `force_broadcast_*` паттерном
- **Verification:** Build exit 0

## Next Phase Readiness

**Ready:**
- s2_core чистый от VizServer — 04-02 может добавить RestAdapter через тот же IVizAdapter
- VizRegistry готов для фанаута WebViz + REST
- NullVizAdapter доступен для тестов без HTTP

**Concerns:**
- `VizCommandHandler` и `SimEngineCommandAdapter` в main.cpp ещё живут — удаляются в 04-02

**Blockers:** None

---
*Phase: 04-rest-api-ivizadapter, Plan: 01*
*Completed: 2026-05-02*
