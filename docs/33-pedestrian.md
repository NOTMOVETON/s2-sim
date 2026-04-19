# Задача 33 — Actor: PedestrianBehavior + присутствие зоны

## Цель

Пешеход — актор, патрулирующий список путевых точек (waypoints).
У пешехода есть коллизионная сфера (LidarPlugin его видит).
Attached-зона `person_presence` вокруг пешехода позволяет зонным эффектам
реагировать на его присутствие.

После задачи:
- Пешеход ходит от точки к точке, делает паузы.
- Робот с LidarPlugin замечает пешехода как препятствие.
- Агенты в радиусе присутствия пешехода получают зонный эффект (если нужно).

## Зависимости

- Задача 32 (IActorBehavior, Actor struct, ActorSnapshot)
- Задача 23 (ZoneSystem, attached zones)
- Задача 22 (LidarPlugin — видит сферические коллизии)

---

## Что сделать

### 1. PedestrianBehavior FSM

**Файл:** `workspace/s2_plugins/behaviors/pedestrian_behavior.hpp` (новый)

```cpp
#pragma once
#include <s2/interfaces/actor_behavior.hpp>
#include <s2/actor.hpp>
#include <s2/sim_bus.hpp>
#include <vector>
#include <cmath>

namespace s2::behaviors {

/// Поведение пешехода: обход путевых точек с паузами.
///
/// States: walking, waiting
/// Transitions:
///   walking → waiting : достиг текущей waypoint (dist < arrival_radius)
///   waiting → walking : таймер(pause_secs) → выбрать следующую waypoint
///
/// Движение: линейная интерполяция к target_waypoint со speed.
/// Z-координата: зафиксирована из waypoint или из ground_z.
class PedestrianBehavior : public IActorBehavior {
public:
    void on_init(const YAML::Node& params) override {
        speed_         = params["speed"].as<double>(1.2);        // м/с
        pause_secs_    = params["pause_secs"].as<double>(1.0);
        arrival_radius_ = params["arrival_radius"].as<double>(0.15);

        if (params["waypoints"]) {
            for (const auto& wp : params["waypoints"]) {
                Vec3 p;
                p.x() = wp["x"].as<double>(0.0);
                p.y() = wp["y"].as<double>(0.0);
                p.z() = wp["z"].as<double>(0.0);
                waypoints_.push_back(p);
            }
        }

        if (waypoints_.empty()) {
            waypoints_.push_back(Vec3::Zero());
        }

        current_wp_idx_ = 0;
    }

    void tick(Actor& actor, SimWorld& /*world*/, SimBus& bus, double dt) override {
        switch (state_) {
            case State::WALKING:
                tick_walking(actor, bus, dt);
                break;
            case State::WAITING:
                tick_waiting(actor, bus, dt);
                break;
        }
    }

    std::string current_state() const override {
        return (state_ == State::WALKING) ? "walking" : "waiting";
    }

private:
    void tick_walking(Actor& actor, SimBus& bus, double dt) {
        const Vec3& target = waypoints_[current_wp_idx_];
        Vec3 current = actor.world_pose.position();

        Vec3 delta = target - current;
        delta.z()  = 0.0;   // движение только по XY
        double dist = delta.norm();

        if (dist < arrival_radius_) {
            // Прибыли — встать точно на точку
            actor.world_pose.x = target.x();
            actor.world_pose.y = target.y();
            actor.world_pose.z = target.z();
            state_ = State::WAITING;
            timer_ = 0.0;
            bus.publish(ActorStateChangedEvent{actor.id, "waiting"});
            return;
        }

        // Шаг к цели
        Vec3 dir = delta / dist;
        double step = std::min(speed_ * dt, dist);
        actor.world_pose.x += dir.x() * step;
        actor.world_pose.y += dir.y() * step;
        actor.world_pose.z = target.z();

        // Поворот лицом к цели
        actor.world_pose.yaw = std::atan2(dir.y(), dir.x());
    }

    void tick_waiting(Actor& actor, SimBus& bus, double dt) {
        timer_ += dt;
        if (timer_ >= pause_secs_) {
            current_wp_idx_ = (current_wp_idx_ + 1) % (int)waypoints_.size();
            state_ = State::WALKING;
            timer_ = 0.0;
            bus.publish(ActorStateChangedEvent{actor.id, "walking"});
        }
    }

    enum class State { WALKING, WAITING };

    State              state_{State::WALKING};
    double             timer_{0.0};
    double             speed_{1.2};
    double             pause_secs_{1.0};
    double             arrival_radius_{0.15};
    int                current_wp_idx_{0};
    std::vector<Vec3>  waypoints_;
};

} // namespace s2::behaviors
```

### 2. Attached-зона `person_presence` для пешехода

Пешеход создаётся вместе с прикреплённой зоной.
SceneLoader читает `attached_zone` секцию из YAML-описания актора:

**Файл:** `workspace/s2_core/include/s2/scene_loader.hpp`

```cpp
// При создании актора с полем attached_zone:
//
// actors:
//   - id: 10
//     name: pedestrian_0
//     type: pedestrian
//     ...
//     attached_zone:
//       id: "ped_0_presence"
//       shape:
//         type: sphere
//         radius: 2.0
//       color: "#FFAA00"
//       opacity: 0.1
//       visible: false       # прозрачная, только для эффектов
//       effects: []          # эффекты настраиваются отдельно

if (actor_node["attached_zone"]) {
    Zone zone = parse_zone(actor_node["attached_zone"]);
    zone.attached_to_actor = actor.id;
    zone.attachment_offset = Vec3::Zero();
    scene.zones.push_back(std::move(zone));
}
```

### 3. Collision для LidarPlugin

Пешеход имеет коллизионную сферу, которую видит LidarPlugin.

В конфиге актора:

```yaml
actors:
  - id: 10
    name: pedestrian_0
    type: pedestrian
    has_collision: true
    collision:
      type: sphere
      radius: 0.3
    behavior: pedestrian
    behavior_params:
      speed: 1.1
      pause_secs: 2.0
      waypoints:
        - {x: 0.0,  y: 0.0,  z: 0.0}
        - {x: 5.0,  y: 0.0,  z: 0.0}
        - {x: 5.0,  y: 5.0,  z: 0.0}
        - {x: 0.0,  y: 5.0,  z: 0.0}
    visual:
      type: cylinder
      radius: 0.25
      height: 1.7
      color: "#FFAA44"
    attached_zone:
      id: "ped_0_presence"
      shape:
        type: sphere
        radius: 2.0
      color: "#FFAA00"
      opacity: 0.08
      visible: true
      label: "Пешеход"
      effects: []
```

### 4. Регистрация поведения в фабрике

**Файл:** `workspace/s2_plugins/src/behaviors_registry.cpp` (новый)

```cpp
#include <s2/behaviors/pedestrian_behavior.hpp>
#include <s2/behaviors/door_behavior.hpp>
#include <s2/interfaces/actor_behavior.hpp>
#include <memory>
#include <string>

namespace s2 {

std::unique_ptr<IActorBehavior> create_behavior(
    const std::string& type, const YAML::Node& params)
{
    std::unique_ptr<IActorBehavior> b;

    if (type == "door")           b = std::make_unique<behaviors::DoorBehavior>();
    else if (type == "pedestrian") b = std::make_unique<behaviors::PedestrianBehavior>();
    // Новые поведения добавляются здесь.

    if (b) b->on_init(params);
    return b;
}

} // namespace s2
```

### 5. ActorSnapshot: дополнительные поля для пешехода

**Файл:** `workspace/s2_core/include/s2/world_snapshot.hpp`

ActorSnapshot уже содержит `behavior_state`. Для пешехода это: `"walking"` или `"waiting"`.

Добавить необязательные поля для более детальной отладки:

```cpp
struct ActorSnapshot {
    // ... существующие поля из задачи 32 ...
    int    wp_index{0};         ///< Текущая waypoint (для отладки, опционально)
    double wp_dist{0.0};        ///< Расстояние до следующей waypoint
};
```

Заполняется только если behavior = pedestrian через dynamic_cast (опционально).

### 6. Визуализация пешехода в Three.js

Пешеход отображается как актор через существующий рендер акторов.
Специальная логика не нужна — `visual.type = "cylinder"` обрабатывается
общим рендером акторов.

Attached-зона видна через ZoneManager (задача 29).

### 7. Пример YAML со сценой

```yaml
actors:
  - id: 10
    name: pedestrian_0
    type: pedestrian
    pose: {x: 0.0, y: 0.0, z: 0.0, yaw: 0.0}
    has_collision: true
    collision:
      type: sphere
      radius: 0.3
    behavior: pedestrian
    behavior_params:
      speed: 1.1
      pause_secs: 1.5
      arrival_radius: 0.2
      waypoints:
        - {x: -3.0, y: -3.0, z: 0.0}
        - {x:  3.0, y: -3.0, z: 0.0}
        - {x:  3.0, y:  3.0, z: 0.0}
        - {x: -3.0, y:  3.0, z: 0.0}
    visual:
      type: cylinder
      radius: 0.25
      height: 1.7
      color: "#FFAA44"
    attached_zone:
      id: "ped_0_zone"
      shape:
        type: sphere
        radius: 2.0
      color: "#FFAA00"
      opacity: 0.1
      visible: true
      label: "Пешеход"
      effects: []
```

---

## Тесты

**Файл:** `workspace/s2_core/tests/test_actor_pedestrian.cpp`

- `Pedestrian_StartsWalking` — начальное состояние = "walking"
- `Pedestrian_ReachesWaypoint_Transitions` — пешеход достигает WP: state="waiting"
- `Pedestrian_PauseExpires_NextWaypoint` — после pause_secs: state="walking", target = wp[1]
- `Pedestrian_Loops_Waypoints` — пешеход проходит все WP и начинает снова с wp[0]
- `Pedestrian_MoveSpeed_Correct` — за 1 сек при speed=1.0: перемещение ≈ 1.0 м
- `Pedestrian_FacesTarget` — world_pose.yaw направлен к target_waypoint
- `Pedestrian_ZOffsetFixed` — Z актора берётся из Z waypoint, а не дрейфует
- `Pedestrian_AttachedZone_FollowsActor` — zone.shape.center == actor.world_pose.position()

---

## Критерии завершения

- [ ] PedestrianBehavior FSM: walking → waiting → walking с правильными таймерами
- [ ] Пешеход обходит все waypoints циклически
- [ ] Attached-зона следует за пешеходом в ZoneSystem
- [ ] LidarPlugin замечает коллизионную сферу пешехода (тест с коллизиями)
- [ ] ActorSnapshot содержит behavior_state = "walking" / "waiting"
- [ ] Все тесты проходят в Docker
