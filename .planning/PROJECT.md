# S2 Simulator

## What This Is

S2 — кинематический многоагентный симулятор для тестирования верхнеуровневой логики роботов.
Задача симулятора — не воспроизводить физику, а давать воспроизводимую, расширяемую среду,
в которой поведение агентов тестируется без реального железа. До 100 агентов, нативная
поддержка ROS2, открытая архитектура плагинов.

## Core Value

Любой разработчик должен уметь добавить нового агента, плагин, эффект или актора
за один файл, не трогая ядро — и сразу увидеть это в браузере.

## Requirements

### Validated

- ✓ SimEngine с тиковым циклом и фазами (1–8) — existing
- ✓ IAgentPlugin: DiffDrive, GNSS, IMU, Battery, Gravity, Lidar, Color, JointVel — existing
- ✓ Плагины визуализации: TrajectoryRecorder, PathDisplay, TopicDisplay — existing
- ✓ ZoneSystem: сферы/боксы/цилиндры, эффекты ice/boost/lock/charging/conveyor/wind/teleport — existing
- ✓ CollisionSystem: capsule/box/sphere vs static geometry, walkable/wall/slope — existing
- ✓ GravityPlugin: snap к поверхности, свободное падение, скольжение на склоне — existing
- ✓ ROS2 транспорт: per-domain ноды, GNSS/IMU/Odometry/Lidar/TF-дерево — existing
- ✓ Web визуализатор: Three.js + SSE, 30fps, боковая панель — existing
- ✓ Редактор сцен: CRUD примитивов, CRUD агентов, undo/redo, edge-snap — existing
- ✓ Браузер сцен: runtime reload без перезапуска Docker — existing
- ✓ URDF loader: кинематическое дерево, JointVelPlugin — existing
- ✓ Система зон: вход/выход, on_agent_exit callback, YAML конфиг — existing

### Active

**Зоны, визуал, сенсорные эффекты:**
- [ ] Редактор зон в UI с инспектором (ZONE-01)
- [ ] VisualHint pipeline для зон (ZONE-02)
- [ ] Сенсорные эффекты: туман, ЭМ помехи (ZONE-03)
- [ ] Lifecycle зон: рост/затухание/сила (ZONE-04)
- [ ] KernelCommands для зон: spawn/remove/toggle (ZONE-05)

**Акторы и пропы:**
- [ ] IActorBehavior интерфейс + FSM утилита (ACTR-01)
- [ ] DoorBehavior: FSM, коллизия, proximity-триггер (ACTR-02)
- [ ] PedestrianBehavior: маршрутные точки, избегание препятствий (ACTR-03)
- [ ] ConveyorActor: attached зона + wire signal (ACTR-04)
- [ ] ElevatorBehavior + ElevatorUserPlugin (ACTR-05)
- [ ] Prop структура: movable, signals, collision (PROP-01)
- [ ] AttachObject/DetachObject KernelCommands (PROP-02)
- [ ] GrabberPlugin для агентов (PROP-03)

**Система восприятия:**
- [ ] Flat зоны под любым углом (PERC-01)
- [ ] ArUco сигналы на сущностях (PERC-02)
- [ ] ArucoDetectorPlugin с dual-zone дальностью (PERC-03)
- [ ] AgentDetectorPlugin (PERC-04)
- [ ] ActorDetectorPlugin (PERC-05)
- [ ] ZoneDetectorPlugin (PERC-06)
- [ ] Ray-zone transit деградация дальности (PERC-07)

**Рефакторинг архитектуры:**
- [ ] HttpTransportAdapter — управление через HTTP/JSON без ROS2 (TRAN-01)
- [ ] Per-agent transport config в YAML (TRAN-02)
- [ ] Единый REST Control API (замена legacy query-params) (TRAN-03)
- [ ] sim_time публикуется всем потребителям (TRAN-04)
- [ ] Capabilities + effect_tags + immune_to_effects (ENTY-01)
- [ ] Plugin lifecycle: on_reset(), on_spawn(), on_despawn() (ENTY-02)
- [ ] Effect model: trigger (WHILE_INSIDE/ON_ENTER/ON_EXIT) + action (ENTY-03)
- [ ] Entity base model с опциональными слоями (ENTY-04)

**Материалы и деформируемая среда:**
- [ ] MaterialTransfer KernelCommand (MATL-01)
- [ ] IMaterialSource/IMaterialReceiver в IAgentPlugin (MATL-02)
- [ ] DirtPile actor: grid деформация, volume conservation (MATL-03)
- [ ] VisualHint для передачи материала (анимация) (MATL-04)

**Визуализация:**
- [ ] Multi-channel данные: Core/Heavy/Static потоки (VIZL-01)
- [ ] IVizAdapter: абстрактный интерфейс визуализатора (VIZL-02)
- [ ] VisualHint pipeline для плагинов агентов (VIZL-03)
- [ ] Scripted behavior плагин (behavior role) (VIZL-04)

### Out of Scope

- Heightmap — убран, вся геометрия через static primitives
- 3D лидар — отложен на далёкое будущее
- Полноценная физика частиц — только имитация через settling
- RViz2 адаптер — v2+, сначала web
- Mesh-коллизии (сложные формы) — v2+, пока box/sphere/cylinder

## Context

Проект разрабатывается как open-source симулятор для fleet management тестирования.
Разработчик работает соло. Docker-first: все сборки и тесты только в контейнере.
Кодовая база — C++17, CMake, s2_core/s2_plugins/s2_transport/s2_visualizer/s2_config/s2_msgs.

**Архитектурный принцип:** никакой плагин не знает про другой плагин. Все общаются
через SharedState (contributions → resolver → effective constraints). Ядро — посредник.

**Визуализатор:** Three.js, SSE для стриминга снапшотов. Управление через REST API.

## Constraints

- **Стек**: C++17, CMake, Docker — не меняем без явной причины
- **Без физики**: кинематика, не динамика. Нет сил/инерции в обычном смысле
- **Docker-first**: тесты только в контейнере, локальный toolchain не авторитет
- **Язык**: весь код, комментарии, документация — на русском
- **Архитектура**: ядро (s2_core) не знает про доменные типы из s2_plugins

## Key Decisions

| Decision | Rationale | Outcome |
|----------|-----------|---------|
| SharedState через contributions | Плагины независимы, нет прямых зависимостей | ✓ Good |
| Единый IAgentPlugin с role() | Проще для open-source, Gravity нарушает B-вариант | ✓ Good |
| Heightmap убран | 3D геометрия через static primitives, пещеры возможны | ✓ Good |
| Zone не Entity | Зона — пространственное правило, не объект мира | ✓ Good |
| Effect = trigger + action | Гибче 4 жёстких типов, новые комбинации без кода | — Pending |
| Per-agent transport | Максимальная гибкость, transport pool для шаринга | — Pending |
| Entity base с опциональными слоями | Prop/Actor/Agent — типичные конфигурации, не ограничения | — Pending |

---
*Last updated: 2026-04-25 after initialization*

## Evolution

This document evolves at phase transitions and milestone boundaries.

**After each phase transition** (via `/gsd-transition`):
1. Requirements invalidated? → Move to Out of Scope with reason
2. Requirements validated? → Move to Validated with phase reference
3. New requirements emerged? → Add to Active
4. Decisions to log? → Add to Key Decisions
5. "What This Is" still accurate? → Update if drifted

**After each milestone** (via `/gsd-complete-milestone`):
1. Full review of all sections
2. Core Value check — still the right priority?
3. Audit Out of Scope — reasons still valid?
4. Update Context with current state
