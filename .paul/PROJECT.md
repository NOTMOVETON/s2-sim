# S2 Simulator

## What This Is

Кинематический мультиагентный симулятор для тестирования верхнеуровневой логики роботов: FSM-поведения, навигация, мультиагентная координация, реакция на события. Не физический движок — нет жёсткой динамики, нет SPH, нет реалистичного трения. Работает headless или с браузерным визуализатором. Управление через ROS2, HTTP или stub транспорт. Целевая нагрузка: до 100 агентов.

## Core Value

Разработчик роботов может тестировать верхнеуровневую логику стека без физического симулятора, подключая произвольный транспорт и дописывая плагины/поведения без лезения в ядро.

## Current State

| Attribute | Value |
|-----------|-------|
| Type | Application |
| Version | 0.1.0 |
| Status | Active refactoring |
| Last Updated | 2026-04-27 |

## Requirements

### Core Features

- Кинематическая симуляция агентов с plugin-based архитектурой (DiffDrive, Lidar, GNSS, Battery и др.)
- Зоны с эффектами: модификаторы скорости, мутации состояния, сенсорные помехи
- Акторы с FSM-поведениями (двери, конвейеры, пешеходы)
- Транспортный слой: ROS2, HTTP, stub — per-agent выбор
- Браузерный визуализатор (Three.js + SSE)

### Validated (Shipped)

- [x] Agent + SharedState + Resolver (speed_scale, velocity_addition, motion_locked)
- [x] IAgentPlugin: DiffDrive, Lidar, GNSS, IMU, Battery, Gravity, Color, JointVel
- [x] IAgentPlugin полный lifecycle: on_reset/on_spawn/on_despawn/on_scene_load, PluginRole, provided_capabilities() — Phase 1
- [x] Unified Entity Model: Agent/Actor/Prop flat structs, SimWorld Variant D (O(1) lookup), AgentData/PropData/ActorData, role() enforcement, SceneLoader YAML tags/transport/immune_to_effects — Phase 2
- [x] KernelCommands Queue: std::variant<24 cmd types>, CommandQueue (mutex+swap-drain), SimEngine PHASE 0, SimBus +10 event types — Phase 3
- [x] REST API + IVizAdapter: RestApiServer (порт viz+1, 19 эндпоинтов → enqueue), IVizAdapter/WebVizAdapter/NullVizAdapter/VizRegistry, VizCommandHandler удалён — Phase 4
- [x] IActorBehavior + BehaviorRegistry: IActorBehavior интерфейс (lifecycle+material stubs), ActorFSM утилита, WorldContext stub, BehaviorRegistry фабрика, Actor::BehaviorSlot, SceneLoader YAML-загрузка — Phase 5
- [x] ZoneSystem с MODIFIER/CONTINUOUS/MUTATION эффектами
- [x] Capabilities для matching эффектов
- [x] ROS2 транспорт (per-agent domain_id)
- [x] VizServer (SSE + HTTP, Three.js)
- [x] Collision, KinematicTree (URDF), RaycastEngine
- [x] SceneLoader (YAML)

### Active (In Progress)

- [ ] Architectural refactoring to spec (Milestone 1, phases 1-13)

### Planned (Next)

- [ ] Milestone 1: Migration — unified Entity model, IActorBehavior, Trigger×Action effects, per-agent transport, REST API, IVizAdapter, 3-channel viz
- [ ] Milestone 2: New Features — signals, actors (door/conveyor/pedestrian), material system, visual hints, scripted agents

### Out of Scope

- Жёсткая физика (импульсы, отскоки, реалистичное трение)
- Симуляция сенсоров на низком уровне (шум фотонов, ARuCO в искажённой плоскости)
- Моделирование среды передачи данных (потери пакетов, задержки сети)
- SPH/CFD жидкостная физика
- Физика падения частиц материала (заменено на mгновенный transfer + visual hint)

## Constraints

### Technical Constraints

- C++17, CMake build
- Docker-first: сборка и запуск только в контейнере
- Ядро не знает про ROS2, Three.js, конкретные плагины — только через интерфейсы
- Детерминизм: dt фиксирован, ускорение = больше тиков/сек
- Зоны — не Entity: нет коллизии, нельзя захватить, нельзя видеть лидаром

### Business Constraints

- Open-source: лёгкость расширения важнее оптимальности
- Пользователь должен добавить плагин/поведение без изменений ядра

## Key Decisions

| Decision | Rationale | Date | Status |
|----------|-----------|------|--------|
| Unified Entity model (Variant A) | Единый реестр, зоны работают со всеми Entity, нет дублирования | 2026-04-27 | Active |
| Один IAgentPlugin интерфейс + role() | Нет жёстких раздельных интерфейсов, проще совмещать роли | 2026-04-27 | Active |
| on_spawn/on_despawn принимают Agent& (не Entity&) | Entity ещё нет в Phase 1; Phase 2 обновит сигнатуры | 2026-04-28 | Active |
| Battery хранит Agent* из initialize() | on_reset() без аргументов, но нужен доступ к SharedState | 2026-04-28 | Active |
| Trigger × Action для эффектов | Заменяет 4 жёстких типа, даёт новые комбинации | 2026-04-27 | Active |
| KernelCommands очередь | Атомарное применение изменений, thread-safety, hot reload | 2026-04-27 | Active |
| Per-agent transport + TransportPool | Mix ROS2+HTTP+stub, domain_id не в ядре | 2026-04-27 | Active |
| IVizAdapter интерфейс | Заменяет захардкоженный VizServer | 2026-04-27 | Active |
| Три канала визуализатора | Core State / Heavy Data / Static — лидар не блокирует команды | 2026-04-27 | Active |
| Imperative actor behavior | Behavior напрямую двигает геометрию и collision (не декларативно) | 2026-04-27 | Active |
| Props без SharedState | Зональные эффекты бинарны для пропов; накопление → актор | 2026-04-27 | Active |
| own_effects TTL + OWN_EFFECT_REMOVE | Постоянные эффекты удаляются зоной, плагином или по таймеру | 2026-04-27 | Active |
| turn_rate_scale отдельно от speed_scale | Лёд может блокировать поворот без влияния на скорость | 2026-04-27 | Active |
| Forward-declare Actor в actor_behavior.hpp | entity.hpp включает actor_behavior.hpp; избегает циклической зависимости | 2026-05-02 | Active |
| WorldContext stub (sim_time+dt) | Phase 9 добавит WorldQuery*; не преждевременно | 2026-05-02 | Active |
| BehaviorRegistry явный параметр (не singleton) | Тестируемость, множественные registry; SceneLoader::load() backward-compatible | 2026-05-02 | Active |

## Success Metrics

| Metric | Target | Current | Status |
|--------|--------|---------|--------|
| Milestone 1 complete | 13 фаз | 5/13 | In progress |
| Milestone 2 complete | 14 фаз | 0/14 | Not started |
| Тесты проходят после каждой фазы | 100% | — | — |
| Визуализатор работает на новой архитектуре | да | — | — |

## Tech Stack

| Layer | Technology | Notes |
|-------|------------|-------|
| Core | C++17 | Стандарт языка |
| Build | CMake 3.16+ | |
| Runtime | Docker | Сборка и тесты только в контейнере |
| Math | Eigen3 | Vec3, Transform3D |
| Config | yaml-cpp | Загрузка сцен |
| JSON | nlohmann_json | Снапшоты, плагин данные |
| Geo | GeographicLib | Earth→map координаты |
| Transport | ROS2 (Humble) | rclcpp, основной транспорт |
| Visualizer | Three.js | Браузерный 3D рендер |
| Viz server | HTTP + SSE | Сервер снапшотов |
| Tests | GTest | Unit + integration |

---
*PROJECT.md — Updated when requirements or context change*
*Last updated: 2026-05-02 after Phase 5*
