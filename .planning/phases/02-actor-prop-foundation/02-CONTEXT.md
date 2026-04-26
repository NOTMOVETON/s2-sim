# Phase 2: Actor & Prop Foundation - Context

**Gathered:** 2026-04-26
**Status:** Ready for planning

<domain>
## Phase Boundary

Система акторов с расширенным IActorBehavior, DoorBehavior с proximity-триггером и wire-сигналами, система пропов с attach/detach, signal controller плагины.

Требования: ACTR-01, ACTR-02, ACTR-06, PROP-01, PROP-02, PROP-03.

Scope фиксирован задачами 2.1–2.8 из ROADMAP.md. Phase 3 (PedestrianBehavior, ConveyorActor, ElevatorBehavior) строится поверх инфраструктуры из Phase 2.

</domain>

<decisions>
## Implementation Decisions

### IActorBehavior интерфейс (ACTR-01)

- **D-01:** IActorBehavior — единое поведение актора. Интерфейс: `on_init(YAML::Node&)`, `on_spawn(Entity&)`, `on_reset()`, `update(dt, Entity&, WorldContext&)`, `on_signal(SignalEvent&)`, `on_interact(EntityId source, string action, json params)`, `current_state()`, `to_json()`. Опциональные методы материалов (can_release/accept_material, is_deformable) — стабы для Phase 7.
- **D-02:** ActorFSM — утилитарный класс, не требование. API: `add_state(name, on_enter, on_update, on_exit)`, `add_transition(from, to, trigger, guard?)`, `fire(trigger)`, `current_state()`, `update(dt)`. DoorBehavior использует FSM, DirtPile — нет.
- **D-03:** Behavior **императивно** управляет геометрией и collision актора. Не публикует абстрактное состояние — сам двигает `actor.parts["door_panel"].pose.yaw` и обновляет `actor.collision`. Декоративные эффекты — через `visual_hint`.

### ActorRegistry и тиковый цикл (2.2)

- **D-04:** Акторы хранятся в `world_.actors` (vector<Actor> в SimWorld). Актор имеет `std::unique_ptr<IActorBehavior> behavior` и опционально вектор плагинов через PluginHost.
- **D-05:** Actor (типичный) = PluginHost(опционально) + SharedState(да) + TransportLink(нет) + Behavior(да). SharedState актора участвует в contribution/resolver — DoorWireController может писать lock contributions.
- **D-06:** Phase 2 тика: `plugins.pre_resolve(dt)` → `Resolver` → `behavior.update(dt, entity, ctx)` → `plugins.update(dt)`. Плагины готовят данные в SharedState, behavior читает effective и действует.
- **D-07:** Behavior **явно** публикует ActorStateChanged события через EventBus при смене состояния FSM. Engine не отслеживает current_state() автоматически — ответственность behavior.

### DoorBehavior (ACTR-02)

- **D-08:** FSM: CLOSED → OPENING → OPEN → CLOSING → CLOSED. Таймерные переходы (open_duration, close_duration, auto_close_secs). collision_enabled=false при OPEN, collision_enabled=true при CLOSED.
- **D-09:** Proximity-триггер: WorldQuery.find_in_radius() ищет агентов вблизи. Агент с DoorOpenerPlugin шлёт KernelCommand::Interact{action:"open"} → ядро маршрутизирует к behavior.on_interact().
- **D-10:** Wire-триггер: DoorWireController (плагин на акторе) слушает wire-сигналы → вызывает actions на behavior через on_signal() или SharedState contributions (lock).

### SignalListenerBase + Controllers (ACTR-06)

- **D-11:** SignalListenerBase — общая база для controller-плагинов. API: `scan_signals()`, `filter_by_id(signal_id)`, `filter_by_source(entity_id)`, virtual `react()`. Контроллеры — плагины роли INTERACTION на акторе.
- **D-12:** DoorWireController: реакции `close_and_lock`, `force_open`, `unlock`. Конфигурация через YAML `reactions: [{signal_id, source_entity, on_active, on_inactive}]`.
- **D-13:** ConveyorWireController: реакции `stop`, `reverse`, `start`. Аналогичная декларативная конфигурация.
- **D-14:** EventReactor — обобщённый плагин для тривиальных случаев: `listen: {signal_id}` + `on_active: {fire_event}` + `on_inactive: {fire_event}`. Сосуществует с конкретными контроллерами.
- **D-15:** Доставка wire-сигналов: SignalListenerBase использует scan_signals() (вероятно через WorldQuery или прямой доступ к entity.signals) для обнаружения активных сигналов. EventBus.SignalActivated/Deactivated — для event-driven уведомлений.

### Prop структура (PROP-01)

- **D-16:** Prop struct: id, name, type, world_pose, movable(bool), has_collision(bool), collision(CollisionShape), visual(VisualDesc), signals(vector<Signal>), capabilities(set<string>), tags(map<string,string>). Без SharedState по умолчанию.
- **D-17:** Граница Prop↔Actor: нет update(dt) → Prop; есть update(dt) → Actor. Ящик с capability `fragile` в зоне vibration → мгновенно уничтожается бинарно (без SharedState).
- **D-18:** Props с has_collision=true и без attached_to_agent участвуют в CollisionSystem как статические объекты.

### AttachObject / DetachObject (PROP-02)

- **D-19:** KernelCommand::AttachObject{parent_id, link, child_id, local_pose} и DetachObject{child_id, drop_pose} уже в variant — нужны обработчики в phase0_kernel_commands(). При attach: вычислить offset = prop.pose - link_world_pos; записать attached_to_agent, attach_link, attach_offset в Prop.
- **D-20:** Phase 6 тика (attachments): итерация props с attached_to_agent → обновить world_pose из agent.get_link_world_pos(attach_link) + offset. Тот же механизм что и owned_zones.
- **D-21:** Collision **отключается** для захваченных пропов: `if (prop.attached_to_agent) continue;` в CollisionSystem. Захваченный проп не блокирует движение.

### GrabberPlugin (PROP-03)

- **D-22:** GrabberPlugin: роль INTERACTION, proximity-based. Использует WorldQuery (query_nearest_movable_prop или find_in_radius с фильтром) для поиска ближайшего movable пропа в interaction_distance.
- **D-23:** Команды grab/release через KernelCommand::Interact{action:"grab"/"release"}. При grab: ядро валидирует дистанцию и передаёт behaviour/plugin. Plugin шлёт AttachObjectCommand. При release: DetachObjectCommand.
- **D-24:** EventBus события: GrabAttempt (при попытке), GrabSucceeded (при успешном захвате), GrabFailed (если нет пропа в радиусе/превышен вес). SharedState contribution: manipulation_locked (bool).
- **D-25:** Ограничения: interaction_distance (дальность), max_weight (если в будущем пропам добавится масса). Конфигурация через YAML.

### Claude's Discretion

- Внутренняя реализация ActorFSM (std::unordered_map для состояний или vector)
- Формат WorldContext — может быть тем же PluginContext или подмножеством
- Точная сигнатура scan_signals() в SignalListenerBase
- Порядок тестов и разбивка на коммиты
- Формат visual_hint для двери (door_progress, sound)

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### Архитектура акторов (главный источник)
- `RESULT_DISCUSS.md` §7 — IActorBehavior: один behavior + опциональные плагины, tick order, FSM утилита, триггеры, императивное визуальное состояние
- `RESULT_DISCUSS.md` §7.2 — Порядок тика для актора с плагинами
- `RESULT_DISCUSS.md` §7.5 — Визуальное состояние ИМПЕРАТИВНОЕ (behavior двигает геометрию и collision напрямую)
- `RESULT_DISCUSS.md` §4.1–4.4 — Entity model, типичные комбинации, граница Prop↔Actor, пропы без SharedState

### Сигналы и wire-контроллеры
- `RESULT_DISCUSS.md` §10.1–10.2 — Signal struct, wire как частный случай сигнала
- `RESULT_DISCUSS.md` §10.6 — SignalListenerBase + конкретные controller-плагины (DoorWireController, ConveyorWireController)
- `RESULT_DISCUSS.md` §10.7 — EventReactor для простых случаев

### Детальные реализации (task-документы)
- `docs/32-actor-base-door.md` — IActorBehavior интерфейс, Actor struct, DoorBehavior FSM, DoorOpenerPlugin, World Query API
- `docs/35-props-attachment.md` — Prop struct, Attachment commands, CollisionSystem + пропы, GrabberPlugin proximity-based

### Требования
- `.planning/REQUIREMENTS.md` §ACTR-01, §ACTR-02, §ACTR-06, §PROP-01–03 — Полные спецификации Phase 2

### Существующий код (точки изменений)
- `workspace/s2_core/include/s2/actor.hpp` — Actor struct (minimal, расширить)
- `workspace/s2_core/include/s2/prop.hpp` — Prop struct (minimal, расширить)
- `workspace/s2_core/include/s2/kernel_command.hpp` — Interact/AttachObject/DetachObject уже в variant
- `workspace/s2_core/include/s2/sim_engine.hpp` — phase2_actors() stub, phase6_attachments() stub, apply_kernel_command() TODO
- `workspace/s2_core/include/s2/event_bus.hpp` — SignalActivated/Deactivated, GrabAttempt/GrabSucceeded/GrabFailed уже объявлены
- `workspace/s2_core/include/s2/types.hpp` — Signal struct (complete)
- `workspace/s2_core/include/s2/world_query.hpp` — find_in_radius, find_signals_of_type (interface ready)

</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets
- `Actor struct` (actor.hpp): базовый — id, name, world_pose, current_state, collision, visual. Нужно расширить: type, collision_enabled, behavior (unique_ptr<IActorBehavior>), plugins vector
- `Prop struct` (prop.hpp): базовый — id, type, world_pose, movable, collision, visual, properties map. Нужно расширить: signals, capabilities, tags, attached_to_agent/link/offset
- `cmd::Interact/AttachObject/DetachObject` в kernel_command.hpp — уже определены, нужны обработчики
- `event::SignalActivated/SignalDeactivated` в event_bus.hpp — готовы для wire-контроллеров
- `event::GrabAttempt/GrabSucceeded/GrabFailed` в event_bus.hpp — готовы для GrabberPlugin
- `PluginRole::INTERACTION` — роль для GrabberPlugin и controller-плагинов
- `phase6_attachments()` stub в SimEngine — место для обновления позиций пропов/owned_zones
- Zone attachment pattern (zone_system.cpp: update_owned_zones_positions()) — аналог для пропов

### Established Patterns
- Contribution-based SharedState: плагин публикует → resolver собирает → actuation читает effective
- EventBus для межмодульной коммуникации (никаких прямых зависимостей между плагинами)
- IAgentPlugin lifecycle: on_spawn, on_reset, on_despawn, update(dt, agent, ctx)
- KernelCommand обработчик в phase0: switch/visitor по variant-типу
- Proximity-based interaction: query_nearest_actor_by_type / find_in_radius (не хардкодить target ID)
- Plugin factory: registry + from_config() для создания по имени типа из YAML

### Integration Points
- SimEngine::phase0_kernel_commands() — добавить обработчики Interact, AttachObject, DetachObject
- SimEngine::phase2_actors() — реализовать тиковый цикл акторов (pre_resolve→resolve→behavior→plugins)
- SimEngine::phase6_attachments() — обновление позиций attached пропов
- SceneLoader — парсинг actors с behavior + plugins, парсинг props
- CollisionSystem — пропы как статика (skip если attached)
- WorldSnapshot — включить actor states и prop positions

</code_context>

<specifics>
## Specific Ideas

- RESULT_DISCUSS.md §7.5: behavior **напрямую** двигает actor.parts["door_panel"].pose.yaw и обновляет collision — НЕ публикует абстрактное состояние для engine интерпретации.
- docs/32: DoorBehavior публикует ActorStateChangedEvent в EventBus сам при переходах FSM (bus.publish).
- docs/35: Prop collision при attachment: `if (prop.attached_to_agent) continue;` — захваченный проп пропускается в CollisionSystem.
- DoorOpenerPlugin (interaction плагин агента) и DoorWireController (interaction плагин актора) — два способа открыть дверь: proximity и wire-сигнал.
- Actor с плагинами: actor.plugins — тот же vector<unique_ptr<IAgentPlugin>> что у агентов. Controller-плагины (DoorWireController) живут на акторе как плагины роли INTERACTION.

</specifics>

<deferred>
## Deferred Ideas

- PedestrianBehavior, ConveyorActor, ElevatorBehavior — Phase 3
- Capabilities auto-declaration из плагинов — Phase 6 (ENTY-08)
- Entity base model с опциональными слоями (ENTY-04) — Phase 6
- MaterialTransfer / is_deformable() — Phase 7 (стабы в интерфейсе сейчас)
- Props с весом (max_weight ограничение в GrabberPlugin) — backlog

</deferred>

---

*Phase: 02-actor-prop-foundation*
*Context gathered: 2026-04-26*
