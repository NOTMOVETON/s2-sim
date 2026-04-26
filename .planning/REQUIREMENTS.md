# Requirements: S2 Simulator

**Defined:** 2026-04-25
**Core Value:** Любой разработчик должен уметь добавить нового агента, плагин, эффект или актора за один файл, не трогая ядро — и сразу увидеть это в браузере.

## v1 Requirements

### Ядро архитектуры

- [x] **ARCH-01**: IAgentPlugin расширенный lifecycle — on_spawn(entity) / on_despawn(entity) / on_scene_load(world) / on_reset(); provided_capabilities() автоматически добавляются в entity; config_schema() для UI редактора; все существующие плагины реализуют on_reset() корректно (DiffDrive сбрасывает external_linear_velocity_, Battery сбрасывает заряд) — COMPLETE: Plans 02+05
- [x] **ARCH-02**: Plugin role system — role() метод (actuation / sensor / interaction / resource / utility); матрица доступа: actuation читает SharedState, sensor читает WorldQuery, interaction публикует KernelCommand и EventBus, resource пишет в SharedState; у агента максимум один actuation-плагин — COMPLETE: Plans 02+05 (role() + валидация в SceneLoader)
- [x] **ARCH-03**: Signal struct на Entity — поля: signal_type, signal_id, local_pose, params (json), range, requires_los, enabled; wire = Signal с range: infinite и requires_los: false; любая Entity (Agent/Actor/Prop) может нести список сигналов
- [x] **ARCH-04**: EventBus typed events — EntitySpawned, EntityDespawned, ActorStateChanged, SignalActivated, SignalDeactivated, ZoneEntered, ZoneExited, GrabAttempt, GrabSucceeded, GrabFailed, DamageDealt; любой плагин/behavior может publish и subscribe
- [ ] **ARCH-05**: WorldQuery read-only API — find_in_radius(center, r, filter), find_in_box(box, filter), find_nearest(pos, filter), find_signals_of_type(type, pos, volume), has_line_of_sight(from, to), raycast(origin, dir, max_range), zones_at(pos), is_in_zone(entity, zone), find_deformable_in_box(box, filter) — PARTIAL: интерфейс создан (Plan 02), WorldQueryImpl реализация — Plan 05
- [x] **ARCH-06**: Tick lifecycle 8 фаз в правильном порядке — Phase 0 (KernelCommands), Phase 1 (transport input), Phase 2 (actors: pre_resolve→resolve→behavior.update→plugins.update), Phase 3 (agents: pre_resolve→zones→own_effects→resolver→sensor_mod→actuation→kinematics→collision→surface), Phase 4 (sensors — после кинематики), Phase 5 (interactions), Phase 6 (attachments), Phase 7 (snapshot+publish), Phase 8 (cleanup+clear_contributions)
- [ ] **ARCH-07**: KernelCommand полный набор — Entity lifecycle: SpawnEntity/DespawnEntity/SetPose/SetEnabled; Plugin lifecycle: AddPlugin/RemovePlugin/ConfigPlugin; Взаимодействие: Interact{source, target, action, params} как единая точка (ядро валидирует дистанцию/capabilities); Scene: LoadScene/SaveScene/NewScene

### Зоны — визуал и управление

- [x] **ZONE-01**: Редактор зон в UI с инспектором (форма, параметры, эффекты)
- [ ] **ZONE-02**: VisualHint pipeline для зон (цвет, прозрачность, анимация въезда/выезда)
- [ ] **ZONE-03**: Сенсорные эффекты: туман (ухудшение видимости), ЭМ помехи (деградация GNSS/IMU)
- [x] **ZONE-04**: Lifecycle зон: zone.strength (0–1), рост/затухание, auto-remove при strength=0, drift (движение)
- [x] **ZONE-05**: KernelCommands для зон: SpawnZone / RemoveZone / ToggleZone (enable/disable)
- [x] **ZONE-06**: Zone detection_mode — center (zone.contains(entity.pose)) / bounding (zone.intersects(entity.shape)) / per_link (∀ link in kinematic_tree); дефолт: bounding
- [x] **ZONE-07**: Zone self_destruct_policy — on_any_contact: зона удаляется при любом контакте независимо от immunity; on_effect_applied: зона удаляется только если эффект не был поглощён
- [x] **ZONE-08**: Zone spawn triggers — command (KernelCommand, уже есть) + event (EventBus фильтр по ти��у/источнику) + timer (N секунд от старта) + state_change (Entity переходит в указанное состояние)
- [x] **ZONE-09**: Entity.owned_zones — список зон привязанных к Entity (и к конкретному link через attached_to_link); зоны двигаются вместе с Entity; объявляются в YAML через owned_zones: [...]
- [x] **ZONE-10**: Zone movement через invisible prop-носитель — SpawnProp(invisible_carrier) + SpawnZone(attached_to: carrier_id); carrier может иметь drift_behavior плагин; переиспользует существующие механизмы вместо «движения зоны»

### Акторы — основа

- [x] **ACTR-01**: IActorBehavior интерфейс: on_init(yaml) / on_spawn(entity) / on_reset() / update(dt, entity, ctx) / on_signal(SignalEvent) / on_interact(source, action, params) / current_state() / to_json(); FSM утилита (ActorFSM: add_state/add_transition/fire/update)
- [ ] **ACTR-02**: DoorBehavior: FSM (CLOSED/OPENING/OPEN/CLOSING); behavior напрямую двигает геометрию door_panel и обновляет collision (императивное управление); proximity-триггер или wire-сигнал через on_signal()
- [ ] **ACTR-03**: PedestrianBehavior: маршрутные точки (waypoints), LinearSteering к следующей точке, ObstacleAvoidance (separation от других агентов/акторов), loop/ping-pong режимы
- [ ] **ACTR-04**: ConveyorActor: owned conveyor зона, DoorWireController/ConveyorWireController для реакции на wire-сигнал, реверс направления
- [ ] **ACTR-05**: ElevatorBehavior + ElevatorUserPlugin (агент вызывает лифт через KernelCommand::Interact)
- [ ] **ACTR-06**: SignalListenerBase + controller плагины — DoorWireController (реакции: close_and_lock / force_open / unlock), ConveyorWireController (stop / reverse / start); EventReactor (декларативный плагин: listen + on_active/on_inactive); один сигнал → разные реакции у разных плагинов

### Пропы

- [x] **PROP-01**: Prop структура: movable (да/нет), signals (список Signal), collision (shape), capabilities, tags; без SharedState по умолчанию
- [ ] **PROP-02**: AttachObject / DetachObject KernelCommands (приклеить prop к entity/link, следует за ней)
- [ ] **PROP-03**: GrabberPlugin для агентов (взять/положить prop через KernelCommand::Interact, ограничения веса/дальности, SharedState contribution)

### Восприятие

- [ ] **PERC-01**: Flat зоны под любым углом (ориентация через quaternion/normal-вектор + толщина)
- [ ] **PERC-02**: ArUco сигналы на сущностях: Signal{type: "aruco", signal_id, local_pose, params: {size}} на любой Entity; используют ARCH-03 Signal struct
- [ ] **PERC-03**: ArucoDetectorPlugin: DetectionVolume (CONE/SPHERE/BOX), dual-zone (read_range / detect_range), requires_los через WorldQuery, выдача в SharedState + ROS2 ArucoArray
- [ ] **PERC-04**: AgentDetectorPlugin: список видимых агентов (id, distance, bearing), опциональная LoS через WorldQuery.has_line_of_sight
- [ ] **PERC-05**: ActorDetectorPlugin: список видимых акторов (id, type, distance); конфигурируемый фильтр через entity_types/required_tags (один EntityDetector с фильтром, не отдельные PedestrianDetector/ConveyorDetector)
- [ ] **PERC-06**: ZoneDetectorPlugin: список зон в радиусе (id, type, distance, strength)
- [ ] **PERC-07**: Ray-zone transit деградация — WorldQuery.raycast через зоны, коэффициент ослабления сигнала per zone type; KernelQuery для трассировки луча

### Транспорт

- [ ] **TRAN-01**: HttpTransportAdapter — управление агентами через HTTP/JSON без ROS2; SSE endpoint для событий; POST для команд
- [ ] **TRAN-02**: Per-agent transport config в YAML (transport: ros2/http/stub + тип-специфичные параметры)
- [ ] **TRAN-03**: Единый REST Control API (замена legacy GET /command?cmd=): Sim Control (pause/resume/reset/step/speed), Plugin Control (POST /agents/{id}/input/{plugin_type}, /agents/{id}/services/{name})
- [ ] **TRAN-04**: sim_time публикуется всем потребителям (ROS2 Clock /clock, HTTP SSE поле sim_time, SharedState); ускорение = больше тиков/сек, dt не меняется
- [ ] **TRAN-05**: TransportPool — singleton-адаптер per-type (один ROS2 контекст на всех агентов с ros2 транспортом); SimTransportBridge итерирует плагины всех агентов → регистрирует command_topics/service_names в адаптере; при смене сцены транспорт корректно переинициализируется
- [ ] **TRAN-06**: domain_id migration — поле domain_id перемещается из конфига агента в секцию ros2: {domain_id: N}; ядро не знает про DDS; другие транспорты игнорируют это поле
- [ ] **TRAN-07**: Hot Patch REST API — /world/entities CRUD, /world/zones CRUD, /world/entities/{id}/plugins CRUD (add/update/delete); симуляция не останавливается (или пауза на 1 тик атомарно); поддерживаются все операции из §17.2 RESULT_DISCUSS.md

### Сущности — унификация

- [ ] **ENTY-01**: Capabilities + effect_tags + immune_to_effects: стандартный список (surface_contact, wheeled, airborne, optical_sensor, gnss_sensor, has_battery, manipulator, detectable, fragile, flammable...); теги эффектов; иммунитет per-entity
- [ ] **ENTY-02**: Plugin lifecycle: on_reset() / on_spawn() / on_despawn() / on_scene_load() в IAgentPlugin (расширение ARCH-01)
- [ ] **ENTY-03**: Effect model: trigger (WHILE_INSIDE/ON_ENTER/ON_EXIT) × action (CONTRIBUTION/STATE_CHANGE/SENSOR_MOD/OWN_EFFECT_SPAWN) — 4 действия включая OWN_EFFECT_SPAWN
- [ ] **ENTY-04**: Entity base model с опциональными слоями (PluginHost, SharedState, TransportLink, Behavior, LinkTree); тип (AGENT/ACTOR/PROP) — семантическая метка, не жёсткий класс
- [ ] **ENTY-05**: Own Effects — ON_ENTER + OWN_EFFECT_SPAWN создаёт persistent contributor на Entity (пример: TirePunctureEffect → entity.add_own_effect(TireDriftContributor)); own effects публикуют contributions каждый тик и живут до явного удаления; state.clear_contributions() их не трогает
- [ ] **ENTY-06**: Effect Absorption — иммунитет поглощает результат, не предотвращает срабатывание эффекта (мина взрывается всегда, что происходит с роботом — зависит от immunity); per-link immunity через detection_mode: per_link (ядро знает через какой линк произошёл контакт и проверяет immunity линка)
- [ ] **ENTY-07**: EffectPlugin унифицированный интерфейс — required_capabilities, excluded_capabilities, effect_tags, apply(SharedState&, EffectContext&); EffectContext: entity, zone, dt, zone_strength, contact_link; правило matching: required ⊆ entity.capabilities И excluded ∩ entity.capabilities = ∅ И effect_tags ∩ entity.immune_to_effects = ∅
- [ ] **ENTY-08**: Capabilities auto-declaration — IAgentPlugin::provided_capabilities() возвращает список capabilities; при initialize(entity) ядро автоматически добавляет их в entity.capabilities; не нужно дублировать в YAML (DiffDrive автоматически даёт surface_contact + wheeled + 2d_motion)

### Материалы

- [ ] **MATL-01**: MaterialTransfer KernelCommand: source_entity + source_plugin + target_entity (NULL = на землю) + target_plugin + volume + material + position + transfer_hint; ядро валидирует source/target и вызывает release_material/accept_material
- [ ] **MATL-02**: Опциональные методы в IAgentPlugin и IActorBehavior — can_release_material / can_accept_material / can_actively_acquire / can_actively_release / release_material(v) / accept_material(v,m,p) / material_volume / remaining_capacity / material_type
- [ ] **MATL-03**: DirtPile actor — behavior: dirt_grid; 2D-grid высот (float[][]), cell_size, friction_angle, settle_rate; settling (стекание по уклону), merge мелких куч, rebuild collision, volume conservation строго соблюдается
- [ ] **MATL-04**: VisualHint для материалов — анимация потока при MaterialTransfer (использует transfer_hint), визуальное обновление grid при деформации
- [ ] **MATL-05**: DeformEntity KernelCommand — target_entity, tool_pose (Pose3D), tool_geometry {type: blade/rake/drill, width, height, depth, side_walls, side_wall_height}, tool_velocity, dt; ядро вызывает target.behavior.apply_deformation(cmd); is_deformable() / apply_deformation(DeformationCommand) в IActorBehavior
- [ ] **MATL-06**: Навесное оборудование — interaction-плагины привязанные к URDF-линку: BucketAttachment (ковш: scoop/dump, accepted_materials), BladeAttachment (отвал: шлёт DeformEntity), RotorAttachment (снежный ротор: шлёт MaterialTransfer по дуге), TankAttachment (цистерна: жидкости); hot-swap через PUT /agents/{id}/plugins/{type}
- [ ] **MATL-07**: Displacement — MaterialTransfer с target=NULL в позицию занятую Entity без accept_material: ядро находит ближайшие свободные позиции вокруг Entity и распределяет материал (снег обтекает робота, не проваливается внутрь)
- [ ] **MATL-08**: Transfer hints — поле transfer_hint в MaterialTransfer: type (arc/pour/dump/spray/stream), from, to, arc_height, material, volume_per_second, particle_preset, active; визуализатор рисует анимацию, симуляция переносит объём мгновенно

### Визуализация

- [ ] **VIZL-01**: Multi-channel данные: Core (30fps, позы+состояния) / Heavy (2–5fps, лидар/траектории) / Static (по запросу, геометрия сцены); команды управления идут отдельным каналом REST POST — не конкурируют со снапшотами
- [ ] **VIZL-02**: IVizAdapter — publish(WorldSnapshot&) + on_command(callback); реализации: WebVizAdapter (текущий SSE), NullAdapter (headless), FileLogAdapter (replay), RViz2Adapter (future)
- [ ] **VIZL-03**: VisualHint pipeline для плагинов — IAgentPlugin::get_visual_hints() / IActorBehavior::visual_hint поле; SimEngine собирает и включает в snapshot; двухуровневая система: Level 1 пресеты (marker/particles/glow/arrows/trail/fan/cone/grid/snow/rain/fog/dust/smoke), Level 2 кастомные JS-модули в s2_visualizer/web/js/visuals/ (render(scene, data, params) — новый плагин = новый .js файл, не трогая app.js)
- [ ] **VIZL-04**: Scripted behavior плагин (utility роль) — transport: stub + scripted_behavior плагин; пишет desired_linear/angular в SharedState как если бы команда пришла извне; DiffDrive не отличает от внешней команды; поддержка режимов: patrol (waypoints), replay (записанная траектория из YAML)
- [ ] **VIZL-05**: FileLogAdapter — IVizAdapter реализация; запись WorldSnapshot в файл (JSON/binary) с меткой sim_time; для replay и post-analysis
- [ ] **VIZL-06**: Visual Hints кастомные JS-модули — lidar_fan.js (веер лидарных лучей), trajectory_trail.js (след), snow_particles.js (частицы снега); default.js для неизвестных; каждый модуль — функция render(scene, data, params)

### API

- [ ] **API-01**: Registry REST endpoints — GET /api/plugins/registry (список типов с label, role, config_schema), GET /api/behaviors/registry, GET /api/effects/registry, GET /api/transports/registry; config_schema позволяет UI генерировать формы редактирования без знания о конкретных типах
- [ ] **API-02**: Scene Management API — GET /scenes (список), POST /scenes/load {name}, POST /scenes/save {name}, POST /scenes/save-as {name}, POST /scenes/new, GET /scenes/active; World Editing: GET/POST/DELETE /world/entities/{id}, PUT /world/entities/{id}/pose, PUT /world/entities/{id}/enabled; GET/POST/DELETE /world/zones/{id}, PUT /world/zones/{id}/enabled|shape|strength

## v2 Requirements

### Безопасность

- **SEC-01**: Аутентификация REST API (токен или Basic Auth)
- **SEC-02**: Исправление shell injection через popen() в WebSocket SHA-1 (viz_server.cpp)
- **SEC-03**: Защита от path traversal в статик-файл-сервере

### Масштабирование

- **SCAL-01**: 100+ агентов без деградации (профилирование contribution resolver)
- **SCAL-02**: Spatial index для эффективного поиска зон/агентов в радиусе

### Сценарии

- **SCEN-01**: Scripted scenario player (воспроизведение YAML-сценариев с событиями)
- **SCEN-02**: Replay система (запись + воспроизведение сессии через FileLogAdapter)

### Визуализация

- **VIZL-EXT-01**: RViz2Adapter — IVizAdapter реализация для RViz2 (будущее)

## Out of Scope

| Feature | Reason |
|---------|--------|
| Heightmap | Убран: 3D-геометрия через static primitives (пещеры возможны) |
| 3D лидар | Отложен, текущий 2D достаточен |
| Полноценная физика частиц | Только имитация через settling (DirtPile) |
| RViz2 адаптер | v2+, сначала web |
| Mesh-коллизии | v2+, пока box/sphere/cylinder |
| Полноценная динамика | Кинематика, не динамика. Нет сил/инерции |
| Multiplayer / cloud sync | Out of scope для локального симулятора |
| Автоматическое высыпание по углу наклона | Высыпание только по запросу от стека робота |
| Жидкостная физика SPH/CFD | Settling 2D-grid — наш уровень абстракции |
| Раздельные интерфейсы IMaterialSource/IMaterialReceiver | Опциональные методы внутри IAgentPlugin |
| Специфичные KernelCommands ScoopDirt/DumpWater | Единые MaterialTransfer + DeformEntity |

## Traceability

| Requirement | Phase | Status |
|-------------|-------|--------|
| ARCH-01 | Phase 0 | Complete |
| ARCH-02 | Phase 0 | Complete |
| ARCH-03 | Phase 0 | Pending |
| ARCH-04 | Phase 0 | Pending |
| ARCH-05 | Phase 0 | Pending |
| ARCH-06 | Phase 0 | Pending |
| ARCH-07 | Phase 0 | Pending |
| ZONE-01 | Phase 1 | Complete (Plan 01-06, 01-07) |
| ZONE-02 | Phase 1 | Pending |
| ZONE-03 | Phase 1 | Pending |
| ZONE-04 | Phase 1 | Complete (Plan 01-03) |
| ZONE-05 | Phase 1 | Complete (Plan 01-04) |
| ZONE-06 | Phase 1 | Complete (Plan 01-03) |
| ZONE-07 | Phase 1 | Complete (Plan 01-03) |
| ZONE-08 | Phase 1 | Complete (Plan 01-04) |
| ZONE-09 | Phase 1 | Complete (Plan 01-03) |
| ZONE-10 | Phase 1 | Complete (Plan 01-04) |
| ACTR-01 | Phase 2 | Complete (Plan 02-01) |
| ACTR-02 | Phase 2 | Pending |
| ACTR-06 | Phase 2 | Pending |
| PROP-01 | Phase 2 | Complete (Plan 02-01) |
| PROP-02 | Phase 2 | Pending |
| PROP-03 | Phase 2 | Pending |
| ACTR-03 | Phase 3 | Pending |
| ACTR-04 | Phase 3 | Pending |
| ACTR-05 | Phase 3 | Pending |
| PERC-01 | Phase 4 | Pending |
| PERC-02 | Phase 4 | Pending |
| PERC-03 | Phase 4 | Pending |
| PERC-04 | Phase 4 | Pending |
| PERC-05 | Phase 4 | Pending |
| PERC-06 | Phase 4 | Pending |
| PERC-07 | Phase 4 | Pending |
| TRAN-01 | Phase 5 | Pending |
| TRAN-02 | Phase 5 | Pending |
| TRAN-03 | Phase 5 | Pending |
| TRAN-04 | Phase 5 | Pending |
| TRAN-05 | Phase 5 | Pending |
| TRAN-06 | Phase 5 | Pending |
| TRAN-07 | Phase 5 | Pending |
| API-01 | Phase 5 | Pending |
| API-02 | Phase 5 | Pending |
| ENTY-01 | Phase 6 | Pending |
| ENTY-02 | Phase 6 | Pending |
| ENTY-03 | Phase 6 | Pending |
| ENTY-04 | Phase 6 | Pending |
| ENTY-05 | Phase 6 | Pending |
| ENTY-06 | Phase 6 | Pending |
| ENTY-07 | Phase 6 | Pending |
| ENTY-08 | Phase 6 | Pending |
| MATL-01 | Phase 7 | Pending |
| MATL-02 | Phase 7 | Pending |
| MATL-03 | Phase 7 | Pending |
| MATL-04 | Phase 7 | Pending |
| MATL-05 | Phase 7 | Pending |
| MATL-06 | Phase 7 | Pending |
| MATL-07 | Phase 7 | Pending |
| MATL-08 | Phase 7 | Pending |
| VIZL-01 | Phase 8 | Pending |
| VIZL-02 | Phase 8 | Pending |
| VIZL-03 | Phase 8 | Pending |
| VIZL-04 | Phase 8 | Pending |
| VIZL-05 | Phase 8 | Pending |
| VIZL-06 | Phase 8 | Pending |

**Coverage:**
- v1 requirements: 65 total
- Mapped to phases: 65
- Unmapped: 0 ✓

---
*Requirements defined: 2026-04-25*
*Last updated: 2026-04-25 after RESULT_DISCUSS.md review — добавлены ARCH-01-07, ZONE-06-10, ACTR-06, ENTY-05-08, TRAN-05-07, MATL-05-08, VIZL-05-06, API-01-02 (29 новых требований)*
