# Project State: S2 Simulator

**Last updated:** 2026-04-25
**Current phase:** 1 (Zone Visual & Control Layer)
**Phase status:** Not started

---

## Active Phase

### Phase 1 — Zone Visual & Control Layer

**Goal:** Зоны видны в UI, имеют lifecycle (strength/drift), управляются KernelCommands и генерируют сенсорные эффекты.

**Requirements:** ZONE-01, ZONE-02, ZONE-03, ZONE-04, ZONE-05

**Status:** Pending — план не создан. Запустить `/gsd-plan-phase 1`.

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
| Wire signal transport | SharedState vs EventBus | TBD в Phase 2 |
| HTTP SSE per-agent vs shared | Per-agent port vs multiplexed | TBD в Phase 5 |
| Entity base inheritance vs composition | Наследование vs слои | TBD в Phase 6 |

---

## Known Risks

- **Shell injection** в viz_server.cpp (popen SHA-1) — требует фикса до публичного релиза
- **Data race** HTTP thread vs sim thread на agents() — критично при Phase 5
- **ROS2 reload bug** — транспорт не переинициализируется при смене сцены

---

## Planning Artifacts

| File | Status |
|------|--------|
| `.planning/PROJECT.md` | ✓ Created |
| `.planning/config.json` | ✓ Created |
| `.planning/REQUIREMENTS.md` | ✓ Created (36 requirements) |
| `.planning/ROADMAP.md` | ✓ Created (8 phases) |
| `.planning/STATE.md` | ✓ This file |
| `.planning/codebase/` | ✓ 7 documents (map-codebase) |

---

*To start executing: `/gsd-plan-phase 1`*
