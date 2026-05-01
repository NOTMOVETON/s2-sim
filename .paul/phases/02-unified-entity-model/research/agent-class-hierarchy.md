# Research: Agent/Actor/Prop class hierarchy

## Файлы

- `workspace/s2_core/include/s2/agent.hpp`
- `workspace/s2_core/include/s2/actor.hpp`
- `workspace/s2_core/include/s2/prop.hpp`
- `workspace/s2_core/include/s2/types.hpp`
- `workspace/s2_core/include/s2/shared_state.hpp`
- `workspace/s2_core/include/s2/plugin_base.hpp`
- `workspace/s2_core/include/s2/kinematic_tree.hpp`

## Agent (agent.hpp, строки 28-54)

12 полей:
- `AgentId id{0}`
- `std::string name`
- `int domain_id{0}`
- `Pose3D world_pose`
- `Velocity world_velocity`
- `SharedState state` — contributions + resolver
- `std::unordered_set<std::string> capabilities`
- `std::vector<std::unique_ptr<IAgentPlugin>> plugins`
- `bool has_collision{false}`
- `double max_slope_rad{0.0}`, `double max_step_height{0.0}`
- `CollisionShape bounding`
- `VisualDesc visual`
- `std::unique_ptr<KinematicTree> kinematic_tree` (nullable)

## Actor (actor.hpp, строки 24-35)

5 полей:
- `ActorId id{0}`
- `std::string name`
- `Pose3D world_pose`
- `ActorState current_state` (= `std::string`)
- `CollisionShape collision`
- `VisualDesc visual`

Нет SharedState, нет плагинов, нет capabilities.

## Prop (prop.hpp, строки 25-37)

7 полей:
- `ObjectId id{0}`
- `std::string type`
- `Pose3D world_pose`
- `bool movable{false}`
- `CollisionShape collision`
- `VisualDesc visual`
- `std::unordered_map<std::string, std::string> properties`

Нет имени, нет SharedState.

## Таблица полей по типам

| Поле | Agent | Actor | Prop |
|------|-------|-------|------|
| id (разные типы) | AgentId | ActorId | ObjectId |
| name | ✓ | ✓ | ✗ |
| world_pose | ✓ | ✓ | ✓ |
| world_velocity | ✓ | ✗ | ✗ |
| SharedState | ✓ | ✗ | ✗ |
| capabilities | ✓ | ✗ | ✗ |
| plugins | ✓ | ✗ | ✗ |
| kinematic_tree | ✓ nullable | ✗ | ✗ |
| collision/bounding | ✓ | ✓ | ✓ |
| visual | ✓ | ✓ | ✓ |
| current_state (FSM) | ✗ | ✓ | ✗ |
| type (строка) | ✗ | ✗ | ✓ |
| movable | ✗ | ✗ | ✓ |
| properties | ✗ | ✗ | ✓ |
| domain_id | ✓ | ✗ | ✗ |

## Базовый класс

Нет. Все три — plain structs без наследования и виртуальных методов.

## ID типы (types.hpp, строки 29-49)

```cpp
using AgentId  = uint32_t;
using ActorId  = uint32_t;
using ObjectId = uint32_t;
using EntityId = uint32_t;   // Уже есть
using ZoneId   = std::string;
```

Все числовые ID — одинаковый underlying type, разные aliases.

## IAgentPlugin сигнатуры (plugin_base.hpp)

Все lifecycle-методы принимают `Agent&`:
- `initialize(Agent& agent)`
- `on_spawn(Agent& agent)`, `on_despawn(Agent& agent)`
- `pre_resolve(double dt, Agent& agent)`
- `update(double dt, Agent& agent)`

В Phase 2 нужно изменить на `Entity&`.
Комментарии об этом уже есть в plugin_base.hpp (строки 156, 162).

## SharedState (shared_state.hpp)

Два слоя:
1. **Single-owner** — type-indexed через `std::any` (Battery, Grabber данные)
2. **Contributions** — накопление от нескольких источников:
   - `add_scale(double, string)` → speed_scale = product
   - `add_lock(bool, string)` → motion_locked = OR
   - `add_velocity_addition(Vec3, string)` → velocity_addition = sum

За тик: contributions накапливаются → `resolve()` → `effective()` → `clear_contributions()`.

## Баг

`world_snapshot.hpp`, строка 54:
```cpp
struct PropSnapshot {
    ActorId id;  // ❌ должно быть ObjectId
```

## Вывод для Phase 2

Общие поля для Entity: `id`, `world_pose`, `collision`, `visual`
Опциональные слои:
- `PluginHost` — только AGENT (plugins vector)
- `SharedState` — только AGENT
- `BehaviorSlot` — только ACTOR (current_state + будущий FSM)
- `LinkTree` — только AGENT (kinematic_tree)
- `TransportLink` — только AGENT (domain_id → transport_config)
