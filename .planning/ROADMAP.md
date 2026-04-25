# Roadmap: S2 Simulator

**Created:** 2026-04-25
**Scope:** Perception, Actors, Transport & Architecture — 8 phases

---

## Overview

| Phase | Title | Requirements | Focus |
|-------|-------|-------------|-------|
| 1 | Zone Visual & Control Layer | ZONE-01–05 | Визуал зон, lifecycle, KernelCommands |
| 2 | Actor & Prop Foundation | ACTR-01–02, PROP-01–03 | IActorBehavior, Door, Props |
| 3 | Actor Ecosystem | ACTR-03–05 | Pedestrian, Conveyor, Elevator |
| 4 | Perception System | PERC-01–07 | ArUco, детекторы, деградация |
| 5 | Transport & Control Refactoring | TRAN-01–04 | HTTP-транспорт, REST API |
| 6 | Entity Model Unification | ENTY-01–04 | Capabilities, lifecycle, effect model |
| 7 | Material System | MATL-01–04 | MaterialTransfer, DirtPile |
| 8 | Visualization Overhaul | VIZL-01–04 | Multi-channel, IVizAdapter, VisualHint |

---

## Phase 1 — Zone Visual & Control Layer

**Goal:** Зоны видны в UI, имеют lifecycle (strength/drift), управляются KernelCommands и генерируют сенсорные эффекты.

**Requirements:** ZONE-01, ZONE-02, ZONE-03, ZONE-04, ZONE-05

**Tasks:**
- [ ] **1.1** — Zone inspector в UI редакторе (ZONE-01): форма создания/редактирования зоны, параметры эффекта, тип зоны
- [ ] **1.2** — VisualHint pipeline для зон (ZONE-02): цветовая индикация типа зоны, прозрачность, анимация активации агента внутри
- [ ] **1.3** — Сенсорные эффекты (ZONE-03): FogEffect (ухудшение дальности лидара), EMIEffect (деградация GNSS/IMU шум)
- [ ] **1.4** — Zone lifecycle (ZONE-04): поле strength (0–1) в ZoneConfig, команды SetZoneStrength, drift (смещение зоны за тик), auto-remove
- [ ] **1.5** — Zone KernelCommands (ZONE-05): SpawnZone / RemoveZone / ToggleZone, обработка в SimEngine

**Success criteria:**
- Зону можно создать через UI, она отображается в браузере с цветовой индикацией
- FogEffect ухудшает дальность лидара агента внутри зоны
- SpawnZone/RemoveZone работают через REST API
- Zone.strength меняется со временем, зона удаляется при strength=0

**Dependencies:** None (baseline zone system already exists)

---

## Phase 2 — Actor & Prop Foundation

**Goal:** Система акторов с IActorBehavior, DoorBehavior с proximity-триггером, система пропов с attach/detach.

**Requirements:** ACTR-01, ACTR-02, PROP-01, PROP-02, PROP-03

**Tasks:**
- [ ] **2.1** — IActorBehavior интерфейс (ACTR-01): initialize/update/on_interact, FSM утилита (State + transitions + timer)
- [ ] **2.2** — ActorRegistry: хранение акторов в SimEngine, тиковый цикл для behavior.update()
- [ ] **2.3** — DoorBehavior (ACTR-02): FSM (CLOSED/OPENING/OPEN/CLOSING), коллизия при CLOSED, proximity-триггер (агент в радиусе → OPENING), wire-сигнал
- [ ] **2.4** — Prop структура (PROP-01): PropConfig (YAML), movable flag, collision shape, signal list
- [ ] **2.5** — AttachObject / DetachObject KernelCommands (PROP-02): prop прикрепляется к entity, следует за ней
- [ ] **2.6** — GrabberPlugin (PROP-03): плагин агента, grab/release команды, ограничения дальности/веса, SharedState contribution

**Success criteria:**
- Дверь создаётся в YAML сцены, открывается при приближении агента
- Prop можно прикрепить к агенту через REST API
- GrabberPlugin публикует статус захвата в SharedState

**Dependencies:** Phase 1 (zone infrastructure patterns)

---

## Phase 3 — Actor Ecosystem

**Goal:** Пешеходы, конвейеры и лифты как первоклассные акторы.

**Requirements:** ACTR-03, ACTR-04, ACTR-05

**Tasks:**
- [ ] **3.1** — PedestrianBehavior (ACTR-03): список waypoints, LinearSteering к следующей точке, ObstacleAvoidance (separation от других агентов/акторов), loop/ping-pong режимы
- [ ] **3.2** — ConveyorActor (ACTR-04): attached conveyor зона, wire signal для направления и скорости, реверс по сигналу
- [ ] **3.3** — ElevatorBehavior (ACTR-05): FSM (IDLE/MOVING_UP/MOVING_DOWN/OPEN), floor positions, call API
- [ ] **3.4** — ElevatorUserPlugin: плагин агента для вызова лифта и заезда, SharedState contribution (elevator_request)
- [ ] **3.5** — Интеграционные тесты для каждого актора (маршрут пешехода, конвейер с wire, лифт с агентом)

**Success criteria:**
- Пешеход патрулирует маршрут и огибает препятствия
- Конвейер меняет направление по wire-сигналу
- Агент вызывает лифт, заезжает и перемещается на другой этаж

**Dependencies:** Phase 2 (IActorBehavior, ActorRegistry)

---

## Phase 4 — Perception System

**Goal:** Агенты воспринимают ArUco-маркеры, других агентов, акторов и зоны. Ray-zone транзит деградирует сигнал.

**Requirements:** PERC-01–07

**Tasks:**
- [ ] **4.1** — Flat зоны (PERC-01): ориентация через quaternion/normal + толщина, корректное пересечение агента
- [ ] **4.2** — ArUcoSignal компонент (PERC-02): ArUcoSignal{id, pose_offset, size} на любой Entity (агент/актор/prop)
- [ ] **4.3** — ArucoDetectorPlugin (PERC-03): dual-zone (read_range > detect_range), угловая видимость, вывод в SharedState + ROS2 ArucoArray
- [ ] **4.4** — AgentDetectorPlugin (PERC-04): список видимых агентов (id, distance, bearing), опциональная LoS-проверка через CollisionSystem
- [ ] **4.5** — ActorDetectorPlugin (PERC-05): список видимых акторов (id, type, distance)
- [ ] **4.6** — ZoneDetectorPlugin (PERC-06): список зон в радиусе (id, type, distance, strength)
- [ ] **4.7** — Ray-zone transit (PERC-07): KernelQuery для трассировки луча через зоны, коэффициент ослабления сигнала per zone type

**Success criteria:**
- ArucoDetectorPlugin обнаруживает маркер в пределах detect_range
- AgentDetectorPlugin публикует список соседей через ROS2 или SharedState
- Сигнал ArUco ослабляется при прохождении через fog-зону

**Dependencies:** Phase 1 (zone types), Phase 2 (Actor entities with signals)

---

## Phase 5 — Transport & Control Refactoring

**Goal:** HTTP-транспорт без ROS2, per-agent transport config, единый REST Control API, sim_time для всех.

**Requirements:** TRAN-01–04

**Tasks:**
- [ ] **5.1** — ITransportAdapter интерфейс: publish(topic, json) / subscribe(topic, callback) / spin_once()
- [ ] **5.2** — HttpTransportAdapter (TRAN-01): SSE-сервер на per-agent порту, HTTP POST endpoint для команд, long-poll
- [ ] **5.3** — ROS2TransportAdapter: обёртка над текущим Ros2Transport в ITransportAdapter
- [ ] **5.4** — StubTransportAdapter: заглушка для тестов, in-memory очередь
- [ ] **5.5** — Per-agent transport config в YAML (TRAN-02): поле transport в AgentConfig, transport pool (shared ros2 node per domain)
- [ ] **5.6** — TransportRegistry в SimEngine: создание и хранение адаптеров по конфигу
- [ ] **5.7** — Unified REST Control API (TRAN-03): POST /sim/control, POST /world/spawn, POST /agent/{id}/plugin, POST /scene/load; удаление legacy GET /command?cmd=
- [ ] **5.8** — sim_time публикация (TRAN-04): SimClock → publish на /sim_time (ROS2 Clock msg + HTTP SSE поток + SharedState)

**Success criteria:**
- Агент с transport: http управляется без запущенного ROS2
- Legacy endpoints возвращают 410 Gone с подсказкой
- sim_time публикуется в ROS2 и доступен через HTTP

**Dependencies:** Phase 1–4 (все существующие плагины должны работать через новый transport)

---

## Phase 6 — Entity Model Unification

**Goal:** Capabilities/tags/immunity, plugin lifecycle hooks, унифицированная модель эффектов.

**Requirements:** ENTY-01–04

**Tasks:**
- [ ] **6.1** — Capabilities enum + effect_tags (ENTY-01): стандартный список (surface_contact, wheeled, airborne, has_battery, optical_sensor...), теги на EffectPlugin (tags: [electric, thermal]), immune_to_effects в EntityConfig
- [ ] **6.2** — Capability matching в ZoneSystem: эффект применяется только если entity имеет требуемые capabilities
- [ ] **6.3** — Plugin lifecycle hooks (ENTY-02): on_reset() вызывается при сбросе симуляции, on_spawn() при создании агента, on_despawn() при удалении
- [ ] **6.4** — Effect trigger model (ENTY-03): поле trigger: WHILE_INSIDE / ON_ENTER / ON_EXIT в EffectConfig, отдельный вызов для entry/exit событий
- [ ] **6.5** — Effect action model: action: CONTRIBUTION / STATE_CHANGE / SENSOR_MOD с соответствующими параметрами
- [ ] **6.6** — Entity base model (ENTY-04): EntityBase с полями id/name/pose/capabilities/tags, Agent/Actor/Prop наследуют или содержат базу; опциональные слои через флаги
- [ ] **6.7** — Per-link immunity (из my_plan): link_overrides в AgentConfig для добавления immune_to_effects к конкретным URDF-звеньям

**Success criteria:**
- Зона с tag: [electric] не действует на агентов с immune_to_effects: [electric]
- on_reset() вызывается для всех плагинов при /sim/reset
- ON_ENTER эффект срабатывает ровно один раз при входе в зону

**Dependencies:** Phase 1 (ZoneSystem), Phase 2–3 (Actor entities)

---

## Phase 7 — Material System

**Goal:** Общий механизм передачи материалов между сущностями, DirtPile актор с деформацией.

**Requirements:** MATL-01–04

**Tasks:**
- [ ] **7.1** — IMaterialSource / IMaterialReceiver (MATL-02): опциональные методы в IAgentPlugin и IActorBehavior; get_available_material() / receive_material()
- [ ] **7.2** — MaterialTransfer KernelCommand (MATL-01): {source_id, receiver_id, material_type, amount}; обработка в SimEngine через IMaterialSource/Receiver
- [ ] **7.3** — DirtPile actor (MATL-03): 2D-grid высот (float[][]), friction_angle, settling(), volume conservation; IDeformable интерфейс для on_deform()
- [ ] **7.4** — DeformMaterial KernelCommand: blade/tool плагин отправляет команду DirtPile, передаёт shape + volume
- [ ] **7.5** — Micro-pile merging: когда two DirtPile пересекаются, объединение в один
- [ ] **7.6** — VisualHint для материалов (MATL-04): анимация потока при MaterialTransfer, визуальное обновление grid при деформации

**Success criteria:**
- BucketPlugin агента набирает материал из DirtPile через MaterialTransfer
- DirtPile деформируется при взаимодействии с blade агентом
- Volume conservation: суммарный объём не меняется при перемещении материала

**Dependencies:** Phase 6 (Entity model, IActorBehavior), Phase 2 (ActorRegistry)

---

## Phase 8 — Visualization Overhaul

**Goal:** Multi-channel стриминг, абстрактный IVizAdapter, VisualHint pipeline для плагинов.

**Requirements:** VIZL-01–04

**Tasks:**
- [ ] **8.1** — Multi-channel SSE (VIZL-01): три SSE endpoint — /stream/core (30fps, agent poses + zone states), /stream/heavy (2–5fps, lidar/trajectory), /stream/static (on-demand, scene geometry)
- [ ] **8.2** — Three.js клиент обновлён для multi-channel: подписывается на разные потоки, приоритизирует core
- [ ] **8.3** — IVizAdapter интерфейс (VIZL-02): publish_snapshot(CoreSnapshot), publish_heavy(HeavyData), publish_static(StaticScene)
- [ ] **8.4** — WebVizAdapter: текущий SSE-сервер обёрнут в IVizAdapter
- [ ] **8.5** — NullVizAdapter: заглушка для тестов (no-op)
- [ ] **8.6** — FileVizAdapter: запись снапшотов в файл (для replay/анализа)
- [ ] **8.7** — VisualHint pipeline для плагинов (VIZL-03): IAgentPlugin::get_visual_hints() → список VisualHint, SimEngine собирает и включает в snapshot
- [ ] **8.8** — Scripted behavior плагин (VIZL-04): плагин с ролью behavior, воспроизводит записанную траекторию из YAML

**Success criteria:**
- Core stream 30fps не лагает от больших данных лидара
- Trajectory/lidar обновляются в отдельном потоке без влияния на core FPS
- VisualHint от BatteryPlugin отображает уровень заряда в браузере
- NullVizAdapter используется в юнит-тестах без запуска HTTP

**Dependencies:** Phase 5 (transport refactoring), Phase 4 (sensor data), Phase 6 (entity model)

---

## Milestones

| Milestone | After Phase | Description |
|-----------|-------------|-------------|
| M1: Rich World | 3 | Зоны с lifecycle, двери, пешеходы, конвейеры, лифты |
| M2: Perception | 4 | Полная система восприятия с ArUco, детекторами, деградацией |
| M3: Transport | 5 | HTTP-транспорт, единый REST API, sim_time |
| M4: Architecture | 6 | Унифицированная модель Entity/Effect/Capabilities |
| M5: Materials | 7 | Передача материалов, DirtPile, деформация |
| M6: Visualization | 8 | Multi-channel стриминг, IVizAdapter, VisualHint |

---

*Roadmap created: 2026-04-25*
*Start with: `/gsd-plan-phase 1`*
