# Phase 2 Research: Unified Entity Model

Дата: 2026-04-29
Агенты: 3 (Explore, параллельно)

## 1. Agent/Actor/Prop — что переезжает в Entity

**Детали:** `research/agent-class-hierarchy.md`

Нет базового класса — три plain struct без наследования.
EntityId (uint32_t) уже существует в types.hpp.

Общие поля для Entity core:
- `EntityId id`, `EntityType type` (AGENT/ACTOR/PROP)
- `Pose3D world_pose`, `CollisionShape collision`, `VisualDesc visual`

Опциональные слои по типам:

| Слой | AGENT | ACTOR | PROP |
|------|-------|-------|------|
| PluginHost (vector<IAgentPlugin>) | ✓ | ✗ | ✗ |
| SharedState (contributions/resolver) | ✓ | ✗ | ✗ |
| capabilities (unordered_set<string>) | ✓ | ✗ | ✗ |
| BehaviorSlot (current_state + FSM) | ✗ | ✓ | ✗ |
| LinkTree (KinematicTree) | ✓ nullable | ✗ | ✗ |
| TransportLink (domain_id → transport_config) | ✓ | ✗ | ✗ |
| world_velocity | ✓ | ✗ | ✗ |
| tags, immune_to_effects | ✓ | ✓ | ✓ |

IAgentPlugin lifecycle-методы принимают `Agent&` — нужно изменить на `Entity&`.
Комментарии об этом уже есть в plugin_base.hpp (строки 156, 162).

**Баг попутно:** `world_snapshot.hpp:54` — `PropSnapshot` использует `ActorId` вместо `ObjectId`.

## 2. SimEngine — текущий реестр и тик

**Детали:** `research/sim-engine-structure.md`

Три отдельных вектора в SimWorld — нет единого реестра.
Поиск O(n) для всех типов.

Порядок тика: Zones → Agents (pre_resolve → resolve → plugins → kinematics → collision) → Snapshot → Transport.
Порядок фаз переработает Phase 10 — Phase 2 не трогает.

Что меняет Phase 2:
- `SimWorld` → `unordered_map<EntityId, Entity>`
- `add_agent/prop/actor` → `add_entity(Entity)`
- `get_agent/prop/actor` → `get_entity(EntityId)` O(1)
- `initial_states_` привязан к AgentId → переработать под EntityId

## 3. ZoneSystem + SceneLoader — scope рефакторинга

**Детали:** `research/zone-system-scene-loader.md`

### ZoneSystem

`tick()` получает `vector<Agent>&` явно — нужно перейти на Entity-агностичный диапазон.
`EffectContext` содержит `AgentId agent_id` → `EntityId entity_id`.
Props без SharedState не должны попадать в zone matching (enforcement через тип).

### SceneLoader

Текущий YAML **не имеет** полей `transport`, `tags`, `immune_to_effects`.
`domain_id` сидит прямо на агенте — нужно переехать в `transport_config.ros2.domain_id`.

Новые поля YAML для Phase 2:
```yaml
transport: "ros2"
ros2:
  domain_id: 50
tags: {}
immune_to_effects: []
```

`SceneData` возвращает 3 отдельных вектора → единый `vector<Entity>`.

### VizServer

`build_snapshot()` итерирует по типам раздельно (`world_.agents()`, `world_.actors()`).
После Phase 2 — итерация по единому реестру с фильтром по `entity.type`.
Структуры WorldSnapshot (AgentSnapshot/ActorSnapshot/PropSnapshot) пока без изменений.

### Баг в attached_to

Зоны привязываются к акторам по `std::string` имени.
При миграции нужен name→EntityId lookup (или хранить EntityId в Zone.attached_to).

## Ключевые выводы

1. **Entity struct** — plain struct, не полиморфизм. Опциональные слои через `std::optional` или nullable pointer.
2. **EntityId уже существует** как `uint32_t` alias — использовать его.
3. **Минимум изменений в порядке тика** — Phase 2 меняет только хранилище и сигнатуры, не логику.
4. **4 места с типами агентов:** SimWorld, ZoneSystem.tick(), SceneData, build_snapshot() — все нужно обновить.
5. **YAML формат** расширяется, но остаётся совместимым (новые поля опциональны с defaults).
6. **Props без SharedState** — enforcement через assertions в Entity constructor/add_layer.

## Файлы для изменения в Phase 2

| Файл | Что меняется |
|------|-------------|
| `s2/agent.hpp` | → входит в Entity как набор полей |
| `s2/actor.hpp` | → входит в Entity |
| `s2/prop.hpp` | → входит в Entity |
| `s2/world.hpp` | 3 вектора → unordered_map<EntityId, Entity> |
| `s2/sim_engine.hpp` | initial_states_ тип, tick() обращения к world |
| `s2/zone_system.hpp/.cpp` | tick() сигнатура, EffectContext тип |
| `s2/interfaces/effect_plugin.hpp` | EffectContext.agent_id → entity_id |
| `s2/plugin_base.hpp` | Agent& → Entity& в lifecycle методах |
| `s2/scene_loader.hpp` | SceneData, парсинг нового YAML |
| `s2/world_snapshot.hpp` | build_snapshot() итерация + баг PropSnapshot |
