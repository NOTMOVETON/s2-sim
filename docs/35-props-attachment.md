# Задача 35 — Props + Attachment + GrabberPlugin

## Цель

Пропы — пассивные физические объекты (ящики, бочки, паллеты) со статической
коллизией. Агент с `GrabberPlugin` может захватить проп и тащить его за собой.

После задачи:
- Ящик стоит в сцене, блокирует движение роботов.
- Робот подъезжает и захватывает ящик: ящик следует за захватом.
- Робот отпускает ящик: он остаётся на месте.

## Зависимости

- Задача 32 (Actor struct, World Query API)
- `world.hpp` — Props коллекция
- `sim_bus.hpp` — AttachObjectCommand, DetachObjectCommand

---

## Что сделать

### 1. Prop struct

**Файл:** `workspace/s2_core/include/s2/prop.hpp` (новый)

```cpp
#pragma once
#include <s2/types.hpp>
#include <optional>
#include <string>

namespace s2 {

using PropId = uint32_t;

/// Пассивный объект в симуляции.
/// Статичен по умолчанию. Если movable=true, можно захватить (GrabberPlugin).
struct Prop {
    PropId id{0};
    std::string name;

    Pose3D world_pose;

    bool has_collision{true};
    CollisionShape collision;

    VisualDesc visual;

    bool movable{false};    ///< Можно ли захватить пропом

    /// Если захвачен: какой агент держит и какой link
    std::optional<AgentId> attached_to_agent;
    std::string            attach_link;         ///< Имя link'а захвата ("gripper_link")
    Vec3                   attach_offset{Vec3::Zero()};  ///< Локальный оффсет в link
};

} // namespace s2
```

### 2. SimWorld: Props коллекция

**Файл:** `workspace/s2_core/include/s2/world.hpp`

```cpp
class SimWorld {
public:
    // ... существующие коллекции (agents, actors, zones) ...

    std::vector<Prop>& props() { return props_; }
    const std::vector<Prop>& props() const { return props_; }

    Prop* find_prop(PropId id) {
        auto it = std::find_if(props_.begin(), props_.end(),
            [id](const Prop& p){ return p.id == id; });
        return it != props_.end() ? &(*it) : nullptr;
    }

    /// Найти ближайший проп с movable=true в радиусе max_distance.
    const Prop* query_nearest_movable_prop(
        const Vec3& position, double max_distance) const
    {
        const Prop* nearest = nullptr;
        double min_dist = max_distance;
        for (const auto& prop : props_) {
            if (!prop.movable) continue;
            double dist = (prop.world_pose.position() - position).norm();
            if (dist < min_dist) {
                min_dist = dist;
                nearest  = &prop;
            }
        }
        return nearest;
    }

private:
    // ...
    std::vector<Prop> props_;
};
```

### 3. Attachment Commands в SimBus

**Файл:** `workspace/s2_core/include/s2/sim_bus.hpp`

```cpp
/// Захватить проп: прикрепить к agentId, link attach_link.
struct AttachObjectCommand {
    AgentId agent_id;
    PropId  prop_id;
    std::string attach_link;  ///< Имя кинематического звена ("gripper_link")
};

/// Отпустить проп.
struct DetachObjectCommand {
    AgentId agent_id;
    PropId  prop_id;
};

/// Событие: проп захвачен/отпущен (для отладки и визуализации).
struct PropAttachmentChangedEvent {
    PropId  prop_id;
    bool    attached;
    AgentId agent_id{0};
};
```

### 4. SimEngine: Attachment System

**Файл:** `workspace/s2_core/include/s2/sim_engine.hpp`

Обработка команд в фазе 1:

```cpp
bus_.subscribe<AttachObjectCommand>([this](const AttachObjectCommand& cmd) {
    auto* prop  = world_.find_prop(cmd.prop_id);
    auto* agent = world_.find_agent(cmd.agent_id);
    if (!prop || !agent || !prop->movable) return;

    prop->attached_to_agent = cmd.agent_id;
    prop->attach_link       = cmd.attach_link;
    // Вычислить offset: позиция пропа относительно захватного звена
    Vec3 link_world_pos = agent->get_link_world_pos(cmd.attach_link);
    prop->attach_offset = prop->world_pose.position() - link_world_pos;

    bus_.publish(PropAttachmentChangedEvent{cmd.prop_id, true, cmd.agent_id});
});

bus_.subscribe<DetachObjectCommand>([this](const DetachObjectCommand& cmd) {
    auto* prop = world_.find_prop(cmd.prop_id);
    if (!prop) return;
    prop->attached_to_agent = std::nullopt;
    prop->attach_link.clear();
    bus_.publish(PropAttachmentChangedEvent{cmd.prop_id, false, 0});
});
```

Attachment tick — обновление позиций прикреплённых пропов (фаза 3, после кинематики):

```cpp
// === Обновить позиции прикреплённых пропов ===
for (auto& prop : world_.props()) {
    if (!prop.attached_to_agent) continue;
    auto* agent = world_.find_agent(*prop.attached_to_agent);
    if (!agent) {
        // Агент исчез — отсоединить
        prop.attached_to_agent = std::nullopt;
        continue;
    }
    Vec3 link_pos = agent->get_link_world_pos(prop.attach_link);
    prop.world_pose.x = link_pos.x() + prop.attach_offset.x();
    prop.world_pose.y = link_pos.y() + prop.attach_offset.y();
    prop.world_pose.z = link_pos.z() + prop.attach_offset.z();
    prop.world_pose.yaw = agent->world_pose.yaw;
}
```

### 5. Agent: get_link_world_pos

**Файл:** `workspace/s2_core/include/s2/agent.hpp`

Для агентов с кинематическим деревом:

```cpp
/// Получить мировую позицию кинематического звена по имени.
/// Если звено не найдено или нет дерева — вернуть позицию агента.
Vec3 get_link_world_pos(const std::string& link_name) const {
    if (!kinematic_tree) return world_pose.position();
    auto* link = kinematic_tree->find_link(link_name);
    if (!link) return world_pose.position();
    return link->world_pose.position();
}
```

Если кинематического дерева нет (простой агент с DiffDrivePlugin):
`get_link_world_pos("gripper_link")` возвращает `world_pose.position()`.

### 6. CollisionSystem: пропы блокируют движение

**Файл:** `workspace/s2_core/include/s2/collision_system.hpp`

Пропы с `has_collision = true` и без `attached_to_agent` участвуют в коллизиях
как статические объекты.

```cpp
// В resolveAgentCollisions():
for (const auto& prop : world.props()) {
    if (!prop.has_collision) continue;
    if (prop.attached_to_agent) continue;  // захваченный проп — не статика
    // ... обработать как статический объект ...
}
```

### 7. GrabberPlugin

**Файл:** `workspace/s2_plugins/include/s2/plugins/grabber_plugin.hpp` (новый)

```cpp
#pragma once
#include <s2/interfaces/agent_plugin.hpp>
#include <s2/sim_bus.hpp>
#include <s2/world.hpp>

namespace s2::plugins {

/// Позволяет агенту захватывать ближайший movable проп.
///
/// Проксимити-based: не хранит ID цели, находит ближайший проп
/// с movable=true в радиусе interaction_distance.
///
/// Команды (через SharedState input):
///   GrabCommand{}    → захватить ближайший проп
///   ReleaseCommand{} → отпустить текущий проп
///
/// Snapshot: nearest_prop_id (-1 если нет), grabbed_prop_id (-1 если нет)
class GrabberPlugin : public IAgentPlugin {
public:
    void on_init(const YAML::Node& params) override {
        interaction_distance_ = params["interaction_distance"].as<double>(0.5);
        attach_link_          = params["attach_link"].as<std::string>("gripper_link");
    }

    void update(double dt, Agent& agent) override {
        // Найти ближайший захватываемый проп
        const Prop* nearest = agent.world_ref->query_nearest_movable_prop(
            agent.world_pose.position(), interaction_distance_);

        // Записать в состояние для snapshot
        agent.state.emplace<GrabberState>(GrabberState{
            nearest ? (int)nearest->id : -1,
            grabbed_prop_id_
        });

        // Обработать команды
        if (auto* grab = agent.state.get<GrabCommand>()) {
            agent.state.remove<GrabCommand>();
            if (nearest && grabbed_prop_id_ < 0) {
                grabbed_prop_id_ = (int)nearest->id;
                agent.bus_ref->publish(AttachObjectCommand{
                    agent.id, (PropId)nearest->id, attach_link_});
            }
        }

        if (auto* rel = agent.state.get<ReleaseCommand>()) {
            agent.state.remove<ReleaseCommand>();
            if (grabbed_prop_id_ >= 0) {
                agent.bus_ref->publish(DetachObjectCommand{
                    agent.id, (PropId)grabbed_prop_id_});
                grabbed_prop_id_ = -1;
            }
        }
    }

private:
    double      interaction_distance_{0.5};
    std::string attach_link_{"gripper_link"};
    int         grabbed_prop_id_{-1};
};

} // namespace s2::plugins
```

Добавить в компоненты:

```cpp
struct GrabberState {
    int nearest_prop_id{-1};
    int grabbed_prop_id{-1};
};

struct GrabCommand {};
struct ReleaseCommand {};
```

### 8. PropSnapshot в WorldSnapshot

**Файл:** `workspace/s2_core/include/s2/world_snapshot.hpp`

```cpp
struct PropSnapshot {
    PropId id{0};
    std::string name;
    double x{0}, y{0}, z{0};
    double yaw{0};
    bool has_collision{true};
    bool attached{false};
    AgentId attached_to{0};
};
```

В `SimEngine::build_snapshot()`:

```cpp
for (const auto& prop : world_.props()) {
    PropSnapshot ps;
    ps.id            = prop.id;
    ps.name          = prop.name;
    ps.x             = prop.world_pose.x;
    ps.y             = prop.world_pose.y;
    ps.z             = prop.world_pose.z;
    ps.yaw           = prop.world_pose.yaw;
    ps.has_collision = prop.has_collision;
    ps.attached      = prop.attached_to_agent.has_value();
    if (ps.attached) ps.attached_to = *prop.attached_to_agent;
    snap.props.push_back(std::move(ps));
}
```

### 9. Пример YAML

```yaml
props:
  - id: 1
    name: box_0
    pose: {x: 3.0, y: 0.0, z: 0.15}
    movable: true
    has_collision: true
    collision:
      type: aabb
      half_size: {x: 0.2, y: 0.2, z: 0.15}
    visual:
      type: box
      size: {x: 0.4, y: 0.4, z: 0.3}
      color: "#886644"

  - id: 2
    name: barrel_0
    pose: {x: -2.0, y: 1.0, z: 0.3}
    movable: false        # нельзя сдвинуть
    has_collision: true
    collision:
      type: sphere
      radius: 0.3
    visual:
      type: cylinder
      radius: 0.3
      height: 0.6
      color: "#443322"

agents:
  - name: robot_0
    plugins:
      - type: grabber
        params:
          interaction_distance: 0.6
          attach_link: gripper_link
```

---

## Тесты

**Файл:** `workspace/s2_core/tests/test_props_attachment.cpp`

- `Prop_BlocksAgentMovement` — агент движется к статичному ящику: коллизия разрешается
- `Prop_MovableFlag_CanAttach` — проп с movable=true: AttachObjectCommand принимается
- `Prop_NotMovable_IgnoredByGrabber` — проп с movable=false: GrabberPlugin не захватывает
- `Attachment_PropFollowsAgent` — после захвата проп следует за агентом (позиция обновляется)
- `Attachment_DetachLeavesPropatPos` — после отпускания проп остаётся на месте
- `GrabberPlugin_FindsNearest` — два пропа, захватывается ближайший
- `GrabberPlugin_NothingNearby_NoEvent` — нет пропов в радиусе — команда игнорируется
- `AttachedProp_NoCollision` — захваченный проп не блокирует движение (collision skip)
- `PropSnapshot_ContainsAllProps` — snapshot содержит все пропы
- `PropSnapshot_AttachedFlag` — snapshot захваченного пропа: attached=true, attached_to=agent_id

---

## Критерии завершения

- [ ] Prop struct создан, коллекция в SimWorld
- [ ] SceneLoader парсит секцию `props` из YAML
- [ ] CollisionSystem обрабатывает пропы как статику (когда не захвачены)
- [ ] AttachObjectCommand / DetachObjectCommand обрабатываются SimEngine
- [ ] Прикреплённый проп следует за link агента
- [ ] GrabberPlugin работает проксимити-based
- [ ] PropSnapshot передаётся клиенту
- [ ] Все тесты проходят в Docker
