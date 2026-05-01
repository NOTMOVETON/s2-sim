# Research: ZoneSystem + SceneLoader current API

## Файлы

- `workspace/s2_core/include/s2/zone_system.hpp`
- `workspace/s2_core/src/zone_system.cpp` (300 строк)
- `workspace/s2_core/include/s2/interfaces/effect_plugin.hpp`
- `workspace/s2_core/include/s2/scene_loader.hpp` (554 строки, всё inline)
- `workspace/s2_core/include/s2/effect_context.hpp`
- `workspace/s2_config/scenes/test_zones.yaml`
- `workspace/s2_config/scenes/test_dozer.yaml`
- `workspace/s2_visualizer/src/viz_server.hpp`

## ZoneSystem.tick() сигнатура

```cpp
void tick(
    std::vector<Agent>& agents,           // Мутабельный — применяет эффекты к state
    const std::vector<Actor>& actors,     // Константный — только для detect
    SimBus& bus,
    double sim_time,
    double dt);
```

Не обращается к SimEngine напрямую — данные передаются явно.
Итерирует линейно, проверяет `agent.world_pose.position()` (только center mode).

Эффекты:
- MUTATION → `on_agent_enter()` — один раз при входе
- MODIFIER, CONTINUOUS → `apply_active_effects()` — каждый тик

## EffectPlugin интерфейс (effect_plugin.hpp)

```cpp
class EffectPlugin {
public:
    virtual void on_init(const YAML::Node& params) = 0;
    virtual EffectType effect_type() const = 0;
    virtual std::vector<std::string> required_capabilities() const { return {}; }

    // Все принимают SharedState& и EffectContext:
    virtual void apply_modifier(SharedState& state, const EffectContext& ctx) {}
    virtual void apply_continuous(SharedState& state, const EffectContext& ctx) {}
    virtual void apply_mutation(SharedState& state, const EffectContext& ctx) {}
    virtual void on_agent_exit(SharedState& state, const EffectContext& ctx) {}
};
```

```cpp
enum class EffectType { MODIFIER, CONTINUOUS, MUTATION, SENSOR };
```

## EffectContext (effect_context.hpp)

```cpp
struct EffectContext {
    double sim_time;
    double dt;
    ZoneId zone_id;
    Vec3 zone_center;
    Vec3 zone_half_size;
    AgentId agent_id;      // ← нужно EntityId в Phase 2
    Vec3 agent_position;
};
```

**Нет поля zone.strength** — добавляется в Phase 6.

## SceneLoader (scene_loader.hpp, inline)

```cpp
static SceneData load(const std::string& yaml_path,
                      PluginFactory plugin_factory = PluginFactory{});
```

SceneData содержит:
```cpp
std::vector<Agent> agents;
std::vector<Prop> props;
std::vector<Actor> actors;
std::vector<Zone> zones;
SimEngine::Config engine_config;
TransportConfig transport_config;
VizConfig viz_config;
// ...
```

### Парсинг агента (строки 121-286)

Поля из YAML:
- `id`, `name`, `domain_id` — прямо на agent
- `pose` → Pose3D
- `capabilities` → `agent.capabilities` (SET<string>)
- `collision.bounding` → `agent.bounding`
- `visual` → VisualDesc
- `plugins` → vector с type + params
- `urdf` → KinematicTree из файла
- `links` → inline KinematicTree

**НЕТ полей:** `transport`, `tags`, `immune_to_effects`
→ Все три нужны в Phase 2 (новый YAML формат).

### Парсинг актора (строки 319-340)

- `id`, `name`, `pose`, `visual`
- Нет capabilities, нет domain_id

### Парсинг зон (строки 342-399)

- `shape` → ZoneShape (aabb/sphere/cylinder)
- `effects` → array с `type`, `effect_type`, `required_capabilities`, `params`
- `attached_to` → имя актора (привязка по имени, не по ID!)
- `detection_mode` → "center" (bounding — TODO)

## Текущий YAML формат агента

```yaml
agents:
  - name: "robot_0"
    pose: {x: 0.0, y: 0.0, z: 0.0, yaw: 0.0}
    domain_id: 50
    capabilities: ["surface_contact", "has_battery", "wheeled"]
    collision:
      bounding:
        type: "sphere"
        radius: 0.4
    visual:
      type: "box"
      size: [0.8, 0.5, 0.3]
      color: "#FF6B35"
    plugins:
      - type: "diff_drive"
        max_linear_vel: 2.0
      - type: "battery"
        initial_level: 0.5
    urdf: "../robots/dozer.urdf"
```

## Новый YAML формат (Phase 2)

Нужно добавить:
```yaml
agents:
  - name: "robot_0"
    transport: "ros2"          # ← новое
    ros2:
      domain_id: 50            # ← переезжает из agent.domain_id
    tags:
      behavior: "default"      # ← новое
    immune_to_effects: []      # ← новое
    # ... остальное без изменений
```

## VizServer + build_snapshot()

```cpp
// Итерация по типам раздельно:
for (const auto& agent : world_.agents()) { ... }
for (const auto& actor : world_.actors()) { ... }
for (const auto& zone : zone_system_.all_zones()) { ... }
```

После Phase 2 нужно итерировать по единому реестру с фильтрацией по type.

## Вывод для Phase 2

### Что меняется в ZoneSystem
- `tick()` сигнатура: `vector<Agent>&` → принимать Entity-агностичный диапазон
- EffectContext: `AgentId agent_id` → `EntityId entity_id`
- Matching capabilities: `agent.capabilities` → `entity.capabilities` (у Prop — пустое множество)

### Что меняется в SceneLoader
- `SceneData` — вместо `vector<Agent>/vector<Prop>/vector<Actor>` → `vector<Entity>`
- Парсинг нового YAML: добавить `transport`, `tags`, `immune_to_effects`
- `domain_id` → `transport_config.ros2.domain_id`
- `attached_to` в зонах — нужен EntityId lookup по имени

### Что меняется в VizServer
- `build_snapshot()` — итерировать по единому реестру с `entity.type` тегом
- `WorldSnapshot` структуры пока можно оставить (AgentSnapshot/ActorSnapshot/PropSnapshot)

### Баг в attached_to
Зоны привязываются по `std::string` имени актора — при переходе на Entity нужно
убедиться, что lookup по имени → EntityId работает (или хранить EntityId напрямую).
