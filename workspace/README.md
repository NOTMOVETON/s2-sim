# S2 — Симулятор автономных агентов

S2 — это модульный симулятор автономных роботов и агентов, предназначенный для тестирования алгоритмов управления, разработки сенсорных плагинов и интеграции с ROS2. Система позволяет запускать несколько агентов в симулированном мире, получать данные сенсоров через транспортный слой (ROS2 или заглушку) и наблюдать за симуляцией в реальном времени через браузерный 3D-визуализатор.

---

## Содержание

1. [Общий обзор системы](#1-общий-обзор-системы)
2. [Ядро симуляции — s2_core](#2-ядро-симуляции--s2_core)
3. [Система плагинов](#3-система-плагинов)
4. [Транспортный слой](#4-транспортный-слой)
5. [Визуализатор](#5-визуализатор)
6. [YAML-конфигурация сцены — полный справочник](#6-yaml-конфигурация-сцены--полный-справочник)

---

## 1. Общий обзор системы

### Назначение

S2 решает задачу быстрого прототипирования и тестирования: вместо запуска реальных роботов разработчик описывает сцену в YAML, запускает `s2_visualizer`, и система генерирует данные сенсоров в ROS2-топиках и позволяет управлять агентами через браузер или ROS2-команды. S2 не симулирует физику или реальные взаимодействия, вместо этого используется система зон и эффектов, которая позволяет эмулировать фактическое совпадение - простые наборы фактов, упрощающие процесс их получения. Высокоуровневой логике не важно как определяется позиция аруко маркера, ее интересует что маркер сдетектирован и известна его позиция.

### Поток данных

```
main.cpp
  │
  ├─ SceneLoader::load(yaml) ──────► SceneData
  │                                     │
  ├─ SimWorld ◄─────────────────────────┘
  │
  ├─ SimEngine::run()  ◄──── тиковый цикл (update_rate Гц)
  │      │
  │      ├── [каждый тик] ──► SimTransportBridge::on_post_tick()
  │      │                         │
  │      │                         └──► ITransportAdapter::publish_agent_frame()
  │      │                                   │
  │      │                              Ros2TransportAdapter ──► ROS2 topics/services
  │      │
  │      └── [viz тик] ──────► VizServer::push_snapshot()
  │                                  │
  │                             SSE /stream ──► браузер (Three.js)
  │                             POST /command ◄── браузер
  │
  └─ SimBus ─── события между модулями (Zone, Actor, Collision)
```

### Таблица модулей

| Модуль | Назначение |
|--------|------------|
| `s2_core` | Ядро: тиковый цикл, агенты, мир, плагины, шина событий |
| `s2_plugins` | Реализации плагинов (diff_drive, gnss, imu, trajectory_recorder и др.) |
| `s2_transport` | Транспортный мост: перевод снапшотов в ROS2/stub-сообщения |
| `s2_visualizer` | HTTP + SSE сервер, точка сборки (`main.cpp`), Three.js фронтенд |
| `s2_msgs` | ROS2 message-типы для s2 (PluginCall, AgentFrame и др.) |
| `s2_config` | YAML-конфиги сцен, URDF-модели роботов |

### Жизненный цикл запуска

```
SceneLoader::load(path)
  ├── Парсит YAML → SceneData
  ├── Создаёт Agent-объекты
  │     └── Для каждого плагина: create_plugin(type, node)
  │               └── plugin->from_config(node)
  └── Возвращает SceneData

main.cpp:
  ├── SimEngine engine(config)
  ├── VizServer viz(port, static_path)
  ├── engine.load_world(world)           ← сохраняет начальное состояние
  ├── engine.set_viz_server(&viz)
  ├── bridge.init(geo_origin)            ← регистрация в транспорте
  │     ├── adapter.register_agent(...)
  │     ├── adapter.register_sensor(...)
  │     ├── adapter.register_input_topic(...)
  │     └── adapter.register_static_transforms(...)
  ├── adapter.start()
  └── engine.run()                       ← бесконечный цикл
```

---

## 2. Ядро симуляции — s2_core

### SimEngine и тиковый цикл

#### Конфигурация

```cpp
struct SimEngine::Config {
    double update_rate    = 100.0;   // Гц — частота физического шага
    double viz_rate       = 30.0;    // Гц — частота обновления визуализатора
    double transport_rate = 100.0;   // Гц — частота публикации в транспорт
};
```

`dt = 1.0 / update_rate` — временной шаг каждого тика.

#### Точный порядок фаз каждого тика

```
SimEngine::tick():
  1. Актуализация Акторов (FSM-переходы)                        [planned]
  2. Проверка зон (entry/exit, обновление active_zones агентов) [planned]
  3. Для каждого агента:
     a. Resource-плагины → add_scale/add_lock/add_velocity_addition [planned]
     b. Собственные эффекты агента (CONTINUOUS)                  [planned]
     c. Эффекты активных зон (CONTINUOUS)                        [planned]
     d. RESOLVER: agent.state.resolve()                          ✓
     e. Плагины update(dt, agent)                                ✓
     f. КИНЕМАТИКА: интеграция позиции                           ✓
     g. Привязка к поверхности                                   [planned]
     h. Обнаружение коллизий                                     [planned]
     i. Обновление KinematicTree / сочленений                    [planned]
     j. Очистка вкладов: agent.state.clear_contributions()       ✓
  4. Обработка прикреплённых объектов (Prop attached_to)        [planned]
  5. [Каждые 1/viz_rate сек] build_snapshot() → VizServer       ✓
  6. [Каждые 1/transport_rate сек] SimTransportBridge           ✓
```

#### Кинематика: перевод скорости в мировые координаты

Скорость агента задаётся в локальной системе координат (тело робота). Перевод в мировые координаты через yaw:

```
world_vx = local_vx * cos(yaw) - local_vy * sin(yaw)
world_vy = local_vx * sin(yaw) + local_vy * cos(yaw)
world_wz = local_wz

new_x   = x   + world_vx * dt * effective_speed_scale
new_y   = y   + world_vy * dt * effective_speed_scale
new_yaw = yaw + world_wz * dt * effective_speed_scale
```

Если `motion_locked == true`, интеграция не выполняется. `velocity_addition` прибавляется к итоговой скорости без масштабирования.

#### Управление жизненным циклом

| Метод | Семантика |
|-------|-----------|
| `pause()` | Останавливает интеграцию позиций и вызовы плагинов. Время не течёт. |
| `resume()` | Возобновляет работу после pause(). |
| `reset()` | Восстанавливает все агенты в начальное состояние (позиции, скорости, состояние плагинов). |
| `stop()` | Завершает цикл run(), освобождает ресурсы. |

#### PostTickCallback

```cpp
using PostTickCallback = std::function<void(const SimWorld&, double sim_time)>;
engine.set_post_tick_callback(callback);
```

Транспортный мост подключается именно через этот колбэк — `SimTransportBridge` вызывает свой `on_post_tick()` из него.

#### SimBus — шина событий

```cpp
SimBus& bus = engine.bus();

// Подписка
bus.subscribe<event::AgentEnteredZone>([](const event::AgentEnteredZone& e) {
    // e.agent_id, e.zone_id
});

// Публикация
bus.publish(event::ActorStateChanged{actor_id, "idle", "open"});
```

Стандартные события:

| Тип события | Поля | Когда публикуется |
|-------------|------|-------------------|
| `AgentEnteredZone` | `agent_id`, `zone_id` | При входе агента в зону |
| `AgentExitedZone` | `agent_id`, `zone_id` | При выходе агента из зоны |
| `ObjectAttached` | `object_id`, `agent_id`, `link` | Объект прикреплён к агенту |
| `ObjectReleased` | `object_id`, `agent_id` | Объект отсоединён |
| `ActorStateChanged` | `actor_id`, `old_state`, `new_state` | Актор сменил состояние |
| `AgentCollision` | `agent_id`, `point` | Коллизия агента |

---

### SharedState и Resolver

`SharedState` живёт внутри каждого `Agent` (поле `agent.state`). Он выполняет две роли:
- **Накапливает вклады** — resource-плагины и зоны записывают ограничения за текущий тик
- **Типобезопасное хранилище** — сенсоры кладут измерения, акторы их читают

#### Три вида вкладов

```cpp
// Мультипликативный коэффициент скорости (0..1 — замедление, >1 — ускорение)
agent.state.add_scale(0.5, "ice_zone");

// Блокировка движения
agent.state.add_lock(true, "battery_dead");

// Аддитивная скорость (конвейер, ветер)
agent.state.add_velocity_addition(Vec3(0.3, 0, 0), "conveyor");
```

#### resolve() — как вычисляются ограничения

```
effective_speed_scale = clamp(product(all scale values), 0, 10)
motion_locked         = any(lock.locked == true)
velocity_addition     = sum(all additive contributions)
```

Результат доступен через `agent.state.effective()`:

```cpp
const EffectiveConstraints& eff = agent.state.effective();
double scale   = eff.speed_scale;         // итоговый масштаб
bool locked    = eff.motion_locked;       // заблокировано?
Vec3 wind      = eff.velocity_addition;   // внешняя скорость
```

#### Типобезопасный словарь для данных сенсоров

```cpp
// Запись (sensor-плагин в update()):
RangeData& d = agent.state.emplace<RangeData>();
d.distance = 1.42;
d.seq++;

// Чтение (другой плагин или транспортный мост):
if (const RangeData* d = agent.state.get<RangeData>()) {
    // используем d->distance
}

// Проверка наличия:
bool has_imu = agent.state.has<ImuData>();
```

Каждый тип хранится в одном экземпляре — последний вызов `emplace<T>()` перезаписывает предыдущий.

---

### Объекты мира

#### SimWorld — контейнер сущностей

```cpp
SimWorld world;

// Агенты
world.add_agent(agent);
std::vector<Agent>& agents = world.agents();
Agent* a = world.get_agent(id);

// Пассивные объекты
world.add_prop(prop);
std::vector<Prop>& props = world.props();

// Активные объекты (двери, лифты)
world.add_actor(actor);

// Статическая геометрия
world.add_static_primitive(prim);
const std::vector<WorldPrimitive>& geo = world.static_geometry();

// Рельеф
world.set_heightmap(heightmap);

// Коллизия
bool hit = world.check_sphere_collision(center, radius);
```

#### Agent — управляемый агент

```cpp
struct Agent {
    AgentId     id;                    // Уникальный числовой ID
    std::string name;                  // Имя ("robot_0")
    int         domain_id;             // ROS2 domain для изоляции сетей
    Pose3D      world_pose;            // Позиция + ориентация в мире
    Velocity    world_velocity;        // Скорость (задаётся плагином)
    SharedState state;                 // Вклады + данные сенсоров
    VisualDesc  visual;                // Визуальное представление
    CollisionShape bounding;           // Объём коллизии
    std::vector<std::unique_ptr<IAgentPlugin>> plugins;
    std::unique_ptr<KinematicTree> kinematic_tree; // Опционально (из URDF)
};
```

#### Базовые типы

```cpp
struct Pose3D {
    double x = 0, y = 0, z = 0;        // Позиция (метры)
    double roll = 0, pitch = 0, yaw = 0; // Углы Эйлера (радианы)
    Vec3 position() const;              // Вернуть {x, y, z}
};

struct Velocity {
    Vec3 linear;   // vx, vy, vz (м/с)
    Vec3 angular;  // wx, wy, wz (рад/с)
};

using Vec3 = Eigen::Vector3d; // x(), y(), z()
```

#### KinematicTree — дерево суставов из URDF

Загружается при наличии `urdf:` в конфиге агента. Обновляется каждый тик через интеграцию скоростей суставов (`joint_vel`-плагин).

```cpp
KinematicTree& tree = *agent.kinematic_tree;

tree.set_joint_value("arm_joint", 0.5);        // рад или м
tree.set_link_color("bucket", "#FF0000");

Pose3D world = tree.compute_world_pose("bucket_tip", agent.world_pose);
Pose3D local = tree.compute_local_pose("arm");

// Собрать трансформы для TF
auto [static_tf, dynamic_tf] = tree.collect_transforms();
```

Типы суставов: `FIXED`, `REVOLUTE`, `PRISMATIC`, `CONTINUOUS`.

#### Prop — пассивный объект

```cpp
struct Prop {
    ObjectId    id;
    std::string type;         // "box", "barrel", "pallet"
    Pose3D      world_pose;
    bool        movable;      // Можно ли переносить
    CollisionShape collision;
    VisualDesc  visual;
    std::map<std::string, std::string> properties; // Произвольные атрибуты
    // attached_to — устанавливается через ObjectAttached event
};
```

#### Actor — FSM-автомат

```cpp
struct Actor {
    ActorId     id;
    std::string name;
    Pose3D      world_pose;
    std::string current_state; // Текущее состояние FSM ("closed", "opening", "open")
    CollisionShape collision;
    VisualDesc  visual;
};
```

Переходы состояний публикуются через `SimBus` (`ActorStateChanged`). Плагины типа `Interaction` подписываются на события и вызывают переходы.

#### Zone — зона эффектов

```cpp
struct Zone {
    ZoneId id;      // Строковый ID ("ice_patch", "no_go_area")
    struct Shape {
        ZoneShapeType type; // SPHERE, AABB, INFINITE
        Vec3   center;
        double radius;      // Для SPHERE
        Vec3   half_size;   // Для AABB
        bool   contains(Vec3 point) const;
    } shape;
    std::vector<ZoneEffect> effects; // Непрерывные/событийные эффекты
};
```

#### WorldPrimitive — статическая геометрия

```cpp
struct WorldPrimitive {
    std::string type;   // "box", "cylinder", "sphere"
    Pose3D      pose;
    Vec3        size;   // Для box: ширина, глубина, высота
    double      radius; // Для cylinder/sphere
    double      height; // Для cylinder
    std::string color;  // "#888888"
};
```

---

## 3. Система плагинов

### Интерфейс IAgentPlugin

Полный интерфейс в `s2_core/include/s2/plugin_base.hpp`:

```cpp
class IAgentPlugin {
public:
    // ── Обязательные ──────────────────────────────────────────────────
    virtual std::string type() const = 0;          // "diff_drive", "gnss", ...
    virtual void update(double dt, Agent& agent) = 0;
    virtual void from_config(const YAML::Node& node) = 0;
    virtual std::string to_json() const = 0;

    // ── Инициализация ─────────────────────────────────────────────────
    virtual void initialize(Agent& agent) {}        // Вызов после регистрации агента

    // ── Входные команды ───────────────────────────────────────────────
    virtual bool        has_inputs()  const { return false; }
    virtual std::string inputs_schema() const { return "{}"; }  // JSON Schema
    virtual void        handle_input(const std::string& json) {}

    // ── ROS2 транспорт ────────────────────────────────────────────────
    virtual std::vector<std::string> command_topics()  const { return {}; }
    virtual std::vector<std::string> service_names()   const { return {}; }
    virtual std::string handle_service(const std::string& svc,
                                       const std::string& req_json) { return "{}"; }
    virtual std::vector<TransportEvent> poll_events()  { return {}; }

    // ── Подписки на внешние топики ────────────────────────────────────
    virtual std::vector<std::string> subscribe_topics() const { return {}; }
    virtual void handle_subscription(const std::string& topic,
                                     const std::string& msg_json) {}

    // ── Метаданные ────────────────────────────────────────────────────
    void        set_sensor_name(const std::string& name);
    std::string sensor_name() const;

    void        set_output_topic(const std::string& topic);
    std::string output_topic() const;

    void   set_base_rate(double hz);
    double publish_rate_hz() const;         // Итоговая частота (config или default)
    virtual double default_publish_rate_hz() const { return 0; }

    void   set_mount_pose(const Pose3D& p);
    Pose3D mount_pose() const;
    FrameTransform mount_frame() const;     // Для регистрации в static TF
};
```

#### Описание методов

| Метод | Описание |
|-------|----------|
| `type()` | Строковый идентификатор. Используется фабрикой и реестром. |
| `update(dt, agent)` | Главный метод обновления. Вызывается каждый тик. |
| `from_config(node)` | Загрузка параметров из YAML-узла плагина. |
| `to_json()` | Сериализация состояния для снапшота (попадает в `plugins_data`). |
| `initialize(agent)` | Вызывается один раз после создания агента. |
| `has_inputs()` | `true` → браузер покажет форму управления. |
| `inputs_schema()` | JSON Schema описывает поля формы. |
| `handle_input(json)` | Получает команды от браузера или транспорта. |
| `command_topics()` | ROS2-топики для подписки (`/cmd_vel`). |
| `service_names()` | ROS2-сервисы, которые плагин предоставляет. |
| `handle_service(svc, req)` | Обработчик ROS2-сервиса, возвращает JSON-ответ. |
| `poll_events()` | Возвращает события (детекции, триггеры) для публикации. |
| `subscribe_topics()` | Топики для входящих подписок (например, `/plan`). |
| `handle_subscription(topic, json)` | Получает данные из подписанных топиков. |
| `set_sensor_name(name)` | Имя экземпляра (для нескольких гносс-сенсоров). |
| `set_output_topic(topic)` | Переопределяет автоматически генерируемое имя топика. |
| `set_base_rate(hz)` | Устанавливает частоту публикации из YAML. |
| `default_publish_rate_hz()` | Базовая частота плагина (переопределяется наследником). |
| `set_mount_pose(pose)` | Задаёт смещение монтирования относительно `base_link`. |
| `mount_frame()` | Возвращает `FrameTransform` для статического TF в ROS2. |

---

### Жизненный цикл плагина

```
SceneLoader::load():
  plugin = create_plugin(type, node)    ← фабрика из plugins_registry.cpp
  plugin->from_config(node)             ← загрузка параметров
  plugin->set_sensor_name(...)          ← из YAML "name:"
  plugin->set_base_rate(...)            ← из YAML "publish_rate_hz:"
  plugin->set_mount_pose(...)           ← из YAML "mount:"
  plugin->set_output_topic(...)         ← из YAML "topic:" (если есть)
  agent.plugins.push_back(plugin)

SimTransportBridge::init():
  for each agent:
    adapter.register_agent(agent)
    for each plugin:
      plugin->initialize(agent)          ← инициализация после регистрации
      adapter.register_sensor(...)       ← если сенсорный плагин
      adapter.register_input_topic(...)  ← для command_topics()
      adapter.register_service(...)      ← для service_names()
      adapter.register_subscription(...) ← для subscribe_topics()
      adapter.register_static_transforms(mount_frame)

SimEngine::tick():
  for each agent:
    plugin->update(dt, agent)            ← каждый тик
    events = plugin->poll_events()       ← сбор событий
    adapter.emit_event(event)
```

---

### Реестр плагинов

#### Паттерн PluginRegistrar

```cpp
// plugins_registry.cpp
#include "s2/plugin_registry.hpp"
#include "s2/plugins/diff_drive.hpp"
// ...

static PluginRegistrar<DiffDrivePlugin>          reg_diff_drive("diff_drive");
static PluginRegistrar<GnssPlugin>               reg_gnss("gnss");
static PluginRegistrar<ImuPlugin>                reg_imu("imu");
static PluginRegistrar<JointVelPlugin>           reg_joint_vel("joint_vel");
static PluginRegistrar<ColorPlugin>              reg_color("color");
static PluginRegistrar<TrajectoryRecorderPlugin> reg_traj("trajectory_recorder");
static PluginRegistrar<PathDisplayPlugin>        reg_path("path_display");
static PluginRegistrar<TopicDisplayPlugin>       reg_topic("topic_display");
```

Регистратор при создании добавляет фабричную функцию в глобальный словарь.

#### create_plugin

```cpp
std::unique_ptr<IAgentPlugin> create_plugin(const std::string& type,
                                             const YAML::Node& node);
```

Ищет тип в реестре, создаёт экземпляр, вызывает `from_config(node)`. Если тип не найден — бросает исключение.

---

### Типы плагинов

| Тип | Назначение | Читает | Пишет | Статус |
|-----|------------|--------|-------|--------|
| **Actuator** | Управляет скоростью агента | `agent.state.effective()` | `agent.world_velocity` | ✓ |
| **Sensor** | Генерирует данные измерений | `agent.world_pose`, `world_velocity` | `agent.state.emplace<T>()` | ✓ |
| **Resource** | Ограничивает движение | `agent.world_pose`, `world_pose.pitch` и др. | `agent.state.add_scale/lock()` | [planned] |
| **Interaction** | Взаимодействует с Prop/Actor | `SimBus` события | `SimBus` события | [planned] |
| **Viz overlay** | Только визуализация | `agent.world_pose` | Только `to_json()` | ✓ |

---

## 4. Транспортный слой

### ITransportAdapter — интерфейс

Полный интерфейс в `s2_core/include/s2/transport_adapter.hpp`:

```cpp
class ITransportAdapter {
public:
    virtual ~ITransportAdapter() = default;

    // Жизненный цикл
    virtual void start() = 0;
    virtual void stop()  = 0;

    // Геодезическое начало координат (для GNSS)
    virtual void set_geo_origin(const GeoOrigin& origin) = 0;

    // Регистрация агента (создаёт TF base_link, odom и т.д.)
    virtual void register_agent(AgentId id, int domain_id,
                                const std::string& name,
                                const Pose3D& initial_pose) = 0;

    // Регистрация подписки на внешний топик
    virtual void register_subscription(const SubscriptionDesc& desc) = 0;

    // Регистрация входного топика для команд (cmd_vel и т.д.)
    virtual void register_input_topic(const InputTopicDesc& desc) = 0;

    // Регистрация ROS2-сервиса
    virtual void register_service(const ServiceDesc& desc) = 0;

    // Регистрация сенсора (создаёт publisher)
    virtual void register_sensor(const SensorRegistration& reg) = 0;

    // Регистрация статических TF (mount points, fixed joints)
    virtual void register_static_transforms(AgentId id, int domain_id,
                                            const std::vector<FrameTransform>& tfs) = 0;

    // Публикация состояния агента + данных сенсоров
    virtual void publish_agent_frame(const AgentSensorFrame& frame) = 0;

    // Публикация события (ArUco, trigger, и т.д.)
    virtual void emit_event(const TransportEvent& event) = 0;
};
```

#### Ключевые структуры данных

```cpp
// Регистрация входного топика
struct InputTopicDesc {
    std::string topic;        // "/cmd_vel"
    std::string plugin_type;  // "diff_drive"
    AgentId     agent_id;
    int         domain_id;
    std::function<void(const std::string& topic_msg_json)> callback;
};

// Регистрация сервиса
struct ServiceDesc {
    std::string service_name; // "/set_color"
    std::string plugin_type;
    AgentId     agent_id;
    int         domain_id;
    bool        is_trigger;   // true → std_srvs/Trigger, false → s2_msgs/PluginCall
    std::function<std::string(const std::string& req_json)> handler;
};

// Пакет данных одного агента за тик
struct AgentSensorFrame {
    AgentId                    agent_id;
    int                        domain_id;
    double                     sim_time;
    Pose3D                     world_pose;
    Velocity                   world_velocity;
    std::vector<SensorOutput>  sensors;            // только изменившиеся (по seq)
    std::vector<FrameTransform> dynamic_transforms; // суставы URDF
};

// Событие от плагина
struct TransportEvent {
    std::string topic;        // "/aruco/detected"
    std::string payload_json;
    AgentId     agent_id;
    int         domain_id;
};
```

---

### SimTransportBridge

Связующее звено между `SimEngine` и `ITransportAdapter`.

```cpp
// s2_transport/include/s2/sim_transport_bridge.hpp
class SimTransportBridge {
public:
    SimTransportBridge(SimEngine* engine,
                       std::shared_ptr<ITransportAdapter> adapter);

    // Обход агентов и регистрация всего в адаптере
    void init(const GeoOrigin& geo_origin);

    void start();
    void stop();

    // Вызывается из PostTickCallback
    void on_post_tick(const SimWorld& world, double sim_time);
};
```

#### init() — что происходит

```
bridge.init(geo_origin):
  adapter.set_geo_origin(geo_origin)
  for each agent in world:
    adapter.register_agent(agent.id, agent.domain_id, agent.name, agent.world_pose)
    for each plugin in agent.plugins:
      plugin.initialize(agent)
      // Регистрация сенсора (если тип известен)
      adapter.register_sensor({agent.id, agent.domain_id, plugin.type(), plugin.sensor_name(), plugin.output_topic()})
      // Входные топики
      for topic in plugin.command_topics():
          adapter.register_input_topic({topic, type, agent.id, domain_id, callback})
      // Сервисы
      for svc in plugin.service_names():
          adapter.register_service({svc, type, agent.id, domain_id, handler})
      // Подписки
      for topic in plugin.subscribe_topics():
          adapter.register_subscription({topic, ..., callback})
      // Статические TF
      adapter.register_static_transforms(agent.id, domain_id, [plugin.mount_frame()])
```

#### on_post_tick() — дедупликация по seq

```
on_post_tick(world, sim_time):
  for each agent:
    frame = AgentSensorFrame{agent.id, domain_id, sim_time, pose, velocity}
    for each plugin:
      // Читаем текущий seq из agent.state.get<SensorData>()
      // Сравниваем с сохранённым prev_seq[agent_id][plugin_type]
      if (current_seq != prev_seq):
          frame.sensors.push_back(sensor_output_from_plugin)
          prev_seq[...] = current_seq
      events = plugin.poll_events()
      for event in events: adapter.emit_event(event)
    frame.dynamic_transforms = kinematic_tree.collect_transforms()
    adapter.publish_agent_frame(frame)
```

Дедупликация по `seq` важна потому, что сенсор публикует данные на своей частоте (например, GNSS 10 Гц), а физический шаг — 100 Гц. Без проверки `seq` один и тот же пакет публиковался бы 10 раз.

---

### Ros2TransportAdapter

```
Архитектура:
  Один rclcpp::Context + rclcpp::Node + rclcpp::Executor на каждый domain_id

  Потоки:
    Главный поток     → publish_agent_frame()  → публикация в ROS2
    Поток домена 0    → rclcpp::spin_some()    → входящие команды (cmd_vel)
    Поток домена 50   → rclcpp::spin_some()    → команды для второго агента
    ...

  Publishers (создаются лениво при первом publish_agent_frame):
    /robot_0/odom            → nav_msgs/Odometry
    /robot_0/gnss/fix        → sensor_msgs/NavSatFix
    /robot_0/imu/data        → sensor_msgs/Imu
    /tf, /tf_static          → tf2_msgs/TFMessage

  Subscribers (создаются в register_input_topic):
    /cmd_vel                 → geometry_msgs/Twist
    /robot_0/cmd_vel_mount   → geometry_msgs/Twist (joint_vel)
```

---

### Как написать новый транспорт

1. Создать класс, унаследованный от `ITransportAdapter`:

```cpp
// my_transport/include/stub_transport.hpp
#include <s2/transport_adapter.hpp>
#include <iostream>

class StubTransportAdapter : public ITransportAdapter {
public:
    void start() override { std::cout << "[Stub] started\n"; }
    void stop()  override { std::cout << "[Stub] stopped\n"; }

    void set_geo_origin(const GeoOrigin& o) override {}

    void register_agent(AgentId id, int domain, const std::string& name,
                        const Pose3D&) override {
        std::cout << "[Stub] register agent " << name << " domain=" << domain << "\n";
    }

    void register_subscription(const SubscriptionDesc&)     override {}
    void register_input_topic(const InputTopicDesc&)         override {}
    void register_service(const ServiceDesc&)                override {}
    void register_sensor(const SensorRegistration&)          override {}
    void register_static_transforms(AgentId, int,
                                    const std::vector<FrameTransform>&) override {}

    void publish_agent_frame(const AgentSensorFrame& f) override {
        std::cout << "[Stub] agent " << f.agent_id
                  << " t=" << f.sim_time
                  << " x=" << f.world_pose.x
                  << " y=" << f.world_pose.y << "\n";
    }

    void emit_event(const TransportEvent& e) override {
        std::cout << "[Stub] event " << e.topic << " : " << e.payload_json << "\n";
    }
};
```

2. Реализовать все методы интерфейса (минимум: `start/stop`, `publish_agent_frame`).

3. Зарегистрировать в `main.cpp` по типу из `TransportConfig`:

```cpp
std::shared_ptr<ITransportAdapter> adapter;
if (scene.transport_config.type == "stub") {
    adapter = std::make_shared<StubTransportAdapter>();
} else if (scene.transport_config.type == "ros2") {
    adapter = std::make_shared<Ros2TransportAdapter>(scene.transport_config);
}
```

---

## 5. Визуализатор

### VizServer

HTTP + SSE сервер на C++ (без внешних зависимостей, встроен в `s2_visualizer`).

```
Эндпоинты:
  GET  /stream                → SSE поток снапшотов (Content-Type: text/event-stream)
  POST /command               → Управляющие команды от браузера
  GET  /                      → Статика (index.html, app.js, Three.js)
  POST /api/scene/geometry    → Обновить статическую геометрию (заменяет текущий список)
  POST /api/scene/save        → Сохранить текущую геометрию в YAML-файл сцены
```

Параметры:
- `port` — TCP-порт (по умолчанию `8080`)
- `static_path` — путь к `s2_visualizer/web/`
- `viz_rate` — частота отправки снапшотов (Гц)

SSE `/stream` отправляет одну строку `data: {...}\n\n` на каждый визуальный тик. Клиент использует `EventSource` для чтения потока.

`/command` принимает параметры в query string:
```
POST /command?cmd=pause
POST /command?cmd=resume
POST /command?cmd=reset
POST /command?cmd=move_agent&id=0&x=1.0&y=2.0&yaw=0.5
POST /command?cmd=plugin_input&agent_id=0&plugin=diff_drive&body={"linear_velocity":1.0}
```

`POST /api/scene/geometry` — тело запроса:
```json
{
  "geometry": [
    { "type": "box",
      "x": 3.0, "y": 0.0, "z": 0.5, "yaw": 0, "pitch": 0, "roll": 0,
      "sx": 2.0, "sy": 1.0, "sz": 1.0, "color": "#4444FF" },
    { "type": "cylinder",
      "x": -2.0, "y": 2.0, "z": 1.0,
      "radius": 0.5, "height": 2.0, "color": "#44FF44" }
  ]
}
```

`POST /api/scene/save` — тело не требуется. Ответ: `{"ok": true, "path": "/path/to/scene.yaml"}`.

---

### WorldSnapshot — формат данных

Полная структура JSON-снапшота, отправляемого через SSE:

```json
{
  "sim_time": 1.23,
  "paused": false,
  "agents": [
    {
      "id": 0,
      "name": "robot_0",
      "pose": {"x": 1.0, "y": 0.5, "z": 0.0, "yaw": 0.3},
      "velocity": {"vx": 1.0, "vy": 0.0, "vz": 0.0, "wz": 0.0},
      "visual": {"type": "box", "size": [0.8, 0.5, 0.3], "color": "#FF6B35"},
      "plugins_data": {
        "diff_drive": "{\"has_inputs\":true,\"linear_velocity\":1.0}",
        "gnss":       "{\"lat\":55.75,\"lon\":37.61,\"alt\":156.0}",
        "trajectory_recorder": "{\"type\":\"trajectory\",\"points\":[[0,0,0],[1,0,0]]}"
      },
      "frames": [
        {"name": "base_link", "pose": {"x":1.0,"y":0.5,"z":0,"yaw":0.3}},
        {"name": "arm",       "pose": {"x":1.2,"y":0.5,"z":0.3,"yaw":0.3}}
      ]
    }
  ],
  "props": [
    {
      "id": 1, "type": "barrel",
      "pose": {"x": 5.0, "y": 2.0, "z": 0},
      "visual": {"type": "cylinder", "radius": 0.3, "height": 0.8, "color": "#888888"}
    }
  ],
  "actors": [
    {
      "id": 0, "name": "door_1", "state": "open",
      "pose": {"x": 10, "y": 0, "z": 0, "yaw": 1.57},
      "visual": {"type": "box", "size": [0.1, 1.2, 2.0], "color": "#AAAAFF"}
    }
  ],
  "geometry": [
    {
      "type": "box", "color": "#888888",
      "x": 10, "y": 0, "z": 0, "yaw": 0, "pitch": 0, "roll": 0,
      "sx": 1, "sy": 5, "sz": 0.5
    }
  ]
}
```

Поле `plugins_data` содержит строки — результаты `plugin->to_json()` для каждого плагина. Формат `"type":"trajectory"` и `"type":"path"` обрабатывается браузером особым образом (линии).

---

### Three.js фронтенд

Файл `s2_visualizer/web/js/app.js`.

#### Как app.js разбирает снапшот

SSE подключение:
```javascript
const src = new EventSource('/stream');
src.onmessage = (e) => updateScene(JSON.parse(e.data));
```

`updateScene(data)` обрабатывает поля снапшота:
- `data.agents` — обновляет/создаёт меши агентов, обновляет боковую панель
- `data.props` — меши пропов
- `data.actors` — меши акторов
- `data.geometry` — статическая геометрия (присылается при первом подключении и после `POST /api/scene/geometry`); в режиме редактора не обновляется автоматически
- `data.plugins_data` — данные плагинов (аккордеон + overlay-линии траекторий)
- `data.plugin_inputs_schemas` — JSON-схемы входных данных плагинов (для генерации форм)

Координатная система: симулятор использует Z-up, Three.js — Y-up.
Преобразование: `position.set(x, z, -y)`, `rotation.set(roll, yaw, -pitch, 'YZX')`.

#### Именование мешей

| Префикс | Описание |
|---------|----------|
| `agent_N` | Агент с ID=N |
| `static_N` | Статический примитив с индексом N (в режиме симуляции) |
| `prop_N` | Проп |
| `actor_N` | Актор |
| `zone_N` | Зона (wireframe) |
| `lm_N_name` | URDF-звено агента N |

#### Боковая панель с данными плагинов

Для каждого агента браузер показывает аккордеон с плагинами. Плагины с `has_inputs: true` получают форму с полями согласно `inputs_schema()`. Кнопка "Send" запускает непрерывную отправку (20 Гц), "Stop" отправляет нулевые значения и останавливает интервал.

---

### Редактор сцены

Браузерный редактор позволяет изменять статическую геометрию без перезапуска симуляции.

**Включение:** кнопка "Edit Scene" в панели управления.

В режиме редактора:
- Кнопки "+ Box", "+ Cylinder", "+ Sphere" добавляют примитив в начало координат.
- Клик по примитиву выбирает его (TransformControls прикрепляется к мешу).
- Кнопки "Move" / "Rotate" / "Scale" переключают режим трансформации.
- Панель свойств справа показывает цвет и размеры выбранного примитива.
- "Применить" — отправляет текущее состояние на сервер (`POST /api/scene/geometry`), симулятор немедленно обновляет коллизионную геометрию.
- "Сохранить" — записывает геометрию в YAML-файл сцены (`POST /api/scene/save`), сохраняя остальные секции нетронутыми.

В режиме симуляции (не Editor) примитивы нельзя выбрать или переместить.

**SceneWriter** (`s2_core/include/s2/scene_writer.hpp`) — C++-класс, загружающий YAML, заменяющий секцию `s2.world.geometry` и записывающий файл обратно. Агенты, пропы и прочие секции не затрагиваются.

---

### Как написать свой клиент визуализации

1. Подключиться к SSE:
```javascript
const src = new EventSource('http://localhost:8080/stream');
src.onmessage = (e) => {
    const snap = JSON.parse(e.data);
    renderSnapshot(snap);
};
```

2. Парсить снапшоты и рендерить агентов по `pose` и `visual`.

3. Отображать данные плагинов из `plugins_data`.

4. Отправлять команды через POST `/command`.

5. Пример минимального HTML/JS клиента:

```html
<!DOCTYPE html>
<html>
<body>
<pre id="out"></pre>
<button onclick="sendCmd('pause')">Pause</button>
<button onclick="sendCmd('resume')">Resume</button>
<script>
const src = new EventSource('http://localhost:8080/stream');
src.onmessage = (e) => {
    const snap = JSON.parse(e.data);
    document.getElementById('out').textContent =
        't=' + snap.sim_time.toFixed(2) + '\n' +
        snap.agents.map(a =>
            a.name + ': x=' + a.pose.x.toFixed(2) + ' y=' + a.pose.y.toFixed(2)
        ).join('\n');
};

function sendCmd(cmd) {
    fetch('http://localhost:8080/command', {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify({cmd})
    });
}

// Отправить команду diff_drive:
function driveForward(agentId) {
    fetch('http://localhost:8080/command', {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify({
            cmd: 'plugin_input', agent_id: agentId,
            plugin: 'diff_drive',
            data: JSON.stringify({linear_velocity: 1.0, angular_velocity: 0.0})
        })
    });
}
</script>
</body>
</html>
```

---

## 6. YAML-конфигурация сцены — полный справочник

### Корневая структура

```yaml
s2:
  update_rate: 100       # [Гц] Частота физического шага. По умолч.: 100
  viz_rate: 30           # [Гц] Частота обновления браузера. По умолч.: 30
  transport_rate: 100    # [Гц] Частота публикации в транспорт. По умолч.: 100
  transport: ...         # Секция транспорта
  visualizer: ...        # Секция визуализатора
  world: ...             # Секция мира
  agents: [...]          # Список агентов
```

---

### Секция `transport`

```yaml
transport:
  type: ros2             # ros2 | stub. Обязательно.
  default_domain_id: 0   # ROS2 domain по умолчанию. По умолч.: 0
```

| Поле | Тип | Обязательно | По умолч. | Описание |
|------|-----|-------------|-----------|----------|
| `type` | string | да | — | Тип транспорта: `ros2` или `stub` |
| `default_domain_id` | int | нет | `0` | Domain ID для агентов без явного `domain_id` |

---

### Секция `visualizer`

```yaml
visualizer:
  enabled: true          # Запускать VizServer. По умолч.: true
  port: 8080             # TCP-порт. По умолч.: 8080
```

| Поле | Тип | Обязательно | По умолч. | Описание |
|------|-----|-------------|-----------|----------|
| `enabled` | bool | нет | `true` | Запускать ли браузерный визуализатор |
| `port` | int | нет | `8080` | HTTP-порт VizServer |

---

### Секция `world`

```yaml
world:
  surface: flat          # flat | {width: 100, height: 100, z: 0}
  geo_origin:
    lat: 55.7522         # Широта [градусы]. Обязательно для GNSS.
    lon: 37.6156         # Долгота [градусы]. Обязательно для GNSS.
    alt: 156.0           # Высота над уровнем моря [м]. По умолч.: 0.
  geometry: [...]        # Статическая геометрия (стены, колонны)
  props: [...]           # Пассивные объекты (ящики, бочки)
  actors: [...]          # Активные объекты с FSM (двери, лифты) [парсируются; FSM-обработка — planned]
  zones: [...]           # Зоны эффектов [парсируются; entry/exit-события и эффекты — planned]
```

#### Статическая геометрия (`geometry`)

```yaml
geometry:
  - type: box            # box | cylinder | sphere
    pose:
      x: 10.0            # [м]
      y: 0.0
      z: 0.0
      yaw: 0.0           # [рад] поворот вокруг Z (вверх)
      pitch: 0.0         # [рад] поворот вокруг Y (по умолч. 0)
      roll: 0.0          # [рад] поворот вокруг X (по умолч. 0)
    size:
      x: 1.0             # [м] ширина (только для box)
      y: 5.0             # [м] глубина (только для box)
      z: 0.5             # [м] высота (только для box)
    radius: 0.5          # [м] только для cylinder/sphere
    height: 2.0          # [м] только для cylinder
    color: "#888888"     # Hex-цвет. По умолч.: "#808080"
```

| Поле | Тип | Обязательно | Описание |
|------|-----|-------------|----------|
| `type` | string | да | `box`, `cylinder`, `sphere` |
| `pose.x/y/z` | float | нет | Позиция (по умолч. 0) |
| `pose.yaw` | float | нет | Поворот вокруг Z [рад] (по умолч. 0) |
| `pose.pitch` | float | нет | Поворот вокруг Y [рад] (по умолч. 0) |
| `pose.roll` | float | нет | Поворот вокруг X [рад] (по умолч. 0) |
| `size.x/y/z` | float | для box | Габариты [м] |
| `radius` | float | для cylinder/sphere | Радиус [м] |
| `height` | float | для cylinder | Высота [м] |
| `color` | string | нет | Hex-цвет (по умолч. `#808080`) |

#### Пропы (`props`) [парсируются; крепление к агентам — planned]

```yaml
props:
  - type: barrel         # Произвольная строка
    pose: {x: 5, y: 2, z: 0}
    movable: true        # Можно ли перемещать. По умолч.: false
    collision:
      bounding:
        type: sphere     # sphere | box | capsule | cylinder
        radius: 0.3
    visual:
      type: cylinder
      radius: 0.3
      height: 0.8
      color: "#AA4400"
```

#### Зоны (`zones`) [planned — парсируются, но runtime entry/exit и эффекты не реализованы]

```yaml
zones:
  - id: ice_zone         # Строковый ID. Обязательно.
    shape:
      type: SPHERE       # SPHERE | AABB | INFINITE
      center: {x: 5, y: 5, z: 0}
      radius: 3.0        # для SPHERE
      # half_size: {x: 5, y: 5, z: 1}  # для AABB
    effects:
      - type: scale
        value: 0.3
        source: ice_zone
```

---

### Секция `agents`

```yaml
agents:
  - name: robot_0           # Имя агента. Обязательно.
    id: 0                   # Числовой ID. По умолч.: порядковый номер.
    domain_id: 50           # ROS2 domain. По умолч.: default_domain_id.
    pose:
      x: 0.0                # Начальная позиция X [м]
      y: 0.0
      z: 0.0
      yaw: 0.0              # Начальный курс [рад]
    collision:
      bounding:
        type: sphere        # sphere | box | capsule | cylinder
        radius: 0.4         # [м]
    visual:
      type: box             # box | sphere | cylinder | mesh
      size: [0.8, 0.5, 0.3] # [длина, ширина, высота] для box
      color: "#FF6B35"
    urdf: robot.urdf        # Путь к URDF (опционально, из s2_config/)
    plugins:
      - ...                 # Список плагинов (см. ниже)
```

| Поле | Тип | Обязательно | По умолч. | Описание |
|------|-----|-------------|-----------|----------|
| `name` | string | да | — | Уникальное имя агента |
| `id` | uint | нет | авто | Числовой ID |
| `domain_id` | int | нет | `default_domain_id` | ROS2 domain для изоляции |
| `pose.x/y/z/yaw` | float | нет | `0` | Начальная поза |
| `collision.bounding.type` | string | нет | — | `sphere`, `box`, `capsule` |
| `collision.bounding.radius` | float | для sphere | — | Радиус сферы коллизии |
| `visual.type` | string | нет | `box` | Тип визуального представления |
| `visual.size` | list[3] | для box | `[1,1,1]` | `[длина, ширина, высота]` |
| `visual.color` | string | нет | `"#AAAAAA"` | Hex-цвет |
| `urdf` | string | нет | — | URDF-файл для KinematicTree |
| `plugins` | list | нет | `[]` | Список плагинов |

---

### Конфигурация плагинов

#### diff_drive

```yaml
- type: diff_drive
  max_linear: 2.0     # [м/с] Макс. линейная скорость. По умолч.: 1.0
  max_angular: 1.5    # [рад/с] Макс. угловая скорость. По умолч.: 1.0
```

#### gnss

```yaml
- type: gnss
  noise_std: 0.3          # [м] Стандартное отклонение шума. По умолч.: 0.3
  publish_rate_hz: 10     # [Гц] Частота публикации. По умолч.: 10
  name: front_gps         # Имя экземпляра (для нескольких GNSS)
  mount: {x: 0.1, y: 0, z: 0.3}   # Смещение монтирования [м]
```

#### imu

```yaml
- type: imu
  publish_rate_hz: 100    # [Гц] Частота публикации. По умолч.: 100
```

#### joint_vel

```yaml
- type: joint_vel
  topic: /cmd_vel_mount   # ROS2-топик входящих Twist. По умолч.: /cmd_vel
  joints:
    - name: arm           # Имя звена в KinematicTree
      axis: linear_x      # linear_x/y/z или angular_x/y/z
      max_vel: 0.01       # [рад/с или м/с]
    - name: bucket
      axis: angular_z
      max_vel: 0.01
```

#### trajectory_recorder

```yaml
- type: trajectory_recorder
  record_interval_s: 0.5  # [с] Интервал записи точек. По умолч.: 1.0
  max_points: 200         # Макс. точек в буфере. По умолч.: 200
  color: "#FFAA00"        # Цвет линии. По умолч.: "#FFAA00"
```

#### path_display

```yaml
- type: path_display
  topic: /global_plan     # ROS2-топик nav_msgs/Path. По умолч.: /plan
  max_points: 500         # Макс. точек. По умолч.: 500
  color: "#00FF88"        # Цвет линии. По умолч.: "#00FF88"
```

#### color

```yaml
- type: color
  service: /set_color     # Имя ROS2-сервиса. По умолч.: /set_color
  color: "#FF0000"        # Цвет при активации. Обязательно.
  duration: 5.0           # [с] Длительность подсветки. 0 = навсегда.
```

---

### Полный пример конфигурации

```yaml
s2:
  update_rate: 100
  viz_rate: 30
  transport_rate: 100

  transport:
    type: ros2
    default_domain_id: 0

  visualizer:
    enabled: true
    port: 8080

  world:
    surface: flat
    geo_origin:
      lat: 55.7522
      lon: 37.6156
      alt: 156.0
    geometry:
      - type: box
        pose: {x: 10, y: 0, z: 0, yaw: 0}
        size: {x: 1, y: 5, z: 2}
        color: "#888888"

  agents:
    - name: robot_0
      id: 0
      domain_id: 50
      pose: {x: 0, y: 0, z: 0, yaw: 0}
      collision:
        bounding: {type: sphere, radius: 0.4}
      visual:
        type: box
        size: [0.8, 0.5, 0.3]
        color: "#FF6B35"
      plugins:
        - type: diff_drive
          max_linear: 2.0
          max_angular: 1.5
        - type: gnss
          noise_std: 0.3
          publish_rate_hz: 10
          name: main_gps
        - type: imu
          publish_rate_hz: 100
        - type: trajectory_recorder
          record_interval_s: 0.5
          max_points: 200
          color: "#FFAA00"

    - name: robot_1
      id: 1
      domain_id: 51
      pose: {x: 3, y: 0, z: 0, yaw: 3.14}
      visual:
        type: box
        size: [0.8, 0.5, 0.3]
        color: "#3567FF"
      plugins:
        - type: diff_drive
          max_linear: 1.5
          max_angular: 1.0
        - type: gnss
          noise_std: 0.1
```
