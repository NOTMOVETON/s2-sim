# Requirements: S2 Simulator

**Defined:** 2026-04-25
**Core Value:** Любой разработчик должен уметь добавить нового агента, плагин, эффект или актора за один файл, не трогая ядро — и сразу увидеть это в браузере.

## v1 Requirements

### Зоны — визуал и управление

- [ ] **ZONE-01**: Редактор зон в UI с инспектором (форма, параметры, эффекты)
- [ ] **ZONE-02**: VisualHint pipeline для зон (цвет, прозрачность, анимация въезда/выезда)
- [ ] **ZONE-03**: Сенсорные эффекты: туман (ухудшение видимости), ЭМ помехи (деградация GNSS/IMU)
- [ ] **ZONE-04**: Lifecycle зон: zone.strength (0–1), рост/затухание, auto-remove при strength=0, drift (движение)
- [ ] **ZONE-05**: KernelCommands для зон: SpawnZone / RemoveZone / ToggleZone (enable/disable)

### Акторы — основа

- [ ] **ACTR-01**: IActorBehavior интерфейс: initialize/update/on_interact + FSM утилита (State + transitions)
- [ ] **ACTR-02**: DoorBehavior: FSM (CLOSED/OPENING/OPEN/CLOSING), коллизия при закрытии, proximity-триггер или wire-сигнал
- [ ] **ACTR-03**: PedestrianBehavior: маршрутные точки (waypoints), ObstacleAvoidance (локальный steering)
- [ ] **ACTR-04**: ConveyorActor: attached зона-conveyor + wire signal для управления направлением/скоростью
- [ ] **ACTR-05**: ElevatorBehavior + ElevatorUserPlugin (агент вызывает лифт через команду)

### Пропы

- [ ] **PROP-01**: Prop структура: movable (да/нет), signals (список сигналов), collision (shape)
- [ ] **PROP-02**: AttachObject / DetachObject KernelCommands (приклеить prop к агенту/актору)
- [ ] **PROP-03**: GrabberPlugin для агентов (взять/положить prop, ограничения веса/дальности)

### Восприятие

- [ ] **PERC-01**: Flat зоны под любым углом (ориентация через quaternion/normal-вектор)
- [ ] **PERC-02**: ArUco сигналы на сущностях: ArUcoSignal компонент, id + pose + размер
- [ ] **PERC-03**: ArucoDetectorPlugin: dual-zone дальность (read_range / detect_range), выдача ROS2 или SharedState
- [ ] **PERC-04**: AgentDetectorPlugin: список видимых агентов с дистанцией, опциональная LoS-проверка
- [ ] **PERC-05**: ActorDetectorPlugin: список видимых акторов с типом и дистанцией
- [ ] **PERC-06**: ZoneDetectorPlugin: список зон в радиусе с их типами
- [ ] **PERC-07**: Ray-zone transit деградация дальности (мощность сигнала ослабляется при прохождении через зоны)

### Транспорт

- [ ] **TRAN-01**: HttpTransportAdapter — управление агентами через HTTP/JSON без ROS2 (subscribe/publish)
- [ ] **TRAN-02**: Per-agent transport config в YAML (transport: ros2/http/stub + параметры)
- [ ] **TRAN-03**: Единый REST Control API (замена legacy GET /command?cmd=): 4 категории команд
- [ ] **TRAN-04**: sim_time публикуется всем потребителям (ROS2 Clock, HTTP SSE, SharedState)

### Сущности — унификация

- [ ] **ENTY-01**: Capabilities + effect_tags + immune_to_effects: стандартный список возможностей, теги эффектов, иммунитет
- [ ] **ENTY-02**: Plugin lifecycle: on_reset() / on_spawn() / on_despawn() в IAgentPlugin
- [ ] **ENTY-03**: Effect model: trigger (WHILE_INSIDE/ON_ENTER/ON_EXIT) + action type (CONTRIBUTION/STATE_CHANGE/SENSOR_MOD)
- [ ] **ENTY-04**: Entity base model с опциональными слоями (PluginHost, SharedState, TransportLink, Behavior)

### Материалы

- [ ] **MATL-01**: MaterialTransfer KernelCommand: source_entity + receiver_entity + material_type + amount
- [ ] **MATL-02**: IMaterialSource / IMaterialReceiver — опциональные методы в IAgentPlugin и IActorBehavior
- [ ] **MATL-03**: DirtPile actor: 2D-grid высот, friction_angle, settling (осыпание), volume conservation
- [ ] **MATL-04**: VisualHint для передачи материала: анимация потока/осыпания

### Визуализация

- [ ] **VIZL-01**: Multi-channel данные: Core (30fps, малый JSON) / Heavy (2–5fps, лидар/траектории) / Static (по запросу, геометрия сцены)
- [ ] **VIZL-02**: IVizAdapter: абстрактный интерфейс визуализатора (Web / Null / File адаптеры)
- [ ] **VIZL-03**: VisualHint pipeline для плагинов агентов (произвольные визуальные аннотации)
- [ ] **VIZL-04**: Scripted behavior плагин (роль behavior — воспроизведение записанного поведения)

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
- **SCEN-02**: Replay система (запись + воспроизведение сессии)

## Out of Scope

| Feature | Reason |
|---------|--------|
| Heightmap | Убран: 3D-геометрия через static primitives, пещеры возможны |
| 3D лидар | Отложен на далёкое будущее, текущий 2D достаточен |
| Полноценная физика частиц | Только имитация через settling (DirtPile) |
| RViz2 адаптер | v2+, сначала web |
| Mesh-коллизии (сложные формы) | v2+, пока box/sphere/cylinder |
| Полноценная динамика | Кинематика, не динамика. Нет сил/инерции |
| Multiplayer / cloud sync | Out of scope для локального симулятора |

## Traceability

| Requirement | Phase | Status |
|-------------|-------|--------|
| ZONE-01 | Phase 1 | Pending |
| ZONE-02 | Phase 1 | Pending |
| ZONE-03 | Phase 1 | Pending |
| ZONE-04 | Phase 1 | Pending |
| ZONE-05 | Phase 1 | Pending |
| ACTR-01 | Phase 2 | Pending |
| ACTR-02 | Phase 2 | Pending |
| PROP-01 | Phase 2 | Pending |
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
| ENTY-01 | Phase 6 | Pending |
| ENTY-02 | Phase 6 | Pending |
| ENTY-03 | Phase 6 | Pending |
| ENTY-04 | Phase 6 | Pending |
| MATL-01 | Phase 7 | Pending |
| MATL-02 | Phase 7 | Pending |
| MATL-03 | Phase 7 | Pending |
| MATL-04 | Phase 7 | Pending |
| VIZL-01 | Phase 8 | Pending |
| VIZL-02 | Phase 8 | Pending |
| VIZL-03 | Phase 8 | Pending |
| VIZL-04 | Phase 8 | Pending |

**Coverage:**
- v1 requirements: 36 total
- Mapped to phases: 36
- Unmapped: 0 ✓

---
*Requirements defined: 2026-04-25*
*Last updated: 2026-04-25 after initial definition*
