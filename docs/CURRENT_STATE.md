# S2 — Полная документация текущего состояния

> Документ описывает только то, что **сейчас реализовано** в кодовой базе репозитория `s2-sim` (ветка `stable`, состояние на 2026-05-04). Никаких будущих фич, никаких «планов», никаких «как должно работать» — только реальный код с цитатами и пояснениями.

Содержание:

1. [Карта репозитория](#1-карта-репозитория)
2. [Сборка, Docker и запуск](#2-сборка-docker-и-запуск)
3. [Базовые типы (`s2_core/types.hpp`)](#3-базовые-типы-s2_coretypeshpp)
4. [Сущности мира: Agent / Actor / Prop / Zone / WorldPrimitive](#4-сущности-мира)
5. [SharedState — contributions и resolver](#5-sharedstate)
6. [SimWorld — контейнер сцены](#6-simworld)
7. [SimEngine — главный тиковый цикл](#7-simengine)
8. [SimBus — типизированная шина событий](#8-simbus)
9. [CollisionSystem — коллизии и опорная поверхность](#9-collisionsystem)
10. [RaycastEngine — лучи по статике + динамические агенты](#10-raycastengine)
11. [KinematicTree и URDF-загрузчик](#11-kinematictree-и-urdf-загрузчик)
12. [ZoneSystem и эффекты](#12-zonesystem-и-эффекты)
13. [Реестр эффектов и реализованные эффекты](#13-реестр-эффектов-и-реализованные-эффекты)
14. [Плагины агентов (`IAgentPlugin`)](#14-плагины-агентов-iagentplugin)
15. [Реестр плагинов и список конкретных плагинов](#15-реестр-плагинов-и-список-конкретных-плагинов)
16. [Транспортный слой (`ITransportAdapter`)](#16-транспортный-слой-itransportadapter)
17. [`SimTransportBridge` — мост между движком и адаптером](#17-simtransportbridge)
18. [`Ros2TransportAdapter` — реализация поверх ROS2 Jazzy](#18-ros2transportadapter)
19. [Старый MVP `ROS2Transport` (только `/cmd_vel`)](#19-старый-mvp-ros2transport)
20. [`s2_msgs` — кастомный ROS2 сервис](#20-s2_msgs)
21. [`SceneLoader` и `SceneWriter`](#21-sceneloader-и-scenewriter)
22. [WorldSnapshot и сериализация в JSON](#22-worldsnapshot)
23. [TripleBuffer](#23-triplebuffer)
24. [`VizServer` — HTTP/SSE сервер визуализатора](#24-vizserver)
25. [Веб-фронтенд (`web/index.html`, `web/js/app.js`)](#25-веб-фронтенд)
26. [`main.cpp` — стартап и связывание всех слоёв](#26-maincpp)
27. [Сцены в `s2_config/scenes/`](#27-сцены)
28. [URDF (`dozer.urdf`)](#28-urdf-dozerurdf)
29. [Тесты (что уже существует)](#29-тесты)
30. [Сводная таблица фаз тика](#30-сводная-таблица-фаз-тика)

---

## 1. Карта репозитория

```
s2-sim/
├── CLAUDE.md                       # методологический контракт (rus)
├── docker/
│   ├── Dockerfile                  # ubuntu:22.04 без ROS2 (dev / build)
│   ├── Dockerfile.ros2             # ubuntu:noble + ROS2 Jazzy (sim/tests)
│   ├── docker-compose.yml          # сервисы: dev, build, tests, sim, sim_ros2
│   └── fastdds.xml                 # UDP-only профиль FastDDS
├── docs/                           # старая документация по задачам
└── workspace/
    ├── CMakeLists.txt              # корневой CMake
    ├── s2_core/                    # ядро (types, world, engine, collisions, zones)
    ├── s2_plugins/                 # реестры плагинов и эффектов + конкретные реализации
    ├── s2_transport/               # ITransportAdapter + ROS2 реализация (+stub)
    ├── s2_visualizer/              # бинарник s2_sim + HTTP/SSE viz сервер + web/
    ├── s2_msgs/                    # ROS2 сервис PluginCall.srv
    ├── s2_msgs_ws/                 # colcon-workspace для сборки s2_msgs
    └── s2_config/
        ├── robots/dozer.urdf
        └── scenes/test_*.yaml      # 11 готовых сцен
```

Корневой CMake (`workspace/CMakeLists.txt`):

```cmake
cmake_minimum_required(VERSION 3.16)
project(s2 VERSION 0.1.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
find_package(Eigen3 3.3 REQUIRED NO_MODULE)
find_package(yaml-cpp REQUIRED)
find_package(nlohmann_json 3.0 REQUIRED)
find_package(GTest REQUIRED)
find_package(OpenSSL REQUIRED)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "/usr/share/cmake/geographiclib")
find_package(GeographicLib REQUIRED)
...
add_subdirectory(s2_core)
add_subdirectory(s2_plugins)
add_subdirectory(s2_visualizer)
add_subdirectory(s2_transport)
```

GeographicLib подключён вручную, потому что его `Find`-скрипт не создаёт imported target и имя библиотеки разное на 22.04 (`Geographic`) и 24.04 (`GeographicLib`). Зависимости ядра жёсткие: `Eigen3`, `yaml-cpp`, `nlohmann_json`, `GTest`, `OpenSSL`, `GeographicLib`. Дополнительно `s2_core` подтягивает `tinyxml2` (для URDF-парсера) — сначала пробует CMake-config, потом `find_library`-fallback.

`s2_transport` управляется опцией `S2_WITH_ROS2` (по умолчанию `OFF`). Если выключено — собираются stub-файлы и весь транспорт превращается в no-op. Если включено — линкуются `rclcpp`, `geometry_msgs`, `sensor_msgs`, `nav_msgs`, `std_srvs`, `std_msgs`, `tf2_ros` и (опционально) `s2_msgs`. При наличии `s2_msgs` определяется `S2_WITH_S2_MSGS`, что переключает не-trigger сервисы с `std_srvs/Trigger` на `s2_msgs/PluginCall`.

`s2_visualizer/CMakeLists.txt` собирает единственный исполняемый файл `s2_sim`:

```cmake
add_executable(s2_sim
    src/viz_server.cpp
    src/main.cpp
    src/sim_engine_viz_impl.cpp)
target_link_libraries(s2_sim PRIVATE
    s2_core s2_plugins s2_transport
    nlohmann_json::nlohmann_json OpenSSL::SSL OpenSSL::Crypto pthread)
```

Тесты лежат в `s2_core/tests/`. Их два набора (`s2_core_tests` и `s2_editor_tests` для `SceneWriter`).

---

## 2. Сборка, Docker и запуск

### 2.1. Образы

`docker/Dockerfile` (Ubuntu 22.04, без ROS2) ставит:

```
build-essential cmake git libeigen3-dev libyaml-cpp-dev nlohmann-json3-dev
libgtest-dev libgmock-dev zlib1g-dev libssl-dev pkg-config
libgeographic-dev geographiclib-tools libtinyxml2-dev
```

И собирает uWebSockets из исходников:

```bash
git clone --recurse-submodules https://github.com/uNetworking/uWebSockets.git /tmp/uws
cp -r /tmp/uws/src/* /usr/local/include/
cp -r /tmp/uws/uSockets/src/* /usr/local/include/
cd /tmp/uws/uSockets && make && cp uSockets.a /usr/local/lib/
```

> Важно: фактический `viz_server.cpp` использует POSIX-сокеты напрямую и **не линкует** uWebSockets. Установка остаётся — это исторический след.

`docker/Dockerfile.ros2` (Ubuntu Noble + ROS2 Jazzy) — multi-stage. Тот же базовый набор пакетов плюс репозиторий ROS2 и пакеты:

```
ros-jazzy-ros-base ros-jazzy-ros2-control ros-jazzy-geometry-msgs
ros-jazzy-sensor-msgs ros-jazzy-nav-msgs ros-jazzy-std-msgs ros-jazzy-std-srvs
ros-jazzy-tf2 ros-jazzy-tf2-ros ros-jazzy-tf2-geometry-msgs
ros-jazzy-fastcdr ros-jazzy-fastrtps
python3-rosdep python3-colcon-common-extensions
```

`/opt/ros/jazzy/setup.bash` подгружается в `.bashrc`, переменная `ROS_DISTRO=jazzy`.

### 2.2. `docker-compose.yml`

Определены пять сервисов:

| Сервис      | Образ                  | Назначение                                                          |
|-------------|------------------------|---------------------------------------------------------------------|
| `dev`       | `Dockerfile`           | Интерактивный bash, проброс `1937:1937`                             |
| `build`     | `Dockerfile.ros2`      | Полная сборка с `S2_WITH_ROS2=ON` (включая colcon-сборку `s2_msgs`) |
| `tests`     | `Dockerfile.ros2`      | Сборка + `ctest --output-on-failure`                                |
| `sim`       | `Dockerfile.ros2`      | Запуск `s2_sim` со сценой `test_two_robots.yaml`                    |
| `sim_ros2`  | `Dockerfile.ros2`      | `network_mode: host` + `fastdds.xml`, сцена `test_zones.yaml`       |

Команда сборки внутри `build` (типичная для всех сервисов):

```bash
source /opt/ros/jazzy/setup.bash \
  && mkdir -p /workspace/s2_msgs_ws/src \
  && cp -r /workspace/s2_msgs /workspace/s2_msgs_ws/src/ \
  && cd /workspace/s2_msgs_ws && colcon build --symlink-install \
  && source /workspace/s2_msgs_ws/install/setup.bash \
  && cd /workspace && mkdir -p build && cd build \
  && cmake .. -DCMAKE_BUILD_TYPE=Debug -DS2_WITH_ROS2=ON \
  && make -j$(nproc)
```

`fastdds.xml` явно отключает SHM-транспорт:

```xml
<transport_descriptor>
  <transport_id>UdpTransport</transport_id>
  <type>UDPv4</type>
  <maxInitialPeersRange>400</maxInitialPeersRange>
</transport_descriptor>
<participant profile_name="udp_transport_profile" is_default_profile="true">
  <rtps>
    <userTransports><transport_id>UdpTransport</transport_id></userTransports>
    <useBuiltinTransports>false</useBuiltinTransports>
  </rtps>
</participant>
```

Эти файлы монтируются в контейнер как `/root/.ros/fastdds.xml`, переменная `FASTRTPS_DEFAULT_PROFILES_FILE` указывает на него.

### 2.3. Стартовая сцена

`sim` запускает:

```bash
./s2_visualizer/s2_sim /workspace/s2_config/scenes/test_two_robots.yaml
```

Если в `s2_visualizer/src/main.cpp` запустить `s2_sim` без аргументов — fallback на `/workspace/s2_config/scenes/test_basic.yaml`:

```cpp
std::string scene_path = "/workspace/s2_config/scenes/test_basic.yaml";
if (argc > 1) {
    scene_path = argv[1];
}
```

После запуска веб-визуализатор слушает `http://localhost:1937`.

---

## 3. Базовые типы (`s2_core/types.hpp`)

В этом файле заложен стиль всего ядра. Все 3D-векторы — `Eigen::Vector3d`, ориентации — углы Эйлера ZYX (yaw → pitch → roll). ID агентов и акторов — `uint32_t`, ID зон — `std::string`.

```cpp
using AgentId  = uint32_t;
using ActorId  = uint32_t;
using ObjectId = uint32_t;
using ZoneId   = std::string;
using EntityId = uint32_t;

using Vec3 = Eigen::Vector3d;

struct Pose3D {
  double x{0}, y{0}, z{0};
  double roll{0}, pitch{0}, yaw{0};
  Vec3 position() const { return Vec3(x, y, z); }
  bool operator==(const Pose3D& other) const { /* покомпонентно с допуском 1e-9 */ }
};

struct Velocity {
  Vec3 linear{Vec3::Zero()};
  Vec3 angular{Vec3::Zero()};
};

struct Transform3D {
  Vec3 translation{Vec3::Zero()};
  Eigen::Matrix3d rotation{Eigen::Matrix3d::Identity()};
  Vec3 transform_point(const Vec3& p) const { return rotation * p + translation; }
  Vec3 inverse_transform_point(const Vec3& p) const {
      return rotation.transpose() * (p - translation);
  }
};
```

Коллизионный шейп — sum-type через `enum class ShapeType { SPHERE, BOX, CAPSULE, CYLINDER }` плюс общий `CollisionShape { type, radius, height, size }`. Однако реально на «горячем пути» (CollisionSystem, GravityPlugin, build_snapshot) используются только `SPHERE` и `BOX`; `CAPSULE`/`CYLINDER` присутствуют в `enum`, но не имеют поддержки.

Визуальное описание:

```cpp
struct VisualDesc {
  std::string type{"box"};       // "box" | "cylinder" | "sphere" | "capsule"
  Vec3 size{1.0, 1.0, 1.0};
  double radius{0.5};
  double height{1.0};
  std::string color{"#FF6B35"};
};
```

«Желаемая скорость» отделена от «текущей» — это выход модулей актуации:

```cpp
struct DesiredVelocity {
  Vec3 linear{Vec3::Zero()};
  Vec3 angular{Vec3::Zero()};
  bool valid{true};
};
```

Зональные формы:

```cpp
enum class ZoneShapeType { SPHERE, AABB, CYLINDER, INFINITE };

struct ZoneShape {
  ZoneShapeType type{ZoneShapeType::SPHERE};
  Vec3 center{Vec3::Zero()};
  double radius{1.0};            // SPHERE и CYLINDER
  Vec3 half_size{1.0,1.0,1.0};   // AABB
  double half_height{1.0};       // CYLINDER

  bool contains(const Vec3& point) const {
    switch (type) {
      case ZoneShapeType::SPHERE:   /* squaredNorm <= r² */
      case ZoneShapeType::AABB:     /* |dx|<=hx && |dy|<=hy && |dz|<=hz */
      case ZoneShapeType::CYLINDER: /* |dz|<=h && dx²+dy² <= r² */
      case ZoneShapeType::INFINITE: return true;
    }
  }
};
```

Тип эффекта:

```cpp
enum class EffectType { MODIFIER, CONTINUOUS, MUTATION, SENSOR };
```

`SENSOR` объявлен, но реальной точки вызова `sensor_mods()` в текущем `ZoneSystem::apply_active_effects()` нет — соответствующий `case` помечен как «вне этого метода».

Состояние актора — просто строка (`using ActorState = std::string`).

---

## 4. Сущности мира

### 4.1. Agent (`agent.hpp`)

```cpp
struct Agent {
  AgentId id{0};
  std::string name;
  int domain_id{0};

  Pose3D world_pose;
  Velocity world_velocity;

  SharedState state;

  std::unordered_set<std::string> capabilities;

  std::vector<std::unique_ptr<plugins::IAgentPlugin>> plugins;

  bool   has_collision{false};
  double max_slope_rad{0.0};       // 0 = только горизонталь
  double max_step_height{0.0};     // переезд ступенек
  CollisionShape bounding;
  VisualDesc     visual;

  std::unique_ptr<KinematicTree> kinematic_tree;  // nullptr = одиночное тело
};
```

`Agent` — `move-only` (внутри `unique_ptr` на плагины и kinematic_tree). `world_velocity.linear` хранится **в локальных координатах корпуса** — это явно используется в `SimEngine::tick()` (см. §7).

### 4.2. Actor (`actor.hpp`)

```cpp
struct Actor {
  ActorId id{0};
  std::string name;
  Pose3D world_pose;
  ActorState current_state;
  CollisionShape collision;
  VisualDesc visual;
  // FSM добавится в задаче 07
};
```

FSM-логика на уровне акторов **не реализована**. В `SimEngine::tick()` фаза 1 «Akторы» — пустая.

### 4.3. Prop (`prop.hpp`)

```cpp
struct Prop {
  ObjectId id{0};
  std::string type;
  Pose3D world_pose;
  bool movable{false};
  CollisionShape collision;
  VisualDesc visual;
  std::unordered_map<std::string, std::string> properties;
};
```

Пропы имеют коллизионные/визуальные данные, но в текущем тике с ними никаких операций не делается — они только сериализуются в снапшот.

### 4.4. Zone (`zone.hpp`)

`Zone` — move-only, потому что эффекты внутри хранят `unique_ptr<EffectPlugin>`:

```cpp
struct Zone {
  ZoneId id;
  bool   enabled{true};
  ZoneShape shape;
  std::string detection_mode{"center"};   // "bounding" пока fallback на center

  std::optional<ActorId>  attached_to_actor;
  std::optional<AgentId>  attached_to_agent;
  Vec3 attachment_offset{Vec3::Zero()};

  std::string color{"#4488FF"};
  double opacity{0.3};
  bool visible{true};
  std::string label;

  struct EffectDesc {
    std::string type;
    bool enabled{true};
    EffectType effect_type{EffectType::MODIFIER};
    std::vector<std::string> required_capabilities;
    YAML::Node params;
    std::unique_ptr<EffectPlugin> plugin;
    // copy = delete, move = default
  };

  std::vector<EffectDesc> effects;
  std::unordered_set<AgentId> inside_agents;  // те, кто внутри *сейчас*
};
```

`detection_mode == "bounding"` упомянут, но `ZoneSystem::detection_point()` всегда возвращает `agent.world_pose.position()`.

### 4.5. WorldPrimitive (`world.hpp`)

```cpp
struct WorldPrimitive {
  std::string type;              // "box" | "cylinder" | "sphere"
  Pose3D pose;
  Vec3 size{1,1,1};              // для box
  double radius{0.5};
  double height{1.0};
  std::string color{"#808080"};
};
```

Это «статика»: стены, пол, рампы, цилиндры в сцене. С точки зрения CollisionSystem и RaycastEngine — единственный известный тип геометрии мира.

---

## 5. SharedState

`SharedState` (`shared_state.hpp`) — хранилище, которое объединяет три механизма:

1. **Single-owner** (typed storage через `std::any`). Любой плагин может зарезервировать тип `T` и получить к нему изменяемый доступ. Используется для `BatteryComponent`, `TirePunctureData`, `GnssData`, `ImuData`, `DiffDriveData`, `LidarScanData`, `BatteryData`.
2. **Contributions** — три списка вкладов, которые модули заполняют каждый тик до `resolve()`:
   ```cpp
   struct ScaleContribution    { double value{1.0}; std::string source; };
   struct AdditiveContribution { Vec3 value{Vec3::Zero()}; std::string source; };
   struct LockContribution     { bool locked{false}; std::string source; };
   ```
3. **Effective / Resolved** — итог `resolve()`, который читают `update()` модули:
   ```cpp
   struct EffectiveConstraints {
     double speed_scale{1.0};
     bool   motion_locked{false};
     Vec3   velocity_addition{Vec3::Zero()};
     std::vector<ScaleContribution>     scale_sources;
     std::vector<LockContribution>      lock_sources;
     std::vector<AdditiveContribution>  additive_sources;
   };
   ```

API:

```cpp
template <typename T, typename... Args>  T& emplace(Args&&...);
template <typename T>                    T*       get();
template <typename T>                    const T* get() const;
template <typename T>                    bool     has() const;

void add_scale(double, const std::string& src);
void add_velocity_addition(const Vec3&, const std::string& src);
void add_lock(bool, const std::string& src);

void resolve();              // вычислить effective
void clear_contributions();  // очистить три списка (effective не трогать)
const EffectiveConstraints& effective() const;
```

Алгоритм `resolve()`:

```cpp
double scale = 1.0;
for (const auto& c : scale_contribs_) scale *= c.value;
effective_.speed_scale = std::clamp(scale, 0.0, 10.0);

bool locked = false;
for (const auto& c : lock_contribs_) locked = locked || c.locked;
effective_.motion_locked = locked;

Vec3 additive = Vec3::Zero();
for (const auto& c : additive_contribs_) additive += c.value;
effective_.velocity_addition = additive;

effective_.scale_sources    = scale_contribs_;
effective_.lock_sources     = lock_contribs_;
effective_.additive_sources = additive_contribs_;
```

`speed_scale` — произведение со clamp `[0; 10]`. `motion_locked` — логический OR. `velocity_addition` — сумма векторов. После `resolve()` `clear_contributions()` сбрасывает только списки, но **не** `effective_` — это важно: между тиками снапшот должен видеть консистентные итоговые значения, а не нули.

---

## 6. SimWorld

`SimWorld` (`world.hpp`) — контейнер сущностей: вектор `agents_`, `props_`, `actors_`, `zones_`, `static_geometry_` плюс одна `Heightmap` (по умолчанию плоская 40×40). Доступ через `agents()`, `props()`, `actors()`, `static_geometry()`, `zones()` и `get_*(id)`.

`SimWorld::check_sphere_collision()` — отдельный метод, проверяющий пересечение сферы с любым статическим примитивом упрощённо (AABB-sphere для box, sphere-sphere для sphere/cylinder). В горячем пути `SimEngine` он **не используется** — там работает более точный `CollisionSystem`. Метод оставлен как утилита.

Heightmap (`heightmap.hpp`) поддерживает только `flat`-режим:

```cpp
static Heightmap flat(double width, double height, double z = 0.0);
double height_at(double x, double y) const;   // is_flat → surface_z
Vec3   normal_at(double x, double y) const;   // is_flat → (0,0,1)
bool   in_bounds(double x, double y) const;
```

Внутри есть готовый код билинейной интерполяции (`interpolate()`), но он работает только если `is_flat_=false` и `data_` непустое — сейчас этот путь нигде не активируется. По факту heightmap почти не влияет на симуляцию: пол строится отдельным `box`-примитивом в YAML.

`GeoOrigin` (`geo_origin.hpp`):

```cpp
struct GeoOrigin {
  double lat{0.0};
  double lon{0.0};
  double alt{0.0};
  bool is_set() const { return lat != 0 || lon != 0 || alt != 0; }
};
```

Используется `GnssPlugin` и `Ros2TransportAdapter` (см. §16, §18).

---

## 7. SimEngine

Главный класс — `SimEngine` (`sim_engine.hpp`). Конфигурация:

```cpp
struct Config {
  double update_rate{100.0};     // Гц симуляции
  double viz_rate{30.0};         // Гц снапшотов в визуализатор
  double transport_rate{30.0};   // Гц вызовов post_tick_callback
};
```

`dt_ = 1.0 / update_rate`. Три таймера (`viz_timer_`, `transport_timer_`) накапливаются и срабатывают по достижении интервала.

### 7.1. Жизненный цикл

```cpp
void load_world(SimWorld world) {
    world_ = std::move(world);
    save_initial_states();                       // запоминает поза/скорость каждого агента
    collision_system_.set_static_geometry(world_.static_geometry());
    raycast_engine_.set_static_geometry(world_.static_geometry());
    zone_system_ = ZoneSystem{};
    if (effect_factory_) zone_system_.set_effect_factory(effect_factory_);
    for (auto& zone : world_.zones())
        zone_system_.add_zone(std::move(zone));
    world_.zones().clear();   // зоны переехали в ZoneSystem
}
```

`update_static_geometry()` используется при редактировании сцены в рантайме — заменяет геометрию у `SimWorld`, `CollisionSystem` и `RaycastEngine`, не трогая агентов.

`set_effect_factory(EffectFactory)` нужно вызвать **до** `load_world`. Сама фабрика — `std::function<unique_ptr<EffectPlugin>(string, YAML::Node)>` — задаётся в `main.cpp` как `s2::create_effect`.

`run()` крутит while-loop с `std::this_thread::sleep_for` до `1/update_rate`. `step(n)` дёргает `tick()` n раз — это для тестов. `pause()`, `resume()`, `is_paused()` управляют флагом `paused_`. `reset()` восстанавливает из `initial_states_` и ставит `paused = true`.

`set_agent_pose(id, pose)` напрямую переписывает `world_pose` — используется UI для drag-перетаскивания робота.

### 7.2. Один тик (`SimEngine::tick`)

Если `paused_=true`, тик не двигает время и сущности, но всё равно публикует viz-снапшот по таймеру. Это нужно, чтобы фронт видел `paused: true`.

При обычном тике:

1. **Фаза 1 (актеры)** — пусто.
2. **Фаза 2 — `zone_system_.tick(agents, actors, bus, sim_time, dt)`.** Зоны перемещают свои центры (если `attached_to_*`), регистрируют входы/выходы, применяют MODIFIER/CONTINUOUS-эффекты.
3. **Фаза 3 — для каждого агента:**
   - **3a. `pre_resolve(dt, agent)`** для всех плагинов — добавляют contributions (например, `BatteryPlugin` разряжает и публикует `add_scale`/`add_lock`).
   - **3d. `agent.state.resolve()`** — собирает effective из contributions и зональных эффектов.
   - **3e. update-фаза.** Перед update в `RaycastEngine` обновляются «динамические агенты»:
     ```cpp
     std::vector<WorldPrimitive> agent_bounds;
     for (const auto& other : world_.agents()) {
       if (&other == &agent) continue;
       if (!other.has_collision) continue;
       WorldPrimitive wp;
       wp.pose = other.world_pose;
       if (other.bounding.type == ShapeType::SPHERE) {
         wp.type   = "sphere"; wp.radius = other.bounding.radius;
       } else {
         wp.type = "box"; wp.size = other.bounding.size * 2.0;
       }
       agent_bounds.push_back(wp);
     }
     raycast_engine_.set_dynamic_agents(agent_bounds);
     ```
     Затем для каждого плагина:
     ```cpp
     plugin->set_collision_system(&collision_system_);
     plugin->set_raycast_engine(&raycast_engine_);
     plugin->update(dt_, agent);
     ```
     Каждый плагин сам выбирает, нужен ли ему `CollisionSystem` или `RaycastEngine` (по умолчанию методы — no-op).
   - **3f. Кинематика.** `world_velocity.linear` хранится в **body-frame**. Его поворот в мировой:
     ```cpp
     Eigen::Matrix3d R = CollisionSystem::rotation_from_pose(agent.world_pose);
     Vec3 body_vel{vx, vy, 0.0};
     Vec3 world_vel = R * body_vel;
     const Vec3& additive = agent.state.effective().velocity_addition;
     agent.world_pose.x += (world_vel.x() + additive.x()) * dt_;
     agent.world_pose.y += (world_vel.y() + additive.y()) * dt_;
     agent.world_pose.z += (agent.world_velocity.linear.z() + additive.z()) * dt_;
     agent.world_pose.yaw += wz * dt_;
     // нормализация yaw в [0, 2π)
     ```
     Z-движение управляется отдельно: `GravityPlugin` правит `world_pose.z` напрямую и обнуляет `world_velocity.linear.z()`, остальные источники Z-смещения — только зональный `additive.z()`.
   - **3h. Коллизии.** Только для `has_collision && bounding.type == SPHERE`. Получаем все контакты:
     ```cpp
     auto contacts = collision_system_.check_sphere_all(pos, agent.bounding.radius);
     ```
     Список отсортирован по убыванию `penetration`. По каждому контакту:
     - `walkable = contact.contact_normal.z >= cos(max_slope_rad)`.
     - Если non-walkable, но верхняя грань препятствия выше нижней точки агента не более чем на `max_step_height` — игнорируем (агент переезжает ступеньку).
     - Walkable: только Z push-out. Формула:
       ```cpp
       if (contact.contact_normal.z() > 1e-4)
         agent.world_pose.z += contact.penetration / contact.contact_normal.z();
       ```
       (комментарий объясняет, почему `p / nz` точнее `nz * p` на склоне).
     - Non-walkable: горизонтальный slide+push-out. Считаем `normal_h = (nx, ny, 0)`, проектируем `velocity` и убираем компоненту, направленную в стену; затем сдвигаем по `(nx, ny)` на `penetration`.

     После прохода всех контактов — выравнивание агента по нормали поверхности через `find_support_surface()`:
     ```cpp
     auto support = collision_system_.find_support_surface(pos, radius);
     if (support && agent.world_pose.z <= support->ground_z + radius + 0.05) {
       const Vec3& n = support->normal;
       const double yaw = agent.world_pose.yaw;
       const double nx_b =  cos(yaw)*n.x() + sin(yaw)*n.y();
       const double ny_b = -sin(yaw)*n.x() + cos(yaw)*n.y();
       agent.world_pose.pitch = atan2( nx_b, n.z());
       agent.world_pose.roll  = atan2(-ny_b, n.z());
     } else {
       agent.world_pose.roll = agent.world_pose.pitch = 0.0;
     }
     ```
     Комментарий поясняет, почему `find_support_surface()`, а не `check_sphere_all()` — после того как `GravityPlugin` снапает Z, контактов нет, и без отдельного raycast агент бы мерцал между «на рампе» и «горизонт».
   - **3i–3l (Joints, KinematicTree, Sensors, Interactions)** — пусто.
   - В конце тика: `agent.state.clear_contributions()`.
4. **Фаза 4 (attachments)** — пусто.
5. **Фаза 5 — viz publish:** `viz_timer_ += dt_`, при достижении `1/viz_rate` вызывается `publish_viz()`. Реализация в `s2_visualizer/src/sim_engine_viz_impl.cpp`:
   ```cpp
   void SimEngine::publish_viz() {
       if (!viz_server_) return;
       viz_server_->publish(build_snapshot());
   }
   ```
   В `s2_core/src/sim_engine_viz.cpp` лежит stub-реализация на случай линковки без визуализатора.
6. **Фаза 6 — transport publish:** аналогично, при `transport_timer_ >= 1/transport_rate - 1e-9` вызывается `post_tick_cb_(world, sim_time)`. Эпсилон нужен из-за накопления IEEE 754 (см. техконтекст).

### 7.3. Снапшот (`build_snapshot`)

`build_snapshot()` собирает `WorldSnapshot`. Для каждого агента:

```cpp
AgentSnapshot as;
as.id = agent.id; as.name = agent.name;
as.pose = agent.world_pose; as.velocity = agent.world_velocity;
as.velocity_addition       = agent.state.effective().velocity_addition;
as.visual                  = agent.visual;
as.effective_speed_scale   = agent.state.effective().speed_scale;
as.motion_locked           = agent.state.effective().motion_locked;
for (const auto& plugin : agent.plugins)
    plugin->contribute_snapshot(as.extra, agent);
const auto* tire = agent.state.get<TirePunctureData>();
if (tire) as.tire_punctured = tire->punctured;
// bounding
if (agent.has_collision) {
    if (agent.bounding.type == ShapeType::SPHERE) {
        as.bounding_type = "sphere"; as.bounding_radius = agent.bounding.radius;
    } else {
        as.bounding_type = "box";
        as.bounding_size = agent.bounding.size * 2.0;
    }
}
// все звенья kinematic_tree → kinematic_frames
```

Зоны попадают в снапшот напрямую из `zone_system_.all_zones()`. Геометрия копируется из `static_geometry()`. Также собираются `plugins_data` (`agent_id → plugin_key → JSON-string`) и `plugin_inputs_schemas`.

### 7.4. Управление вводом плагинов

Точка входа для любого транспорта:

```cpp
bool handle_plugin_input(AgentId agent_id, const std::string& plugin_type, const std::string& json_input) {
  for (auto& agent : world_.agents()) {
    if (agent.id == agent_id) {
      for (auto& plugin : agent.plugins) {
        if (plugin->type() == plugin_type || plugin_key(*plugin) == plugin_type) {
          plugin->handle_input(json_input);
          return true;
        }
      }
      return false;
    }
  }
  return false;
}
```

`plugin_key` строится так:

```cpp
static std::string plugin_key(const plugins::IAgentPlugin& p) {
  return p.sensor_name().empty() ? p.type() : p.type() + "_" + p.sensor_name();
}
```

Это позволяет одному агенту иметь два плагина одного типа (например, `lidar` с `sensor_name="front_lidar"` и `lidar` с `sensor_name="rear_lidar"`) — UI обращается по полному ключу.

`get_plugin_inputs_schemas(agent_id)` собирает Schemas всех плагинов агента в JSON и возвращает строкой.

---

## 8. SimBus

`SimBus` (`sim_bus.hpp`) — типизированная синхронная шина событий через `std::any` + `std::type_index`. Подписка и публикация:

```cpp
template <typename EventT>
void subscribe(std::function<void(const EventT&)> handler) {
  auto wrapper = [handler](const std::any& e) {
    handler(std::any_cast<const EventT&>(e));
  };
  handlers_[typeid(EventT)].push_back(std::move(wrapper));
}

template <typename EventT>
void publish(const EventT& event) {
  auto it = handlers_.find(typeid(EventT));
  if (it == handlers_.end()) return;
  for (const auto& h : it->second) h(event);
}
```

Зарегистрированные стандартные события:

```cpp
namespace event {
  struct AgentEnteredZone { AgentId agent; ZoneId zone; };
  struct AgentExitedZone  { AgentId agent; ZoneId zone; };
  struct ObjectAttached   { ObjectId obj; AgentId agent; std::string link; };
  struct ObjectReleased   { ObjectId obj; AgentId agent; };
  struct ActorStateChanged{ ActorId actor; ActorState old_state; ActorState new_state; };
  struct AgentCollision   { AgentId agent; Vec3 point; };
}
```

Реально публикуются только `AgentEnteredZone` / `AgentExitedZone` (из `ZoneSystem::on_agent_*`). Остальные структуры лежат как заготовка — никаких подписчиков на `ObjectAttached`, `ActorStateChanged`, `AgentCollision` в текущем коде нет.

---

## 9. CollisionSystem

`CollisionSystem` (`collision_system.hpp`, реализация inline) обслуживает фазу 3h `SimEngine` и `GravityPlugin`. Хранит копию `static_geometry`:

```cpp
void set_static_geometry(const std::vector<WorldPrimitive>& prims);
```

Возвращаемые типы:

```cpp
struct CollisionContact {
    bool   has_contact{false};
    Vec3   contact_normal{0,0,1};   // от поверхности к центру сферы
    double penetration{0.0};
    double obstacle_top_z{0.0};
};
struct SupportInfo { double ground_z; Vec3 normal; };
struct RayHit       { double z; Vec3 normal; };
```

### 9.1. `check_sphere_all(center, radius)`

Перебирает все примитивы, для каждого считает:
- `box`: переводит центр сферы в локальную систему (через ZYX-ротацию `pose`), считает ближайшую точку на AABB, дальше стандартный sphere-vs-AABB. Если центр сферы внутри box, нормаль выводится по оси минимального перекрытия.
- `sphere`: стандартное sphere-sphere пересечение.
- `cylinder`: переводит центр в локальную систему цилиндра (ось Z локальная — продольная), считает radial+axis distance, ближайшую точку на боковой/торцевой поверхности.

Каждый контакт получает `obstacle_top_z` — для box это **локальная** верхняя грань, спроецированная в мир (так агент корректно заезжает на наклонную рампу снизу); для sphere/cylinder — глобальный максимум (`primitive_top_z`).

После сбора контакты сортируются по убыванию penetration:
```cpp
std::sort(contacts.begin(), contacts.end(),
    [](const auto& a, const auto& b){ return a.penetration > b.penetration; });
```

### 9.2. `apply_slide(vel, normal)` (статический helper)

```cpp
double proj = vel.linear.dot(normal);
if (proj < 0.0) result.linear -= normal * proj;   // убираем компоненту в стену
return result;
```

### 9.3. `find_support_surface(position, radius)`

Бросает луч `(0,0,-1)` из точки `position - (0,0, radius - 0.01)` (чуть выше нижней точки сферы) и берёт ближайшее попадание с `z` в пределах 2 м ниже origin. Под капотом — три приватных раздельных raycast: `ray_down_vs_box`, `ray_down_vs_cylinder`, `ray_down_vs_sphere` (slab-метод для box, торцы+боковая для цилиндра, квадратное уравнение для сферы). Возвращает `{ground_z, normal}` — ground_z это Z самой поверхности, не центра агента.

`rotation_from_pose(Pose3D)` — публичный helper, строит матрицу `Rz(yaw) * Ry(pitch) * Rx(roll)`. Используется и в `SimEngine::tick` (body-frame → world).

---

## 10. RaycastEngine

`RaycastEngine` (`raycast_engine.hpp`) — отдельный движок лучей, у него два набора примитивов:

```cpp
void set_static_geometry(const std::vector<WorldPrimitive>& prims);
void set_dynamic_agents (const std::vector<WorldPrimitive>& agent_bounds);
```

`static_prims_` задаётся раз при `load_world()`/`update_static_geometry()`, `dynamic_prims_` — каждый тик в `SimEngine::tick` перед update-фазой.

API:

```cpp
struct Ray          { Vec3 origin; Vec3 direction; double max_range{30.0}; };
struct RaycastResult{ bool hit{false}; double distance{0}; Vec3 point; Vec3 normal{0,0,1}; };

RaycastResult cast(const Ray& ray) const;
std::vector<RaycastResult> cast_batch(const std::vector<Ray>& rays) const;
```

`cast()` brute-force перебирает все примитивы (статику + динамику), вызывает `intersect_box`, `intersect_sphere` или `intersect_cylinder`. Для box и cylinder делается полный OBB через `build_rotation_transpose()` (трансформация луча в локальное пространство примитива и slab-тест), для sphere — стандартное квадратное уравнение. Учитываются повороты вокруг всех трёх осей. Точка попадания и нормаль приближённые: нормаль box/cylinder = `-direction` (упрощение), sphere — точная.

Используется только `LidarPlugin`. Всех агентов с `has_collision` собирает SimEngine и передаёт в виде «динамики» — текущий агент исключается, чтобы лидар не «видел сам себя».

---

## 11. KinematicTree и URDF-загрузчик

### 11.1. KinematicTree (`kinematic_tree.hpp` + `.cpp`)

Узлы — `Link` и `Joint`:

```cpp
enum class JointType { FIXED, REVOLUTE, PRISMATIC, CONTINUOUS };

struct Joint {
  JointType type{JointType::FIXED};
  Vec3   axis{0,0,1};
  double value{0.0};
  double min{-M_PI};
  double max{ M_PI};
  bool is_static() const { return type == JointType::FIXED; }
};

struct LinkVisual {
  std::string type;                              // "" = нет геометрии
  double sx{1}, sy{1}, sz{1};                    // box
  double radius{0.5}, length{1.0};               // cylinder/sphere
  std::string color{"#888888"};
  Pose3D origin;
};

struct Link {
  std::string name;
  std::string parent;     // "" для корня
  Pose3D origin;          // оffset+rpy относительно родителя при value=0
  Joint  joint;
  LinkVisual visual;
};
```

`KinematicTree`:

```cpp
void add_link(Link);
const std::vector<Link>& links() const;
void set_joint_value(const std::string& link, double v);   // clamp по joint.min/max
void set_link_color(const std::string& link, const std::string& color);

Pose3D compute_world_pose(const std::string& link, const Pose3D& base) const;
Pose3D compute_local_pose(const std::string& link) const;

void collect_transforms(std::vector<KinematicFrameTransform>& static_out,
                        std::vector<KinematicFrameTransform>& dynamic_out) const;
```

Реализация через `Eigen::Isometry3d`. Для одного звена:
```cpp
T_local = T_origin * T_joint
```
где `T_joint` — identity для FIXED, поворот `AngleAxisd(value, axis)` для REVOLUTE/CONTINUOUS, трансляция `axis * value` для PRISMATIC.

`compute_world_pose` собирает цепочку link → ... → root и применяет трансформы в обратном порядке от base. `isometry_to_pose` нормализует углы Эйлера в `[-π, π]`, чтобы избежать прыжков.

`collect_transforms` строит два массива `KinematicFrameTransform { parent_frame, child_frame, relative_pose, is_static }` — один для FIXED-джоинтов (статические TF), другой для всего остального (публикуется каждый кадр).

### 11.2. URDF-загрузчик (`urdf_loader.hpp` + `.cpp`)

Использует `tinyxml2`. Алгоритм `load_urdf(path, root_frame="base_link")`:

1. Парсит все `<joint>` в карту `child_link → JointDef{type, parent, child, origin, axis, limit_lower, limit_upper}`. Поддерживаются `fixed`, `revolute`, `continuous`, `prismatic`.
2. Парсит все `<link>` в карту `name → LinkVisual` (поддерживаются `<box>`, `<cylinder>`, `<sphere>` плюс `<material><color rgba>`; mesh пропускается).
3. Строит обратную карту `parent → [children]`.
4. Стартует BFS от `root_frame`. Корень добавляется как FIXED-link с пустым parent. Звенья выше root_frame (типа `base_footprint`) **игнорируются**.

```cpp
KinematicTree tree;
{
    Link root_link;
    root_link.name = root_frame; root_link.parent = "";
    root_link.joint.type = JointType::FIXED;
    auto vit = link_visuals.find(root_frame);
    if (vit != link_visuals.end()) root_link.visual = vit->second;
    tree.add_link(std::move(root_link));
}
std::queue<std::string> bfs_queue;
bfs_queue.push(root_frame);
while (!bfs_queue.empty()) {
    auto current = bfs_queue.front(); bfs_queue.pop();
    auto it = children_of.find(current);
    if (it == children_of.end()) continue;
    for (const auto& child_name : it->second) {
        const JointDef& jdef = joints_by_child.at(child_name);
        Link lk;
        lk.name = child_name; lk.parent = current; lk.origin = jdef.origin;
        if      (jdef.type == "revolute")   lk.joint.type = JointType::REVOLUTE;
        else if (jdef.type == "continuous") lk.joint.type = JointType::CONTINUOUS;
        else if (jdef.type == "prismatic")  lk.joint.type = JointType::PRISMATIC;
        else                                lk.joint.type = JointType::FIXED;
        lk.joint.axis = jdef.axis;
        lk.joint.min  = jdef.limit_lower; lk.joint.max = jdef.limit_upper;
        lk.joint.value = 0.0;
        // visual
        tree.add_link(std::move(lk));
        bfs_queue.push(child_name);
    }
}
```

`load_urdf_collision(path, root_frame)` отдельно достаёт `<link name=root_frame><collision><geometry>` (sphere/box/cylinder) и возвращает `CollisionShape`. Для box `size` интерпретируется как полный размер и делится на 2 (half-extents). Используется `SceneLoader`, чтобы URDF имел приоритет над YAML `collision:`.

---

## 12. ZoneSystem и эффекты

### 12.1. `EffectPlugin` (`interfaces/effect_plugin.hpp`)

Базовый интерфейс эффекта зоны:

```cpp
class EffectPlugin {
public:
  virtual ~EffectPlugin() = default;
  virtual void on_init(const YAML::Node& params) = 0;
  virtual EffectType effect_type() const = 0;
  virtual std::vector<std::string> required_capabilities() const { return {}; }

  virtual void apply_modifier  (SharedState&, const EffectContext&) {}
  virtual void apply_continuous(SharedState&, const EffectContext&) {}
  virtual void apply_mutation  (SharedState&, const EffectContext&) {}
  virtual void on_agent_exit   (SharedState&, const EffectContext&) {}

  struct SensorMod  { std::string param; double multiplier{1.0}; double addend{0.0}; };
  virtual std::vector<SensorMod> sensor_mods(const EffectContext&) const { return {}; }

  struct VisualHint { std::string type; nlohmann::json params; };
  virtual std::optional<VisualHint> visual_hint() const { return std::nullopt; }
};
```

`EffectContext` (`effect_context.hpp`) содержит копии нужных значений (без указателей):

```cpp
struct EffectContext {
    double sim_time{0.0}, dt{0.0};
    ZoneId zone_id;
    Vec3 zone_center{Vec3::Zero()};
    Vec3 zone_half_size{Vec3::Zero()};
    AgentId agent_id{0};
    Vec3 agent_position{Vec3::Zero()};
};
```

`sensor_mods()` и `visual_hint()` пока не используются движком (визхинты — задел для UI, sensor_mods — задел для SENSOR-эффектов).

### 12.2. `ZoneSystem` (`zone_system.hpp` + `.cpp`)

```cpp
using EffectFactory = std::function<std::unique_ptr<EffectPlugin>(const std::string&, const YAML::Node&)>;

class ZoneSystem {
public:
  void set_effect_factory(EffectFactory);
  void add_zone(Zone);
  void tick(std::vector<Agent>&, const std::vector<Actor>&, SimBus&, double sim_time, double dt);

  bool resize_zone(const ZoneId&, const ZoneShape&);
  bool move_zone  (const ZoneId&, const Vec3& center);
  bool attach_zone_to_actor(const ZoneId&, ActorId, const Vec3& offset);
  bool toggle_zone(const ZoneId&, bool enabled);
  bool toggle_effect(const ZoneId&, size_t effect_idx, bool enabled);

  std::vector<ZoneId> zones_containing(const Vec3& point) const;
  const std::vector<Zone>& all_zones() const;
};
```

`add_zone()` — для каждого `EffectDesc` без plugin'а вызывает фабрику, затем `on_init(params)`. `effect_type` всегда берётся из плагина (`desc.plugin->effect_type()`). `required_capabilities` ведёт себя так: если YAML задал список — он остаётся, иначе берётся дефолт плагина (это и есть «приоритет YAML над плагином»):

```cpp
if (!desc.plugin) {
    desc.plugin = effect_factory_(desc.type, desc.params);
    if (desc.plugin) {
        desc.plugin->on_init(desc.params);
        desc.effect_type = desc.plugin->effect_type();
        if (desc.required_capabilities.empty()) {
            desc.required_capabilities = desc.plugin->required_capabilities();
        }
    }
}
```

`tick()` идёт в три шага:

1. **Обновить позиции attached-зон.** Если `attached_to_actor`/`attached_to_agent` — `zone.shape.center = entity.world_pose.position() + attachment_offset`.
2. **Enter/Exit для каждой зоны.** Для каждого агента сравнивается `is_inside = zone.shape.contains(detection_point(agent, mode))` с `was_inside = zone.inside_agents.count(agent.id) > 0`.
   - Enter → `inside_agents.insert(agent.id)`, `on_agent_enter()`. Внутри:
     - `bus.publish(event::AgentEnteredZone{...})`.
     - Для каждого MUTATION-эффекта (с подходящими capabilities) вызывается `desc.plugin->apply_mutation(agent.state, ctx)`.
   - Exit → `inside_agents.erase(agent.id)`, `on_agent_exit()`. Внутри:
     - `bus.publish(event::AgentExitedZone{...})`.
     - Для каждого эффекта вызывается `desc.plugin->on_agent_exit(state, ctx)`.
   - Если зона выключена (`!zone.enabled`) — все агенты в `inside_agents` форс-выходят.
3. **Активные эффекты:** для каждой включённой зоны и каждого агента внутри — `apply_active_effects()`:
   ```cpp
   switch (desc.effect_type) {
     case MODIFIER:   desc.plugin->apply_modifier(state, ctx);   break;
     case CONTINUOUS: desc.plugin->apply_continuous(state, ctx); break;
     case MUTATION:   /* применяется только при входе */          break;
     case SENSOR:     /* запрашивается отдельно (TODO в коде) */   break;
   }
   ```

`detection_point()` всегда возвращает `agent.world_pose.position()` (даже при `mode == "bounding"` — fallback). `capabilities_match()`: пустой список → подходит всем; иначе все capabilities должны быть у агента.

---

## 13. Реестр эффектов и реализованные эффекты

`s2_plugins/src/effects_registry.cpp`:

```cpp
std::unique_ptr<EffectPlugin> create_effect(const std::string& type, const YAML::Node& params) {
    std::unique_ptr<EffectPlugin> plugin;
    if      (type == "ice_modifier") plugin = std::make_unique<effects::IceModifier>();
    else if (type == "boost_zone")   plugin = std::make_unique<effects::BoostZone>();
    else if (type == "motion_lock")  plugin = std::make_unique<effects::MotionLockZone>();
    else if (type == "conveyor")     plugin = std::make_unique<effects::ConveyorEffect>();
    else if (type == "wind")         plugin = std::make_unique<effects::WindEffect>();
    else if (type == "charging")      plugin = std::make_unique<effects::ChargingEffect>();
    else if (type == "tire_puncture") plugin = std::make_unique<effects::TirePunctureEffect>();
    if (plugin) plugin->on_init(params);
    return plugin;
}
```

Семь типов:

| YAML-тип       | Класс                | EffectType  | Capabilities  | Что делает |
|----------------|----------------------|-------------|---------------|------------|
| `ice_modifier` | `IceModifier`        | MODIFIER    | `surface_contact` | `add_scale(traction)`; опционально шум `sin(time*7.3 + agent_id*1.7)` |
| `boost_zone`   | `BoostZone`          | MODIFIER    | `surface_contact` | `add_scale(speed_multiplier)` |
| `motion_lock`  | `MotionLockZone`     | MODIFIER    | (none)        | `add_lock(true, source_label_+zone_id)` |
| `conveyor`     | `ConveyorEffect`     | MODIFIER    | `surface_contact` | `add_velocity_addition(direction.normalized()*speed)` |
| `wind`         | `WindEffect`         | MODIFIER    | (none)        | `add_velocity_addition(wind_vector + normalize*sin(2π*sim_time*freq)*amp)` |
| `charging`     | `ChargingEffect`     | CONTINUOUS  | `has_battery` | `BatteryComponent.level += charge_rate*dt`, `charging=true`; на выход `charging=false` |
| `tire_puncture`| `TirePunctureEffect` | MUTATION    | `wheeled`     | `TirePunctureData.punctured = true`; на выход ничего |

Примеры реализации:

```cpp
// IceModifier
void apply_modifier(SharedState& state, const EffectContext& ctx) override {
    double scale = traction_coeff_;
    if (noise_amplitude_ > 0.0) {
        double noise = noise_amplitude_ * std::sin(ctx.sim_time*7.3 + ctx.agent_id*1.7);
        scale = std::clamp(traction_coeff_ + noise, 0.01, 1.0);
    }
    state.add_scale(scale, "ice_zone_" + ctx.zone_id);
}
```

```cpp
// ChargingEffect
void apply_continuous(SharedState& state, const EffectContext& ctx) override {
    auto* bat = state.get<BatteryComponent>();
    if (!bat) bat = &state.emplace<BatteryComponent>();
    bat->level    = std::min(1.0, bat->level + charge_rate_ * ctx.dt);
    bat->charging = true;
}
void on_agent_exit(SharedState& state, const EffectContext&) override {
    if (auto* bat = state.get<BatteryComponent>()) bat->charging = false;
}
```

```cpp
// TirePunctureEffect
void apply_mutation(SharedState& state, const EffectContext&) override {
    auto* data = state.get<TirePunctureData>();
    if (!data) data = &state.emplace<TirePunctureData>();
    data->punctured = true;
}
```

Каждый эффект также возвращает `visual_hint()` — структуру вида `{type, params}`, которая передаётся в JSON, но в текущем фронтенде она не отрисовывается.

---

## 14. Плагины агентов (`IAgentPlugin`)

Базовый интерфейс лежит в `s2_core/include/s2/plugin_base.hpp`. Это чистый виртуальный класс с большим числом опциональных хуков:

```cpp
class IAgentPlugin {
public:
    virtual ~IAgentPlugin() = default;

    // Имя экземпляра ("front", "rear", "")
    const std::string& sensor_name() const;
    void set_sensor_name(const std::string&);

    // Частота публикации
    virtual double publish_rate_hz() const {
        return base_rate_hz_ > 0.0 ? base_rate_hz_ : default_publish_rate_hz();
    }
    virtual double default_publish_rate_hz() const { return 0.0; }
    void set_base_rate(double hz);

    // Топик
    const std::string& output_topic() const;
    void set_output_topic(const std::string&);

    // Идентификация
    virtual std::string type() const = 0;

    // Хуки тика
    virtual void initialize(Agent&) {}
    virtual void pre_resolve(double, Agent&) {}
    virtual void set_collision_system(const CollisionSystem*) {}
    virtual void set_raycast_engine  (const RaycastEngine*)   {}
    virtual std::string sensor_frame_id() const { return ""; }
    virtual void update(double dt, Agent& agent) = 0;

    // Конфиг/сериализация
    virtual void from_config(const YAML::Node&) = 0;
    virtual std::string to_json() const = 0;
    virtual void contribute_snapshot(nlohmann::json&, const Agent&) const {}

    // UI-схема
    virtual std::string display_label() const { return type(); }
    virtual std::string config_schema() const { return "[]"; }

    // Внешние команды
    virtual bool        has_inputs() const { return false; }
    virtual std::string inputs_schema() const { return ""; }
    virtual void        handle_input(const std::string&) {}

    // Транспорт
    virtual std::vector<std::string> command_topics() const { return {}; }
    virtual std::vector<std::string> service_names()  const { return {}; }
    virtual std::string handle_service(const std::string& name, const std::string& req) { ... }
    virtual std::vector<TransportEvent> poll_events() { return {}; }

    // Внешние подписки
    virtual std::vector<std::string> subscribe_topics() const { return {}; }
    virtual std::string subscription_msg_type() const { return "nav_msgs/Path"; }
    virtual void handle_subscription(const std::string&, const std::string&) {}

    // Точка монтажа
    void set_mount_pose(const Pose3D&);
    std::optional<FrameTransform> mount_frame() const;

private:
    std::string sensor_name_;
    Pose3D      mount_pose_;
    std::string output_topic_;
    double      base_rate_hz_{0.0};
};
```

`mount_frame()` возвращает `FrameTransform{ "base_link", child, mount_pose_ }`, где `child` — `<sensor_name>_<type>_link` или `<type>_link` если name пустой. Используется `SimTransportBridge` для регистрации статических TF.

`PluginFactoryFn` — `std::function<std::unique_ptr<IAgentPlugin>(string, YAML::Node)>`.

В `s2_plugins/include/s2/plugins/plugin_base.hpp` объявлены две функции:

```cpp
std::unique_ptr<IAgentPlugin> create_plugin(const std::string& type, const YAML::Node& node);
std::string list_plugin_schemas();   // для UI редактора
```

---

## 15. Реестр плагинов и список конкретных плагинов

Реестр (`s2_plugins/src/plugins_registry.cpp`):

```cpp
static const PluginRegistrar register_battery("battery", []{ return std::make_unique<BatteryPlugin>(); });
static const PluginRegistrar register_color("color", []{ return std::make_unique<ColorPlugin>(); });
static const PluginRegistrar register_diff_drive("diff_drive", []{ return std::make_unique<DiffDrivePlugin>(); });
static const PluginRegistrar register_gravity("gravity", []{ return std::make_unique<GravityPlugin>(); });
static const PluginRegistrar register_gnss("gnss", []{ return std::make_unique<GnssPlugin>(); });
static const PluginRegistrar register_imu("imu", []{ return std::make_unique<ImuPlugin>(); });
static const PluginRegistrar register_joint_vel("joint_vel", []{ return std::make_unique<JointVelPlugin>(); });
static const PluginRegistrar register_trajectory_recorder("trajectory_recorder", []{ return std::make_unique<TrajectoryRecorderPlugin>(); });
static const PluginRegistrar register_path_display("path_display", []{ return std::make_unique<PathDisplayPlugin>(); });
static const PluginRegistrar register_topic_display("topic_display", []{ return std::make_unique<TopicDisplayPlugin>(); });
static const PluginRegistrar register_lidar("lidar", []{ return std::make_unique<LidarPlugin>(); });
```

`create_plugin(type, node)` — берёт фабрику по типу, вызывает её и затем `plugin->from_config(node)`. Возвращает `nullptr` при неизвестном типе.

`list_plugin_schemas()` строит JSON-массив `[{type, label, params}]` в фиксированном порядке: `diff_drive, gnss, imu, lidar, battery, trajectory_recorder, path_display, topic_display, joint_vel, color`. Используется UI-редактором сцены.

Ниже разбор каждого плагина. У каждого описана конфигурация, что плагин делает в тиках и какие данные публикует.

### 15.1. `DiffDrivePlugin` (`diff_drive.hpp`)

Конфиг:
```yaml
- type: diff_drive
  wheel_base: 0.4         # читается, но не используется в логике
  max_linear: 2.0         # совместимо с max_linear_vel
  max_angular: 1.5        # совместимо с max_angular_vel
```

Тип `"diff_drive"`. Принимает команды по `/cmd_vel`:

```cpp
std::vector<std::string> command_topics() const override { return {"/cmd_vel"}; }
std::string inputs_schema() const override {
    return R"({
        "linear_velocity": {"type":"number","default":0,"min":-2,"max":2,"unit":"m/s"},
        "angular_velocity":{"type":"number","default":0,"min":-1.5,"max":1.5,"unit":"rad/s"}
    })";
}
```

`handle_input` валидирует JSON через yaml-cpp и сохраняет команду как latch (`has_external_input_=true`):

```cpp
external_linear_velocity_  = std::clamp(data["linear_velocity"].as<double>(),  -max_linear_,  max_linear_);
external_angular_velocity_ = std::clamp(data["angular_velocity"].as<double>(), -max_angular_, max_angular_);
has_external_input_ = true;
```

`update(dt, agent)` каждый тик:

1. Читает `DiffDriveData` из SharedState (если уже опубликован).
2. Если есть external input — берёт его (latch, флаг **не сбрасывается**), иначе — `desired_*` из SharedState с clamp.
3. Если `effective().motion_locked` → агент мгновенно останавливается, `world_velocity = 0`.
4. Если `TirePunctureData.punctured == true`:
   ```cpp
   clamped_linear *= 0.5;
   time_acc_ += dt;
   clamped_angular += 0.05 * std::sin(time_acc_ * 15.0);
   ```
5. Применяет `effective().speed_scale` (умножение и повторный clamp по аппаратному лимиту).
6. Записывает `world_velocity.linear = (clamped_linear, 0, 0)` и `angular = (0, 0, clamped_angular)`. Это body-frame.
7. В SharedState кладёт `DiffDriveData{seq, desired_*, max_linear, max_angular}`. Important: при external input `desired_*` **не перезаписывается** (иначе внешний цикл `update→read desired→repeat` зациклит latch).

### 15.2. `GnssPlugin` (`gnss.hpp`)

```yaml
- type: gnss
  noise_std: 0.5
  publish_rate_hz: 10
```

`set_geo_origin(GeoOrigin)` создаёт `GeographicLib::LocalCartesian` с (lat, lon, alt). `update`:
- Управляет своим `publish_timer_`. При интервале `1/rate` (10 Гц по умолчанию) пропускает остальные тики.
- Берёт `agent.world_pose.x/y/z` (в локальных координатах) и через `converter_.Reverse(y, x, z, lat, lon, alt)` получает LLA. Заметьте: **первый аргумент `y`, второй `x`** — это East/North/Up convention.
- Накладывает гауссовский шум: `noise_std/111320.0` для широты, `noise_std/(111320*cos(lat))` для долготы, `noise_std` для высоты. RNG — `std::mt19937{42}`.
- Публикует `GnssData{seq, lat, lon, alt, azimuth, accuracy}` в SharedState.
- `azimuth = fmod(yaw, 2π)` нормализованный в `[0, 2π)`.
- `to_json()` отдаёт текущие значения для UI-аккордеона.

### 15.3. `ImuPlugin` (`imu.hpp`)

```yaml
- type: imu
  publish_rate_hz: 100
```

Простой: каждый тик с интервалом `1/rate` записывает `ImuData{gyro_x/y/z, accel_z=9.81, yaw}`. Гироскоп = `agent.world_velocity.angular`, акселерация только по Z (статика). Шума нет.

### 15.4. `LidarPlugin` (`lidar.hpp`)

```yaml
- type: lidar
  name: front_lidar       # sensor_name
  num_rays: 360
  min_range: 0.1
  max_range: 10.0
  start_angle: -3.14159
  end_angle:   3.14159
  mount_link: ""          # имя линка URDF, на котором смонтирован
  viz_color: "#00FFFF"
  publish_rate_hz: 10
```

`update`:
1. Если `RaycastEngine` не инжектирован — выходит.
2. По таймеру `publish_timer_`.
3. Определяет монтажную позу: `agent.world_pose`, либо `kinematic_tree->compute_world_pose(mount_link_, agent.world_pose)`.
4. Строит `num_rays_` лучей в горизонтальной (по yaw/pitch/roll) плоскости через ZYX-ротацию: первые два столбца R умножаются на `cos(a), sin(a)`, чтобы получить мировое направление.
5. `cast_batch(rays)` — `RaycastEngine` возвращает массив `RaycastResult`.
6. Заполняет `LidarScanData{seq, angle_min, angle_max, angle_increment, time_increment=0, scan_time=1/rate, range_min, range_max, ranges[N]}`. Если попадание есть и `distance >= min_range` — записывает дистанцию, иначе `max_range`.
7. Если `visible_=true`, накапливает точки попадания в `scan_points_` для UI-визуализации.
8. `to_json()` отдаёт `{type:"lidar_points", visible, color, points:[[x,y,z], ...]}`.

`has_inputs()=true`, schema — единственное поле `visible` (boolean) — кнопка/чекбокс в UI.

`sensor_frame_id()` возвращает `mount_link_` или `"base_link"`.

### 15.5. `BatteryPlugin` (`battery.hpp`)

```yaml
- type: battery
  initial_level: 1.0
  nominal_voltage: 24.0
  capacity_ah: 10.0
  design_capacity_ah: 10.0
  technology: lion        # nimh|lion|lipo|life|nicd|limn
  location: ""
  serial_number: ""
  drain_rate: 0.01        # уровень/с
  low_level: 0.20         # порог замедления
  critical_level: 0.05    # порог блокировки
  publish_rate_hz: 1.0
```

`initialize(agent)` создаёт `BatteryComponent{initial_level, false}` если его ещё нет.

`pre_resolve(dt, agent)`:
- Если `!charging`: `level = max(0, level - drain_rate*dt)`.
- Если `level <= 0.05` → `add_lock(true, "battery_critical")`.
- Если `0.05 < level < 0.20` → `add_scale((level-0.05)/0.15, "battery_low")`.
- `level >= 0.20` — без contributions.

`update(dt, agent)`:
- Читает `BatteryComponent`, кеширует в локальные `cached_level_`, `cached_charging_`.
- По таймеру публикует `BatteryData{seq, level, charging, nominal_voltage, capacity_ah, design_capacity_ah, technology, location, serial_number}` в SharedState.

`contribute_snapshot(out, agent)`:
```cpp
out["battery_level"]    = bat ? bat->level : -1.0;
out["battery_charging"] = bat ? bat->charging : false;
```

### 15.6. `GravityPlugin` (`gravity.hpp`)

```yaml
- type: gravity
  gravity_accel: 9.81
  max_fall_speed: 20.0
  grounded_epsilon: 0.02
  friction_coef: 0.0       # 0 = лёд, 1 = полное сцепление
```

Не сенсор — это «Resource»-плагин, влияющий на движение. `set_collision_system()` сохраняет указатель.

`update(dt, agent)`:

1. `support = collision_->find_support_surface(pos, radius)`.
2. `ground_z = support ? support->ground_z + radius : -inf`.
3. `grounded = support && pose.z <= ground_z + epsilon`.
4. **Grounded-ветка:**
   - `fall_velocity_ = 0`, `pose.z = ground_z`.
   - Считает тангенциальную составляющую гравитации вдоль склона:
     ```cpp
     Vec3 g_tang = g_vec - g_vec.dot(n) * n;   // компонента вдоль поверхности
     ```
   - При `friction < 1`:
     ```cpp
     slide_velocity_ += g_tang * (1 - friction) * dt;
     // двухрежимный кап
     if (drive_speed > 0)   max_slide = drive_speed * (1 - friction);
     else                   max_slide = max_fall_speed_;
     // ограничиваем slide_velocity_.norm()
     ```
   - При `friction == 1` или `g_tang ≈ 0` (плоский пол) — `slide_velocity_ = 0`.
   - Прибавляет slide к body-velocity:
     ```cpp
     Eigen::Matrix3d R = CollisionSystem::rotation_from_pose(agent.world_pose);
     Vec3 slide_body = R.transpose() * slide_velocity_;
     agent.world_velocity.linear.x() += slide_body.x();
     agent.world_velocity.linear.y() += slide_body.y();
     ```
5. **Не-grounded ветка (свободное падение):**
   - `fall_velocity_ -= gravity_accel * dt; clamp в [-max_fall_speed, +inf]`.
   - `pose.z += fall_velocity_ * dt`.
   - `slide_velocity_ = 0`.
   - Если `support && pose.z < ground_z` → snap (`pose.z = ground_z`, `fall_velocity_ = 0`).
6. В конце: `world_velocity.linear.z() = 0` — фаза кинематики не должна повторно двигать Z.

### 15.7. `JointVelPlugin` (`joint_vel.hpp` + `.cpp`)

```yaml
- type: joint_vel
  topic: /cmd_vel_mount
  joints:
    - name: arm
      axis: linear_x
      max_vel: 0.01
    - name: bucket
      axis: angular_z
      max_vel: 0.01
```

`from_config()` читает `topic_` и список `JointMapping{joint_name, twist_axis, max_vel, target_vel=0}`.

`inputs_schema()` строится **динамически** из текущих джоинтов:

```json
{
  "arm":   {"type":"number","default":0,"min":-1,"max":1,"unit":"rad/s"},
  "bucket":{"type":"number","default":0,"min":-1,"max":1,"unit":"rad/s"}
}
```

`handle_input(json)`:
- Сначала пытается прочитать поле по имени джоинта (`"arm": 0.5`), иначе — `extract_twist_field(j, axis)` (поддерживает `linear:{x,y,z}`/`angular:{x,y,z}` и плоский формат `linear_x`/`angular_z`).
- Сохраняет `mapping.target_vel` с clamp по `max_vel`.

`update(dt, agent)`:
```cpp
if (!agent.kinematic_tree) return;
for (auto& m : joints_) {
    if (m.target_vel == 0) continue;
    double current_val = ...;  // из tree.links()
    agent.kinematic_tree->set_joint_value(m.joint_name, current_val + m.target_vel*dt);
    // clamp по min/max происходит внутри set_joint_value
}
```

`to_json()` — `{plugin:"joint_vel", joints:[{name, target_vel}, ...]}`.

### 15.8. `ColorPlugin` (`color.hpp` + `.cpp`)

```yaml
- type: color
  service: /set_color
  color: "#FF0000"
  duration: 5.0
```

ROS2-сервис без параметров (`std_srvs/Trigger` или `s2_msgs/PluginCall` если есть). `handle_service(...)` запускает таймер на `duration_`. `initialize(agent)`:
- Если у агента есть `kinematic_tree` (URDF), `is_urdf_ = true`, сохраняются цвета всех link'ов с непустой visual.
- Иначе сохраняется `agent.visual.color` в `default_color_`.

`update(dt, agent)`:
- `timer_ -= dt`. Если `timer_ > 0` — устанавливает `configured_color_` для всех соответствующих link'ов / visual.
- Иначе — восстанавливает оригинал.

`to_json()` отдаёт `{plugin, active_color, remaining}`.

### 15.9. `TrajectoryRecorderPlugin` (`trajectory_recorder.hpp`)

```yaml
- type: trajectory_recorder
  record_interval_s: 0.5
  max_points: 200
  color: "#FFAA00"
```

Пишет позицию агента каждые `record_interval_s` в `points_`. Когда `>max_points` — удаляет с начала. `has_inputs() = true`, единственное поле `enabled` (boolean): при выключении буфер очищается.

`to_json()`: `{type:"trajectory", points:[[x,y,z]...], color, enabled}`.

### 15.10. `PathDisplayPlugin` (`path_display.hpp`)

```yaml
- type: path_display
  topic: /plan
  max_points: 500
  color: "#00FF88"
```

Подписывается на топик `nav_msgs/Path`. `subscribe_topics() = {topic_}`, `subscription_msg_type() = "nav_msgs/Path"`. `handle_subscription` парсит JSON `{poses:[{position:{x,y,z}}, ...]}` (формат конвертируется в `Ros2TransportAdapter`) и хранит точки.

`to_json()` → `{type:"path", points, color, visible}`.

UI-input: `visible` — boolean.

### 15.11. `TopicDisplayPlugin` (`topic_display.hpp`)

```yaml
- type: topic_display
  name: robot_state
  topic: /robot_state
```

Подписывается на `std_msgs/String`, считает входящую строку JSON-данными и встраивает их в `to_json()`:
```cpp
result["topic"] = topic_;
auto parsed = json::parse(raw_, nullptr, false);
result["data"] = parsed.is_discarded() ? json(raw_) : parsed;
```

Используется для отображения произвольных «сообщений робота» в UI.

---

## 16. Транспортный слой (`ITransportAdapter`)

Транспорт намеренно отделён от ROS2: интерфейс лежит в `s2_core/include/s2/transport_adapter.hpp` (не зависит от rclcpp).

```cpp
class ITransportAdapter {
public:
    virtual ~ITransportAdapter() = default;
    virtual void start() = 0;
    virtual void stop()  = 0;

    virtual void set_geo_origin(const GeoOrigin&) = 0;
    virtual void register_agent(AgentId, int domain_id, const std::string&, const Pose3D&) = 0;
    virtual void register_subscription(SubscriptionDesc) = 0;
    virtual void register_input_topic(InputTopicDesc) = 0;
    virtual void register_service(ServiceDesc) = 0;
    virtual void register_sensor(SensorRegistration) = 0;
    virtual void register_static_transforms(AgentId, int domain_id, const std::vector<FrameTransform>&) = 0;

    virtual void publish_agent_frame(const AgentSensorFrame&) = 0;
    virtual void emit_event(const TransportEvent&) = 0;
};
```

Сопутствующие структуры:

```cpp
struct SensorOutput {
    std::string sensor_type;        // "gnss"|"imu"|"diff_drive"|"lidar"|"battery"
    std::string sensor_name;
    std::optional<GnssData>      gnss;
    std::optional<ImuData>       imu;
    std::optional<DiffDriveData> diff_drive;
    std::optional<LidarScanData> lidar_scan;
    std::optional<BatteryData>   battery;
};

struct SensorRegistration {
    AgentId agent_id; int domain_id;
    std::string sensor_type; std::string sensor_name;
    std::string topic_override;
    std::string frame_id;
};

struct FrameTransform {
    std::string parent_frame, child_frame;
    Pose3D      relative_pose;
};

struct AgentSensorFrame {
    AgentId agent_id; int domain_id;
    double  sim_time;
    Pose3D    world_pose;
    Velocity  world_velocity;
    std::vector<SensorOutput>     sensors;            // только новые seq
    std::vector<FrameTransform>   dynamic_transforms; // revolute/prismatic
};

struct SubscriptionDesc { topic, msg_type, plugin_type, agent_id, domain_id, callback(topic, json) };
struct InputTopicDesc   { topic, plugin_type, agent_id, domain_id, callback(json) };
struct ServiceDesc      { service_name, plugin_type, agent_id, domain_id, is_trigger, handler(req)→resp };
struct TransportEvent   { topic, payload_json, agent_id, domain_id };
```

Сенсорные данные — копии (а не указатели), что снимает проблемы со временем жизни при асинхронной публикации.

---

## 17. SimTransportBridge

`s2_transport/src/sim_transport_bridge.cpp` — мост между `SimEngine` и адаптером. `is_sensor_plugin` определяет «сенсорные» типы:

```cpp
bool SimTransportBridge::is_sensor_plugin(const std::string& t) {
    return t == "gnss" || t == "imu" || t == "diff_drive" || t == "lidar" || t == "battery";
}
```

### 17.1. `init(geo_origin)`

```cpp
adapter_->set_geo_origin(geo_origin);
for (const auto& agent : engine_->world().agents()) {
    adapter_->register_agent(agent.id, agent.domain_id, agent.name, agent.world_pose);
    // 1. собрать static TF
    std::vector<FrameTransform> static_tfs;
    if (agent.kinematic_tree) {
        std::vector<KinematicFrameTransform> kin_static, kin_dyn;
        agent.kinematic_tree->collect_transforms(kin_static, kin_dyn);
        for (const auto& k : kin_static)
            static_tfs.push_back({k.parent_frame, k.child_frame, k.relative_pose});
    }
    // 2. инициализировать плагины (например, ColorPlugin запоминает исходные цвета)
    for (const auto& plugin : agent.plugins)
        plugin->initialize(const_cast<Agent&>(agent));
    // 3. mount_frame() сенсоров
    for (const auto& plugin : agent.plugins) {
        auto mf = plugin->mount_frame();
        if (mf) static_tfs.push_back(*mf);
    }
    if (!static_tfs.empty())
        adapter_->register_static_transforms(agent.id, agent.domain_id, static_tfs);

    // 4. сенсоры, командные топики, подписки, сервисы
    for (const auto& plugin : agent.plugins) {
        if (is_sensor_plugin(plugin->type())) {
            SensorRegistration reg;
            reg.agent_id = agent.id; reg.domain_id = agent.domain_id;
            reg.sensor_type = plugin->type(); reg.sensor_name = plugin->sensor_name();
            reg.topic_override = plugin->output_topic();
            reg.frame_id       = plugin->sensor_frame_id();
            if (plugin->type() == "lidar" && reg.topic_override.empty())
                reg.topic_override = "/" + (sn.empty() ? "lidar" : sn);
            adapter_->register_sensor(reg);
            last_published_seq_[{agent.id, plugin->type(), plugin->sensor_name()}] = 0;
        }
        for (const auto& topic : plugin->command_topics()) {
            InputTopicDesc d; d.topic=topic; d.plugin_type=plugin->type();
            d.agent_id=agent.id; d.domain_id=agent.domain_id;
            d.callback = [eng=engine_, id=agent.id, t=plugin->type()](const std::string& json) {
                eng->handle_plugin_input(id, t, json);
            };
            adapter_->register_input_topic(std::move(d));
        }
        for (const auto& topic : plugin->subscribe_topics()) {
            SubscriptionDesc d;
            d.topic=topic; d.msg_type=plugin->subscription_msg_type();
            d.plugin_type=plugin->type(); d.agent_id=agent.id; d.domain_id=agent.domain_id;
            auto* raw = plugin.get();
            d.callback = [raw](const std::string& t, const std::string& j) {
                raw->handle_subscription(t, j);
            };
            adapter_->register_subscription(std::move(d));
        }
        for (const auto& svc : plugin->service_names()) {
            ServiceDesc d;
            d.service_name=svc; d.plugin_type=plugin->type();
            d.agent_id=agent.id; d.domain_id=agent.domain_id; d.is_trigger=true;
            auto* raw = plugin.get(); std::string captured = svc;
            d.handler = [raw, captured](const std::string& r) { return raw->handle_service(captured, r); };
            adapter_->register_service(std::move(d));
        }
    }
}
engine_->set_post_tick_callback([this](const SimWorld& w, double t){ on_post_tick(w, t); });
```

Заметим: все сервисы регистрируются с `is_trigger=true` (то есть пока используется `std_srvs/Trigger`; код для `s2_msgs/PluginCall` присутствует, но `is_trigger=false` нигде не выставляется).

### 17.2. `on_post_tick(world, sim_time)`

Для каждого агента строит `AgentSensorFrame`:

```cpp
AgentSensorFrame frame;
frame.agent_id = agent.id; frame.domain_id = agent.domain_id;
frame.sim_time = sim_time;
frame.world_pose = agent.world_pose;
frame.world_velocity = agent.world_velocity;
```

Для каждого сенсорного плагина проверяет `seq`:

```cpp
auto* data = agent.state.get<GnssData>();   // или ImuData/DiffDriveData/LidarScanData/BatteryData
auto it = last_published_seq_.find(key);
if (data && (it == ... || data->seq > it->second)) {
    out.gnss = *data;            // копия!
    has_new_data = true;
    last_published_seq_[key] = data->seq;
}
if (has_new_data) frame.sensors.push_back(out);
```

То есть сенсорный outлет публикуется адаптером **только если плагин обновил `seq`**. Это и реализует «частоту публикации» каждого плагина.

Динамические TF из `KinematicTree`:
```cpp
agent.kinematic_tree->collect_transforms(kin_static, kin_dyn);
for (const auto& k : kin_dyn)
    frame.dynamic_transforms.push_back({k.parent_frame, k.child_frame, k.relative_pose});
```

Затем `adapter_->publish_agent_frame(frame)` и опрос событий:
```cpp
for (const auto& plugin : agent.plugins)
    for (auto& evt : plugin->poll_events()) {
        evt.agent_id = agent.id; evt.domain_id = agent.domain_id;
        adapter_->emit_event(evt);
    }
```

`start()`/`stop()` просто прокси на адаптер.

---

## 18. Ros2TransportAdapter

`s2_transport/src/ros2_transport_adapter.cpp` (899 строк) — реальная реализация под флагом `S2_WITH_ROS2`. Ключевая идея: **физическая изоляция через изолированные `rclcpp::Context` для каждого `domain_id`**. Один `Node` + один `SingleThreadedExecutor` + один поток на каждый домен.

### 18.1. `NodeInfo`

```cpp
struct NodeInfo {
    std::shared_ptr<rclcpp::Context>  context;
    std::shared_ptr<rclcpp::Node>     node;
    std::unique_ptr<rclcpp::executors::SingleThreadedExecutor> executor;

    std::vector<rclcpp::SubscriptionBase::SharedPtr> subscriptions;

    std::map<std::string, Publisher<NavSatFix>>     gnss_pubs;
    std::map<std::string, Publisher<Imu>>           imu_pubs;
    std::map<std::string, Publisher<Odometry>>      odom_pubs;
    std::map<std::string, Publisher<LaserScan>>     lidar_pubs;
    std::map<std::string, std::string>              lidar_frames;
    std::map<std::string, Publisher<BatteryState>>  battery_pubs;

    std::shared_ptr<TransformBroadcaster>       tf_broadcaster;
    std::shared_ptr<StaticTransformBroadcaster> static_tf_broadcaster;

    std::vector<ServiceBase::SharedPtr> services;
    std::map<std::string, Publisher<String>> event_pubs;

    geometry_msgs::msg::TransformStamped earth_map_tf;
    rclcpp::TimerBase::SharedPtr earth_map_timer;
    Pose3D initial_pose{};
    std::vector<TransformStamped> extra_static_tfs;
    bool initialized{false};
};
std::map<int, NodeInfo> domain_nodes_;
std::vector<std::thread> spin_threads_;
```

### 18.2. Создание нод

`get_or_create_node(domain_id)` создаёт `Context` с `InitOptions::set_domain_id(domain_id)`, `Node` с `NodeOptions::context(context)` и `SingleThreadedExecutor` с тем же контекстом. Имя ноды — `s2_sim_domain_<domain_id>`.

`register_agent(id, domain_id, name, initial_pose)` сохраняет `initial_pose` в `NodeInfo`, создаёт `tf_broadcaster` и `static_tf_broadcaster`, вызывает `setup_static_tf(info)` и помечает `initialized=true`.

### 18.3. `setup_static_tf`

Если `geo_origin_set_=true`, считает `earth_map_tf` через `compute_earth_map_tf(origin, initial_pose, node)`:
1. Переводит `(lat, lon, alt)` в ECEF (WGS84):
   ```cpp
   N = a / sqrt(1 - e² sin²(lat));
   ecef_origin = ((N+alt)*cos*cos, (N+alt)*cos*sin, (N(1-e²)+alt)*sin);
   ```
2. Строит матрицу ECEF→ENU.
3. ECEF позиция = `ecef_origin + R_enu^T * initial_pose_xyz` — то есть начало отсчёта робота смещено в ECEF на его стартовую ENU-позу.
4. Ориентация фрейма `map` в ECEF — `R_enu` (как кватернион).

Этот `earth_map_tf` отправляется через static broadcaster и затем переотправляется каждую секунду по таймеру (для устойчивости в DDS).

Также добавляется identity `map → odom`. Если позже `register_static_transforms()` добавит больше TF (mount_frame, fixed-джоинты), вся пачка `extra_static_tfs` отправляется заново.

### 18.4. `register_sensor`

Создаёт publisher по типу. Соглашение об именах топиков:

| sensor_type   | sensor_name=""    | sensor_name="X"          |
|---------------|-------------------|--------------------------|
| `gnss`        | `/gnss/fix`       | `/gnss/X/fix`            |
| `imu`         | `/imu/data`       | `/imu/X/data`            |
| `diff_drive`  | `/odom`           | `/X/odom`                |
| `lidar`       | `/lidar`          | `/X` (имя сенсора как топик) |
| `battery`     | `/battery_state`  | `/X/battery_state`       |

`topic_override` всегда побеждает. `frame_id` для лидара сохраняется в `lidar_frames[sensor_name]` и потом подставляется в `LaserScan.header.frame_id`.

### 18.5. `register_subscription`

Хитрая часть: реестр десериализаторов по `msg_type`:

```cpp
using SubscriberCreator = std::function<rclcpp::SubscriptionBase::SharedPtr(
    rclcpp::Node&, const std::string&, std::function<void(const std::string&, const std::string&)>)>;

static std::unordered_map<std::string, SubscriberCreator>& sub_registry();

// nav_msgs/Path
sub_registry()["nav_msgs/Path"] = [](Node& n, const std::string& topic, auto cb){
    return n.create_subscription<nav_msgs::msg::Path>(topic, QoS(10),
        [cb, topic](nav_msgs::msg::Path::ConstSharedPtr msg){
            json j; auto poses = json::array();
            for (auto& ps : msg->poses) poses.push_back({{"position", ...}});
            j["poses"] = poses; cb(topic, j.dump());
        });
};

// std_msgs/String
sub_registry()["std_msgs/String"] = [](Node& n, const std::string& topic, auto cb){
    return n.create_subscription<std_msgs::msg::String>(topic, QoS(10),
        [cb, topic](auto msg){ json j; j["data"] = msg->data; cb(topic, j.dump()); });
};
```

Регистрация — через статические `bool s_reg_path = []{...; return true;}()` лямбды, которые выполняются при загрузке translation unit. Чтобы добавить новый тип сообщения — достаточно добавить ещё один блок.

`register_input_topic` создаёт subscription на `geometry_msgs/Twist`. Если `plugin_type=="diff_drive"`, JSON формируется как `{linear_velocity, angular_velocity}`, иначе как плоский Twist `{linear_x, linear_y, linear_z, angular_x, angular_y, angular_z}`.

### 18.6. `register_service`

Если `is_trigger==true` (всегда так в текущем мосту) — `std_srvs::srv::Trigger`. Handler парсит JSON-ответ плагина и заполняет `success`/`message`. Если `is_trigger==false` и определён `S2_WITH_S2_MSGS` — `s2_msgs/srv/PluginCall`: `request.request_json` идёт в handler, ответ записывается в `response.response_json`. Без `S2_WITH_S2_MSGS` — fallback на Trigger.

### 18.7. `publish_agent_frame`

1. **`odom → base_link`** каждый кадр:
   ```cpp
   tf.transform.translation = world_pose - initial_pose;
   tf.transform.rotation = Quaternion(yaw)*Quaternion(pitch)*Quaternion(roll);
   info.tf_broadcaster->sendTransform(tf);
   ```
   То есть робот стартует в `(0,0,0)` своего odom-фрейма; `earth→map` уже компенсирует разные стартовые позы разных роботов.

2. **Динамические TF** (revolute/prismatic) — каждый отдельным `TransformStamped`.

3. **Сенсоры** — для каждого `sensor` в `frame.sensors`:
   - GNSS → `sensor_msgs/NavSatFix`. `frame_id = "<sname>_gnss_link"` или `"gnss_link"`. Status=GPS, covariance — diagonal_known с `accuracy²` по диагонали.
   - IMU → `sensor_msgs/Imu`. `frame_id = "<sname>_imu_link"` или `"imu_link"`. Ориентация — только yaw как `AngleAxisd(yaw, Z)`. Все ковариации помечены `[0]=-1` (неизвестно).
   - DiffDrive → `nav_msgs/Odometry`. `header.frame_id="odom"`, `child_frame_id = "<sname>_base_link"` или `"base_link"`. Pose — мировая, twist — `world_velocity`.
   - Lidar → `sensor_msgs/LaserScan`. Frame из `lidar_frames[sname]`.
   - Battery → `sensor_msgs/BatteryState`. `voltage = nominal_voltage*level`, `charge=capacity_ah*level`, `percentage=level`. Status: `FULL` если `level>=1 && charging`, иначе `CHARGING`/`DISCHARGING`.

### 18.8. `emit_event`

Создаёт publisher `std_msgs/String` для нового топика лениво и публикует `payload_json`. Используется для произвольных событий плагинов (никаких текущих плагинов с `poll_events()` нет, но контракт реализован).

Stub-реализация (`ros2_transport_adapter_stub.cpp`) — все методы `{}`-пустые.

---

## 19. Старый MVP `ROS2Transport`

Файлы `ros2_transport.hpp`, `ros2_transport.cpp`, `ros2_transport_stub.cpp` — это **старый** MVP, поддерживавший только подписку на `/cmd_vel`. CMake собирает оба файла:

```cmake
add_library(s2_transport STATIC
    src/ros2_transport.cpp              # MVP: /cmd_vel
    src/ros2_transport_adapter.cpp      # Новый полный адаптер
    src/sim_transport_bridge.cpp
)
```

Но `main.cpp` создаёт только `Ros2TransportAdapter`. Класс `ROS2Transport` оставлен как «совместимость», без активных пользователей в основном пути. У него есть тесты (`test_ros2_transport.cpp`), которые компилируются только если включён `S2_WITH_ROS2` (см. `target_compile_definitions(s2_core_tests PRIVATE S2_WITH_ROS2)`).

В заголовке `ros2_transport.hpp` есть лёгкая несогласованность: декларирует `void spin_thread()`/`std::thread spin_thread_`, а реализация использует `std::vector<std::thread> spin_threads_`. На сборку это не влияет (последняя реализация `ros2_transport.cpp` и `ros2_transport_stub.cpp` объявляют свои `spin_threads_`/`spin_thread()`).

---

## 20. s2_msgs

Кастомный ROS2-пакет. `srv/PluginCall.srv`:

```
# Request
string request_json
---
bool success
string response_json
```

Сборка через colcon (см. `docker-compose.yml`): пакет копируется в `/workspace/s2_msgs_ws/src/`, билдится `colcon build --symlink-install`, затем `source install/setup.bash`. Если `find_package(s2_msgs)` находит его — выставляется `S2_WITH_S2_MSGS`, и `register_service(is_trigger=false)` использует этот сервис.

Дубликат лежит в `s2_msgs_ws/src/s2_msgs/` (для повторной colcon-сборки), что и реализует docker-compose.

---

## 21. SceneLoader и SceneWriter

### 21.1. `SceneLoader::load(yaml_path, plugin_factory)`

Возвращает `SceneData`:

```cpp
struct TransportConfig { std::string type="ros2"; int default_domain_id=0; };
struct VizConfig       { bool enabled=true; int port=8080; };
struct SceneData {
    SimEngine::Config engine_config;
    TransportConfig   transport_config;
    VizConfig         viz_config;
    Heightmap         heightmap;
    GeoOrigin         geo_origin;
    std::vector<WorldPrimitive> geometry;
    std::vector<Agent>          agents;
    std::vector<Prop>           props;
    std::vector<Actor>          actors;
    std::vector<Zone>           zones;
};
```

Парсятся четыре корневые секции `s2:` → `update_rate`/`viz_rate`/`transport_rate`, `transport.type`/`default_domain_id`, `visualizer.enabled/port`, `world.{surface, geometry, geo_origin}`, `agents`, `props`, `actors`, `zones`.

Агент:
```yaml
- name: robot_0
  domain_id: 50
  pose: {x, y, z, yaw, pitch, roll}      # все опционально, default 0
  collision: { bounding: { type: sphere|box|capsule, radius, height, size: [x,y,z] } }
  max_slope_deg: 25
  max_step_height: 0.2
  visual: { type, size, color }
  capabilities: [surface_contact, has_battery, wheeled]
  velocity: { linear_x, linear_y, angular_z }
  plugins: [...]
  urdf: ../robots/dozer.urdf            # путь относительно директории сцены
  links: [...]                          # альтернатива urdf
```

Каждый плагин:
```yaml
- type: diff_drive
  name: front                # sensor_name
  topic: /custom_topic       # output_topic_
  publish_rate_hz: 30        # base_rate_hz_
  mount: {x, y, z, ...}      # mount_pose_
  ...                        # доменные параметры
```

После создания плагина через `plugin_factory(type, plugin_node)` — `set_sensor_name`, `set_output_topic`, `set_base_rate`, `set_mount_pose` (если соответствующие поля есть). `from_config(node)` вызывается уже внутри `plugin_factory` (см. `create_plugin`).

URDF имеет приоритет над `links:`. Если URDF задан и `load_urdf_collision()` нашёл коллизионный шейп — он перезаписывает YAML `collision:`.

`links:` — упрощённое описание дерева:
```yaml
links:
  - name: arm
    parent: base_link
    origin: {x, y, z, yaw, pitch, roll}
    joint:
      type: fixed | revolute | prismatic | continuous
      axis: [0, 0, 1]
      min: -3.14
      max:  3.14
      value: 0
```

Зоны:
```yaml
zones:
  - id: ice_patch
    enabled: true
    color, opacity, visible, label
    detection_mode: center | bounding
    shape:
      type: sphere|aabb|cylinder|infinite
      center: {x, y, z}
      radius: 1.0
      half_size: {x, y, z}
      half_height: 1.0
    attached_to: <actor.name>     # ищется в уже распарсенных актерах
    effects:
      - type: ice_modifier
        enabled: true
        effect_type: modifier|continuous|mutation|sensor   # переопределяется плагином
        required_capabilities: [surface_contact]
        params: {...}
```

Полный код парсинга — в одном `inline`-методе в заголовке (~400 строк). `parse_pose`, `parse_visual`, `parse_collision`, `parse_heightmap`, `parse_geometry`, `parse_geo_origin`, `parse_zone_shape` — отдельные хелперы.

### 21.2. `SceneWriter`

Делает обратный процесс — сохраняет геометрию или агентов поверх существующего YAML, не трогая остальные секции.

```cpp
static void save_geometry(const std::string& yaml_path, const std::vector<WorldPrimitive>& prims);
static void save_agents(const std::string& yaml_path, const nlohmann::json& agents_json);
```

`save_geometry`:
1. `YAML::LoadFile(yaml_path)`.
2. Строит новую секцию `geom_seq` из `prims` (тип, поза, size/radius/height, color).
3. `root["s2"]["world"]["geometry"] = geom_seq`.
4. Записывает обратно через `std::ofstream`.

`save_agents` принимает nlohmann::json массив, конвертирует его в `YAML::Node` через рекурсивный `json_to_yaml` (поддержка null/bool/int64/double/string/array/object) и подставляет в `root["s2"]["agents"]`.

Используется UI-редактором сцены через `SimEngineCommandAdapter::on_save_scene()` / `on_update_agents()`.

---

## 22. WorldSnapshot

`WorldSnapshot` (`world_snapshot.hpp`) — полная снимаемая структура. Подструктуры:

```cpp
struct LinkFrameSnapshot { std::string name; Pose3D world_pose; LinkVisual visual; };

struct AgentSnapshot {
    AgentId id; std::string name;
    Pose3D pose; Velocity velocity;
    Vec3 velocity_addition;
    VisualDesc visual;
    double effective_speed_scale{1.0};
    bool   motion_locked{false};
    nlohmann::json extra = nlohmann::json::object();
    bool tire_punctured{false};
    std::vector<LinkFrameSnapshot> kinematic_frames;
    bool has_collision{false};
    std::string bounding_type;
    double bounding_radius{0.5};
    Vec3 bounding_size;
};

struct PropSnapshot   { ... pose, visual, movable, attached_to_agent };
struct ActorSnapshot  { ... pose, visual, state };
struct ZoneSnapshot   { id, enabled, shape_type, center, radius, half_size, half_height, color, opacity, visible, label, agents_inside };
struct GeometrySnapshot { type, x, y, z, yaw, pitch, roll, sx, sy, sz, radius, height, color };

struct WorldSnapshot {
    double sim_time = 0.0;
    bool   paused   = false;
    std::vector<AgentSnapshot> agents;
    std::vector<PropSnapshot>  props;
    std::vector<ActorSnapshot> actors;
    std::vector<ZoneSnapshot>  zones;
    std::map<std::string, std::map<std::string, std::string>> plugins_data;
    std::map<std::string, std::string>                         plugin_inputs_schemas;
    std::vector<GeometrySnapshot> geometry;
};
```

Сериализация — `nlohmann::json snapshot_to_json(const WorldSnapshot&, bool include_geometry=false, bool include_plugins=true)` (`world_snapshot.cpp`). Несколько важных мест:

- `pose_to_json` отдаёт `{x, y, z, roll, pitch, yaw}`.
- `velocity_to_json` отдаёт `{vx, vy, vz, wx, wy, wz}`.
- `agent_snapshot_to_json` сначала кладёт стандартные поля, затем `j.update(agent.extra)` — чтобы каждый плагин (`BatteryPlugin`, …) мог дополнять ключи.
- `kinematic_frames` сериализуются вместе с visual (box → `sx, sy, sz`; cylinder → `radius, length`; sphere → `radius`).
- `bounding` сериализуется только если `has_collision`.
- `plugins_data` — `agent_id → { plugin_key → JSON }`. JSON парсится из строки и подставляется как объект; если парсинг не удался — кладётся как строка.
- При `include_geometry=false` (кадры real-time) геометрия в JSON не входит — её отправляют только при первом подключении клиента и после редактирования.

### 22.1. Доменные данные

```cpp
struct GnssData      { uint64_t seq{0}; double lat, lon, alt, azimuth, accuracy; };
struct ImuData       { uint64_t seq{0}; double gyro_x/y/z, accel_x/y/z, yaw; };
struct DiffDriveData { uint64_t seq{0}; double desired_linear, desired_angular, max_linear, max_angular; std::vector<std::string> warnings; };
struct BatteryData   { uint64_t seq{0}; double level{1.0}; bool charging{false}; double nominal_voltage, capacity_ah, design_capacity_ah; uint8_t technology; std::string location, serial_number; };
struct LidarScanData { uint64_t seq{0}; float angle_min, angle_max, angle_increment, time_increment, scan_time, range_min, range_max; std::vector<float> ranges; };

struct BatteryComponent { double level{1.0}; bool charging{false}; };  // в s2_plugins
struct TirePunctureData { bool punctured{false}; };                      // в s2_core
```

Все «sensor data» используют монотонный `seq`, на котором `SimTransportBridge` различает «новые/старые» кадры.

---

## 23. TripleBuffer

`triple_buffer.hpp` — шаблон для thread-safe обмена между sim и transport-потоком. Реализация **с тремя мьютексами** (не lock-free):

```cpp
template <typename T>
class TripleBuffer {
public:
    T& writer_buffer();         // под lock writer_mutex_
    void publish();             // swap writer<->ready под обоими мьютексами
    T& acquire_read();          // swap ready<->reader, возвращает старое ready
    const T& read() const;
private:
    enum class Index { Writer=0, Ready=1, Reader=2 };
    std::array<T, 3> buffers_;
    mutable std::mutex writer_mutex_, ready_mutex_, reader_mutex_;
};
```

Любопытно, что в текущем коде `TripleBuffer` **не используется**. Снапшоты передаются через `VizServer::publish` → `pending_snapshot_` под `std::mutex`. Класс лежит как заготовка с тестами.

---

## 24. VizServer

`s2_visualizer/src/viz_server.{hpp,cpp}` — HTTP/SSE-сервер на чистых POSIX-сокетах. Один порт обслуживает HTTP (статика + REST) и SSE-стрим. WebSocket код есть, но фронтенд использует SSE.

### 24.1. `VizCommandHandler`

Виртуальный интерфейс для команд от UI:

```cpp
struct VizCommandHandler {
    virtual void on_pause()  = 0;
    virtual void on_resume() = 0;
    virtual void on_reset()  = 0;
    virtual void on_move_agent(AgentId, double x, double y, double yaw) = 0;
    virtual void on_plugin_input(AgentId, const std::string& plugin_type, const std::string& json_input) = 0;
    virtual void on_update_geometry(const std::vector<WorldPrimitive>&) = 0;

    struct SaveSceneResult { bool ok; std::string path_or_error; };
    virtual SaveSceneResult on_save_scene() = 0;
    virtual std::string     on_get_scene_state() { return "{\"agents\":[],\"geometry\":[]}"; }
    virtual std::string     on_get_urdf_list()   { return "{\"files\":[]}"; }
    virtual SaveSceneResult on_update_agents(const std::string&)   { return {false,"not implemented"}; }
    virtual std::string     on_get_scene_list()                    { return "{\"scenes\":[]}"; }
    virtual SaveSceneResult on_load_scene(const std::string&)      { return {false,"not implemented"}; }
    virtual SaveSceneResult on_save_scene_as(const std::string&)   { return {false,"not implemented"}; }
    virtual SaveSceneResult on_new_scene(const std::string&)       { return {false,"not implemented"}; }
};
```

Реальный handler — `SimEngineCommandAdapter` в `main.cpp` (см. §26).

### 24.2. Цикл сервера

`run_server()`:
1. `socket(AF_INET, SOCK_STREAM)`, `setsockopt(SO_REUSEADDR)`, `bind`, `listen(128)`, `O_NONBLOCK`.
2. В цикле `accept(...)` (с маленьким `sleep`, если EAGAIN). Каждое соединение становится non-blocking.
3. Читает первый чанк запроса (до 64 KB).
4. Если URL содержит `/stream` — запускает SSE в отдельном потоке (`std::thread sse_thr(...)`), сохраняет поток в `sse_threads_`.
5. Если запрос — WebSocket handshake (наличие `Sec-WebSocket-Key` и `Upgrade: websocket`) — рассчитывает SHA-1 + base64 (через popen `openssl`), отвечает `101 Switching Protocols` и переходит в WebSocket-цикл.
6. Иначе → `serve_http(client_fd, request)`.

Под капотом SHA-1 хитро реализован через `popen("echo -n ... | openssl sha1 -binary | openssl base64", "r")` — то есть для WS-handshake запускается внешний процесс openssl. В коде есть и пустой `sha1()`-stub, но он не используется.

### 24.3. SSE

`run_sse_client(client_fd)`:
1. Отправляет HTTP-заголовки `text/event-stream`, `Cache-Control: no-cache`, `Connection: keep-alive`, `Access-Control-Allow-Origin: *`.
2. Регистрирует `client_fd` в `ws_clients_`.
3. `send_snapshot_now(client_fd)` — отправляет полный текущий снапшот **с геометрией и плагинами** при первом подключении.
4. Цикл: `recv(client_fd, ...)` non-blocking; при отсутствии данных вызывает `handle_pending_snapshots()` каждые 5 мс.

`publish(snapshot)`:
```cpp
std::lock_guard<std::mutex> lock(snapshot_mutex_);
pending_snapshot_ = snapshot;
has_pending_.store(true);
```

`handle_pending_snapshots()`:
1. Копирует `pending_snapshot_` под мьютексом, освобождает мьютекс (минимизация stall'а sim-потока).
2. Сериализует JSON **вне** мьютекса. Каждый 10-й кадр включает `plugins_data` (поле `PLUGIN_DATA_INTERVAL=10`); это throttling — при 30 fps плагины отправляются ~3 Гц.
3. Рассылает `data: <json>\n\n` всем клиентам через `send(MSG_NOSIGNAL | MSG_DONTWAIT)`. Мёртвые клиенты удаляются.

`force_broadcast_latest()` и `force_broadcast_with_geometry()` — мгновенная отправка после команд (pause/resume/move/plugin_input/update_geometry). Первый — без геометрии, второй — с геометрией (после редактирования).

### 24.4. HTTP API

`serve_http()` обрабатывает следующие маршруты:

| Метод | URL                       | Что делает |
|-------|---------------------------|------------|
| OPTIONS | `*` | CORS preflight (`Access-Control-Allow-Origin: *`, methods: GET POST OPTIONS) |
| GET   | `/api/plugins/registry`     | `s2::plugins::list_plugin_schemas()` — JSON-схема всех плагинов |
| GET   | `/api/scene/state`          | `on_get_scene_state()` — текущий YAML, разобранный в JSON |
| GET   | `/api/scene/urdf-list`      | `on_get_urdf_list()` |
| POST  | `/api/scene/agents`         | `on_update_agents(body)` — записывает агентов в YAML |
| POST  | `/api/scene/geometry`       | `on_update_geometry(prims)` — обновляет статическую геометрию в SimEngine |
| GET   | `/api/scenes`               | `on_get_scene_list()` |
| POST  | `/api/scene/save-as`        | `on_save_scene_as(name)` |
| POST  | `/api/scene/load`           | `on_load_scene(filename)` |
| POST  | `/api/scene/new`            | `on_new_scene(name)` |
| POST  | `/api/scene/save`           | `on_save_scene()` — сохраняет геометрию в текущую YAML |
| POST/GET | `/command?cmd=...`        | команды: `pause`, `resume`, `reset`, `reset_and_resume`, `move_agent` (id, x, y, yaw), `plugin_input` (agent_id, plugin, body) |
| GET   | `/` или прочее              | статика из `static_path_` (по умолчанию `/workspace/s2_visualizer/web`) |

`extract_http_body()` корректно дочитывает тело по `Content-Length`, если оно не пришло целиком в первом `recv`.

Throttle plugins_data:

```cpp
std::atomic<int> snap_counter_{0};
static constexpr int PLUGIN_DATA_INTERVAL = 10;
bool include_plugins = (snap_counter_++ % PLUGIN_DATA_INTERVAL == 0);
```

---

## 25. Веб-фронтенд

`s2_visualizer/web/index.html` (~830 строк) и `web/js/app.js` (~2750 строк).

### 25.1. Стек

ES-модули, импорт через CDN (импорт-карта в `index.html`):

```html
<script type="importmap">
{
  "imports": {
    "three": "https://...",
    "three/addons/": "https://.../examples/jsm/"
  }
}
</script>
```

Использует `three.js`, `OrbitControls`, `TransformControls`. Никаких бандлеров — обычный `<script type="module" src="js/app.js">`.

### 25.2. Главные компоненты UI

- **info-panel** (top-left): connection, sim_time, agent_count, actor_count, fps, индикатор Paused.
- **control-panel** (top-center): `Play / Pause / Reset / Mode: Translate / Axes: ON / Edit Scene / Scenes / Collisions: OFF`.
- **scenes-panel**: список .yaml в `s2_config/scenes/` плюс кнопки `Save As` / `New Scene` (обращаются к `/api/scenes`, `/api/scene/load`, …).
- **side-panel** (top-left под info): открывается при клике на робота. Показывает позу, ориентацию, скорости, эффективные ограничения (`speed_scale`, motion_lock, `velocity_addition`), tire_punctured, чекбокс TF frames, аккордеон плагинов.
- **editor-panel** (right): режим редактирования сцены — две вкладки `Геометрия` и `Агенты`. Геометрия: Add Box/Cyl/Sphere, transform mode (Move/Rotate/Scale), редактирование цвета и размеров выбранного примитива, Apply/Save. Агенты: список, форма редактирования (имя, domain_id, поза, visual, capabilities, плагины).

### 25.3. Связь с сервером

Подключение SSE:

```js
function connectSSE() {
    const url = `http://${host}:${port}/stream`;
    const evtSource = new EventSource(url);
    evtSource.onopen = () => { ... статус Connected ... };
    evtSource.onmessage = (event) => {
        const data = JSON.parse(event.data);
        if (data.agents) data.agents.forEach(a => { lastAgentData[a.id] = a; });
        updateScene(data);
    };
    evtSource.onerror = () => { setTimeout(connectSSE, 2000); };
}
```

`updateScene(data)`:
1. Определяет `data.sim_time` — если время «прыгнуло назад» (Reset) — очищает overlay-линии (траектории, пути).
2. Применяет `data.paused`.
3. Сохраняет `pluginsData` и обновляет аккордеон.
4. Для каждого агента в `plugins_data` рендерит overlay-линии:
   - `trajectory_recorder` → `THREE.Line` точек.
   - `path_display` → отдельная линия.
   - Плагины с `type:"lidar_points"` → `THREE.Points`.
5. Сохраняет `plugin_inputs_schemas` и пересобирает форму при изменении.
6. Создаёт/обновляет меши: `agent_<id>`, `prop_<id>`, `actor_<id>`, `zone_<id>`, `static_<i>`, `lm_<agent>_<link>`. Ключевая функция — `updateOrCreateMesh(key, type, pose, visual)`. Удаление через `removeMesh(key)`.
7. Если у агента `kinematic_frames` — обновляет/создаёт меши для каждого link'а.

Команды (POST `http://host:port/command?...`):

```js
function sendCommand(cmd) {
    fetch(`http://${host}:${port}/command?cmd=${cmd}`, { method: 'POST' });
}
function sendMoveAgent(id, x, y, yaw) {
    fetch(`...command?cmd=move_agent&id=${id}&x=${x}&y=${y}&yaw=${yaw}`);
}
function _sendValues(agentId, pluginName, values) {
    const body = JSON.stringify(values);
    fetch(`...command?cmd=plugin_input&agent_id=${agentId}&plugin=${pluginName}&body=${encodeURIComponent(body)}`);
}
```

`startPluginInput` запускает `setInterval(_sendValues, 50)` — 20 Гц. `stopPluginInput` шлёт нули и очищает интервал.

REST-клиенты:

```js
fetchPluginRegistry()  → GET /api/plugins/registry  → строит формы плагинов в редакторе
fetchSceneAgents()     → GET /api/scene/state        → агенты + geometry для редактора
fetchUrdfList()        → GET /api/scene/urdf-list    → список URDF в robots/
sendGeometryToServer() → POST /api/scene/geometry    → массив примитивов
sendAgentsToServer()   → POST /api/scene/agents      → массив агентов
saveScene(), loadScene(), saveSceneAs(), newScene() → /api/scene/save…/load/save-as/new
```

### 25.4. Координаты

Three.js — Y-up, но S2 — Z-up. Поэтому на стороне фронта:
- При показе агента: `mesh.position.x = pose.x`, `mesh.position.y = pose.z`, `mesh.position.z = -pose.y`. (Видно в обработчике drag: `m.position.y = -m.position.z` и т.п.)
- Стоит `GridHelper` 40×40 в плоскости XZ.

Drag-and-drop через `TransformControls`:
- `onmouseup` → если переключён в `Translate`/`Rotate`, отправляет `move_agent` с пересчётом X/Y/yaw.
- В режиме редактирования геометрии — `pushUndoSnapshot()`, `syncPrimitiveFromMesh()`, `sendGeometryToServer()`.

### 25.5. Прочие фишки

- Shift+LMB — ручной pan (через `manualPanning`-режим, перехвачен на capture phase до OrbitControls).
- Чекбокс «Show TF frames» — рисует `THREE.AxesHelper(2.0)` для всех агентов.
- Кнопка «Collisions: OFF/ON» — для каждого агента создаёт полупрозрачный mesh по `bounding`.
- Undo через `undoStack` (массив JSON-снапшотов editor_primitives, до 50). Ctrl+Z, Copy/Paste/Delete (Ctrl+C, Ctrl+V, Del).

---

## 26. main.cpp

Точка входа `s2_sim` (`s2_visualizer/src/main.cpp`):

1. Регистрирует SIGINT/SIGTERM.
2. При `S2_WITH_ROS2` — `rclcpp::init(argc, argv)` (глобальный контекст; адаптер всё равно создаёт изолированные контексты для каждого домена).
3. Берёт путь к сцене из `argv[1]` (default `/workspace/s2_config/scenes/test_basic.yaml`).
4. Загружает сцену:
   ```cpp
   auto plugin_factory = s2::plugins::create_plugin;
   scene_data = SceneLoader::load(scene_path, plugin_factory);
   ```
5. Создаёт `VizServer(0, viz_config.port, "/workspace/s2_visualizer/web")` если включён, запускает.
6. Собирает `SimWorld` из `scene_data` и кладёт в `SimEngine`:
   ```cpp
   SimEngine engine(scene_data.engine_config);
   engine.set_effect_factory(s2::create_effect);
   engine.load_world(std::move(world));
   engine.set_viz_server(viz.get());
   ```
7. Создаёт `SimEngineCommandAdapter` (см. ниже) и `viz->set_command_handler(&adapter)`.
8. Если задан `geo_origin` — для каждого `gnss`-плагина вызывает `gnss->set_geo_origin(...)`.
9. Транспорт:
   ```cpp
   std::shared_ptr<ITransportAdapter> adapter;
   if (transport.type == "stub") adapter = std::make_shared<Ros2TransportAdapter>();
   else                          adapter = std::make_shared<Ros2TransportAdapter>();
   ```
   (По факту всегда `Ros2TransportAdapter`. В stub-сборке его методы — no-op.)
10. `auto bridge = make_unique<SimTransportBridge>(&engine, adapter);`
    `bridge->init(geo_origin); bridge->start();`
11. `engine.run()` — блокирующий тиковый цикл.
12. `engine.stop()` по сигналу. Виз/мост закрываются. Если был `rclcpp::init` — `rclcpp::shutdown`.

`SimEngineCommandAdapter` имплементирует `VizCommandHandler` и связывает UI-команды с движком:

- `on_pause/on_resume` → `engine_->pause/resume()` + `broadcast_snapshot()`.
- `on_reset` → `engine_->reset()`.
- `on_move_agent(id, x, y, yaw)` → `engine_->set_agent_pose(id, {x,y,0,0,0,yaw})`.
- `on_plugin_input(agent_id, plugin_type, json)` → `engine_->handle_plugin_input(...)`.
- `on_update_geometry(prims)` → `engine_->update_static_geometry(prims)` + `force_broadcast_with_geometry()`.
- `on_save_scene()` → `SceneWriter::save_geometry(scene_path_, engine_->world().static_geometry())`.
- `on_get_scene_state()` → читает yaml и конвертирует в JSON.
- `on_get_urdf_list()` → ищет `*.urdf` в `<scene_dir>/../robots/`.
- `on_update_agents(agents_json)` → `SceneWriter::save_agents(scene_path_, parsed_array)`.
- `on_get_scene_list()` → `*.yaml` в `scenes_dir_`.
- `on_load_scene(filename)` → `engine_->pause()`, `SceneLoader::load(...)`, новый `SimWorld`, `engine_->load_world(...)`, force-broadcast, `engine_->resume()`.
- `on_save_scene_as(new_name)` → `std::filesystem::copy_file` + `save_geometry`.
- `on_new_scene(new_name)` → создаёт минимальный YAML (один агент `robot_0` с `diff_drive`) и вызывает `on_load_scene`.

Защита от path traversal: имя файла не может содержать `/` или `..`.

---

## 27. Сцены

В `s2_config/scenes/` лежат 11 YAML-файлов. Все они уже находятся в репозитории и работают с текущим кодом.

| Файл | Что демонстрирует |
|------|-------------------|
| `test_basic.yaml`        | Один робот в коробке стен, начальная скорость, без плагинов кроме diff_drive. Запуск по умолчанию. |
| `test_two_robots.yaml`   | Два робота со сферами стен и колонной. У `robot_0` плагины `diff_drive + gnss + imu`, у `robot_1` — только `gnss`. Сценарий compose `sim`. |
| `test_collision.yaml`    | Stub-транспорт. Геометрия с наклонными box, цилиндром, рампами; `max_slope_deg=20`, `max_step_height=0.1`. |
| `test_geometry.yaml`     | Тест геометрии (повёрнутые/вложенные boxes). |
| `test_gravity_ramp.yaml` | Робот стартует в воздухе z=3 над платформой; `gravity_accel=9.81, max_fall_speed=1.0, friction_coef=0.99`, рампы вверх и вниз. |
| `test_lidar.yaml`        | Три робота с лидаром 3600 лучей в комнате с колонной. |
| `test_dozer.yaml`        | Три робота с URDF `dozer.urdf` в трёх ROS2-доменах (50/51/52). Плагины: `diff_drive, gnss, imu, joint_vel, color, trajectory_recorder, path_display, topic_display`. Реальный «полный» сценарий. |
| `test_zones.yaml`        | Демонстрация всех зон/эффектов: `ice_modifier`, `boost_zone`, `motion_lock`, `conveyor`, `wind`, `tire_puncture`, `charging`. Два робота с разными capabilities. |
| `test_ros2.yaml`         | Три робота на плоской сцене, разные домены. Только `diff_drive`. Минимальный ROS2-тест. |
| `test_ros2_full.yaml`    | Один робот, `diff_drive + trajectory_recorder + path_display`, ROS2-домен 50. |
| `test_viz_overlay.yaml`  | Визуальный тест — повёрнутые box'ы, проверка отрисовки. Один робот с URDF dozer. |

Пример (`test_zones.yaml`, ключевые места):

```yaml
agents:
  - name: robot_0
    pose: {x: 0, y: 0, z: 0, yaw: 0}
    capabilities: [surface_contact, has_battery, wheeled]
    collision: { bounding: { type: sphere, radius: 0.4 } }
    plugins:
      - type: diff_drive
        max_linear_vel: 2.0
        max_angular_vel: 1.5
      - type: gravity

zones:
  - id: ice_patch
    shape: { type: aabb, center: {x:5, y:0, z:0.5}, half_size: {x:3, y:3, z:1} }
    effects:
      - type: ice_modifier
        required_capabilities: [surface_contact]
        params: { traction_coefficient: 0.2, noise_amplitude: 0.0 }

  - id: charging_station
    shape: { type: cylinder, center: {x:0, y:-7, z:0.5}, radius: 1.5, half_height: 1.0 }
    effects:
      - type: charging
        required_capabilities: [has_battery]
        params: { charge_rate: 0.05 }
```

---

## 28. URDF (dozer.urdf)

`s2_config/robots/dozer.urdf` (446 строк) — модель бульдозера. Структура:

- `base_footprint` (без visual) → fixed → `base_link` (visual: box 2.5×1.5×1.5 + дополнительный вытянутый box между корпусом и стрелой).
- `middle_base_link` (fixed) — служебный.
- 4 колеса: `FR_wheel`, `FL_wheel`, `BR_wheel`, `BL_wheel`. Все — `cylinder length=0.25 radius=0.5`. Joint `continuous`, ось `[0,0,1]` в локальной системе колеса (после `rpy=-π/2, 0, 0` это вокруг оси Y машины — нормальное вращение колеса).
- Цепочка стрелы: `arm_link` (revolute), `bucket_link` (revolute) — управляются `joint_vel` плагином по полям `arm` и `bucket`.

Корень загружается как `base_link` (звено `base_footprint` игнорируется загрузчиком). `<collision>` базового звена — box 2.5×1.5×1.5 — становится `agent.bounding` (если не переопределён YAML).

Цвета: оранжевый (`rgba="1 0.3 0.1"` → `#FF4D1A`), синий (`#3333FF`) для колёс, белый для соединительной балки.

---

## 29. Тесты

Сборка тестов (`s2_core/CMakeLists.txt`):

```cmake
add_executable(s2_core_tests
    tests/test_smoke.cpp
    tests/test_types.cpp
    tests/test_shared_state.cpp
    tests/test_sim_bus.cpp
    tests/test_sim_engine.cpp
    tests/test_geometry.cpp
    tests/test_world_snapshot.cpp
    tests/test_snapshot_viz.cpp
    tests/test_plugin_input.cpp
    tests/test_triple_buffer.cpp
    tests/test_ros2_transport.cpp
    tests/test_sim_transport_bridge.cpp
    tests/test_kinematic_tree.cpp
    tests/test_urdf_loader.cpp
    tests/test_joint_vel_plugin.cpp
    tests/test_color_plugin.cpp
    tests/test_collision_system.cpp
    tests/test_gravity_plugin.cpp
    tests/test_lidar_plugin.cpp
    tests/test_zone_system.cpp
    tests/test_effect_modifier.cpp
    tests/test_effect_velocity_addition.cpp
    tests/test_effect_charging.cpp
    tests/test_battery_plugin.cpp
    tests/test_effect_mutation.cpp)
target_link_libraries(s2_core_tests PRIVATE s2_core s2_plugins s2_transport GTest::gtest_main)
target_compile_definitions(s2_core_tests PRIVATE S2_WITH_ROS2)
add_test(NAME s2_core_tests COMMAND s2_core_tests)

add_executable(s2_editor_tests tests/test_scene_writer.cpp)
target_link_libraries(s2_editor_tests PRIVATE s2_core s2_plugins GTest::gtest_main)
add_test(NAME s2_editor_tests COMMAND s2_editor_tests)
```

Все тесты `s2_core_tests` собираются с `S2_WITH_ROS2` (так что `test_ros2_transport` и `test_sim_transport_bridge` могут проверять реальные ROS2-вызовы). `s2_editor_tests` — только базовые библиотеки (без ROS2), чтобы редактор сцены тестировался независимо.

Тестовый URDF `tests/test_urdf.xml` копируется в `${CMAKE_CURRENT_BINARY_DIR}` и его путь подкладывается макросом `S2_TEST_URDF_PATH`.

Запуск из контейнера: `cd build && ctest --output-on-failure` (как в сервисе compose `tests`).

---

## 30. Сводная таблица фаз тика

| Фаза | Что происходит | Где в коде |
|------|----------------|-----------|
| 0    | `paused?` → если да, обновить только `viz_timer_`, опубликовать снапшот | `SimEngine::tick` |
| 0.1  | `sim_time_ += dt_` | `SimEngine::tick` |
| 1    | Акторы (FSM) | **пусто** |
| 2    | `zone_system_.tick(...)`: attached-зоны, enter/exit, MUTATION при входе, MODIFIER/CONTINUOUS поверх SharedState | `ZoneSystem::tick` |
| 3a   | `plugin->pre_resolve(dt, agent)` для каждого плагина | `BatteryPlugin::pre_resolve` |
| 3b/c | own/zone effects CONTINUOUS (фактически совмещены с фазой 2) | — |
| 3d   | `agent.state.resolve()` | `SharedState::resolve` |
| 3e   | Обновить `RaycastEngine::set_dynamic_agents`; для каждого плагина `set_collision_system / set_raycast_engine / update` | `SimEngine::tick`, `LidarPlugin`, `GravityPlugin`, `DiffDrivePlugin`, … |
| 3f   | Кинематика: `body→world` через ZYX-ротацию, добавление `velocity_addition`, обновление позы, нормализация yaw | `SimEngine::tick` |
| 3g   | Surface snap | внутри `GravityPlugin` (фаза 3e) |
| 3h   | Коллизии сферы со статикой: walkable Z push-out, non-walkable XY slide+push-out, max_step_height. `find_support_surface()` → `pitch/roll` агента | `SimEngine::tick` + `CollisionSystem` |
| 3i…l | Joints, KinematicTree update, Sensors, Interactions | **пусто** (соответственно работают `JointVelPlugin` через `update`, sensors через свои плагины) |
| 3m   | `agent.state.clear_contributions()` | `SimEngine::tick` |
| 4    | Attachments | **пусто** |
| 5    | viz publish (`viz_rate`) → `publish_viz()` → `viz_server_->publish(build_snapshot())` | `SimEngine::tick`, `sim_engine_viz_impl.cpp` |
| 6    | transport publish (`transport_rate`) → `post_tick_cb_(world, sim_time)` → `SimTransportBridge::on_post_tick` → `Ros2TransportAdapter::publish_agent_frame` | `SimEngine::tick`, `SimTransportBridge::on_post_tick` |

---

## Резюме границ

- **Что работает по факту:** загрузка сцены из YAML/URDF, тиковый цикл с фиксированным `dt`, set из 11 плагинов агента и 7 эффектов зон, коллизии сферы со статикой (box/sphere/cylinder), GravityPlugin со склонами и трением, RaycastEngine с OBB-боксами/цилиндрами для лидара, ROS2-публикация GNSS / IMU / Odometry / LaserScan / BatteryState через изолированные домены, TF (`earth → map → odom → base_link` плюс mount-фреймы и kinematic-фреймы), HTTP/SSE визуализатор с редактором сцены и runtime-перезагрузкой YAML.
- **Что объявлено, но ничего не делает:** актеры (FSM), `Prop`, `ObjectAttached`/`ActorStateChanged`/`AgentCollision` события, `EffectType::SENSOR`, `sensor_mods`/`visual_hint` (только сериализация), heightmap PNG-режим, `TripleBuffer`, старый `ROS2Transport` (есть тесты, но не подключён в основной путь), `detection_mode = "bounding"`.
- **Дубликаты/архаизмы:** uWebSockets устанавливается в Dockerfile, но не используется (свой POSIX-сервер); WebSocket-handshake реализован, но фронтенд использует SSE; SHA-1 для WS считается через `popen("openssl ...")`; `ros2_transport.hpp/.cpp` — старый MVP, оставлен ради тестов.

Файл сгенерирован вручную по чтению исходников. Все цитаты соответствуют состоянию веток `stable` на 2026-05-04.
