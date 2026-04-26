# Roadmap: S2 Simulator

**Created:** 2026-04-25
**Updated:** 2026-04-26 — Phase 0 complete (6/6 планов, gap ARCH-04 закрыт)
**Scope:** Ядро архитектуры, Perception, Actors, Transport & Architecture — 9 фаз

---

## Overview

| Phase | Title | Requirements | Focus |
|-------|-------|-------------|-------|
| 0 | Core Architecture Foundation | ARCH-01–07 | Lifecycle, EventBus, WorldQuery, Signals, Tick order |
| 1 | Zone Visual & Control Layer | ZONE-01–10 | Визуал зон, lifecycle, detection_mode, owned_zones |
| 2 | Actor & Prop Foundation | ACTR-01–02, ACTR-06, PROP-01–03 | IActorBehavior, Door, Props, Signal controllers |
| 3 | Actor Ecosystem | ACTR-03–05 | Pedestrian, Conveyor, Elevator |
| 4 | Perception System | PERC-01–07 | ArUco, детекторы, деградация |
| 5 | Transport & Control Refactoring | TRAN-01–07, API-01–02 | HTTP-транспорт, REST API, Hot Patch |
| 6 | Entity Model Unification | ENTY-01–08 | Capabilities, Own Effects, Effect Absorption |
| 7 | Material System | MATL-01–08 | MaterialTransfer, DeformEntity, навесное оборудование |
| 8 | Visualization Overhaul | VIZL-01–06 | Multi-channel, IVizAdapter, VisualHint двухуровневый |

---

## Phase 0 — Core Architecture Foundation

**Goal:** Правильный lifecycle плагинов, typed EventBus, WorldQuery API, Signal struct на Entity, 8-фазный tick lifecycle, полный набор KernelCommands — фундамент, на котором строится всё остальное.

**Requirements:** ARCH-01, ARCH-02, ARCH-03, ARCH-04, ARCH-05, ARCH-06, ARCH-07

**Plans:** 6 plans

Plans:
- [x] 00-01-PLAN.md — Signal struct в types.hpp + Agent::signals + EventBus (rename + новые события)
- [x] 00-02-PLAN.md — WorldQuery header + IAgentPlugin: lifecycle/role/PluginContext/update()
- [x] 00-03-PLAN.md — KernelCommand variant — полный набор 16 команд
- [x] 00-04-PLAN.md — SimEngine tick рефакторинг — 8 именованных фаз + command_queue_
- [x] 00-05-PLAN.md — Обновить все 11 плагинов + on_reset баги DiffDrive/Battery + SceneLoader валидация
- [x] 00-06-PLAN.md — Gap ARCH-04 closure: ZoneSystem публикует event::ZoneEntered / ZoneExited

**Success criteria:**
- on_reset() вызывается для всех плагинов при /sim/reset; DiffDrive и Battery корректно сбрасывают состояние
- EventBus доставляет ZoneEntered событие подписчикам при входе агента в зону
- WorldQuery.raycast возвращает корректный результат через статическую геометрию
- Сенсоры (Lidar) работают из финальной позиции агента (после collision response)
- KernelCommand::Interact вызывает target.behavior.on_interact() через ядро с валидацией

**Dependencies:** None (базовый рефакторинг существующего кода)

---

## Phase 1 — Zone Visual & Control Layer

**Goal:** Зоны видны в UI, имеют lifecycle (strength/drift), управляются KernelCommands, поддерживают detection_mode, self_destruct, spawn triggers и owned_zones.

**Requirements:** ZONE-01, ZONE-02, ZONE-03, ZONE-04, ZONE-05, ZONE-06, ZONE-07, ZONE-08, ZONE-09, ZONE-10

**Plans:** 6 plans

Plans:
- [x] 01-01-PLAN.md — Zone struct расширение: DetectionMode, ZoneLifecycle, SelfDestructPolicy + ZoneSnapshot.strength/visual_hints
- [x] 01-02-PLAN.md — FogEffect + EMIEffect сенсорные плагины + регистрация в effect_factory
- [x] 01-03-PLAN.md — ZoneSystem: BOUNDING/PER_LINK detection, lifecycle, self_destruct, remove_zone, owned_zones
- [x] 01-04-PLAN.md — KernelCommands SpawnZone/DespawnZone/ToggleZone + ZoneSpawnSystem (4 триггера)
- [x] 01-05-PLAN.md — SceneLoader: owned_zones, lifecycle, zone_templates + SimEngine build_snapshot
- [ ] 01-06-PLAN.md — UI Zone Inspector (вкладка Зоны) + VisualHint рендеринг (glow/arrows/particles/grid)

**Tasks:**
- [ ] **1.1** — Zone inspector в UI редакторе (ZONE-01): форма создания/редактирования зоны, параметры эффекта, тип зоны; использует /api/effects/registry (Phase 5)
- [ ] **1.2** — VisualHint pipeline для зон (ZONE-02): цветовая индикация типа зоны, прозрачность, анимация активации агента внутри
- [ ] **1.3** — Сенсорные эффекты (ZONE-03): FogEffect (WHILE_INSIDE + SENSOR_MOD, ухудшение max_range, required_capabilities: [optical_sensor]), EMIEffect (WHILE_INSIDE + STATE_CHANGE, деградация GNSS/IMU шум)
- [ ] **1.4** — Zone lifecycle (ZONE-04): поле strength (0–1) в ZoneConfig, рост/затухание через lifecycle.decay, auto-remove при strength < threshold, drift через invisible prop-носитель
- [ ] **1.5** — Zone KernelCommands (ZONE-05): SpawnZone / DespawnZone / ToggleZone; ToggleZone: enabled=false → все Entity получают ON_EXIT; enabled=true → все Entity внутри получают ON_ENTER заново
- [ ] **1.6** — Zone detection_mode (ZONE-06): enum DetectionMode {CENTER, BOUNDING, PER_LINK}; YAML поле detection_mode; PER_LINK: итерация kinematic_tree, EffectContext.contact_link заполняется
- [ ] **1.7** — Zone self_destruct_policy (ZONE-07): поле self_destruct_policy {on_any_contact, on_effect_applied}; on_any_contact удаляет зону при любом контакте независимо от immunity
- [ ] **1.8** — Zone spawn triggers (ZONE-08): поле spawn_trigger в lifecycle; тип event (EventBus фильтр), timer (N сек), state_change (entity_id + state); ZoneSpawnSystem подписывается на EventBus
- [ ] **1.9** — Entity.owned_zones (ZONE-09): поле owned_zones: [] в YAML; зоны с attached_to_link: <link_name>; при движении Entity → зоны двигаются; ZoneSystem отслеживает owned_zones
- [ ] **1.10** — Zone movement через invisible prop (ZONE-10): SpawnEntity{type: prop, invisible: true} + SpawnZone{attached_to: prop_id}; документированный паттерн для динамических зон (пыль, дым, ударная волна)

**Success criteria:**
- Зону можно создать через UI, она отображается с цветовой индикацией
- FogEffect ухудшает дальность лидара агента с optical_sensor capability
- SpawnZone/DespawnZone работают через REST API
- detection_mode: per_link корректно определяет через какой линк произошёл контакт
- Зона с self_destruct_policy: on_any_contact удаляется при первом контакте
- owned_zone агента двигается вместе с агентом

**Dependencies:** Phase 0 (EventBus, KernelCommand, WorldQuery, tick lifecycle)

---

## Phase 2 — Actor & Prop Foundation

**Goal:** Система акторов с расширенным IActorBehavior, DoorBehavior с proximity-триггером и wire-сигналами, система пропов с attach/detach, signal controller плагины.

**Requirements:** ACTR-01, ACTR-02, ACTR-06, PROP-01, PROP-02, PROP-03

**Tasks:**
- [ ] **2.1** — IActorBehavior расширенный (ACTR-01): on_init(yaml) / on_spawn(entity) / on_reset() / update(dt, entity, WorldContext) / on_signal(SignalEvent) / on_interact(source, action, params) / current_state() / to_json(); ActorFSM утилита (add_state/add_transition/fire/update)
- [ ] **2.2** — ActorRegistry + тиковый цикл: хранение акторов в SimEngine, Phase 2 тика (pre_resolve→resolve→behavior.update→plugins.update)
- [ ] **2.3** — DoorBehavior (ACTR-02): FSM (CLOSED/OPENING/OPEN/CLOSING); behavior напрямую двигает door_panel.pose.yaw и обновляет collision (imperatively); proximity-триггер через WorldQuery.find_in_radius; wire-сигнал через on_signal(); visual_hint {door_progress, sound}
- [ ] **2.4** — SignalListenerBase + DoorWireController (ACTR-06, часть 1): база с scan_signals()/filter_by_id()/filter_by_source(); DoorWireController: reactions config (signal_id, source_entity, on_active, on_inactive) с actions (close_and_lock / force_open / unlock)
- [ ] **2.5** — ConveyorWireController + EventReactor (ACTR-06, часть 2): ConveyorWireController: reactions с actions (stop/reverse/start); EventReactor: декларативный плагин listen + on_active/on_inactive → fire_event
- [ ] **2.6** — Prop структура (PROP-01): PropConfig (YAML); movable flag; signals список; collision shape; capabilities/tags; без SharedState по умолчанию; бинарные эффекты (fragile+vibration → destroy)
- [ ] **2.7** — AttachObject / DetachObject KernelCommands (PROP-02): prop прикрепляется к entity/link, следует за ней; Phase 6 тика (attachments) обновляет позы
- [ ] **2.8** — GrabberPlugin (PROP-03): interaction роль; grab/release через KernelCommand::Interact; ограничения дальности/веса; SharedState contribution (manipulation_locked); EventBus: GrabAttempt/GrabSucceeded/GrabFailed

**Success criteria:**
- Дверь создаётся в YAML сцены, открывается при приближении агента
- DoorWireController: сигнал "factory_power" от button_1 → дверь закрывается и блокируется
- Prop можно прикрепить к агенту через REST API
- GrabberPlugin публикует статус захвата в SharedState и EventBus

**Dependencies:** Phase 0 (EventBus, KernelCommand, Signal struct, WorldQuery), Phase 1 (zone patterns)

---

## Phase 3 — Actor Ecosystem

**Goal:** Пешеходы, конвейеры и лифты как первоклассные акторы.

**Requirements:** ACTR-03, ACTR-04, ACTR-05

**Tasks:**
- [ ] **3.1** — PedestrianBehavior (ACTR-03): список waypoints, LinearSteering к следующей точке, ObstacleAvoidance (separation от агентов/акторов через WorldQuery.find_in_radius), loop/ping-pong режимы
- [ ] **3.2** — ConveyorActor (ACTR-04): owned conveyor зона (ZONE-09), ConveyorWireController для направления и скорости, реверс по сигналу
- [ ] **3.3** — ElevatorBehavior (ACTR-05): FSM (IDLE/MOVING_UP/MOVING_DOWN/OPEN_DOORS), floor positions в YAML, реакция на KernelCommand::Interact{action: "call"}
- [ ] **3.4** — ElevatorUserPlugin: interaction плагин агента; вызов лифта через Interact; SharedState contribution (elevator_request)
- [ ] **3.5** — Интеграционные тесты: маршрут пешехода, конвейер с wire-сигналом, лифт с агентом

**Success criteria:**
- Пешеход патрулирует маршрут и огибает препятствия через WorldQuery
- Конвейер меняет направление по wire-сигналу (через ConveyorWireController)
- Агент вызывает лифт через Interact, заезжает и перемещается на другой этаж

**Dependencies:** Phase 2 (IActorBehavior, ActorRegistry, SignalListenerBase), Phase 0 (WorldQuery)

---

## Phase 4 — Perception System

**Goal:** Агенты воспринимают ArUco-маркеры, других агентов, акторов и зоны. Ray-zone транзит деградирует сигнал.

**Requirements:** PERC-01, PERC-02, PERC-03, PERC-04, PERC-05, PERC-06, PERC-07

**Tasks:**
- [ ] **4.1** — Flat зоны (PERC-01): ориентация через quaternion/normal + толщина, корректное пересечение агента
- [ ] **4.2** — ArUcoSignal компонент (PERC-02): использует ARCH-03 Signal struct; Signal{type: "aruco", signal_id, local_pose, params: {size}}; добавляется на любую Entity через YAML signals:[]
- [ ] **4.3** — ArucoDetectorPlugin (PERC-03): DetectionVolume (CONE/SPHERE/BOX); dual-zone (read_range / detect_range); WorldQuery.has_line_of_sight для LoS; выдача в SharedState + ROS2 ArucoArray
- [ ] **4.4** — AgentDetectorPlugin (PERC-04): WorldQuery.find_in_radius с filter AGENT; опциональная LoS через WorldQuery; вывод (id, distance, bearing); публикация в SharedState + transport
- [ ] **4.5** — EntityDetectorPlugin (PERC-05 + PERC-06): один плагин с конфигурируемым фильтром entity_types + required_tags; заменяет PedestrianDetector/ConveyorDetector; ZoneDetector как специальный фильтр
- [ ] **4.6** — Ray-zone transit (PERC-07): WorldQuery.raycast с zone intersection; накопление attenuation по zone_type; KernelQuery для сторонних запросов трассировки
- [ ] **4.7** — Интеграционный тест: ArucoDetector обнаруживает маркер через fog-зону с ослаблением сигнала

**Success criteria:**
- ArucoDetectorPlugin обнаруживает маркер в detect_range, теряет в read_range
- EntityDetector с filter actor+pedestrian видит только пешеходов
- Сигнал ArUco ослабляется при прохождении через fog-зону

**Dependencies:** Phase 0 (WorldQuery, Signal struct), Phase 1 (zone types), Phase 2 (Actor entities)

---

## Phase 5 — Transport & Control Refactoring

**Goal:** TransportPool, HttpTransportAdapter, per-agent transport config, единый REST Control API с Hot Patch и Registry, domain_id перенос в ROS2-адаптер.

**Requirements:** TRAN-01, TRAN-02, TRAN-03, TRAN-04, TRAN-05, TRAN-06, TRAN-07, API-01, API-02

**Tasks:**
- [ ] **5.1** — ITransportAdapter интерфейс: register_command_topic / register_service / register_subscription / publish_event / tick(sim_time)
- [ ] **5.2** — SimTransportBridge: итерирует плагины всех агентов → читает command_topics()/service_names()/subscribe_topics() → регистрирует в адаптере агента; плагин не знает про конкретный транспорт
- [ ] **5.3** — HttpTransportAdapter (TRAN-01): SSE endpoint для событий (per-agent порт или multiplexed); HTTP POST для команд; long-poll; управление без ROS2
- [ ] **5.4** — ROS2TransportAdapter: обёртка над текущим Ros2Transport в ITransportAdapter интерфейс
- [ ] **5.5** — StubTransportAdapter: заглушка для тестов, in-memory очередь
- [ ] **5.6** — Per-agent transport config (TRAN-02): поле transport в AgentConfig (ros2/http/stub); YAML парсинг ros2: {domain_id: N} (TRAN-06: domain_id перенесён из конфига агента); TransportPool (singleton per type) (TRAN-05)
- [ ] **5.7** — Unified REST Control API (TRAN-03): POST /sim/pause|resume|reset|step|speed; GET /sim/status; legacy GET /command?cmd= → 410 Gone с подсказкой
- [ ] **5.8** — sim_time публикация (TRAN-04): SimClock → publish на ROS2 /clock, HTTP SSE поле, WorldSnapshot.sim_time; ускорение = больше тиков, dt не меняется
- [ ] **5.9** — Hot Patch REST API (TRAN-07): GET/POST/DELETE /world/entities/{id}; PUT /world/entities/{id}/pose|enabled; GET/POST/DELETE /world/zones/{id}; PUT /world/zones/{id}/enabled|shape|strength; GET/POST/PUT/DELETE /world/entities/{id}/plugins; ROS2 транспорт корректно переинициализируется при смене сцены
- [ ] **5.10** — Registry endpoints (API-01): GET /api/plugins/registry (с label, role, config_schema), /api/behaviors/registry, /api/effects/registry, /api/transports/registry
- [ ] **5.11** — Scene Management API (API-02): GET /scenes, POST /scenes/load|save|save-as|new, GET /scenes/active; World Editing: CRUD /world/entities + /world/zones

**Success criteria:**
- Агент с transport: http управляется без запущенного ROS2
- Legacy endpoints возвращают 410 Gone с подсказкой
- sim_time публикуется в ROS2 и доступен через HTTP
- Hot Patch: добавить плагин агенту через REST без остановки симуляции
- /api/plugins/registry возвращает все зарегистрированные типы с config_schema

**Dependencies:** Phase 1–4 (все плагины должны работать через новый transport), Phase 0 (KernelCommand, plugin lifecycle)

---

## Phase 6 — Entity Model Unification

**Goal:** Own Effects, Effect Absorption с per-link immunity, EffectPlugin унифицированный интерфейс, capabilities auto-declaration, полная Entity base model.

**Requirements:** ENTY-01, ENTY-02, ENTY-03, ENTY-04, ENTY-05, ENTY-06, ENTY-07, ENTY-08

**Tasks:**
- [ ] **6.1** — Capabilities + effect_tags + immune_to_effects (ENTY-01): стандартный список capabilities (surface_contact, wheeled, airborne, optical_sensor, gnss_sensor, has_battery, manipulator, detectable, fragile, flammable, perishable); теги эффектов; immune_to_effects в EntityConfig
- [ ] **6.2** — EffectPlugin унифицированный интерфейс (ENTY-07): required_capabilities / excluded_capabilities / effect_tags / apply(SharedState&, EffectContext&); EffectContext: entity, zone, dt, zone_strength, contact_link; правило matching в 4 шага
- [ ] **6.3** — Capability matching в ZoneSystem (ENTY-01 продолжение): эффект применяется только если matching прошёл; рефакторинг всех существующих эффектов (ice, boost, lock, charging, conveyor, wind) под новый интерфейс с required_capabilities
- [ ] **6.4** — Capabilities auto-declaration (ENTY-08): provided_capabilities() в каждом плагине; SimEngine::initialize_entity() автоматически добавляет из плагинов; DiffDrive → {surface_contact, wheeled, 2d_motion}; Lidar → {optical_sensor}; GNSS → {gnss_sensor}; Battery → {has_battery}
- [ ] **6.5** — Plugin lifecycle hooks (ENTY-02): on_reset() / on_spawn() / on_despawn() вызываются в правильных фазах тика (из Phase 0); верификация корректности для всех плагинов
- [ ] **6.6** — Effect trigger model (ENTY-03): поле trigger: WHILE_INSIDE/ON_ENTER/ON_EXIT; action: CONTRIBUTION/STATE_CHANGE/SENSOR_MOD/OWN_EFFECT_SPAWN; refactor существующих 4-х типов эффектов в trigger×action
- [ ] **6.7** — Own Effects (ENTY-05): entity.add_own_effect(contributor); OWN_EFFECT_SPAWN в EffectPlugin; TireDriftContributor как пример (TirePunctureEffect → entity.add_own_effect(TireDriftContributor)); фаза 3c тика (own effects) вызывает contribute() для каждого own effect
- [ ] **6.8** — Effect Absorption + per-link immunity (ENTY-06): иммунитет поглощает результат после apply(); при detection_mode=PER_LINK ядро знает contact_link → проверяет link.immune_to_effects; мина взрывается всегда, поглощение — per-link; link_overrides в YAML
- [ ] **6.9** — Entity base model (ENTY-04): EntityBase {id, name, type, world_pose, collision, visual, signals, capabilities, tags, enabled, owned_zones}; опциональные слои: PluginHost / SharedState / TransportLink / Behavior / LinkTree; тип = семантическая метка

**Success criteria:**
- Зона с effect_tags: [electric] не действует на entity с immune_to_effects: [electric]
- on_reset() вызывается при /sim/reset; on_spawn() при AddPlugin
- ON_ENTER эффект срабатывает ровно один раз при входе в зону
- DiffDrive автоматически добавляет {surface_contact, wheeled, 2d_motion} без YAML
- TirePunctureEffect создаёт own_effect, который продолжает влиять на агента после выхода из зоны

**Dependencies:** Phase 1 (ZoneSystem), Phase 2–3 (Actor entities), Phase 0 (tick lifecycle, EventBus)

---

## Phase 7 — Material System

**Goal:** MaterialTransfer + DeformEntity KernelCommands, DirtPile деформируемый актор, навесное оборудование, displacement, transfer hints.

**Requirements:** MATL-01, MATL-02, MATL-03, MATL-04, MATL-05, MATL-06, MATL-07, MATL-08

**Tasks:**
- [ ] **7.1** — Опциональные методы материалов (MATL-02): can_release/accept/actively_acquire/actively_release / release_material(v) / accept_material(v,m,p) / material_volume / remaining_capacity / material_type / is_deformable / apply_deformation(cmd) в IAgentPlugin и IActorBehavior
- [ ] **7.2** — MaterialTransfer KernelCommand (MATL-01): {source_entity, source_plugin, target_entity (nullable), target_plugin, volume, material, position, transfer_hint}; ядро: validate → release_material → accept_material или Displacement
- [ ] **7.3** — DeformEntity KernelCommand (MATL-05): {target_entity, tool_pose, tool_geometry {type, width, height, depth, side_walls, side_wall_height}, tool_velocity, dt}; ядро вызывает target.behavior.apply_deformation(cmd)
- [ ] **7.4** — DirtPile actor (MATL-03): behavior: dirt_grid; 2D-grid высот; settling (стекание если уклон > friction_angle); merge мелких куч в radius merge_radius; rebuild collision (bilintear); volume conservation строго
- [ ] **7.5** — Transfer hints (MATL-08): поле transfer_hint в MaterialTransfer: type (arc/pour/dump/spray/stream), from/to, arc_height, material, volume_per_second, particle_preset, active; визуализатор рисует анимацию, симуляция мгновенна
- [ ] **7.6** — VisualHint для материалов (MATL-04): анимация потока через transfer_hint; визуальное обновление DirtPile grid
- [ ] **7.7** — Навесное оборудование (MATL-06): BucketAttachment (interaction, ковш: scoop/dump, accepted_materials, mount_link); BladeAttachment (шлёт DeformEntity каждый тик); RotorAttachment (шлёт MaterialTransfer с arc hint); TankAttachment (жидкости); hot-swap через PUT /agents/{id}/plugins/{type}
- [ ] **7.8** — Displacement (MATL-07): MaterialTransfer с target=NULL в занятую позицию; ядро находит ближайшие свободные позиции вокруг Entity без accept_material; распределяет материал; если рядом есть DirtPile того же типа → merge
- [ ] **7.9** — Micro-pile merging: DirtPile пересечение → объединение в один актор

**Success criteria:**
- BucketPlugin агента набирает материал из DirtPile через MaterialTransfer
- DirtPile деформируется при взаимодействии с BladeAttachment
- Volume conservation: суммарный объём не меняется при перемещении
- RotorAttachment выбрасывает снег по параболе (arc hint) вокруг препятствий (displacement)
- hot-swap: замена BucketAttachment на RotorAttachment через REST без перезапуска

**Dependencies:** Phase 6 (Entity model, IActorBehavior с is_deformable), Phase 2 (ActorRegistry)

---

## Phase 8 — Visualization Overhaul

**Goal:** Multi-channel SSE стриминг, IVizAdapter с FileLogAdapter, двухуровневый VisualHint pipeline (пресеты + кастомные JS), Scripted agent.

**Requirements:** VIZL-01, VIZL-02, VIZL-03, VIZL-04, VIZL-05, VIZL-06

**Tasks:**
- [ ] **8.1** — Multi-channel SSE (VIZL-01): /stream/core (30fps, позы+zone states), /stream/heavy (2–5fps, лидар/траектории), /stream/static (on-demand, геометрия); команды управления отдельным REST POST — не конкурируют со снапшотами
- [ ] **8.2** — Three.js клиент multi-channel: подписка на разные потоки, приоритизация core, heavy-данные обновляются независимо
- [ ] **8.3** — IVizAdapter интерфейс (VIZL-02): publish(WorldSnapshot&) + on_command(callback); WebVizAdapter (текущий SSE → рефакторинг); NullAdapter (headless, no-op)
- [ ] **8.4** — FileLogAdapter (VIZL-05): IVizAdapter реализация; запись WorldSnapshot в файл с sim_time; форматы JSON/binary; для replay и post-analysis
- [ ] **8.5** — VisualHint двухуровневый (VIZL-03): Level 1 пресеты — marker/particles/glow/arrows/trail/fan/cone/grid/snow/rain/fog/dust/smoke; Level 2 кастомные JS-модули в web/js/visuals/ (lidar_fan.js, trajectory_trail.js, snow_particles.js, default.js); render(scene, data, params) — новый плагин = новый .js, не трогая app.js
- [ ] **8.6** — IAgentPlugin::get_visual_hints(): SimEngine собирает hints из всех плагинов → включает в snapshot; BatteryPlugin отображает уровень заряда; LidarPlugin — веер лучей через custom module
- [ ] **8.7** — Scripted behavior плагин (VIZL-04): utility роль; transport: stub; waypoints patrol или replay из YAML; пишет desired_linear/angular в SharedState; DiffDrive не различает источник команды

**Success criteria:**
- Core stream 30fps не лагает от больших данных лидара (лидар в heavy stream)
- Heavy/lidar обновляются в отдельном потоке без влияния на core FPS
- VisualHint от BatteryPlugin отображает уровень заряда в браузере
- NullVizAdapter используется в юнит-тестах без запуска HTTP
- FileLogAdapter записывает сессию для последующего воспроизведения
- Scripted agent патрулирует маршрут без внешнего контроллера

**Dependencies:** Phase 5 (transport refactoring), Phase 4 (sensor data), Phase 6 (entity model, visual hints)

---

## Milestones

| Milestone | After Phase | Description |
|-----------|-------------|-------------|
| M0: Foundation | 0 | Стабильный lifecycle, EventBus, WorldQuery, Tick порядок |
| M1: Rich World | 3 | Зоны с lifecycle, двери, сигналы, пешеходы, конвейеры, лифты |
| M2: Perception | 4 | Полная система восприятия с ArUco, детекторами, деградацией |
| M3: Transport | 5 | HTTP-транспорт, единый REST API с Hot Patch, Registry |
| M4: Architecture | 6 | Own Effects, Effect Absorption, per-link immunity, Capabilities |
| M5: Materials | 7 | MaterialTransfer, DeformEntity, навесное оборудование, DirtPile |
| M6: Visualization | 8 | Multi-channel, IVizAdapter, двухуровневый VisualHint |

---

*Roadmap created: 2026-04-25*
*Updated: 2026-04-25 — Phase 0 добавлена; Phase 1 расширена ZONE-06-10; Phase 2 добавлен ACTR-06; Phase 5 добавлены TRAN-05-07 + API-01-02; Phase 6 добавлены ENTY-05-08; Phase 7 добавлены MATL-05-08; Phase 8 добавлены VIZL-05-06; итого 9 фаз, 65 требований*
*Start with: `/gsd-plan-phase 0`*
