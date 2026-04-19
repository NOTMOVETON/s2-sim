# Задача 36 — Actor: ElevatorActor + ElevatorUserPlugin

## Цель

Лифт перевозит агентов между этажами. Агент с `ElevatorUserPlugin`
вызывает лифт, заходит и выезжает на нужном этаже.

Механизм посадки строится на Attachment из задачи 35:
лифт прикрепляет агента к себе при посадке → движется → отсоединяет при выходе.

После задачи:
- Лифт стоит у этажа, двери открыты.
- Агент подъезжает в зону посадки, отправляет команду "BOARD".
- Лифт прикрепляет агента, движется к целевому этажу.
- Двери открываются, агент выезжает.

## Зависимости

- Задача 32 (IActorBehavior, ActorCommandEvent, World Query API)
- Задача 35 (Attachment System: AttachObjectCommand → Agent attachment)
- Задача 23 (ZoneSystem — attached-зоны у платформы лифта)

---

## Что сделать

### 1. Расширение Attachment: агент как пассажир

Задача 35 реализовала Attachment для пропов.
Для агентов нужна аналогичная система:

**Файл:** `workspace/s2_core/include/s2/sim_bus.hpp`

```cpp
/// Прикрепить агента к актору (посадка в лифт, конвейер с посадкой).
struct AttachAgentCommand {
    AgentId  agent_id;
    ActorId  actor_id;
    Vec3     local_offset{Vec3::Zero()};  ///< Смещение в локальных координатах актора
};

/// Отсоединить агента от актора.
struct DetachAgentCommand {
    AgentId  agent_id;
    ActorId  actor_id;
};
```

**Файл:** `workspace/s2_core/include/s2/agent.hpp`

```cpp
struct Agent {
    // ... существующие поля ...

    /// Если агент прикреплён к актору — его позиция управляется Attachment System
    std::optional<ActorId> attached_to_actor;
    Vec3                   actor_attach_offset{Vec3::Zero()};
};
```

**Файл:** `workspace/s2_core/include/s2/sim_engine.hpp`

Обработка AttachAgentCommand / DetachAgentCommand в фазе 1:

```cpp
bus_.subscribe<AttachAgentCommand>([this](const AttachAgentCommand& cmd) {
    auto* agent = world_.find_agent(cmd.agent_id);
    auto* actor = world_.find_actor(cmd.actor_id);
    if (!agent || !actor) return;

    agent->attached_to_actor  = cmd.actor_id;
    agent->actor_attach_offset = cmd.local_offset;
});

bus_.subscribe<DetachAgentCommand>([this](const DetachAgentCommand& cmd) {
    auto* agent = world_.find_agent(cmd.agent_id);
    if (!agent) return;
    agent->attached_to_actor = std::nullopt;
});
```

Attachment tick (после кинематики агентов и кинематики акторов):

```cpp
// === Обновить позиции агентов-пассажиров ===
for (auto& agent : world_.agents()) {
    if (!agent.attached_to_actor) continue;
    auto* actor = world_.find_actor(*agent.attached_to_actor);
    if (!actor) {
        agent.attached_to_actor = std::nullopt;
        continue;
    }
    // Позиция агента = позиция актора + offset
    Eigen::Matrix3d R = rotation_from_yaw(actor->world_pose.yaw);
    Vec3 world_offset = R * agent.actor_attach_offset;
    agent.world_pose.x   = actor->world_pose.x + world_offset.x();
    agent.world_pose.y   = actor->world_pose.y + world_offset.y();
    agent.world_pose.z   = actor->world_pose.z + world_offset.z();
    agent.world_pose.yaw = actor->world_pose.yaw;
    // Скорость агента определяется лифтом: обнулить собственную
    agent.world_velocity.linear  = Vec3::Zero();
    agent.world_velocity.angular = Vec3::Zero();
}
```

### 2. ElevatorBehavior FSM

**Файл:** `workspace/s2_plugins/behaviors/elevator_behavior.hpp` (новый)

```cpp
#pragma once
#include <s2/interfaces/actor_behavior.hpp>
#include <s2/actor.hpp>
#include <s2/sim_bus.hpp>
#include <vector>
#include <string>

namespace s2::behaviors {

/// FSM лифта.
///
/// States: idle_at_floor, doors_opening, doors_open, boarding,
///         doors_closing, moving, arrived
///
/// Команды:
///   "CALL {floor_id}"  → если idle: начать движение к этажу
///   "BOARD {agent_id}" → посадить агента (открыты двери)
///   "REQUEST {floor_id}" → агент внутри запросил этаж
///
/// Движение: лифт плавно движется по Z к target_z со speed.
class ElevatorBehavior : public IActorBehavior {
public:
    struct Floor {
        std::string id;
        double z;
    };

    void on_init(const YAML::Node& params) override {
        speed_         = params["speed"].as<double>(0.5);
        door_open_secs_ = params["door_open_secs"].as<double>(3.0);

        if (params["floors"]) {
            for (const auto& f : params["floors"]) {
                Floor fl;
                fl.id = f["id"].as<std::string>("");
                fl.z  = f["z"].as<double>(0.0);
                floors_.push_back(fl);
            }
        }

        if (!floors_.empty()) {
            current_floor_idx_ = 0;
            current_z_         = floors_[0].z;
        }
    }

    void tick(Actor& actor, SimWorld& world, SimBus& bus, double dt) override {
        switch (state_) {
            case State::IDLE:
                break;

            case State::DOORS_OPENING:
                timer_ += dt;
                if (timer_ >= 0.5) {   // Анимация открытия: 0.5 сек
                    state_ = State::DOORS_OPEN;
                    timer_ = 0.0;
                    actor.collision_enabled = false;
                    bus.publish(ActorStateChangedEvent{actor.id, "doors_open"});
                    // Уведомить ElevatorUserPlugin о том, что можно сесть
                    bus.publish(ElevatorDoorsOpenEvent{actor.id, current_floor_id()});
                }
                break;

            case State::DOORS_OPEN:
                timer_ += dt;
                if (timer_ >= door_open_secs_) {
                    // Автозакрытие
                    state_ = State::DOORS_CLOSING;
                    timer_ = 0.0;
                }
                break;

            case State::DOORS_CLOSING:
                timer_ += dt;
                if (timer_ >= 0.5) {
                    actor.collision_enabled = true;
                    state_ = State::MOVING;
                    timer_ = 0.0;
                    bus.publish(ActorStateChangedEvent{actor.id, "moving"});
                }
                break;

            case State::MOVING:
                tick_moving(actor, bus, dt);
                break;

            case State::ARRIVED:
                // Немного подождать, затем открыть двери
                timer_ += dt;
                if (timer_ >= 0.3) {
                    state_ = State::DOORS_OPENING;
                    timer_ = 0.0;
                    bus.publish(ActorStateChangedEvent{actor.id, "doors_opening"});
                }
                break;
        }

        actor.world_pose.z = current_z_;
    }

    void on_command(Actor& actor, const std::string& cmd,
                    const std::string& params_json) override {
        if (cmd == "CALL") {
            // Вызов лифта к этажу
            if (state_ != State::IDLE && state_ != State::DOORS_OPEN) return;
            auto j = nlohmann::json::parse(params_json, nullptr, false);
            if (!j.is_discarded()) {
                std::string floor_id = j.value("floor_id", "");
                set_target_floor(floor_id);
                if (state_ == State::IDLE) {
                    state_ = State::MOVING;
                    timer_ = 0.0;
                }
            }
        } else if (cmd == "BOARD") {
            if (state_ != State::DOORS_OPEN) return;
            auto j = nlohmann::json::parse(params_json, nullptr, false);
            if (!j.is_discarded()) {
                AgentId agent_id = j.value("agent_id", 0u);
                if (agent_id > 0) {
                    passengers_.push_back(agent_id);
                    bus_cache_->publish(AttachAgentCommand{
                        agent_id, actor.id,
                        Vec3{0.0, 0.0, 0.5}  // агент стоит сверху платформы
                    });
                }
            }
        } else if (cmd == "REQUEST_FLOOR") {
            auto j = nlohmann::json::parse(params_json, nullptr, false);
            if (!j.is_discarded()) {
                std::string floor_id = j.value("floor_id", "");
                set_target_floor(floor_id);
                if (state_ == State::DOORS_OPEN) {
                    state_ = State::DOORS_CLOSING;
                    timer_ = 0.0;
                }
            }
        }
    }

    std::string current_state() const override {
        switch (state_) {
            case State::IDLE:           return "idle";
            case State::DOORS_OPENING:  return "doors_opening";
            case State::DOORS_OPEN:     return "doors_open";
            case State::DOORS_CLOSING:  return "doors_closing";
            case State::MOVING:         return "moving";
            case State::ARRIVED:        return "arrived";
        }
        return "unknown";
    }

    void set_bus(SimBus* bus) { bus_cache_ = bus; }

private:
    enum class State { IDLE, DOORS_OPENING, DOORS_OPEN, DOORS_CLOSING, MOVING, ARRIVED };

    void tick_moving(Actor& actor, SimBus& bus, double dt) {
        if (target_floor_idx_ < 0) {
            state_ = State::IDLE;
            return;
        }
        double target_z = floors_[target_floor_idx_].z;
        double dist = target_z - current_z_;
        double step = speed_ * dt;

        if (std::abs(dist) <= step) {
            current_z_         = target_z;
            current_floor_idx_ = target_floor_idx_;
            target_floor_idx_  = -1;
            state_ = State::ARRIVED;
            timer_ = 0.0;
            bus.publish(ActorStateChangedEvent{actor.id, "arrived"});

            // Высадить пассажиров
            for (AgentId ag_id : passengers_) {
                bus_cache_->publish(DetachAgentCommand{ag_id, actor.id});
            }
            passengers_.clear();
        } else {
            current_z_ += (dist > 0 ? 1.0 : -1.0) * step;
        }
    }

    void set_target_floor(const std::string& floor_id) {
        for (int i = 0; i < (int)floors_.size(); ++i) {
            if (floors_[i].id == floor_id) {
                target_floor_idx_ = i;
                return;
            }
        }
    }

    std::string current_floor_id() const {
        if (current_floor_idx_ >= 0 && current_floor_idx_ < (int)floors_.size())
            return floors_[current_floor_idx_].id;
        return "";
    }

    State  state_{State::IDLE};
    double timer_{0.0};
    double speed_{0.5};
    double door_open_secs_{3.0};
    double current_z_{0.0};
    int    current_floor_idx_{0};
    int    target_floor_idx_{-1};

    std::vector<Floor>   floors_;
    std::vector<AgentId> passengers_;
    SimBus*              bus_cache_{nullptr};
};

} // namespace s2::behaviors
```

Добавить в `sim_bus.hpp`:

```cpp
/// Двери лифта открылись на этаже — агенты могут садиться.
struct ElevatorDoorsOpenEvent {
    ActorId actor_id;
    std::string floor_id;
};
```

### 3. ElevatorUserPlugin

**Файл:** `workspace/s2_plugins/include/s2/plugins/elevator_user_plugin.hpp` (новый)

```cpp
#pragma once
#include <s2/interfaces/agent_plugin.hpp>
#include <s2/sim_bus.hpp>
#include <s2/world.hpp>

namespace s2::plugins {

/// Позволяет агенту пользоваться лифтом.
///
/// Работает проксимити-based: ищет ближайший актор типа "elevator".
///
/// Команды (через SharedState input):
///   CallElevatorCommand{floor_id}    → вызвать лифт к floor_id
///   BoardElevatorCommand{}           → сесть в лифт (если двери открыты рядом)
///   RequestFloorCommand{floor_id}    → запросить этаж (пока в лифте)
class ElevatorUserPlugin : public IAgentPlugin {
public:
    void on_init(const YAML::Node& params) override {
        interaction_distance_ = params["interaction_distance"].as<double>(2.0);
    }

    void update(double dt, Agent& agent) override {
        // Если агент в лифте — ждать команду RequestFloor
        if (agent.attached_to_actor) {
            handle_in_elevator(agent);
            return;
        }

        // Найти ближайший лифт
        const Actor* elevator = agent.world_ref->query_nearest_actor_by_type(
            agent.world_pose.position(), "elevator", interaction_distance_);

        // Обработка команды вызова
        if (auto* call = agent.state.get<CallElevatorCommand>()) {
            agent.state.remove<CallElevatorCommand>();
            if (elevator) {
                agent.bus_ref->publish(ActorCommandEvent{
                    elevator->id, "CALL",
                    nlohmann::json{{"floor_id", call->floor_id}}.dump()
                });
            }
        }

        // Посадка
        if (auto* board = agent.state.get<BoardElevatorCommand>()) {
            agent.state.remove<BoardElevatorCommand>();
            if (elevator) {
                agent.bus_ref->publish(ActorCommandEvent{
                    elevator->id, "BOARD",
                    nlohmann::json{{"agent_id", agent.id}}.dump()
                });
            }
        }
    }

private:
    void handle_in_elevator(Agent& agent) {
        if (auto* req = agent.state.get<RequestFloorCommand>()) {
            agent.state.remove<RequestFloorCommand>();
            if (!agent.attached_to_actor) return;
            agent.bus_ref->publish(ActorCommandEvent{
                *agent.attached_to_actor, "REQUEST_FLOOR",
                nlohmann::json{{"floor_id", req->floor_id}}.dump()
            });
        }
    }

    double interaction_distance_{2.0};
};

} // namespace s2::plugins
```

Добавить компоненты:

```cpp
struct CallElevatorCommand  { std::string floor_id; };
struct BoardElevatorCommand {};
struct RequestFloorCommand  { std::string floor_id; };
```

### 4. Регистрация поведения

**Файл:** `workspace/s2_plugins/src/behaviors_registry.cpp`

```cpp
else if (type == "elevator") b = std::make_unique<behaviors::ElevatorBehavior>();
```

### 5. Зона посадки у лифта

Для удобного взаимодействия у лифта есть attached-зона посадки на каждом этаже.
Агент, попавший в зону, получает уведомление через ZoneSystem.

В сцене — только одна основная attached-зона (платформа лифта):

```yaml
attached_zone:
  id: "elevator_0_platform"
  shape:
    type: aabb
    center: {x: 0.0, y: 0.0, z: 0.3}
    half_size: {x: 0.8, y: 0.8, z: 0.5}
  color: "#AADDFF"
  opacity: 0.15
  visible: true
  label: "Лифт"
  effects: []
```

### 6. Пример YAML

```yaml
actors:
  - id: 30
    name: elevator_0
    type: elevator
    pose: {x: 10.0, y: 0.0, z: 0.0, yaw: 0.0}
    has_collision: true
    collision:
      type: aabb
      half_size: {x: 0.9, y: 0.9, z: 1.5}
    behavior: elevator
    behavior_params:
      speed: 0.8
      door_open_secs: 4.0
      floors:
        - id: "floor_1"
          z: 0.0
        - id: "floor_2"
          z: 3.5
        - id: "floor_3"
          z: 7.0
    visual:
      type: box
      size: {x: 1.8, y: 1.8, z: 2.4}
      color: "#889999"
    attached_zone:
      id: "elevator_0_platform"
      shape:
        type: aabb
        center: {x: 0.0, y: 0.0, z: 0.2}
        half_size: {x: 0.85, y: 0.85, z: 0.4}
      color: "#AADDFF"
      opacity: 0.15
      visible: true
      label: "Лифт"
      effects: []

agents:
  - name: robot_0
    plugins:
      - type: elevator_user
        params:
          interaction_distance: 1.5
```

---

## Тесты

**Файл:** `workspace/s2_core/tests/test_actor_elevator.cpp`

- `ElevatorFSM_StartsIdle` — начальное состояние = "idle"
- `ElevatorFSM_CallCommand_StartMoving` — команда "CALL floor_2": state="moving"
- `ElevatorFSM_ArrivesAtFloor` — лифт двигается до target_z: z ≈ floor_2.z
- `ElevatorFSM_DoorsOpenOnArrival` — после прибытия: state="doors_open"
- `ElevatorFSM_BoardCommand_AttachesAgent` — команда "BOARD agent_id": AttachAgentCommand публикуется
- `ElevatorFSM_RequestFloor_StartsClosing` — команда "REQUEST_FLOOR floor_1": doors_closing
- `ElevatorFSM_DisembarkOnArrival` — прибытие с пассажирами: DetachAgentCommand для всех
- `AttachAgent_PositionFollowsElevator` — после AttachAgentCommand: agent.world_pose.z = elevator.z
- `DetachAgent_AgentStopsFollowing` — после DetachAgentCommand: агент управляет собой

---

## Критерии завершения

- [ ] ElevatorBehavior FSM: все переходы корректны
- [ ] Agent Attachment System: агент следует за лифтом по Z
- [ ] AttachAgentCommand / DetachAgentCommand обрабатываются SimEngine
- [ ] ElevatorUserPlugin работает проксимити-based (без hardcoded ID)
- [ ] Посадка/высадка через ActorCommandEvent → on_command
- [ ] ActorSnapshot содержит behavior_state лифта
- [ ] Все тесты проходят в Docker
