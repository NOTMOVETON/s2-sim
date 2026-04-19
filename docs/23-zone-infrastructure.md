# Задача 23 — Инфраструктура зон (Zone, ZoneSystem, EffectPlugin)

## Цель

Фундамент для всей системы зон и эффектов. После этой задачи:
зоны загружаются из YAML, агенты обнаруживаются при входе/выходе,
SimBus получает события `AgentEnteredZone` / `AgentExitedZone`,
зоны отображаются в визуализаторе как полупрозрачные области.
Конкретные эффекты — в следующих задачах.

## Зависимости

- Задача 22 (LidarPlugin) — финальная точка предыдущего блока
- `shared_state.hpp` — contribution система (уже реализована)
- `types.hpp` — ZoneShape, ZoneShapeType, EffectType (уже реализованы)
- `sim_bus.hpp` — шина событий (уже реализована)

---

## Что сделать

### 1. Расширить ZoneShape — добавить CYLINDER

**Файл:** `workspace/s2_core/include/s2/types.hpp`

Добавить в enum `ZoneShapeType`:

```cpp
enum class ZoneShapeType {
    SPHERE,
    AABB,
    CYLINDER,   // ← добавить
    INFINITE
};
```

Добавить поле `half_height` в `ZoneShape` и логику `contains()` для CYLINDER:

```cpp
struct ZoneShape {
    ZoneShapeType type{ZoneShapeType::SPHERE};
    Vec3 center{Vec3::Zero()};
    double radius{1.0};
    Vec3 half_size{1.0, 1.0, 1.0};
    double half_height{1.0};   // ← добавить для CYLINDER

    bool contains(const Vec3& point) const {
        switch (type) {
            case ZoneShapeType::SPHERE: {
                return (point - center).squaredNorm() <= radius * radius;
            }
            case ZoneShapeType::AABB: {
                Vec3 d = (point - center).cwiseAbs();
                return d.x() <= half_size.x() &&
                       d.y() <= half_size.y() &&
                       d.z() <= half_size.z();
            }
            case ZoneShapeType::CYLINDER: {
                // Проверка по Z
                double dz = std::abs(point.z() - center.z());
                if (dz > half_height) return false;
                // Проверка по радиусу (2D)
                double dx = point.x() - center.x();
                double dy = point.y() - center.y();
                return dx * dx + dy * dy <= radius * radius;
            }
            case ZoneShapeType::INFINITE:
                return true;
            default:
                return false;
        }
    }
};
```

### 2. Добавить capabilities в Agent

**Файл:** `workspace/s2_core/include/s2/agent.hpp`

```cpp
struct Agent {
    // ... существующие поля ...
    std::unordered_set<std::string> capabilities;  // ← добавить если ещё нет
};
```

Capabilities задаются в YAML и загружаются SceneLoader-ом.

### 3. EffectContext

**Файл:** `workspace/s2_core/include/s2/effect_context.hpp` (новый)

```cpp
#pragma once
#include <s2/types.hpp>
#include <string>

namespace s2 {

/// Контекст, передаваемый плагину эффекта при каждом вызове.
/// Содержит всё необходимое для рандомизированных и time-based эффектов.
struct EffectContext {
    double sim_time{0.0};        ///< Время с начала симуляции (сек)
    double dt{0.0};              ///< Шаг тика (сек)
    ZoneId zone_id;              ///< ID зоны
    Vec3 zone_center{Vec3::Zero()};  ///< Текущий центр зоны
    Vec3 zone_half_size{Vec3::Zero()}; ///< Текущие полуразмеры зоны
    AgentId agent_id{0};         ///< ID агента
    Vec3 agent_position{Vec3::Zero()}; ///< Текущая позиция агента
};

} // namespace s2
```

### 4. EffectPlugin интерфейс

**Файл:** `workspace/s2_core/include/s2/interfaces/effect_plugin.hpp` (новый)

```cpp
#pragma once
#include <s2/effect_context.hpp>
#include <s2/shared_state.hpp>
#include <s2/types.hpp>
#include <yaml-cpp/yaml.h>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <vector>

namespace s2 {

/// Интерфейс плагина эффекта.
/// Конкретные эффекты (IceModifier, ChargingEffect и т.д.) наследуются от него.
class EffectPlugin {
public:
    virtual ~EffectPlugin() = default;

    virtual void on_init(const YAML::Node& params) = 0;

    virtual EffectType effect_type() const = 0;

    /// Capabilities, которые должны быть у агента для применения эффекта.
    /// Пустой список = применяется ко всем.
    virtual std::vector<std::string> required_capabilities() const = 0;

    /// MODIFIER: публикует contribution в SharedState (add_scale / add_lock / add_velocity_addition).
    /// Вызывается каждый тик пока агент в зоне.
    virtual void apply_modifier(SharedState& state, const EffectContext& ctx) {}

    /// CONTINUOUS: напрямую изменяет single-owner поле в SharedState.
    /// Вызывается каждый тик пока агент в зоне.
    virtual void apply_continuous(SharedState& state, const EffectContext& ctx) {}

    /// MUTATION: однократное необратимое воздействие при входе в зону.
    /// Вызывается один раз. Состояние сохраняется при выходе.
    virtual void apply_mutation(SharedState& state, const EffectContext& ctx) {}

    /// SENSOR: описывает модификации параметров сенсоров.
    /// Применяется перед вызовом sensor->update() (задача 32).
    struct SensorMod {
        std::string param;          ///< Имя параметра: "max_range", "noise_std"
        double multiplier{1.0};     ///< Множитель (применяется к текущему значению)
        double addend{0.0};         ///< Слагаемое (добавляется после умножения)
    };
    virtual std::vector<SensorMod> sensor_mods(const EffectContext& ctx) const { return {}; }

    /// Опциональная подсказка для визуализатора: как анимировать зону.
    struct VisualHint {
        std::string type;            ///< "arrows", "particles", "glow", "grid"
        nlohmann::json params;       ///< {"direction":[1,0,0], "speed":2.0, "color":"#F60"}
    };
    virtual std::optional<VisualHint> visual_hint() const { return std::nullopt; }
};

} // namespace s2
```

### 5. Zone struct

**Файл:** `workspace/s2_core/include/s2/zone.hpp` (новый)

```cpp
#pragma once
#include <s2/interfaces/effect_plugin.hpp>
#include <s2/types.hpp>
#include <memory>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

namespace s2 {

/// Описание зоны и список эффектов, которые она применяет.
struct Zone {
    ZoneId id;
    bool enabled{true};

    ZoneShape shape;
    std::string detection_mode{"center"}; ///< "center" | "bounding"

    // Привязка к актору или агенту (зона следует за объектом)
    std::optional<ActorId> attached_to_actor;
    std::optional<AgentId> attached_to_agent;
    Vec3 attachment_offset{Vec3::Zero()};

    // Визуальные параметры
    std::string color{"#4488FF"};
    double opacity{0.3};
    bool visible{true};
    std::string label;

    /// Описание одного эффекта в зоне.
    struct EffectDesc {
        std::string type;                        ///< "ice_modifier", "charging", ...
        bool enabled{true};                      ///< можно выключить отдельный эффект
        EffectType effect_type;
        std::vector<std::string> required_capabilities;
        YAML::Node params;

        std::unique_ptr<EffectPlugin> plugin;    ///< создаётся фабрикой
    };
    std::vector<EffectDesc> effects;

    /// Агенты, находящиеся внутри в текущий момент.
    std::unordered_set<AgentId> inside_agents;
};

} // namespace s2
```

### 6. SimBus события

**Файл:** `workspace/s2_core/include/s2/sim_bus.hpp`

Добавить события (если ещё нет):

```cpp
struct AgentEnteredZoneEvent {
    AgentId agent_id;
    ZoneId zone_id;
};

struct AgentExitedZoneEvent {
    AgentId agent_id;
    ZoneId zone_id;
};
```

### 7. ZoneSystem

**Файл:** `workspace/s2_core/include/s2/zone_system.hpp` (новый)

```cpp
#pragma once
#include <s2/zone.hpp>
#include <s2/agent.hpp>
#include <s2/actor.hpp>
#include <s2/sim_bus.hpp>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace s2 {

using EffectFactory = std::function<
    std::unique_ptr<EffectPlugin>(const std::string& type, const YAML::Node& params)>;

class ZoneSystem {
public:
    void set_effect_factory(EffectFactory factory);

    void add_zone(Zone zone);

    /// Основной тик: обновить attached-позиции, проверить входы/выходы,
    /// применить MODIFIER и CONTINUOUS эффекты.
    void tick(
        std::vector<Agent>& agents,
        const std::vector<Actor>& actors,
        SimBus& bus,
        double sim_time,
        double dt);

    /// Kernel Command: изменить геометрию зоны.
    bool resize_zone(const ZoneId& id, const ZoneShape& new_shape);

    /// Kernel Command: переместить центр (только для не-attached зон).
    bool move_zone(const ZoneId& id, const Vec3& new_center);

    /// Kernel Command: привязать зону к актору.
    bool attach_zone_to_actor(const ZoneId& id, ActorId actor_id, const Vec3& offset);

    /// Kernel Command: включить/выключить зону.
    bool toggle_zone(const ZoneId& id, bool enabled);

    /// Kernel Command: включить/выключить конкретный эффект в зоне.
    bool toggle_effect(const ZoneId& id, size_t effect_idx, bool enabled);

    /// World Query API: зоны, содержащие данную точку.
    std::vector<ZoneId> zones_containing(const Vec3& point) const;

    /// Получить снапшоты зон для визуализатора.
    std::vector<Zone*> zones();
    const std::vector<Zone>& all_zones() const { return zones_; }

private:
    Vec3 detection_point(const Agent& agent, const std::string& mode) const;

    void on_agent_enter(Agent& agent, Zone& zone, SimBus& bus,
                        double sim_time, double dt);
    void on_agent_exit(Agent& agent, Zone& zone, SimBus& bus);

    void apply_active_effects(Agent& agent, Zone& zone,
                              double sim_time, double dt);

    std::vector<Zone> zones_;
    EffectFactory effect_factory_;
};

} // namespace s2
```

**Логика `tick()`:**

```
1. Обновить позиции attached зон:
   Для каждой зоны с attached_to_actor:
       актор = найти в actors
       zone.shape.center = actor.world_pose.position() + attachment_offset
   Для каждой зоны с attached_to_agent:
       агент = найти в agents
       zone.shape.center = agent.world_pose.position() + attachment_offset

2. Для каждой зоны (если !enabled — вызвать exit для всех inside):
   Для каждого агента:
       point = detection_point(agent, zone.detection_mode)
       is_inside = zone.shape.contains(point)
       was_inside = zone.inside_agents.count(agent.id) > 0

       Если is_inside && !was_inside:
           zone.inside_agents.insert(agent.id)
           on_agent_enter(agent, zone, bus, sim_time, dt)
       Если !is_inside && was_inside:
           zone.inside_agents.erase(agent.id)
           on_agent_exit(agent, zone, bus)

3. Применить активные эффекты для агентов внутри зон:
   Для каждой зоны, для каждого agent_id в inside_agents:
       apply_active_effects(agent, zone, sim_time, dt)
```

**`on_agent_enter`:**
```
- Публикуем AgentEnteredZoneEvent на SimBus
- Для каждого MUTATION-эффекта (enabled):
    - Проверяем capabilities matching
    - Вызываем plugin->apply_mutation(agent.state, ctx)
```

**`on_agent_exit`:**
```
- Публикуем AgentExitedZoneEvent на SimBus
- MODIFIER и CONTINUOUS прекращают применяться автоматически (агент не в inside_agents)
- MUTATION не отменяется
```

**`apply_active_effects`:**
```
- Для каждого EffectDesc (enabled):
    - Проверяем capabilities matching
    - Если MODIFIER: plugin->apply_modifier(agent.state, ctx)
    - Если CONTINUOUS: plugin->apply_continuous(agent.state, ctx)
    - MUTATION здесь не вызывается (уже выполнена при входе)
```

**`detection_point`:**
```
- "center": agent.world_pose.position()
- "bounding": TODO задача — bounding overlap
  В текущей реализации использовать center как fallback
```

### 8. Обновить SimWorld

**Файл:** `workspace/s2_core/include/s2/world.hpp`

```cpp
class SimWorld {
public:
    // ... существующие методы ...

    void add_zone(Zone zone);
    std::vector<Zone>& zones();
    const std::vector<Zone>& zones() const;
    Zone* get_zone(const ZoneId& id);

private:
    // ... существующие члены ...
    std::vector<Zone> zones_;
};
```

### 9. Обновить SimEngine

**Файл:** `workspace/s2_core/include/s2/sim_engine.hpp`

Добавить `ZoneSystem zone_system_` как член.

Заполнить фазу 2 в `tick()`:

```cpp
// === Фаза 2: Зоны ===
zone_system_.tick(world_.agents(), world_.actors(), bus_, sim_time_, dt_);
```

В `load_world()` — создать зоны из мира:

```cpp
for (auto& zone : world_.zones()) {
    zone_system_.add_zone(std::move(zone));
}
world_.zones().clear();
```

Добавить геттер:
```cpp
ZoneSystem& zone_system() { return zone_system_; }
```

### 10. Обновить SceneLoader

**Файл:** `workspace/s2_core/include/s2/scene_loader.hpp`

Добавить парсинг `zones:` в `SceneData` и `SceneLoader::load()`:

```cpp
struct SceneData {
    // ... существующие ...
    std::vector<Zone> zones;
};
```

```cpp
// Парсинг зон из YAML
if (const auto& zones_node = root["zones"]) {
    for (const auto& zn : zones_node) {
        Zone z;
        z.id = zn["id"].as<std::string>();
        z.enabled = zn["enabled"].as<bool>(true);
        z.color = zn["color"].as<std::string>("#4488FF");
        z.opacity = zn["opacity"].as<double>(0.3);
        z.visible = zn["visible"].as<bool>(true);
        z.label = zn["label"].as<std::string>("");
        z.detection_mode = zn["detection_mode"].as<std::string>("center");

        // shape
        if (const auto& sn = zn["shape"]) {
            z.shape = parse_zone_shape(sn);
        }

        // attached_to
        if (zn["attached_to"]) {
            z.attached_to_actor = /* поиск актора по имени */;
        }

        // effects — без plugin (плагины создаются ZoneSystem через фабрику)
        if (const auto& efn = zn["effects"]) {
            for (const auto& en : efn) {
                Zone::EffectDesc desc;
                desc.type = en["type"].as<std::string>();
                desc.enabled = en["enabled"].as<bool>(true);
                if (en["required_capabilities"]) {
                    desc.required_capabilities =
                        en["required_capabilities"].as<std::vector<std::string>>();
                }
                desc.params = en["params"] ? en["params"] : YAML::Node{};
                z.effects.push_back(std::move(desc));
            }
        }

        scene.zones.push_back(std::move(z));
    }
}
```

Добавить `parse_zone_shape()`:

```cpp
static ZoneShape parse_zone_shape(const YAML::Node& node) {
    ZoneShape s;
    std::string type = node["type"].as<std::string>("sphere");
    if (type == "sphere") {
        s.type = ZoneShapeType::SPHERE;
        s.radius = node["radius"].as<double>(1.0);
        if (node["center"]) {
            auto c = node["center"];
            s.center = Vec3{c["x"].as<double>(0), c["y"].as<double>(0), c["z"].as<double>(0)};
        }
    } else if (type == "aabb") {
        s.type = ZoneShapeType::AABB;
        if (node["center"]) {
            auto c = node["center"];
            s.center = Vec3{c["x"].as<double>(0), c["y"].as<double>(0), c["z"].as<double>(0)};
        }
        if (node["half_size"]) {
            auto hs = node["half_size"];
            s.half_size = Vec3{hs["x"].as<double>(1), hs["y"].as<double>(1), hs["z"].as<double>(1)};
        }
    } else if (type == "cylinder") {
        s.type = ZoneShapeType::CYLINDER;
        s.radius = node["radius"].as<double>(1.0);
        s.half_height = node["half_height"].as<double>(1.0);
        if (node["center"]) {
            auto c = node["center"];
            s.center = Vec3{c["x"].as<double>(0), c["y"].as<double>(0), c["z"].as<double>(0)};
        }
    } else if (type == "infinite") {
        s.type = ZoneShapeType::INFINITE;
    }
    return s;
}
```

Парсинг `capabilities` агента:

```cpp
if (agent_node["capabilities"]) {
    agent.capabilities = agent_node["capabilities"].as<std::unordered_set<std::string>>();
}
```

### 11. Обновить WorldSnapshot

**Файл:** `workspace/s2_core/include/s2/world_snapshot.hpp`

```cpp
struct ZoneSnapshot {
    ZoneId id;
    bool enabled{true};
    ZoneShapeType shape_type;
    Vec3 center{Vec3::Zero()};
    double radius{1.0};
    Vec3 half_size{Vec3::Zero()};
    double half_height{1.0};
    std::string color{"#4488FF"};
    double opacity{0.3};
    bool visible{true};
    std::string label;
    std::vector<AgentId> agents_inside;
};

struct WorldSnapshot {
    // ... существующие поля ...
    std::vector<ZoneSnapshot> zones;  // ← добавить
};
```

В `SimEngine::build_snapshot()` добавить заполнение зон.

### 12. Визуализатор — рендер зон

**Файл:** `workspace/s2_visualizer/web/js/app.js`

Получать `zones` из snapshot и рендерить полупрозрачные меши:

```js
const zoneObjects = {};  // id -> THREE.Mesh

function updateZones(zones) {
    const seen = new Set();

    for (const z of zones) {
        seen.add(z.id);
        if (!z.visible || !z.enabled) {
            if (zoneObjects[z.id]) { scene.remove(zoneObjects[z.id]); delete zoneObjects[z.id]; }
            continue;
        }

        // Разобрать цвет с opacity
        const color = new THREE.Color(z.color);

        let mesh = zoneObjects[z.id];
        if (!mesh) {
            let geo;
            if (z.shape_type === 'sphere') {
                geo = new THREE.SphereGeometry(z.radius, 32, 16);
            } else if (z.shape_type === 'aabb') {
                geo = new THREE.BoxGeometry(
                    z.half_size[0] * 2, z.half_size[2] * 2, z.half_size[1] * 2);
            } else if (z.shape_type === 'cylinder') {
                geo = new THREE.CylinderGeometry(
                    z.radius, z.radius, z.half_height * 2, 32);
            } else {
                continue;
            }
            const mat = new THREE.MeshBasicMaterial({
                color, transparent: true, opacity: z.opacity,
                side: THREE.DoubleSide, depthWrite: false
            });
            mesh = new THREE.Mesh(geo, mat);
            scene.add(mesh);
            zoneObjects[z.id] = mesh;
        } else {
            mesh.material.color.set(color);
            mesh.material.opacity = z.opacity;
        }

        // Позиция (конвертация в Three.js координаты: Y вверх, Z вперёд)
        mesh.position.set(z.center[0], z.center[2], -z.center[1]);
    }

    // Удалить исчезнувшие
    for (const id of Object.keys(zoneObjects)) {
        if (!seen.has(id)) { scene.remove(zoneObjects[id]); delete zoneObjects[id]; }
    }
}
```

### 13. Пример YAML сцены с зонами

**Файл:** `workspace/s2_config/scenes/test_zones.yaml` (новый)

```yaml
s2:
  update_rate: 100
  transport:
    type: stub

world:
  geometry:
    - type: box
      pose: {x: 0, y: 0, z: -0.025}
      size: [20, 20, 0.05]
      color: "#222222"

agents:
  - name: robot_0
    pose: {x: 0, y: 0, z: 0}
    capabilities: ["surface_contact"]
    collision:
      bounding: {type: sphere, radius: 0.4}
    visual:
      type: box
      size: [0.8, 0.5, 0.3]
      color: "#FF6B35"
    plugins:
      - type: diff_drive
        max_linear_vel: 2.0

zones:
  - id: "ice_patch"
    shape:
      type: aabb
      center: {x: 3.0, y: 0.0, z: 0.5}
      half_size: {x: 2.0, y: 2.0, z: 1.0}
    color: "#4488FF"
    opacity: 0.3
    label: "Лёд"

  - id: "charging_station"
    shape:
      type: cylinder
      center: {x: -4.0, y: 0.0, z: 0.5}
      radius: 1.0
      half_height: 1.0
    color: "#FFDD44"
    opacity: 0.4
    label: "Зарядка"
```

---

## Тесты

**Файл:** `workspace/s2_core/tests/test_zone_system.cpp`

- `ZoneSystem_AgentEnterSphere` — агент входит в сферическую зону → AgentEnteredZone event
- `ZoneSystem_AgentExitSphere` — агент выходит → AgentExitedZone event
- `ZoneSystem_NoEventIfOutside` — агент снаружи → никаких событий
- `ZoneSystem_AABB_MultiFloor` — две AABB зоны на разных Z → агент на z=0 только в нижней
- `ZoneSystem_CylinderContains` — агент в пределах cylinder → inside; над/под — нет
- `ZoneSystem_AttachedZone` — зона attached к агенту; агент движется → зона следует
- `ZoneSystem_DisabledZone` — зона с enabled=false → агент внутри не получает enter event
- `ZoneSystem_ZonesContaining` — метод zones_containing() возвращает корректный список
- `ZoneSystem_RuntimeResize` — resize_zone() → новые размеры применяются в следующем тике
- `ZoneSystem_CapabilitiesMatch` — агент без нужного capability → MODIFIER не применяется (тест заглушки-эффекта)
- `ZoneShape_CylinderContains` — unit тест ZoneShape::contains() для CYLINDER

---

## Критерии завершения

- [ ] ZoneShapeType::CYLINDER добавлен, ZoneShape::contains() корректен для всех типов
- [ ] Zone struct, EffectContext, EffectPlugin interface созданы
- [ ] ZoneSystem.tick() обнаруживает enter/exit и публикует SimBus-события
- [ ] Attached zones следуют за актором/агентом
- [ ] Zone загружается из YAML через SceneLoader
- [ ] WorldSnapshot содержит ZoneSnapshot для всех зон
- [ ] Визуализатор отображает зоны как полупрозрачные меши
- [ ] Все тесты проходят в Docker: `docker compose --project-directory docker up --build tests`
