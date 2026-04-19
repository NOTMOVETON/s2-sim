# Задача 32 — Actor: базовый IActorBehavior + DoorBehavior + DoorOpenerPlugin

## Цель

Первые интерактивные акторы в симуляции. После задачи:
- Дверь — актор с FSM (closed → opening → open → closing → closed).
- Коллизия двери выключается при открытии, включается обратно при закрытии.
- Агент с `DoorOpenerPlugin` открывает ближайшую дверь при отправке команды.
  Плагин работает **проксимити-based**: не хранит ID двери, находит ближайшую
  по типу `"door"` в радиусе `interaction_distance`.

## Зависимости

- Задача 23 (Zone infrastructure, World Query API)
- `actor.hpp` — расширить Actor: `type`, `collision_enabled`, `IActorBehavior`
- `world.hpp` — `query_nearest_actor_by_type(position, type, max_distance)`
- `sim_bus.hpp` — `ActorCommandEvent`

---

## Что сделать

### 1. IActorBehavior — интерфейс поведения актора

**Файл:** `workspace/s2_core/include/s2/interfaces/actor_behavior.hpp` (новый)

```cpp
#pragma once
#include <s2/types.hpp>
#include <yaml-cpp/yaml.h>
#include <string>

namespace s2 {

class Actor;
class SimWorld;
class SimBus;

/// Базовый интерфейс для поведения актора (FSM, патруль, лифт и т.д.).
class IActorBehavior {
public:
    virtual ~IActorBehavior() = default;

    /// Вызывается один раз при инициализации актора.
    virtual void on_init(const YAML::Node& params) = 0;

    /// Вызывается каждый тик симуляции.
    virtual void tick(Actor& actor, SimWorld& world, SimBus& bus, double dt) = 0;

    /// Вызывается при получении именованной команды (из ActorCommandEvent).
    virtual void on_command(Actor& actor, const std::string& command,
                            const std::string& params_json) {}

    /// Текущее состояние в виде строки (для отладки и визуализатора).
    virtual std::string current_state() const { return ""; }
};

} // namespace s2
```

### 2. Расширение Actor

**Файл:** `workspace/s2_core/include/s2/actor.hpp`

```cpp
#pragma once
#include <s2/types.hpp>
#include <s2/interfaces/actor_behavior.hpp>
#include <memory>
#include <string>

namespace s2 {

struct Actor {
    ActorId id{0};
    std::string name;
    std::string type;           ///< Тип для поиска: "door", "elevator", "pedestrian"

    Pose3D world_pose;

    bool has_collision{false};
    bool collision_enabled{true};   ///< Управляется FSM-поведением
    CollisionShape collision;

    VisualDesc visual;

    std::unique_ptr<IActorBehavior> behavior;
};

} // namespace s2
```

### 3. ActorCommandEvent в SimBus

**Файл:** `workspace/s2_core/include/s2/sim_bus.hpp`

```cpp
/// Команда к актору (от плагина агента, от Kernel Command или от FSM).
struct ActorCommandEvent {
    ActorId actor_id;
    std::string command;        ///< "OPEN", "CLOSE", "ARRIVE", и т.д.
    std::string params_json;    ///< Опциональные параметры в JSON
};
```

### 4. World Query API

**Файл:** `workspace/s2_core/include/s2/world.hpp`

```cpp
/// Найти ближайшего актора заданного типа в радиусе max_distance.
/// Возвращает nullptr если нет подходящих.
const Actor* query_nearest_actor_by_type(
    const Vec3& position,
    const std::string& type,
    double max_distance) const
{
    const Actor* nearest = nullptr;
    double min_dist = max_distance;

    for (const auto& actor : actors_) {
        if (actor.type != type) continue;
        double dist = (actor.world_pose.position() - position).norm();
        if (dist < min_dist) {
            min_dist = nearest_dist;
            nearest  = &actor;
        }
    }
    return nearest;
}
```

### 5. DoorBehavior FSM

**Файл:** `workspace/s2_plugins/behaviors/door_behavior.hpp` (новый)

```cpp
#pragma once
#include <s2/interfaces/actor_behavior.hpp>
#include <s2/actor.hpp>
#include <s2/sim_bus.hpp>

namespace s2::behaviors {

/// FSM для двери.
///
/// States: closed, opening, open, closing
/// Transitions:
///   closed  → opening : command("OPEN")         → ничего
///   opening → open    : таймер(open_duration)   → actor.collision_enabled = false
///   open    → closing : таймер(auto_close_secs) → ничего  (или command("CLOSE"))
///   closing → closed  : таймер(close_duration)  → actor.collision_enabled = true
class DoorBehavior : public IActorBehavior {
public:
    void on_init(const YAML::Node& params) override {
        open_duration_   = params["open_duration"].as<double>(1.0);
        close_duration_  = params["close_duration"].as<double>(1.0);
        auto_close_secs_ = params["auto_close_secs"].as<double>(-1.0); // -1 = нет автозакрытия
    }

    void tick(Actor& actor, SimWorld& /*world*/, SimBus& bus, double dt) override {
        timer_ += dt;

        switch (state_) {
            case State::OPENING:
                if (timer_ >= open_duration_) {
                    state_ = State::OPEN;
                    timer_ = 0.0;
                    actor.collision_enabled = false;
                    bus.publish(ActorStateChangedEvent{actor.id, "open"});
                }
                break;

            case State::OPEN:
                if (auto_close_secs_ > 0 && timer_ >= auto_close_secs_) {
                    state_ = State::CLOSING;
                    timer_ = 0.0;
                    bus.publish(ActorStateChangedEvent{actor.id, "closing"});
                }
                break;

            case State::CLOSING:
                if (timer_ >= close_duration_) {
                    state_ = State::CLOSED;
                    timer_ = 0.0;
                    actor.collision_enabled = true;
                    bus.publish(ActorStateChangedEvent{actor.id, "closed"});
                }
                break;

            case State::CLOSED:
                // Ждём команду OPEN
                break;
        }
    }

    void on_command(Actor& actor, const std::string& cmd,
                    const std::string& /*params*/) override {
        if (cmd == "OPEN" && state_ == State::CLOSED) {
            state_ = State::OPENING;
            timer_ = 0.0;
        } else if (cmd == "CLOSE" && state_ == State::OPEN) {
            state_ = State::CLOSING;
            timer_ = 0.0;
        }
    }

    std::string current_state() const override {
        switch (state_) {
            case State::CLOSED:  return "closed";
            case State::OPENING: return "opening";
            case State::OPEN:    return "open";
            case State::CLOSING: return "closing";
        }
        return "unknown";
    }

private:
    enum class State { CLOSED, OPENING, OPEN, CLOSING };

    State  state_{State::CLOSED};
    double timer_{0.0};
    double open_duration_{1.0};
    double close_duration_{1.0};
    double auto_close_secs_{-1.0};
};

} // namespace s2::behaviors
```

Добавить в `sim_bus.hpp`:

```cpp
/// Актор сменил FSM-состояние.
struct ActorStateChangedEvent {
    ActorId actor_id;
    std::string new_state;
};
```

### 6. SimEngine: обработка ActorCommandEvent → on_command

**Файл:** `workspace/s2_core/include/s2/sim_engine.hpp`

В фазе 1 (обработка команд SimBus):

```cpp
bus_.subscribe<ActorCommandEvent>([this](const ActorCommandEvent& ev) {
    auto* actor = world_.find_actor(ev.actor_id);
    if (!actor || !actor->behavior) return;
    actor->behavior->on_command(*actor, ev.command, ev.params_json);
});
```

В фазе 2 (тик акторов):

```cpp
// === Фаза 1: тик акторов ===
for (auto& actor : world_.actors()) {
    if (actor.behavior) {
        actor.behavior->tick(actor, world_, bus_, dt_);
    }
}
```

В `SimWorld` добавить:

```cpp
Actor* find_actor(ActorId id) {
    auto it = std::find_if(actors_.begin(), actors_.end(),
        [id](const Actor& a){ return a.id == id; });
    return it != actors_.end() ? &(*it) : nullptr;
}
```

### 7. CollisionSystem: проверка collision_enabled для акторов

**Файл:** `workspace/s2_core/include/s2/collision_system.hpp`

В методе разрешения коллизий агентов с акторами:

```cpp
// Пропустить акторы с выключенной коллизией
if (!actor.has_collision || !actor.collision_enabled) continue;
```

### 8. DoorOpenerPlugin

**Файл:** `workspace/s2_plugins/include/s2/plugins/door_opener_plugin.hpp` (новый)

```cpp
#pragma once
#include <s2/interfaces/agent_plugin.hpp>
#include <s2/sim_bus.hpp>
#include <s2/world.hpp>

namespace s2::plugins {

/// Позволяет агенту открывать ближайшую дверь в радиусе interaction_distance.
///
/// Работает проксимити-based: не хранит ID двери.
/// Каждый тик определяет ближайший актор типа "door" (или actor_type из конфига).
///
/// ROS2-команды (через SharedState input):
///   - "OPEN_DOOR"  → открыть ближайшую дверь
///   - "CLOSE_DOOR" → закрыть ближайшую дверь
///
/// Публикует в ROS2-топик (через snapshot):
///   - nearest_interactive: {actor_id, actor_type, distance} или null
class DoorOpenerPlugin : public IAgentPlugin {
public:
    void on_init(const YAML::Node& params) override {
        interaction_distance_ = params["interaction_distance"].as<double>(1.5);
        actor_type_           = params["actor_type"].as<std::string>("door");
    }

    void update(double dt, Agent& agent) override {
        // Найти ближайший актор нужного типа
        const Actor* nearest = agent.world_ref->query_nearest_actor_by_type(
            agent.world_pose.position(), actor_type_, interaction_distance_);

        // Записать в SharedState для snapshot
        if (nearest) {
            agent.state.emplace<NearestInteractiveData>(NearestInteractiveData{
                nearest->id, nearest->type,
                (nearest->world_pose.position() - agent.world_pose.position()).norm()
            });
        } else {
            agent.state.remove<NearestInteractiveData>();
        }

        // Обработать входящую команду
        const auto* cmd = agent.state.get<DoorOpenerCommand>();
        if (!cmd) return;
        agent.state.remove<DoorOpenerCommand>();

        if (!nearest) return;

        if (cmd->action == "open") {
            agent.bus_ref->publish(ActorCommandEvent{nearest->id, "OPEN", ""});
        } else if (cmd->action == "close") {
            agent.bus_ref->publish(ActorCommandEvent{nearest->id, "CLOSE", ""});
        }
    }

private:
    double interaction_distance_{1.5};
    std::string actor_type_{"door"};
};

} // namespace s2::plugins
```

Добавить в `shared_state.hpp` или `components/`:

```cpp
/// Ближайший интерактивный объект (для публикации в snapshot).
struct NearestInteractiveData {
    ActorId actor_id{0};
    std::string actor_type;
    double distance{0.0};
};

/// Команда открыть/закрыть дверь (записывается из ROS2 или другого источника).
struct DoorOpenerCommand {
    std::string action;  ///< "open" | "close"
};
```

### 9. ActorSnapshot — состояние актора в снимке мира

**Файл:** `workspace/s2_core/include/s2/world_snapshot.hpp`

```cpp
struct ActorSnapshot {
    ActorId id{0};
    std::string name;
    std::string type;
    std::string behavior_state;     ///< Текущее FSM-состояние (например "open")

    double x{0}, y{0}, z{0};
    double yaw{0};

    bool collision_enabled{true};
};
```

В `SimEngine::build_snapshot()`:

```cpp
for (const auto& actor : world_.actors()) {
    ActorSnapshot as;
    as.id               = actor.id;
    as.name             = actor.name;
    as.type             = actor.type;
    as.behavior_state   = actor.behavior ? actor.behavior->current_state() : "";
    as.x                = actor.world_pose.x;
    as.y                = actor.world_pose.y;
    as.z                = actor.world_pose.z;
    as.yaw              = actor.world_pose.yaw;
    as.collision_enabled = actor.collision_enabled;
    snap.actors.push_back(std::move(as));
}
```

### 10. SceneLoader: парсинг актора с DoorBehavior

**Файл:** `workspace/s2_core/include/s2/scene_loader.hpp`

```cpp
// Пример записи сцены для двери
// actors:
//   - id: 1
//     name: door_main
//     type: door
//     pose: {x: 5.0, y: 0.0, z: 0.0, yaw: 0.0}
//     has_collision: true
//     collision:
//       type: aabb
//       half_size: {x: 0.05, y: 0.5, z: 1.0}
//     behavior: door
//     behavior_params:
//       open_duration: 1.2
//       close_duration: 1.0
//       auto_close_secs: 5.0
//     visual:
//       type: box
//       size: {x: 0.1, y: 1.0, z: 2.0}
//       color: "#AA8855"

static std::unique_ptr<IActorBehavior> create_behavior(
    const std::string& type, const YAML::Node& params)
{
    if (type == "door") {
        auto b = std::make_unique<behaviors::DoorBehavior>();
        b->on_init(params);
        return b;
    }
    return nullptr;
}
```

### 11. Пример YAML

```yaml
agents:
  - name: robot_0
    plugins:
      - type: door_opener
        params:
          interaction_distance: 1.5
          actor_type: door

actors:
  - id: 1
    name: door_main
    type: door
    pose: {x: 5.0, y: 0.0, z: 0.0, yaw: 0.0}
    has_collision: true
    collision:
      type: aabb
      half_size: {x: 0.05, y: 0.5, z: 1.0}
    behavior: door
    behavior_params:
      open_duration: 1.0
      close_duration: 1.0
      auto_close_secs: 4.0
    visual:
      type: box
      size: {x: 0.1, y: 1.0, z: 2.0}
      color: "#AA8855"
```

---

## Тесты

**Файл:** `workspace/s2_core/tests/test_actor_door.cpp`

- `DoorFSM_StartsInClosed` — начальное состояние = "closed"
- `DoorFSM_OpenCommand_Transitions` — команда "OPEN" → через open_duration: state="open"
- `DoorFSM_OpenDisablesCollision` — после открытия collision_enabled = false
- `DoorFSM_AutoClose_ClosesAfterTimeout` — open → через auto_close_secs: state="closing"
- `DoorFSM_CloseEnablesCollision` — после closing → closed: collision_enabled = true
- `DoorFSM_ManualClose_OverridesAutoClose` — команда "CLOSE" пока открыта: начинает закрываться
- `DoorOpenerPlugin_FindsNearestDoor` — агент в радиусе: ActorCommandEvent с nearest door_id
- `DoorOpenerPlugin_NoNearbyDoor_NoEvent` — нет двери в радиусе: событие не публикуется
- `DoorOpenerPlugin_PicksNearest_NotFarthest` — две двери, агент рядом с первой:
  команда идёт к первой
- `DoorOpenerPlugin_OnlyDoorType` — рядом "elevator" и "door": плагин с actor_type="door"
  выбирает дверь

---

## Критерии завершения

- [ ] IActorBehavior интерфейс определён и используется в Actor
- [ ] DoorBehavior FSM корректно проходит все 4 состояния
- [ ] collision_enabled = false при open, true при closed
- [ ] DoorOpenerPlugin работает проксимити-based (без hardcoded ID)
- [ ] ActorCommandEvent публикуется в SimBus и вызывает on_command у актора
- [ ] ActorSnapshot содержит behavior_state
- [ ] Все тесты проходят в Docker
