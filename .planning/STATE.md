---
gsd_state_version: 1.0
milestone: v1.0
milestone_name: milestone
current_phase: 2 (Actor & Prop Foundation)
status: executing
last_updated: "2026-04-26T19:18:00.000Z"
last_activity: "2026-04-26 — Completed Plan 02-02: Engine Integration (phase2_actors, phase6_attachments, KernelCommands, SceneLoader)"
progress:
  total_phases: 3
  completed_phases: 2
  total_plans: 18
  completed_plans: 15
  percent: 83
---

# Project State: S2 Simulator

**Last updated:** 2026-04-26 (Phase 2 executing)
**Current phase:** 2 (Actor & Prop Foundation)
**Phase status:** EXECUTING — 2/5 plans complete, Wave 2 in progress

---

## Active Phase

### Phase 0 — Core Architecture Foundation

**Goal:** Правильный lifecycle плагинов, typed EventBus, WorldQuery API, Signal struct на Entity, 8-фазный tick lifecycle, полный набор KernelCommands.

**Requirements:** ARCH-01, ARCH-02, ARCH-03, ARCH-04, ARCH-05, ARCH-06, ARCH-07

**Status:** COMPLETE. 6/6 планов выполнено (включая gap-closure Plan 06 — ARCH-04).

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
| Gap ARCH-04 closure | Публиковать новые события после legacy vs переписать | Решено: двойная публикация — backward compat нулевой |
| Wire signal transport | SharedState vs EventBus | Решено: ARCH-03 — wire = Signal с range:infinite |
| HTTP SSE per-agent vs shared | Per-agent port vs multiplexed | TBD в Phase 5 |
| Entity base inheritance vs composition | Наследование vs слои | Решено: ENTY-04 — опциональные слои |
| SimBus backward compat | Удалить vs alias | Решено: using SimBus = EventBus в sim_bus.hpp — нулевая миграция |
| Signal::params тип | Typed struct vs json | Решено: nlohmann::json — произвольные параметры без типизации |
| KernelCommand тип | forward-declare vs placeholder struct | Решено: полный placeholder struct {} — std::vector требует complete type |
| KernelCommand include location | внутри namespace vs глобально | Решено: #include <s2/kernel_command.hpp> помещён в глобальную область plugin_base.hpp (до namespace s2) — иначе двойное namespace s2::s2::cmd |
| config_schema() return type | std::string vs nlohmann::json | Решено: nlohmann::json — избегает parse + JSON type safety |
| Миграция плагинов update() | Plan 02 vs Plan 06 | Решено: Plan 02 — необходимо для компиляции Docker build (D-03) |
| NullWorldQuery расположение | отдельный файл vs вложенный класс | Решено: private вложенный класс в SimEngine — не загрязняет namespace s2 |
| PluginContext хранение | поле SimEngine vs локальное в фазе | Решено: локальное в каждой фазе — разные tick_cmds буферы для phase3/phase4/phase5 |
| command_queue_ drain pattern | lock всё время vs swap+unlock | Решено: lock-swap drain — swap под mutex, обработка без блокировки (минимальное contention) |
| BOUNDING detection расширение | Только sphere vs все формы | Решено: расширять все формы (sphere/AABB/cylinder) на agent.bounding.radius |
| SpawnZone attached_to конвертация | string vs uint32_t | Решено: std::to_string(EntityId) — соответствует update_owned_zones_positions() |
| ZoneSpawnSystem EventBus подписки | Все события vs только нужные | Решено: 6 типов (ZoneEntered/Exited, SignalActivated, GrabSucceeded/Failed, ActorStateChanged) |
| StateChangeTrigger id тип | string vs ActorId (uint32_t) | Решено: ActorId — точно соответствует event::ActorStateChanged.actor |
| zones_to_destroy pattern | Удаление inline vs post-iteration | Решено: zones_to_destroy set, удаление после итерации — безопасно (Pitfall 4) |
| InZoneResult vs bool | agent_in_zone bool vs расширенный результат | Решено: InZoneResult struct + inline делегат agent_in_zone() — backward compat нулевой |
| Agent-прокси для плагинов актора | Agent& vs новый тип | Решено: Agent proxy с id+world_pose в phase2_actors() — Phase 6 ENTY унифицирует через EntityBase |
| BehaviorFactory в SceneLoader | Inline vs отдельный setter | Решено: третий параметр load() с default {} — нулевая обратная совместимость |
| Pose composition в phase6 | Матрица вращения vs сложение | Решено: упрощённое сложение поз (x+x, yaw+yaw) — Phase 6 ENTY заменит на Transform3D |
| Actor explicit constructors | User-declared vs implicit | Решено: implicit (без явных конструкторов) — сохраняет aggregate-инициализацию в C++17; move-only семантика обеспечена unique_ptr полями |
| Zone.spawn_time хранение | В Zone struct vs computed | Решено: поле spawn_time в Zone struct, заполняется при add_zone() |
| load_zone_templates API | Параметр load_world() vs отдельный метод | Решено: отдельный метод load_zone_templates() — backward compat с существующими тестами |

---

## Known Risks

- **Shell injection** в viz_server.cpp (popen SHA-1) — требует фикса до публичного релиза (SEC-02 в v2)
- **Data race** HTTP thread vs sim thread на agents() — критично при Phase 5
- **ROS2 reload bug** — транспорт не переинициализируется при смене сцены (решается в TRAN-05)
- **on_reset bugs** — RESOLVED (Plan 00-05): DiffDrive сбрасывает external_linear_velocity_, Battery сбрасывает заряд до initial_level_

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
| `.planning/phases/00-core-architecture-foundation/00-04-SUMMARY.md` | ✓ Plan 00-04 complete — SimEngine 8-фазный tick lifecycle + command_queue_ + NullWorldQuery |
| `.planning/phases/00-core-architecture-foundation/00-05-SUMMARY.md` | ✓ Plan 00-05 complete — on_reset() для DiffDrive/Battery + SceneLoader ACTUATION validation |
| `.planning/phases/00-core-architecture-foundation/00-06-SUMMARY.md` | ✓ Plan 00-06 complete — Gap ARCH-04 закрыт: ZoneEntered/ZoneExited публикуются ZoneSystem |
| `.planning/phases/01-zone-visual-control-layer/01-01-SUMMARY.md` | ✓ Plan 01-01 complete — Zone data types: DetectionMode, ZoneLifecycle, SelfDestructPolicy, ZoneSnapshot strength/visual_hints |
| `.planning/phases/01-zone-visual-control-layer/01-02-SUMMARY.md` | ✓ Plan 01-02 complete — FogEffect + EMIEffect сенсорные эффекты |
| `.planning/phases/01-zone-visual-control-layer/01-03-SUMMARY.md` | ✓ Plan 01-03 complete — ZoneSystem lifecycle, detection mode, self_destruct, owned_zones |
| `.planning/phases/01-zone-visual-control-layer/01-04-SUMMARY.md` | ✓ Plan 01-04 complete — Zone KernelCommands (SpawnZone/DespawnZone/ToggleZone) + ZoneSpawnSystem |
| `.planning/phases/01-zone-visual-control-layer/01-05-SUMMARY.md` | ✓ Plan 01-05 complete — SceneLoader YAML extensions + SimEngine snapshot/ZoneSpawnSystem integration |
| `.planning/phases/01-zone-visual-control-layer/01-06-SUMMARY.md` | ✓ Plan 01-06 complete — Zone Inspector UI + VisualHint рендеринг |
| `.planning/phases/01-zone-visual-control-layer/01-07-SUMMARY.md` | ✓ Plan 01-07 complete — Gap closure: REST spawn/despawn + UI fetch + contact_link |
| `.planning/phases/02-actor-prop-foundation/02-01-SUMMARY.md` | ✓ Plan 02-01 complete — IActorBehavior + ActorFSM + Actor/Prop struct extension |
| `.planning/phases/02-actor-prop-foundation/02-02-SUMMARY.md` | ✓ Plan 02-02 complete — Engine Integration (phase2_actors, phase6_attachments, KernelCommands, SceneLoader) |

---

## Roadmap Summary

| Phase | Title | Req | Status |
|-------|-------|-----|--------|
| 0 | Core Architecture Foundation | ARCH-01–07 | COMPLETE (5/5 планов) |
| 1 | Zone Visual & Control Layer | ZONE-01–10 | Pending |
| 2 | Actor & Prop Foundation | ACTR-01–02,06, PROP-01–03 | Pending |
| 3 | Actor Ecosystem | ACTR-03–05 | Pending |
| 4 | Perception System | PERC-01–07 | Pending |
| 5 | Transport & Control Refactoring | TRAN-01–07, API-01–02 | Pending |
| 6 | Entity Model Unification | ENTY-01–08 | Pending |
| 7 | Material System | MATL-01–08 | Pending |
| 8 | Visualization Overhaul | VIZL-01–06 | Pending |

---

---

### Quick Tasks Completed

| # | Description | Date | Commit | Directory |
|---|-------------|------|--------|-----------|
| 260426-001 | Исправить лидар в тумане, EMI эффект и Zone Tab UI | 2026-04-26 | 2cee76f | [260426-001-fix-zone-ui-and-effects](.planning/quick/260426-001-fix-zone-ui-and-effects/) |
| 260426-002 | Редактирование зон: backend + гизмо + форма | 2026-04-26 | 4f5577e | [260426-002-zone-editor-backend](.planning/quick/260426-002-zone-editor-backend/) |

---

**Last activity:** 2026-04-26 — Completed Plan 02-02: Engine Integration (phase2_actors, phase6_attachments, KernelCommands, SceneLoader)

**Executing Phase:** 2 (Actor & Prop Foundation) -- 5 plans, 3 waves -- 2/5 complete
