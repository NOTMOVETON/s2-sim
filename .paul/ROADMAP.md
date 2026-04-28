# Roadmap: S2 Simulator

## Overview

Два этапа: сначала полный рефакторинг существующего кода на новую архитектуру (Milestone 1, 13 фаз) — симулятор работает так же, но на правильном фундаменте. Затем новые фичи поверх этого фундамента (Milestone 2, 14 фаз).

## Current Milestone

**v1.0 Architecture Migration** (v1.0.0)
Status: In progress
Phases: 0 of 13 complete

## Milestones

| Version | Name | Phases | Status | Completed |
|---------|------|--------|--------|-----------|
| v1.0 | Architecture Migration | 1-13 | In progress | - |
| v2.0 | New Features | 14-27 | Not started | - |

---

## 🚧 Milestone v1.0: Architecture Migration

**Goal:** Переписать симулятор на новую архитектуру из RESULT_DISCUSS.md. После завершения — всё что работало раньше, работает на новом фундаменте.

### Phases

| Phase | Name | Plans | Status | Completed |
|-------|------|-------|--------|-----------|
| 1 | Plugin Lifecycle | 1 | ✅ Complete | 2026-04-28 |
| 2 | Unified Entity Model | 1 | Not started | - |
| 3 | KernelCommands Queue | 1 | Not started | - |
| 4 | REST API + IVizAdapter | 1 | Not started | - |
| 5 | IActorBehavior + BehaviorRegistry | 1 | Not started | - |
| 6 | EffectPlugin Trigger×Action | 1 | Not started | - |
| 7 | SharedState Revision | 1 | Not started | - |
| 8 | Per-agent Transport + Pool | 1 | Not started | - |
| 9 | WorldQuery Formalization | 1 | Not started | - |
| 10 | Tick Lifecycle Revision | 1 | Not started | - |
| 11 | Zone Lifecycle | 1 | Not started | - |
| 12 | Moving Zones + Owned Zones | 1 | Not started | - |
| 13 | Three-channel Visualizer | 1 | Not started | - |

### Phase Details

#### Phase 1: Plugin Lifecycle

**Goal:** Все плагины имеют полный lifecycle. Баги on_reset() исправлены.
**Depends on:** Nothing
**Research:** Unlikely

**Scope:**
- Добавить `on_spawn(Entity&)`, `on_despawn(Entity&)`, `on_scene_load(World&)` в IAgentPlugin
- Исправить `on_reset()` во всех существующих плагинах: DiffDrive (external_linear_velocity_), Battery (заряд), все остальные
- Добавить `role()` метод с PluginRole enum (actuation/sensor/interaction/resource/utility)
- Добавить `provided_capabilities()` вызов при инициализации (автодобавление в entity.capabilities)
- `config_schema()` — объявить в интерфейсе (пустая дефолт реализация, заполнят позже)
- Тесты: lifecycle вызывается в правильном порядке; on_reset сбрасывает state

**Plans:**
- [ ] 01-01: Plugin lifecycle complete

---

#### Phase 2: Unified Entity Model

**Goal:** Единый `Entity` с опциональными слоями. Agent/Actor/Prop — семантические метки, не классы.
**Depends on:** Phase 1 (lifecycle в новом Entity)
**Research:** Unlikely (архитектура определена)

**Scope:**
- Struct `Entity` с полями: id, type (AGENT/ACTOR/PROP), name, world_pose, collision, visual, capabilities, tags, immune_to_effects, signals (заглушка), enabled, owned_zones (заглушка)
- Опциональные слои: `PluginHost`, `SharedState`, `TransportLink`, `BehaviorSlot`, `LinkTree`
- Props явно не получают SharedState (assertion/enforcement)
- `role()` enforcement: максимум один actuation-плагин на Entity
- SimEngine: единый реестр `unordered_map<EntityId, Entity>`
- ZoneSystem, транспорт, viz — рефакторинг под новый Entity
- SceneLoader: новый YAML формат (transport field, capabilities, immune_to_effects, tags)
- LinkTree: per-link capabilities, immune_to_effects, tags (через link_overrides в YAML)
- Тесты: Entity создаётся с правильными слоями; Props без SharedState; YAML загружается

**Plans:**
- [ ] 02-01: Unified Entity model

---

#### Phase 3: KernelCommands Queue

**Goal:** Все изменения мира идут через буфер команд. Применяются атомарно в начале тика.
**Depends on:** Phase 2 (Entity model для SpawnEntity)
**Research:** Unlikely

**Scope:**
- Struct-per-command (variant или std::any): SpawnEntity, DespawnEntity, SetPose, SetEnabled, AddPlugin, RemovePlugin, ConfigPlugin, SpawnZone, DespawnZone, ToggleZone, SetZoneShape, SetZoneStrength, Interact, AttachObject, DetachObject, PauseSim, ResumeSim, ResetSim, StepSim, SetSpeed, LoadScene, SaveScene, NewScene
- `RemoveOwnEffect { entity_id, effect_tags }` — для ремонта
- Thread-safe очередь (mutex): REST поток пишет, sim поток читает
- SimEngine: ФАЗА 0 применяет всё накопленное атомарно
- EventBus: формальный список типов событий (EntitySpawned, EntityDespawned, ActorStateChanged, ZoneEntered, ZoneExited, GrabAttempt/Succeeded/Failed, DamageDealt, SignalActivated/Deactivated)
- Тесты: команды применяются в начале следующего тика, не в середине

**Plans:**
- [ ] 03-01: KernelCommands queue

---

#### Phase 4: REST API + IVizAdapter

**Goal:** REST API отделён от визуализатора. VizServer за интерфейсом IVizAdapter.
**Depends on:** Phase 3 (команды через очередь)
**Research:** Unlikely

**Scope:**
- `IVizAdapter` интерфейс: `publish(WorldSnapshot&)`, `on_command(cb)`
- `WebVizAdapter` — рефакторинг VizServer (только SSE публикация снапшотов)
- `NullVizAdapter` — пустая реализация для headless/тестов
- `VizRegistry` — регистрация адаптеров
- Отдельный HTTP REST сервер для управления (не VizServer):
  - `POST /sim/pause|resume|reset|step|speed`
  - `GET /sim/status`
  - `GET|POST|DELETE /world/entities`
  - `PUT /world/entities/{id}/pose|enabled`
  - `GET|POST|DELETE /world/zones`
  - `PUT /world/zones/{id}/enabled|shape|strength`
  - `GET|POST|PUT|DELETE /world/entities/{id}/plugins`
  - `POST /agents/{id}/input/{plugin_type}`
  - `POST /agents/{id}/services/{name}`
  - `GET /agents/{id}/topics/{name}`
  - `GET|POST /scenes`
- VizCommandHandler удалён — команды идут через KernelCommands
- SimEngine принимает `IVizAdapter*`
- Тесты: REST команды применяются; viz публикует снапшот

**Plans:**
- [ ] 04-01: REST API + IVizAdapter

---

#### Phase 5: IActorBehavior + BehaviorRegistry

**Goal:** Акторы имеют behavior систему. FSM утилита готова. BehaviorRegistry работает.
**Depends on:** Phase 2 (Entity model с BehaviorSlot), Phase 9 (WorldContext/WorldQuery — частичная зависимость)
**Research:** Unlikely

**Scope:**
- `IActorBehavior` интерфейс: type(), on_init(yaml), on_spawn(Entity&), on_reset(), update(dt, Entity&, WorldContext&), on_signal(SignalEvent&) (заглушка), on_interact(EntityId, action, params), current_state(), to_json()
- Материальные методы в интерфейсе (заглушки): can_release_material(), can_accept_material(), release_material(), accept_material(), is_deformable(), apply_deformation()
- `ActorFSM` утилитарный класс: add_state(), add_transition(), fire(), update(dt), current_state()
- `WorldContext` — обёртка над WorldQuery для передачи в behavior.update()
- `BehaviorRegistry`: регистрация типов по строке, factory create(type, yaml)
- SceneLoader: загружает behavior по имени из YAML
- Тесты: behavior создаётся, обновляется, сбрасывается; FSM переключает состояния

**Plans:**
- [ ] 05-01: IActorBehavior + BehaviorRegistry

---

#### Phase 6: EffectPlugin Trigger×Action

**Goal:** Эффекты описываются двумя измерениями. Новые action types. effect_tags + immune_to_effects работают.
**Depends on:** Phase 2 (Entity с immune_to_effects, capabilities), Phase 7 (own_effects в SharedState)
**Research:** Unlikely

**Scope:**
- `EffectTrigger` enum: WHILE_INSIDE, ON_ENTER, ON_EXIT
- `EffectAction` enum: CONTRIBUTION, STATE_CHANGE, SENSOR_MOD, OWN_EFFECT_SPAWN, OWN_EFFECT_REMOVE
- Новый `EffectPlugin` интерфейс: type(), trigger(), apply(SharedState&, EffectContext&), required_capabilities(), excluded_capabilities(), effect_tags()
- `EffectContext`: entity, zone, dt, zone_strength, contact_link
- `Zone.strength` поле (0..1) — используется в EffectContext
- Matching rule (3 условия): capabilities, excluded_capabilities, immune_to_effects
- Миграция существующих эффектов: IceModifier, BoostZone, MotionLockZone, ChargingEffect, ConveyorEffect, WindEffect, TirePunctureEffect → новые trigger+action
- `detection_mode` на Zone: center, bounding, per_link (per_link — полная реализация в Phase 27)
- Тесты: matching работает; immune_to_effects блокирует; ON_ENTER срабатывает один раз

**Plans:**
- [ ] 06-01: EffectPlugin Trigger×Action

---

#### Phase 7: SharedState Revision

**Goal:** Полный набор resolved fields. own_effects с TTL и удалением.
**Depends on:** Phase 2 (Entity), Phase 6 (OWN_EFFECT_SPAWN/REMOVE actions)
**Research:** Unlikely

**Scope:**
- Новые resolved fields: `turn_rate_scale` (product), `max_angular_cap` (MIN)
- Полный список: speed_scale, turn_rate_scale, motion_locked, velocity_addition, angular_drift, max_speed_cap, max_angular_cap, manipulation_locked, all_plugins_disabled, damage_rate
- `OwnEffect` struct: type, tags, ttl (optional), age, contribute() метод
- `Entity.own_effects: vector<OwnEffect>` — живут постоянно пока не удалены
- Contribute фаза: own_effects.contribute() → публикует в SharedState каждый тик
- TTL: age инкрементируется, при age >= ttl — удаляется
- `KernelCommand::RemoveOwnEffect` применяется в Phase 0
- Resolver обновлён: считает turn_rate_scale, max_angular_cap
- DiffDrive обновлён: читает turn_rate_scale и max_angular_cap из effective
- Тесты: own_effect живёт, вносит contribution, удаляется по TTL и по команде

**Plans:**
- [ ] 07-01: SharedState revision

---

#### Phase 8: Per-agent Transport + TransportPool

**Goal:** Каждый агент выбирает транспорт. domain_id только в ROS2-конфиге. HttpTransportAdapter работает.
**Depends on:** Phase 2 (Entity с transport_type + transport_config), Phase 4 (REST API)
**Research:** Unlikely (HttpAdapter — новая реализация)

**Scope:**
- `TransportPool`: get_or_create(type) → ITransportAdapter&
- `Entity.transport_type: string`, `Entity.transport_config: YAML::Node`
- `Agent.domain_id` удалён — переехал в `transport_config.ros2.domain_id`
- YAML формат: `transport: ros2`, `ros2: { domain_id: 50 }`
- `SimTransportBridge` рефакторинг: использует TransportPool, регистрирует по типу
- `HttpTransportAdapter`: минимальная реализация
  - `POST /agents/{name}/input/{plugin}` → handle_input()
  - `GET /agents/{name}/topics/{topic}` → last published value
  - `POST /agents/{name}/services/{name}` → handle_service()
  - SSE endpoint для agent events
  - `sim_time` в каждом ответе
- `StubTransportAdapter`: no-op, для scripted и тестовых агентов
- Тесты: ROS2 агент и HTTP агент в одной сцене; stub агент не публикует

**Plans:**
- [ ] 08-01: Per-agent transport + Pool

---

#### Phase 9: WorldQuery Formalization

**Goal:** Плагины и behaviors используют только read-only WorldQuery API. Нет прямого доступа к world.
**Depends on:** Phase 2 (Entity model)
**Research:** Unlikely

**Scope:**
- `WorldQuery` класс: find_in_radius(), find_in_box(), find_nearest(), find_entity_below(), find_signals_of_type() (заглушка), has_line_of_sight(), raycast(), zones_at(), is_in_zone(), find_deformable_in_box() (заглушка)
- `WorldContext` = WorldQuery + sim_time + dt (передаётся в behavior.update и plugin.update)
- Все существующие плагины рефакторятся: используют WorldQuery вместо прямого доступа к SimEngine или world
- Тесты: find_in_radius возвращает правильные Entity; raycast работает

**Plans:**
- [ ] 09-01: WorldQuery formalization

---

#### Phase 10: Tick Lifecycle Revision

**Goal:** Новый порядок фаз тика. Сенсоры после кинематики. Зоны в двух проходах. tick_index в снапшоте.
**Depends on:** Phase 2-9 (все предыдущие)
**Research:** Unlikely

**Scope:**
- Новый порядок фаз (см. RESULT_DISCUSS.md §16 + обсуждение):
  - 0: KernelCommands apply
  - 0.5: Zone membership compute (без apply)
  - 1: Transport incoming (handle_input)
  - 2: Акторы (pre_resolve → zone effects → own_effects → resolver → behavior.update → plugins.update)
  - 3: Агенты (pre_resolve → zone effects → own_effects → resolver → SENSOR_MOD → actuation → кинематика → collision → surface align)
  - 4: Sensor plugins (из финальной позиции)
  - 5: Interaction plugins
  - 6: Attachments update
  - 7: Snapshot build + viz publish
  - 8: Transport publish
  - 9: clear_contributions() + TTL decrement + deferred despawn
- `WorldSnapshot` добавляет: `tick_index`, `speed_factor`, `real_time`
- Тесты: сенсоры берут позицию после коллизий; зоны на акторах применяются до behavior.update

**Plans:**
- [ ] 10-01: Tick lifecycle revision

---

#### Phase 11: Zone Lifecycle

**Goal:** Динамические зоны с шаблонами, силой, ростом/затуханием и триггерами.
**Depends on:** Phase 3 (SpawnZone KernelCommand), Phase 10 (tick order)
**Research:** Unlikely

**Scope:**
- `Zone.strength: double` (0..1) — текущая сила; EffectPlugin уже использует (Phase 6)
- Zone templates: именованные шаблоны в YAML, SpawnZone по имени шаблона
- Lifecycle config: initial_strength, growth (rate, max), decay (delay, rate, remove_at)
- Self-destruct policy: on_any_contact, on_effect_applied
- Spawn triggers: command (уже), event (по EventBus событию), timer (через N секунд), state_change (когда Entity меняет состояние)
- ZoneSystem.tick(): инкрементирует age, обновляет strength, применяет growth/decay, удаляет при remove_at
- Тесты: зона растёт, затухает, удаляется; self-destruct срабатывает

**Plans:**
- [ ] 11-01: Zone lifecycle

---

#### Phase 12: Moving Zones + Owned Zones

**Goal:** Зоны двигаются с Entity. Entity владеет зонами на своих линках.
**Depends on:** Phase 2 (Entity.owned_zones), Phase 11 (Zone lifecycle)
**Research:** Unlikely

**Scope:**
- `Zone.attached_to: optional<EntityId>` — зона следует за Entity
- `Zone.attached_to_link: optional<string>` — следует за конкретным линком
- ZoneSystem.tick(): update_attached_zone_positions() перед membership compute
- `Entity.owned_zones: vector<ZoneId>` — при despawn entity → despawn owned zones
- `owned_zones` секция в YAML: inline зоны при описании Entity
- Invisible prop carrier подход: для движущихся зон без явного носителя
- Тесты: зона двигается с Entity; owned_zone despawns вместе с owner

**Plans:**
- [ ] 12-01: Moving zones + owned zones

---

#### Phase 13: Three-channel Visualizer

**Goal:** Три канала данных. Лидар не блокирует команды. Static geometry по запросу.
**Depends on:** Phase 4 (IVizAdapter), Phase 10 (tick order)
**Research:** Unlikely

**Scope:**
- Канал 1 (Core State, 30fps): позы Entity, скорости, состояния акторов — маленький JSON
- Канал 2 (Heavy Data, 2-5fps): лидар points, траектории, overlay данные — отдельный SSE endpoint
- Канал 3 (Static, по запросу): геометрия сцены, зоны, URDF — GET с ETag
- `WebVizAdapter` разделяет публикацию на три потока
- plugin.contribute_snapshot() тегирует данные как core/heavy/static
- Команды (REST POST) не конкурируют со stream'ами
- Тесты: heavy data не блокирует core state; static отдаётся только при изменении

**Plans:**
- [ ] 13-01: Three-channel visualizer

---

## 📋 Milestone v2.0: New Features

**Goal:** Новые возможности поверх новой архитектуры.
**Prerequisite:** v1.0 complete

| Phase | Focus | Research |
|-------|-------|----------|
| 14 | Signals (Signal struct, Wire, range+LoS) | Unlikely |
| 15 | First actors: DoorBehavior, ConveyorBehavior | Unlikely |
| 16 | Signal controllers: DoorWireController, ConveyorWireController, EventReactor | Unlikely |
| 17 | PedestrianBehavior | Likely (pathfinding) |
| 18 | ScriptedBehavior + scripted agents (patrol, waypoints) | Unlikely |
| 19 | EntityDetector + SignalDetector (configurable filters) | Unlikely |
| 20 | Material system core: MaterialTransfer, DeformEntity, DirtGridBehavior, Displacement | Likely |
| 21 | Attachments v1: BucketAttachment, TruckCargo | Unlikely |
| 22 | Attachments v2: TankAttachment, BladeAttachment, RotorAttachment, RakeAttachment | Unlikely |
| 23 | Visual Hints v1: presets (marker/particles/glow/arrows/trail) + custom JS modules | Unlikely |
| 24 | Transfer hints: arc/pour/dump/spray/stream анимация | Unlikely |
| 25 | Hot patch UI: add/remove plugin из визуализатора | Unlikely |
| 26 | Registry endpoints: /api/plugins/registry с config_schema | Unlikely |
| 27 | Per-link detection: detection_mode per_link; link-level immunity | Unlikely |

---
*Roadmap created: 2026-04-27*
*Last updated: 2026-04-27*
