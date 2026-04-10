# Task 07 — Акторы: FSM + дверь + пешеход

> **Предыдущий шаг:** `06-sensors-resources.md` (сенсоры и ресурсы)
> **Следующий шаг:** `08-interactions.md`
> **Контекст:** `ARCHITECTURE.md` раздел 10

## Цель

Акторы с FSM. После этого шага: дверь открывается по команде (через Event Bus), пешеход ходит по waypoints, актор меняет коллизионный шейп при смене состояния.

## Что сделать

### 1. ActorBehavior интерфейс

```cpp
// interfaces/actor_behavior.hpp
class ActorBehavior {
public:
    virtual ~ActorBehavior() = default;
    virtual void on_init(const YAML::Node& config, SimBus& bus) = 0;
    virtual void on_tick(Actor& actor, double dt) = 0;
    virtual ActorState initial_state() const = 0;
};
```

### 2. FSM базовый класс

```cpp
// s2_core/include/s2/fsm.hpp
class FSM {
public:
    using Action = std::function<void()>;
    using Guard = std::function<bool()>;

    void add_state(const ActorState& state);
    void add_transition(const ActorState& from, const ActorState& to,
                         const std::string& trigger, Guard guard = nullptr);

    void add_on_enter(const ActorState& state, Action action);
    void add_on_exit(const ActorState& state, Action action);
    void add_while_in(const ActorState& state, std::function<void(double dt)> action);

    void set_state(const ActorState& state);
    void send_trigger(const std::string& trigger);
    void tick(double dt);  // таймеры + while_in

    // Таймер: автоматический переход через N секунд
    void add_timed_transition(const ActorState& from, const ActorState& to,
                               double seconds);

    const ActorState& current_state() const;

private:
    struct Transition {
        ActorState to;
        std::string trigger;
        Guard guard;
    };
    struct TimedTransition {
        ActorState to;
        double delay;
    };

    std::unordered_map<ActorState, std::vector<Transition>> transitions_;
    std::unordered_map<ActorState, std::optional<TimedTransition>> timed_;
    std::unordered_map<ActorState, Action> on_enter_;
    std::unordered_map<ActorState, Action> on_exit_;
    std::unordered_map<ActorState, std::function<void(double)>> while_in_;

    ActorState current_;
    double timer_{0.0};
};
```

### 3. DoorBehavior

```
s2_plugins/actors/door_behavior.hpp / .cpp
```

```
States: closed, opening, open, closing
Transitions:
  closed → opening:  trigger "open_command"
  opening → open:    timed 2.0 sec
  open → closing:    timed 10.0 sec (auto-close) OR trigger "close_command"
  closing → closed:  timed 2.0 sec
Actions:
  on_enter(open):    actor.collision = {} (убрать шейп)
  on_enter(closed):  actor.collision = door_shape (вернуть)
  on transition:     bus.publish(ActorStateChanged{...})
```

Параметры из YAML: open_time, close_time, auto_close_delay.

DoorBehavior подписывается на Event Bus для получения DoorCommand.

### 4. PedestrianBehavior

```
s2_plugins/actors/pedestrian_behavior.hpp / .cpp
```

```
States: walking, waiting
Transitions:
  walking → waiting:   достиг waypoint (distance < threshold)
  waiting → walking:   timed pause_duration
While in:
  walking: двигаться к текущему waypoint со скоростью speed
  waiting: стоять
On enter:
  waiting: выбрать следующий waypoint (loop или reverse)
```

Параметры: waypoints (vector<Vec3>), speed, pause_duration, loop.

Пешеход двигается **сквозь стены** (по прямой между waypoints). Навигация для акторов — v2.

**[СПРОСИТЬ]** Нужно ли обходить стены для пешеходов в v1? Если да — это значительно усложняет задачу.

### 5. Обновить Actor struct

```cpp
struct Actor {
    ActorId id;
    std::string name;
    Pose3D world_pose;
    CollisionShape collision;
    CollisionShape collision_default;  // для восстановления
    VisualDesc visual;
    std::unique_ptr<ActorBehavior> behavior;
    ActorState current_state;
};
```

### 6. Обновить tick() — фаза 1

```cpp
// Фаза 1: Акторы
for (auto& actor : world_.actors()) {
    if (actor.behavior) {
        actor.behavior->on_tick(actor, dt_);
        actor.current_state = actor.behavior->current_state();
    }
}
```

### 7. Обновить сцену

```yaml
actors:
  - id: 1
    name: "door_1"
    type: "door"
    pose: {x: 5.0, y: 0.0, z: 0.0}
    collision:
      type: "box"
      size: {x: 1.0, y: 0.1, z: 2.0}
    visual:
      type: "box"
      size: {x: 1.0, y: 0.1, z: 2.0}
      color: "#8B4513"
    behavior:
      type: "door"
      params:
        open_time: 2.0
        close_time: 2.0
        auto_close_delay: 10.0

  - id: 2
    name: "pedestrian_0"
    type: "pedestrian"
    pose: {x: 0.0, y: 3.0, z: 0.0}
    collision:
      type: "capsule"
      radius: 0.3
      height: 1.8
    visual:
      type: "capsule"
      radius: 0.3
      height: 1.8
      color: "#4ECDC4"
    behavior:
      type: "pedestrian"
      params:
        waypoints: [{x: 0, y: 3}, {x: 8, y: 3}]
        speed: 1.4
        pause_duration: 2.0
        loop: true
```

## Тесты

```
s2_core/tests/test_fsm.cpp
s2_core/tests/test_door.cpp
s2_core/tests/test_pedestrian.cpp
```

### test_fsm.cpp
- Создать FSM, add states/transitions. send_trigger → state changes
- Timed transition: tick 200 раз (2 сек при dt=0.01) → автоматический переход
- on_enter/on_exit вызываются при переходах
- while_in вызывается каждый tick
- Guard false → переход не происходит

### test_door.cpp
- Initial state = closed
- send "open_command" → state = opening
- tick 200 раз → state = open, коллизия убрана
- tick 1000 раз → state = closing (auto_close)
- tick 200 раз → state = closed, коллизия вернулась
- Событие ActorStateChanged публикуется при каждом переходе

### test_pedestrian.cpp
- Initial state = walking
- tick N раз → позиция меняется в сторону waypoint 0
- Достиг waypoint → state = waiting
- tick pause_duration → state = walking к следующему waypoint
- loop = true → после последнего waypoint идёт к первому

## Критерии приёмки

- FSM работает с triggers и timed transitions
- Дверь открывается/закрывается, коллизия меняется
- Пешеход ходит по waypoints
- События публикуются в Event Bus
