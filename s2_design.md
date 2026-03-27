# S2 — Simple Swarm Sim: полный дизайн системы

---

## 1. Концепция и цели

**S2** (Simple Swarm Sim) — лёгкая симуляция мобильных роботов для тестирования высокоуровневых алгоритмов управления роями. Аналог Isaac Sim по роли в системе, но без рендера и физического движка.

### Что делает S2

- Идеально отрабатывает все команды управления
- Публикует GT позиции, TF и сенсорные данные в ROS топики
- Работает в изолированных ROS доменах (один домен = один робот)
- Симулирует физические объекты мира (захват, перенос, зоны взаимодействия)
- Предоставляет веб-визуализатор для наблюдения за роем

### Что S2 не делает

- Не физическая симуляция (нет трения, столкновений с импульсом, инерции)
- Не заменяет Gazebo для тестирования низкоуровневого управления
- Не привязана к конкретному типу робота — всё через плагины

### Ограничения первой версии

- Максимум 100 роботов (ROS domain ID: 0–98, 99 зарезервирован для viz)
- Wall time, без глобальной синхронизации времени

---

## 2. Принцип изоляции доменов

**Всё что относится к конкретному роботу — живёт только в его домене.**

```
domain N  →  только данные робота N:
               /tf, /tf_static, /joint_states, /odom
               /gnss/<n>, /imu/<n>, /lidar/<n>
               /detected_objects
               /s2/event/<n>, /s2/collision
               /grab, /release, ...

domain 99 →  только геометрия мира для визуализатора:
               /s2/world_state  (позиции роботов, объекты, зоны, карта)
               НЕТ сенсорных данных
               НЕТ joint_states
               НЕТ TF
```

Если нужно смотреть на сенсорные данные конкретного робота — запустить ROS клиент с `ROS_DOMAIN_ID=N` и подписаться напрямую. Визуализатор не является агрегатором сенсорных данных.

---

## 3. Архитектура верхнего уровня

```
┌──────────────────────────────────────────────────────────────────────┐
│                       s2_node  (один процесс)                        │
│                                                                      │
│  SimEngine                                                           │
│  ├── SimWorld                                                        │
│  │   ├── Map              — occupancy grid (2D) / octomap (3D)      │
│  │   ├── WorldObjects[]   — физические объекты (бочки, коробки)     │
│  │   └── TriggerZones[]   — зоны взаимодействия                     │
│  └── Robots[]                                                        │
│      ├── RobotState + KinematicTree                                  │
│      ├── MotionPlugins[]                                             │
│      └── SensorPlugins[]                                             │
│                                                                      │
│  DomainBridge                                                        │
│  ├── domain 0..N   — изолированный pub/sub каждого робота            │
│  └── domain 99     — только /s2/world_state для viz сервера          │
└──────────────────────────────────────────────────────────────────────┘

┌──────────────────────────────────────────────────────────────────────┐
│  s2_viz_server  (отдельный процесс, ROS_DOMAIN_ID=99)                │
│  sub: /s2/world_state @ 30 Hz                                        │
│  WebSocket сервер → браузер (Three.js)                               │
│  HTTP сервер     → раздаёт s2_viz_web/ статику                       │
└──────────────────────────────────────────────────────────────────────┘

Стек управления (отдельный процесс, ROS_DOMAIN_ID=N):
  Nav2 / MoveIt2  →  топики  →  s2_node (domain N)
  s2_node  →  /tf, /joint_states, /odom  →  Nav2 / MoveIt2
  RSP с publish_tf: false — только holder для robot_description
```

### Почему s2_viz_server — отдельный процесс

Симуляция работает независимо от состояния визуализатора. Можно переписать или заменить `s2_viz_server` без изменения `s2_node`. Краш визуализатора не останавливает симуляцию.

---

## 4. Структура пакетов

```
s2/
├── s2_core/           # SimEngine, SimWorld, DomainBridge, KinematicTree
├── s2_plugins/        # MotionPlugin'ы и SensorPlugin'ы
├── s2_world/          # WorldObjectBase, TriggerZone, встроенные примитивы
├── s2_viz_server/     # WebSocket + HTTP сервер (Python/C++)
├── s2_viz_web/        # Веб-клиент (Three.js, HTML/JS/CSS)
├── s2_bringup/        # launch файлы, URDF, конфиги стека управления
└── s2_config/         # сценарии (YAML)
```

---

## 5. Структуры данных

### RobotState

```cpp
struct JointState {
    std::string name;
    double position{0.0};
    double velocity{0.0};
    double effort{0.0};
    double pos_min{-1e9};
    double pos_max{1e9};
    double vel_max{1e9};
    bool   is_continuous{false};
};

struct RobotState {
    // Поза в world frame — GT, единственный источник
    double x{0.0}, y{0.0}, z{0.0};
    double roll{0.0}, pitch{0.0}, yaw{0.0};
    // Скорости в world frame
    double vx{0.0}, vy{0.0}, vz{0.0};
    double wx{0.0}, wy{0.0}, wz{0.0};

    std::vector<JointState> joints;
    std::string  robot_name;
    int          domain_id{0};
    std::string  urdf_string;
    rclcpp::Time stamp;
};
```

### WorldObjectState

Два состояния: объект либо свободен в мире, либо прикреплён к линку робота.

```cpp
enum class ObjectStatus { FREE, ATTACHED };

struct WorldObjectState {
    std::string  id;       // "barrel_0"
    std::string  type;     // "barrel", "box", ...
    ObjectStatus status{ObjectStatus::FREE};

    // FREE: хранится напрямую
    double x{0.0}, y{0.0}, z{0.0}, yaw{0.0};

    // ATTACHED: вычисляется каждый тик
    // pose = ktree.get_world_transform(attached_link) ⊕ offset
    std::string attached_robot;
    std::string attached_link;
    double offset_x{0.0}, offset_y{0.0}, offset_z{0.0};
};
```

---

## 6. SimWorld

### Конфигурация мира

```yaml
s2:
  update_rate: 100
  viz_rate:    30

  world:
    gnss_origin:              # глобальная настройка — все GNSSPlugin берут отсюда
      latitude:  54.9554693833
      longitude: 61.5010060133
      altitude:  200.0
    map:
      type: "2d"              # "2d" | "3d_octomap" | "3d_primitives"
      file: "$(find s2_config)/maps/warehouse.pgm"
      yaml: "$(find s2_config)/maps/warehouse.yaml"

    objects:
      - id: "barrel_0"
        type: "barrel"
        pose: {x: 3.0, y: 2.0, z: 0.0, yaw: 0.0}
      - id: "box_0"
        type: "box"
        pose: {x: 7.0, y: 4.0, z: 0.0, yaw: 0.0}

    trigger_zones:
      - id: "aruco_42"
        type: "s2_world/ProximityTrigger"
        params:
          zone: {type: circle, center: {x: 10.0, y: 5.0}, radius: 1.5}
          watch: "any_robot"
          link:  "base_link"
          on_enter:
            event: "aruco_detected"
            data:  {marker_id: 42, pose: {x: 10.0, y: 5.0}}
            domain: "robot"       # "robot" = домен робота который вошёл
          on_exit:
            event: "aruco_lost"
            data:  {marker_id: 42}
          repeat: "always"        # "once" | "always" | N

      - id: "loading_bay"
        type: "s2_world/ProximityTrigger"
        params:
          zone: {type: box, center: {x: 0.0, y: 0.0}, width: 4.0, height: 4.0}
          watch: "any_robot"
          link:  "base_link"
          on_enter:
            event: "receive_mode_enabled"
            data:  {}
          on_exit:
            event: "receive_mode_disabled"
            data:  {}
          repeat: "always"
```

### Коллизии

- Collision body — круг радиуса `collision_radius` вокруг `base_link`
- Проверяется в `DiffDriveMotionPlugin` и `MulticopterMotionPlugin` после интеграции
- При коллизии: позиция не обновляется (робот стоит), публикуется `/s2/collision` в домене робота
- Объекты мира не участвуют в коллизиях с роботами (только карта)

### TriggerZone — принцип развязки

Зона публикует именованные **события** в домен робота. Плагины подписываются на нужные им события. Зона ничего не знает о плагинах — плагины ничего не знают о зонах.

```
TriggerZone срабатывает
  → pub /s2/event/<event_name> {data} в domain N

SensorPlugin
  → sub /s2/event/<event_name>
  → обновляет внутреннее состояние
  → отвечает на запросы через сервис
```

### WorldObjectBase

```cpp
class WorldObjectBase {
public:
    virtual void initialize(
        const WorldObjectConfig& config,
        SimWorld* world
    ) = 0;

    // Вызывается каждый тик после обновления всех роботов
    virtual void update(
        const std::vector<RobotState>& robots,
        double dt
    ) = 0;

    virtual ~WorldObjectBase() = default;
};
```

---

## 7. Плагинная система

### Иерархия

```
SimPluginBase
├── MotionPluginBase
│   ├── DiffDriveMotionPlugin          [обязательно]
│   ├── JointVelocityMotionPlugin      [обязательно]
│   ├── JointTrajectoryMotionPlugin    [позже — для MoveIt2]
│   └── MulticopterMotionPlugin        [позже — для дронов]
│
└── SensorPluginBase
    ├── GNSSPlugin                     [обязательно]
    ├── GrabPlugin                     [обязательно]
    ├── ObjectDetectionPlugin          [обязательно]
    ├── ArucoReceiverPlugin            [обязательно]
    └── Lidar2DPlugin                  [позже]
```

### MotionPluginBase

```cpp
class MotionPluginBase {
public:
    virtual void initialize(
        const PluginConfig& config,
        rclcpp::Node::SharedPtr node,
        const std::vector<JointState>& urdf_joints
    ) = 0;

    virtual void update(RobotState& state, double dt) = 0;

    virtual std::vector<std::string> owned_joints() const { return {}; }
    virtual bool owns_base() const { return false; }

    virtual void activate()   {}
    virtual void deactivate() {}
    virtual ~MotionPluginBase() = default;
};
```

### SensorPluginBase

```cpp
class SensorPluginBase {
public:
    virtual void initialize(
        const PluginConfig& config,
        rclcpp::Node::SharedPtr node
    ) = 0;

    // world_T_sensor — GT трансформ фрейма сенсора, вычисляется KinematicTree
    // state — read-only
    virtual void update(
        const RobotState& state,
        const geometry_msgs::msg::Transform& world_T_sensor,
        const rclcpp::Time& stamp,
        double dt
    ) = 0;

    const std::string& virtual_frame() const { return virtual_frame_; }

    virtual void activate()   {}
    virtual void deactivate() {}
    virtual ~SensorPluginBase() = default;

protected:
    std::string virtual_frame_;  // "robot_name/plugin_name_link"
};
```

---

## 8. Motion плагины

### DiffDriveMotionPlugin [обязательно]

```
Входы:  /cmd_vel (geometry_msgs/Twist)

update(state, dt):
    state.vx  = clamp(cmd.linear.x,  ±max_linear_vel)
    state.wz  = clamp(cmd.angular.z, ±max_angular_vel)
    state.x  += state.vx * cos(state.yaw) * dt
    state.y  += state.vx * sin(state.yaw) * dt
    state.yaw += state.wz * dt
    # после интеграции — проверка коллизии с картой
    # при коллизии: откат позиции, pub /s2/collision

params:
  cmd_vel_topic:   "/cmd_vel"
  max_linear_vel:  10.0
  max_angular_vel: 5.0
```

### JointVelocityMotionPlugin [обязательно]

```
Входы:  /<topic> (sensor_msgs/JointState, поле velocity + поле name)

update(state, dt):
    для каждого joint в owned_joints:
        j.velocity = cmd_velocity[joint]
        j.position = clamp(j.position + j.velocity * dt, pos_min, pos_max)

params:
  command_topic: "/arm_vel"
  owned_joints:  [arm_joint, bucket_joint]
```

### JointTrajectoryMotionPlugin [позже]

```
Входы:  /<topic> (trajectory_msgs/JointTrajectory) — формат MoveIt2

update(state, dt):
    elapsed_ += dt
    найти текущий сегмент по time_from_start
    alpha = (elapsed - t0) / (t1 - t0)
    для каждого owned_joint:
        target = lerp(p0.positions[i], p1.positions[i], alpha)
        j.velocity = (target - j.position) / dt
        j.position = clamp(target, pos_min, pos_max)
    если elapsed >= последняя точка: зафиксировать

params:
  command_topic: "/joint_trajectory_controller/joint_trajectory"
  owned_joints:  [arm_joint, bucket_joint]
```

### MulticopterMotionPlugin [позже]

```
Входы:  /cmd_vel (Twist: linear.x/y/z = желаемые скорости, angular.z = yaw_rate)

update(state, dt):
    # Перевод body frame → world frame через yaw
    target_vx = cmd.vx*cos(yaw) - cmd.vy*sin(yaw)
    target_vy = cmd.vx*sin(yaw) + cmd.vy*cos(yaw)

    # Инерционный фильтр
    alpha = dt / (velocity_tau + dt)   # tau=0 → мгновенная отработка
    state.vx += alpha * (target_vx - state.vx)
    state.vy += alpha * (target_vy - state.vy)

    # Вертикаль: гравитация если нет команды
    if has_active_cmd: state.vz += alpha * (cmd.vz - state.vz)
    else:              state.vz -= gravity * dt

    state.yaw += cmd.wz * dt
    state.x   += state.vx * dt
    state.y   += state.vy * dt
    state.z    = max(0.0, state.z + state.vz * dt)

params:
  cmd_vel_topic: "/cmd_vel"
  velocity_tau:  0.3     # секунды, 0 = идеально
  gravity:       9.81
```

---

## 9. Sensor плагины

### GNSSPlugin [обязательно]

```
Публикует в домен робота: /gnss/<name> (sensor_msgs/NavSatFix)

update():
    x, y, z = world_T_sensor.translation
    lat, lon, alt = enu_to_lla(x, y, z, world.gnss_origin)
    # gnss_origin берётся из s2.world — не дублируется в конфиге плагина
    lat += gaussian(0, noise_std)
    lon += gaussian(0, noise_std)
    publish NavSatFix

params:
  parent_link:  "base_link"
  offset:       {x: -1.25, y: -0.75, z: 0.8}
  noise_std:    0.01
  publish_rate: 10.0
```


### GrabPlugin [обязательно]

```
Публикует в домен робота:
  сервисы: /grab, /release, /get_held_objects

Слушает события в домене робота:
  /s2/event/receive_mode_enabled  → receive_mode = true
  /s2/event/receive_mode_disabled → receive_mode = false

Логика /grab:
  1. Найти все FREE объекты, где dist(world_T_grab_link, obj.pose) < grab_radius
  2. Если len(held) < max_held: прикрепить ближайший
     obj.status = ATTACHED
     obj.attached_robot = robot_name
     obj.attached_link  = container_link
     obj.offset = world_T_grab_link⁻¹ ⊕ obj.pose  (относительный оффсет)

Логика /release:
  1. Если рядом робот с receive_mode=true И len(его_held) < его_max_held:
       передать объект тому роботу
  2. Иначе:
       obj.status = FREE
       obj.pose   = текущая world позиция объекта (вычислена из ATTACHED)

params:
  grab_link:       "bucket"
  grab_radius:     0.1          # метры от grab_link до объекта
  max_held:        1            # манипулятор=1, самосвал=10, ...
  container_link:  "bucket"     # линк к которому крепится объект
  receive_mode:    false        # начальное состояние
```

### ObjectDetectionPlugin [обязательно]

```
Публикует в домен робота: /detected_objects (кастомный тип)
  [{id, type, pose, distance}] — только FREE объекты в радиусе

GT топик публикуется в domain 99 для отладки:
  /s2/world/objects_gt — все объекты всегда

update():
    для каждого FREE объекта в world:
        d = dist(world_T_detection_link, obj.pose)
        если d < detection_radius: добавить в список
    publish detected_objects

params:
  detection_link:   "base_link"
  detection_radius: 5.0
  publish_rate:     5.0
```

### ArucoReceiverPlugin [обязательно]

```
Слушает события в домене робота:
  /s2/event/aruco_detected → запомнить {marker_id, pose}
  /s2/event/aruco_lost     → сбросить

Сервис в домене робота:
  /get_detected_marker → {marker_id, pose} или пусто
```

### Lidar2DPlugin [позже]

```
Публикует в домен робота: /lidar/<name> (sensor_msgs/LaserScan)

v1: все лучи = range_max (нет препятствий)
v2: raycast против occupancy grid карты

params:
  parent_link:  "base_link"
  offset:       {x: 1.25, y: 0.0, z: 0.5}
  range_max:    30.0
  num_rays:     360
  noise_std:    0.005
  publish_rate: 10.0
```

---

## 10. KinematicTree

Строится из URDF. Загружает только кинематику — `<visual>`, `<collision>`, `<inertial>` игнорируются.

### Виртуальные линки

Для сенсоров без записи в URDF — статичный трансформ `parent_link → virtual_frame`:

```cpp
ktree.add_virtual_link("dozer_0/gnss_left_link", "base_link", offset);
// → /tf_static, transient_local, один раз при старте
```

Нулевой оффсет = прикрепление к существующему линку напрямую.

### Публикуемые трансформы (в домен робота)

| Топик | Содержимое | Частота |
|-------|-----------|---------|
| `/tf_static` | все fixed joints + virtual links | один раз, transient_local |
| `/tf` | `map→odom`, `odom→base_link`, все подвижные joints | 100 Hz |

S2 — единственный источник TF в домене каждого робота. RSP в стеке управления работает с `publish_tf: false`.

---

## 11. DomainBridge

Каждый домен: отдельный `rclcpp::Context` + `rclcpp::Node` + `SingleThreadedExecutor` в своём `std::thread`.

### Публикует (domain N — только данные робота N)

```
/tf                    — динамические трансформы (100 Hz)
/tf_static             — fixed joints + virtual links (transient_local)
/joint_states          — все joints из URDF (100 Hz)
/odom                  — GT nav_msgs/Odometry (100 Hz)
/s2/collision          — предупреждение при коллизии со стеной
/s2/event/<n>          — события от TriggerZone
/gnss/<name>           — NavSatFix (частота из конфига плагина)
/imu/<name>            — Imu (частота из конфига плагина)
/lidar/<name>          — LaserScan (частота из конфига плагина)
/detected_objects      — объекты в радиусе обнаружения
```

### Слушает (domain N)

```
/cmd_vel               — DiffDriveMotionPlugin / MulticopterMotionPlugin
/<joint_topic>         — JointVelocityMotionPlugin
/joint_trajectory_controller/joint_trajectory — JointTrajectoryMotionPlugin
/grab, /release        — GrabPlugin (сервисы)
/get_held_objects      — GrabPlugin (сервис)
/get_detected_marker   — ArucoReceiverPlugin (сервис)
```

### Публикует (domain 99 — только для viz)

```
/s2/world_state        — WorldStateMsg @ 30 Hz
/s2/world/objects_gt   — GT все объекты @ 30 Hz
```

---

## 12. SimEngine — главный цикл

```cpp
void SimEngine::run() {
    auto prev = std::chrono::steady_clock::now();

    while (running_) {
        auto now  = std::chrono::steady_clock::now();
        double dt = std::clamp(
            std::chrono::duration<double>(now - prev).count(), 0.0, 0.05);
        prev = now;

        rclcpp::Time stamp = get_wall_time();

        // 1. Обновить всех роботов
        for (auto& robot : robots_) {
            for (auto& mp : robot.motion_plugins)
                mp->update(robot.state, dt);

            robot.ktree.update_joints(robot.state);

            for (auto& sp : robot.sensor_plugins) {
                auto T = robot.ktree.get_world_transform(
                    sp->virtual_frame(), robot.state);
                sp->update(robot.state, T, stamp, dt);
            }
        }

        // 2. Обновить позиции прикреплённых объектов
        for (auto& obj : world_.objects) {
            if (obj.status == ObjectStatus::ATTACHED) {
                auto& robot = find_robot(obj.attached_robot);
                auto T = robot.ktree.get_world_transform(obj.attached_link,
                                                          robot.state);
                obj.x = T.translation.x + obj.offset_x;
                obj.y = T.translation.y + obj.offset_y;
                obj.z = T.translation.z + obj.offset_z;
            }
        }

        // 3. Проверить триггеры (публикуют /s2/event в домены роботов)
        for (auto& zone : world_.trigger_zones)
            zone.update(robots_, domain_bridge_, dt);

        // 4. Публикация данных роботов в их домены
        for (auto& robot : robots_) {
            robot.state.stamp = stamp;
            domain_bridge_.publish_robot(robot);
        }

        // 5. Публикация состояния мира в domain 99 (@ 30 Hz, не каждый тик)
        viz_accum_ += dt;
        if (viz_accum_ >= 1.0 / viz_rate_) {
            domain_bridge_.publish_world_state(world_, robots_, stamp);
            viz_accum_ = 0.0;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10)); // 100 Hz
    }
}
```

### Проверки при инициализации

1. Нет двух MotionPlugin'ов с пересекающимися `owned_joints`
2. Нет двух MotionPlugin'ов с `owns_base() == true`
3. `parent_link` каждого SensorPlugin существует в URDF
4. `grab_link` и `container_link` в GrabPlugin существуют в URDF
5. `domain_id` каждого робота уникален и ≠ 99
6. `id` каждого WorldObject уникален

---

## 13. s2_viz_server

Отдельный процесс (`ROS_DOMAIN_ID=99`). Подписывается на `/s2/world_state`, рассылает JSON всем WebSocket клиентам @ 30 Hz. Также раздаёт статику `s2_viz_web/` по HTTP.

### Формат WorldState (JSON, domain 99 → браузер)

```json
{
  "t": 1234567890.123,
  "robots": [
    {
      "name": "dozer_0",
      "domain": 0,
      "pose": {"x": 1.2, "y": 3.4, "z": 0.0, "yaw": 0.5},
      "velocity": {"vx": 0.5, "wz": 0.1},
      "joints": {"arm_joint": 0.3, "bucket_joint": 0.1},
      "collision": false,
      "held_objects": ["barrel_0"],
      "receive_mode": false,
      "status": "navigating"
    }
  ],
  "objects": [
    {
      "id": "barrel_0",
      "type": "barrel",
      "pose": {"x": 1.2, "y": 3.3, "z": 0.0},
      "status": "attached",
      "attached_to": {"robot": "dozer_0", "link": "bucket"}
    }
  ],
  "zones": [
    {
      "id": "aruco_42",
      "shape": {"type": "circle", "x": 10.0, "y": 5.0, "r": 1.5},
      "active_robots": ["dozer_1"]
    }
  ],
  "map": {
    "width": 200, "height": 200, "resolution": 0.05,
    "origin": {"x": -5.0, "y": -5.0},
    "data": "base64..."
  }
}
```

Карта (`map.data`) отправляется только при изменении — не каждый кадр.

---

## 14. Веб-клиент (Three.js)

### Режимы просмотра

- **2D ортогональный** (вид сверху) — основной для наземных роботов
- **3D перспективный** — для дронов и смешанных роёв
- Переключение кнопкой, плавная анимация камеры

### Отображение роботов — три варианта

| Режим | Описание | Когда использовать |
|-------|----------|--------------------|
| `shape` | Прямоугольник/цилиндр + стрелка направления | По умолчанию, всегда работает |
| `links_only` | Точки + линии TF дерева | Отладка кинематики |
| `urdf` | Загрузка через `urdf-loader` | Когда нужна полная модель |

Настраивается в `viz` секции конфига каждого робота и переключается в UI.

### Слои (включаются/выключаются отдельно для каждого робота)

- Позиция и ориентация
- Маршрут (Nav2 path, если настроен `relay_topics`)
- Следы (история позиций, настраиваемая длина)
- Зоны триггеров (полупрозрачные меши с подсветкой при активации)
- Объекты мира (иконки типа объекта, статус)
- Радиус обнаружения объектов (круг вокруг робота)
- Статус задачи (текстовый оверлей над роботом)
- TF дерево (для отладки)

### Панель управления

- Список всех роботов с цветовым индикатором статуса
- Фильтрация: показать/скрыть отдельных роботов
- Настройка слоёв на каждого робота
- Глобальная статистика: объектов захвачено, триггеров сработало
- Настройки камеры: следить за роботом, центрировать всех

---

## 15. Полный конфиг сценария

```yaml
s2:
  update_rate: 100
  viz_rate:    30

  world:
    gnss_origin:
      latitude:  54.9554693833
      longitude: 61.5010060133
      altitude:  200.0
    map:
      type: "2d"
      file: "$(find s2_config)/maps/warehouse.pgm"
      yaml: "$(find s2_config)/maps/warehouse.yaml"

    objects:
      - id: "barrel_0"
        type: "barrel"
        pose: {x: 3.0, y: 2.0, z: 0.0, yaw: 0.0}
      - id: "box_0"
        type: "box"
        pose: {x: 7.0, y: 4.0, z: 0.0, yaw: 0.0}

    trigger_zones:
      - id: "aruco_42"
        type: "s2_world/ProximityTrigger"
        params:
          zone: {type: circle, center: {x: 10.0, y: 5.0}, radius: 1.5}
          watch: "any_robot"
          link:  "base_link"
          on_enter:
            event: "aruco_detected"
            data:  {marker_id: 42}
            domain: "robot"
          on_exit:
            event: "aruco_lost"
            data:  {marker_id: 42}
          repeat: "always"

      - id: "loading_bay"
        type: "s2_world/ProximityTrigger"
        params:
          zone: {type: box, center: {x: 0.0, y: 0.0}, width: 4.0, height: 4.0}
          watch: "any_robot"
          link:  "base_link"
          on_enter:
            event: "receive_mode_enabled"
            data:  {}
          on_exit:
            event: "receive_mode_disabled"
            data:  {}
          repeat: "always"

robots:

  - name: "dozer_0"
    domain_id: 0
    urdf: "$(find s2_bringup)/urdf/d12.urdf.xacro"
    pose: {x: 0.0, y: 0.0, z: 0.0, yaw: 0.0}
    collision_radius: 1.5

    viz:
      color: "#FF6B35"
      shape: "box"                  # "box" | "cylinder" | "urdf" | "links_only"
      show_path: true
      show_detection_radius: true
      show_held_objects: true
      relay_topics:                 # топики из домена робота для отображения
        - topic: "/plan"            # пробрасываются через domain_bridge в viz state
          label: "nav2_path"

    motion_plugins:
      - type: "s2_plugins/DiffDriveMotionPlugin"
        name: "base"
        params:
          cmd_vel_topic:   "/cmd_vel"
          max_linear_vel:  3.0
          max_angular_vel: 1.5

      - type: "s2_plugins/JointVelocityMotionPlugin"
        name: "arm_ctrl"
        params:
          command_topic: "/arm_vel"
          owned_joints:  [arm_joint, bucket_joint]

    sensor_plugins:
      - type: "s2_plugins/GNSSPlugin"
        name: "gnss_left"
        parent_link: "base_link"
        offset: {x: -1.25, y: -0.75, z: 0.8}
        params:
          noise_std:    0.01
          publish_rate: 10.0

      - type: "s2_plugins/GNSSPlugin"
        name: "gnss_right"
        parent_link: "base_link"
        offset: {x: -1.25, y: 0.75, z: 0.8}
        params:
          noise_std:    0.01
          publish_rate: 10.0

      - type: "s2_plugins/IMUPlugin"
        name: "imu"
        parent_link: "base_link"
        offset: {x: 0.0, y: 0.0, z: 0.0}
        params:
          accel_noise_std: 0.01
          gyro_noise_std:  0.001
          publish_rate:    100.0

      - type: "s2_plugins/GrabPlugin"
        name: "bucket_grab"
        parent_link: "bucket"
        offset: {x: 0.0, y: 0.0, z: 0.0}
        params:
          grab_link:      "bucket"
          grab_radius:    0.1
          max_held:       1
          container_link: "bucket"
          receive_mode:   false

      - type: "s2_plugins/ObjectDetectionPlugin"
        name: "obj_sensor"
        parent_link: "base_link"
        offset: {x: 0.0, y: 0.0, z: 0.0}
        params:
          detection_radius: 5.0
          publish_rate:     5.0

      - type: "s2_plugins/ArucoReceiverPlugin"
        name: "aruco"
        parent_link: "base_link"
        offset: {x: 0.0, y: 0.0, z: 0.0}

  - name: "drone_0"
    domain_id: 2
    urdf: "$(find s2_bringup)/urdf/quadrotor.urdf.xacro"
    pose: {x: 10.0, y: 0.0, z: 2.0, yaw: 0.0}
    collision_radius: 0.5

    viz:
      color: "#4ECDC4"
      shape: "cylinder"
      show_path: true

    motion_plugins:
      - type: "s2_plugins/MulticopterMotionPlugin"
        name: "flight"
        params:
          cmd_vel_topic: "/cmd_vel"
          velocity_tau:  0.3
          gravity:       9.81

    sensor_plugins:
      - type: "s2_plugins/GNSSPlugin"
        name: "gnss"
        parent_link: "base_link"
        offset: {x: 0.0, y: 0.0, z: 0.1}
        params:
          noise_std:    0.005
          publish_rate: 10.0
```

---

## 16. URDF — что убирается

Блок `<ros2_control>` удаляется полностью. S2 публикует `/joint_states` напрямую. Остаётся только кинематика.

```xml
<!-- УБРАТЬ: -->
<ros2_control name="IsaacSystem" type="system"> ... </ros2_control>

<!-- ОСТАВИТЬ (без изменений): -->
<joint name="arm_joint" type="revolute">
  <limit lower="-1.0" upper="0.57" effort="200000000" velocity="0.01"/>
</joint>
```

---

## 17. Стек управления — launch файл

```python
# robot_control.launch.py
# Запуск: ROS_DOMAIN_ID=0 ros2 launch s2_bringup robot_control.launch.py

Node(
    package="robot_state_publisher",
    executable="robot_state_publisher",
    parameters=[{
        "robot_description": robot_desc,
        "publish_tf": False,      # S2 — единственный источник TF
        "use_sim_time": False,
    }]
)
# + Nav2, MoveIt2 — без controller_manager, без joint_state_broadcaster
```

---

## 18. Что нужно обязательно и что позже

### Обязательно

- `RobotState`, `JointState`, `WorldObjectState` структуры
- `KinematicTree` — загрузка URDF, `/tf_static` + `/tf`, виртуальные линки
- `DomainBridge` — N доменов, pub/sub, domain 99 для viz
- `SimEngine` — главный цикл, парсинг YAML, проверки при инициализации
- `SimWorld` — Map (2D pgm), WorldObjects, TriggerZone (ProximityTrigger)
- Коллизии с картой — стоп при достижении occupied cell
- `DiffDriveMotionPlugin`
- `JointVelocityMotionPlugin`
- `GNSSPlugin` (берёт origin из `s2.world.gnss_origin`)
- `IMUPlugin`
- `GrabPlugin` (FREE/ATTACHED, передача между роботами, receive_mode через события)
- `ObjectDetectionPlugin`
- `ArucoReceiverPlugin`
- `s2_viz_server` — WebSocket + HTTP сервер
- `s2_viz_web` — Three.js клиент, 2D/3D вид, слои, панель управления
- `s2.launch.py`, `robot_control.launch.py`

### Позже

- `JointTrajectoryMotionPlugin` — для MoveIt2
- `MulticopterMotionPlugin` — для дронов
- `Lidar2DPlugin` — v1 без препятствий, v2 с raycast
- 3D карты (octomap)
- 3D коллизии для дронов
- Глобальный `/clock` publisher для детерминированного воспроизведения
- 2.5D рельеф для реалистичного IMU (roll/pitch из DEM)
- Mavros-совместимый MotionPlugin для PX4/ArduPilot стека
- URDF отображение в визуализаторе через `urdf-loader`
- Запись и воспроизведение сценариев

---

## 19. Зависимости

```xml
<!-- s2_core -->
<depend>rclcpp</depend>
<depend>tf2_ros</depend>
<depend>sensor_msgs</depend>
<depend>geometry_msgs</depend>
<depend>nav_msgs</depend>
<depend>trajectory_msgs</depend>
<depend>std_srvs</depend>
<depend>pluginlib</depend>
<depend>urdf</depend>
<depend>yaml-cpp</depend>

<!-- s2_plugins -->
<depend>s2_core</depend>
<depend>pluginlib</depend>
<depend>sensor_msgs</depend>
<depend>geometry_msgs</depend>
<depend>trajectory_msgs</depend>
<depend>std_srvs</depend>

<!-- s2_world -->
<depend>s2_core</depend>
<depend>pluginlib</depend>

<!-- s2_viz_server -->
<depend>s2_core</depend>
<depend>rclcpp</depend>
<!-- WebSocket: libwebsocketpp или uWebSockets -->

<!-- s2_bringup -->
<depend>robot_state_publisher</depend>
<depend>nav2_bringup</depend>
<depend>moveit_ros_move_group</depend>
```