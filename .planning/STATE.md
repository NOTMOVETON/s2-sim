# Project State: S2 Simulator

**Last updated:** 2026-04-25
**Current phase:** 0 (Core Architecture Foundation)
**Phase status:** In progress — 3/5 plans complete

---

## Active Phase

### Phase 0 — Core Architecture Foundation

**Goal:** Правильный lifecycle плагинов, typed EventBus, WorldQuery API, Signal struct на Entity, 8-фазный tick lifecycle, полный набор KernelCommands.

**Requirements:** ARCH-01, ARCH-02, ARCH-03, ARCH-04, ARCH-05, ARCH-06, ARCH-07

**Status:** In progress. 3/5 планов выполнено.

**Context file:** `.planning/phases/00-core-architecture-foundation/00-CONTEXT.md`

---

## Completed Phases

Все предыдущие фичи реализованы вне GSD (tasks 1–28):

- SimEngine с тиковым циклом (8 фаз)
- IAgentPlugin: DiffDrive, GNSS, IMU, Battery, Gravity, Lidar, Color, JointVel
- ZoneSystem: сферы/боксы/цилиндры, эффекты ice/boost/lock/charging/conveyor/wind/teleport
- CollisionSystem: capsule/box/sphere vs static geometry
- GravityPlugin: snap к поверхности, свободное падение
- ROS2 транспорт: per-domain ноды, GNSS/IMU/Odometry/Lidar/TF
- Web визуализатор: Three.js + SSE, 30fps
- Редактор сцен: CRUD примитивов/агентов, undo/redo
- Браузер сцен: runtime reload
- URDF loader: кинематическое дерево, JointVelPlugin

---

## Open Decisions

| Decision | Options | Status |
|----------|---------|--------|
| Wire signal transport | SharedState vs EventBus | Решено: ARCH-03 — wire = Signal с range:infinite |
| HTTP SSE per-agent vs shared | Per-agent port vs multiplexed | TBD в Phase 5 |
| Entity base inheritance vs composition | Наследование vs слои | Решено: ENTY-04 — опциональные слои |
| SimBus backward compat | Удалить vs alias | Решено: using SimBus = EventBus в sim_bus.hpp — нулевая миграция |
| Signal::params тип | Typed struct vs json | Решено: nlohmann::json — произвольные параметры без типизации |
| KernelCommand тип | forward-declare vs placeholder struct | Решено: полный placeholder struct {} — std::vector требует complete type |
| KernelCommand include location | внутри namespace vs глобально | Решено: #include <s2/kernel_command.hpp> помещён в глобальную область plugin_base.hpp (до namespace s2) — иначе двойное namespace s2::s2::cmd |
| config_schema() return type | std::string vs nlohmann::json | Решено: nlohmann::json — избегает parse + JSON type safety |
| Миграция плагинов update() | Plan 02 vs Plan 06 | Решено: Plan 02 — необходимо для компиляции Docker build (D-03) |

---

## Known Risks

- **Shell injection** в viz_server.cpp (popen SHA-1) — требует фикса до публичного релиза (SEC-02 в v2)
- **Data race** HTTP thread vs sim thread на agents() — критично при Phase 5
- **ROS2 reload bug** — транспорт не переинициализируется при смене сцены (решается в TRAN-05)
- **on_reset bugs** — DiffDrive не сбрасывает external_linear_velocity_, Battery не сбрасывает заряд (решается в ARCH-01)

---

## Planning Artifacts

| File | Status |
|------|--------|
| `.planning/PROJECT.md` | ✓ Created |
| `.planning/config.json` | ✓ Created |
| `.planning/REQUIREMENTS.md` | ✓ Updated — 65 requirements (ARCH-01-07, ZONE-01-10, ACTR-01-06, PROP-01-03, PERC-01-07, TRAN-01-07, ENTY-01-08, MATL-01-08, VIZL-01-06, API-01-02) |
| `.planning/ROADMAP.md` | ✓ Updated — 9 phases (Phase 0 добавлена) |
| `.planning/STATE.md` | ✓ This file |
| `.planning/codebase/` | ✓ 7 documents (map-codebase) |
| `.planning/phases/00-core-architecture-foundation/00-01-SUMMARY.md` | ✓ Plan 00-01 complete — Signal struct + EventBus |
| `.planning/phases/00-core-architecture-foundation/00-02-SUMMARY.md` | ✓ Plan 00-02 complete — WorldQuery + IAgentPlugin lifecycle + PluginRole + PluginContext |
| `.planning/phases/00-core-architecture-foundation/00-03-SUMMARY.md` | ✓ Plan 00-03 complete — KernelCommand variant (16 команд) + test_kernel_command.cpp |

---

## Roadmap Summary

| Phase | Title | Req | Status |
|-------|-------|-----|--------|
| 0 | Core Architecture Foundation | ARCH-01–07 | Ready to execute (5 планов) |
| 1 | Zone Visual & Control Layer | ZONE-01–10 | Pending |
| 2 | Actor & Prop Foundation | ACTR-01–02,06, PROP-01–03 | Pending |
| 3 | Actor Ecosystem | ACTR-03–05 | Pending |
| 4 | Perception System | PERC-01–07 | Pending |
| 5 | Transport & Control Refactoring | TRAN-01–07, API-01–02 | Pending |
| 6 | Entity Model Unification | ENTY-01–08 | Pending |
| 7 | Material System | MATL-01–08 | Pending |
| 8 | Visualization Overhaul | VIZL-01–06 | Pending |

---

*To start executing: `/gsd-plan-phase 0`*
