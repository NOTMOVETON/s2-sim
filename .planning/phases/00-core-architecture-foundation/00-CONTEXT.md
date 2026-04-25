# Phase 0: Core Architecture Foundation - Context

**Gathered:** 2026-04-25
**Status:** Ready for planning

<domain>
## Phase Boundary

Правильный lifecycle плагинов, typed EventBus, WorldQuery API, Signal struct на Entity,
8-фазный tick lifecycle, полный набор KernelCommands — фундамент, на котором строится
всё остальное. Это рефакторинг и расширение существующего кода, а не новые фичи.

Требования: ARCH-01, ARCH-02, ARCH-03, ARCH-04, ARCH-05, ARCH-06, ARCH-07.

</domain>

<decisions>
## Implementation Decisions

### WorldQuery → плагины (ARCH-05)

- **D-01:** Сигнатура update() меняется: `update(double dt, Agent& agent, const PluginContext& ctx)`, где `struct PluginContext { const WorldQuery& world; EventBus& bus; KernelCommandQueue& commands; }`.
- **D-02:** `pre_resolve(double dt, Agent& agent)` — сигнатура НЕ меняется. Resource-плагины (Battery, Payload) пишут в SharedState, WorldQuery им не нужен.
- **D-03:** Все существующие плагины (DiffDrive, GNSS, IMU, Lidar, Battery, Gravity, Color, JointVel) обновляются за один проход: добавляют `const PluginContext&` как последний параметр, реализация игнорирует его (default-impl в base принимает и не использует).

### KernelCommand хранение (ARCH-07)

- **D-04:** `using KernelCommand = std::variant<cmd::SpawnEntity, cmd::DespawnEntity, cmd::SetPose, cmd::SetEnabled, cmd::AddPlugin, cmd::RemovePlugin, cmd::ConfigPlugin, cmd::SpawnZone, cmd::DespawnZone, cmd::ToggleZone, cmd::Interact, cmd::AttachObject, cmd::DetachObject, cmd::LoadScene, cmd::SaveScene, cmd::NewScene>`. Все команды в одном variant-типе.
- **D-05:** `SimEngine` хранит `std::vector<KernelCommand> command_queue_` с mutex-защитой (HTTP thread и sim thread). Phase 0 тика дренирует очередь.
- **D-06:** Единая точка входа: и плагины (через `PluginContext::commands`), и REST API / VizServer пушат в `command_queue_`. Никаких прямых вызовов `world_.entities.push_back()` из внешнего кода.

### EventBus (ARCH-04)

- **D-07:** Переименовать `SimBus` → `EventBus`: файл `sim_bus.hpp` → `event_bus.hpp`, класс `SimBus` → `EventBus`. Все существующие вхождения обновить за один проход.
- **D-08:** Все event:: типы (и существующие, и новые из ARCH-04) живут в одном файле `event_bus.hpp`. Новые типы: `EntitySpawned`, `EntityDespawned`, `ActorStateChanged` (уже есть), `SignalActivated`, `SignalDeactivated`, `ZoneEntered`/`ZoneExited` (переименовать из `AgentEnteredZone`/`AgentExitedZone`), `GrabAttempt`, `GrabSucceeded`, `GrabFailed`, `DamageDealt`.

### Plugin role system (ARCH-02)

- **D-09:** `enum class PluginRole { ACTUATION, SENSOR, INTERACTION, RESOURCE, UTILITY }`. Метод `virtual PluginRole role() const = 0` в `IAgentPlugin`.
- **D-10:** ACTUATION = плагин, пишущий в `agent.velocity` (базовое движение тела). JointVelPlugin = UTILITY (пишет в KinematicTree суставы, не в velocity тела).
- **D-11:** Валидация при `SceneLoader::load()` и при `AddPlugin` KernelCommand: если у агента уже есть ACTUATION-плагин и добавляется ещё один — `throw std::runtime_error`. Текущие сцены не нарушают правило.

### Plugin lifecycle (ARCH-01)

- **D-12:** Добавить в `IAgentPlugin`: `on_spawn(Agent&)`, `on_despawn(Agent&)`, `on_scene_load(const SimWorld&)`, `on_reset(Agent&)`. Все методы с пустой default-реализацией.
- **D-13:** Исправить известные баги on_reset: DiffDrive сбрасывает `external_linear_velocity_`; Battery сбрасывает заряд до начального значения.
- **D-14:** `provided_capabilities()` → `std::vector<std::string>` — список capabilities, которые плагин автоматически добавляет сущности. `config_schema()` → `nlohmann::json` — JSON Schema для UI-редактора.

### Signal struct (ARCH-03)

- **D-15:** Новый `struct Signal { std::string signal_type; std::string signal_id; Pose3D local_pose; nlohmann::json params; double range; bool requires_los; bool enabled; }` в `types.hpp`.
- **D-16:** `std::vector<Signal> signals` добавляется в `struct Agent` для Phase 0. Расширение на Actor/Prop — в Phase 2 (on_spawn акторов) и Phase 6 (Entity base).
- **D-17:** wire = Signal с `range = std::numeric_limits<double>::infinity()` и `requires_los = false`. YAML парсинг `signals:` секции в SceneLoader.

### Tick lifecycle 8 фаз (ARCH-06)

- **D-18:** Рефакторинг `SimEngine::tick()` в явные методы: `phase0_kernel_commands()`, `phase1_transport_input()`, `phase2_actors()`, `phase3_agents()`, `phase4_sensors()`, `phase5_interactions()`, `phase6_attachments()`, `phase7_snapshot_publish()`, `phase8_cleanup()`.
- **D-19:** Сенсоры (Lidar) вызываются только в Phase 4, строго после кинематики и коллизий в Phase 3. Существующий код случайно работает правильно (порядок в массиве), теперь это гарантируется архитектурой.
- **D-20:** `state.clear_contributions()` — только в Phase 8, не раньше.

### Claude's Discretion

- Конкретное представление KernelCommandQueue (можно как `std::vector<KernelCommand>` с std::mutex, или как thread-safe очередь — на усмотрение планировщика)
- Формат `config_schema()` — JSON Schema draft-7 или упрощённый формат
- Порядок migrate-коммитов (rename SimBus → EventBus + update all usages)

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### Архитектурные решения (основной документ)
- `RESULT_DISCUSS.md` §15 — Коммуникационные каналы: EventBus, WorldQuery, KernelCommand
- `RESULT_DISCUSS.md` §16 — Жизненный цикл тика (8 фаз, порядок фаз, почему сенсоры в фазе 4)
- `RESULT_DISCUSS.md` §6 — IAgentPlugin: lifecycle методы, role(), provided_capabilities()
- `RESULT_DISCUSS.md` §10 — Сигналы и детекция (Signal struct, wire signals)

### Требования
- `.planning/REQUIREMENTS.md` §ARCH-01–ARCH-07 — Полные спецификации требований Phase 0

### Существующий код (точки изменений)
- `workspace/s2_core/include/s2/plugin_base.hpp` — IAgentPlugin: добавить lifecycle + PluginContext
- `workspace/s2_core/include/s2/sim_bus.hpp` → `event_bus.hpp` — переименование + новые типы
- `workspace/s2_core/include/s2/sim_engine.hpp` — рефакторинг tick(), command_queue_, mutex
- `workspace/s2_core/include/s2/agent.hpp` — добавить `signals` поле
- `workspace/s2_core/include/s2/types.hpp` — добавить Signal struct
- `workspace/s2_plugins/src/plugins_registry.cpp` — обновить сигнатуры при создании плагинов

</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets
- `SimBus` (sim_bus.hpp): уже является typed event bus с template subscribe/publish — переименование, не переписывание
- `SimEngine::tick()`: существующий tick уже делает правильные вещи, нужно только формализовать порядок в именованные фазы
- `EffectContext` (effect_context.hpp): паттерн context-struct уже существует — аналог для PluginContext
- `RaycastEngine` (raycast_engine.hpp): основа для WorldQuery::raycast и has_line_of_sight
- `ZoneSystem` (zone_system.hpp): основа для WorldQuery::zones_at, is_in_zone

### Established Patterns
- Contribution-based state resolution через SharedState — НЕ меняется в Phase 0
- YAML from_config() паттерн во всех плагинах — добавить config_schema() рядом
- Типобезопасный dispatch через std::type_index уже в SimBus — сохраняется
- `std::cout` с prefix [Module] — стиль логирования для новых предупреждений

### Integration Points
- `workspace/s2_visualizer/src/main.cpp`: добавить push KernelCommand в command_queue вместо прямых вызовов
- `workspace/s2_plugins/src/plugins_registry.cpp`: обновить сигнатуры create_plugin при factory-вызовах
- `workspace/s2_core/tests/`: тесты SimBus → обновить на EventBus; добавить тесты WorldQuery, KernelCommand
- `workspace/s2_transport/src/sim_transport_bridge.cpp`: handle_input остаётся, но теперь через KernelCommand очередь

</code_context>

<specifics>
## Specific Ideas

- RESULT_DISCUSS.md — главный референс архитектуры. Downstream agents должны читать его ПЕРВЫМ.
- WorldQuery интерфейс (с точной сигнатурой методов) задан в RESULT_DISCUSS.md §15.3 — копировать оттуда.
- KernelCommand полный список команд задан в RESULT_DISCUSS.md §15.4 — копировать оттуда.

</specifics>

<deferred>
## Deferred Ideas

- Расширение `signals` на Actor/Prop — Phase 2 (Actor Foundation)
- Entity base model с опциональными слоями — Phase 6 (Entity Model Unification)
- `provided_capabilities()` auto-add в initialize_entity() — Phase 6 (ENTY-08)
- RViz2Adapter для WorldQuery — v2+

</deferred>

---

*Phase: 00-core-architecture-foundation*
*Context gathered: 2026-04-25*
