# Структура проекта S2

## Корневой уровень

```
s2-sim/
├── AGENTS.md                   # Методологический контракт AI-агентов (= CLAUDE.md)
├── CLAUDE.md                   # Симлинк или дубль AGENTS.md
├── LICENSE
├── .gitignore
├── docker/                     # Среда сборки и запуска
├── docs/                       # Задачи (task-файлы) и архитектурная документация
├── memory-bank/                # Живая память проекта между сессиями
├── workspace/                  # Исходный код (монтируется в контейнер как /workspace)
└── .planning/                  # GSD-планировщик (roadmap, фазы, intel)
```

---

## docker/

```
docker/
├── Dockerfile          # Базовый dev-образ (без ROS2)
├── Dockerfile.ros2     # Образ с ROS2 Jazzy + FastDDS (основной для sim/tests)
├── docker-compose.yml  # Сервисы: dev, build, tests, sim, sim_ros2
└── fastdds.xml         # Конфиг FastDDS для sim_ros2 (host network)
```

Сервисы docker-compose:
- `build` — сборка проекта (cmake + make)
- `tests` — сборка + `ctest --output-on-failure`
- `sim`   — сборка + запуск `s2_sim` со сценой `test_two_robots.yaml`, порт 1937
- `sim_ros2` — запуск с ROS2 в host-сети, сцена `test_zones.yaml`

---

## workspace/ — исходный код

```
workspace/
├── CMakeLists.txt (корневой)
├── s2_core/            # Ядро симуляции (статическая библиотека s2_core)
├── s2_plugins/         # Плагины агентов и реестр эффектов зон
├── s2_transport/       # ROS2-транспорт и мост к движку
├── s2_visualizer/      # HTTP/SSE-сервер + Web-UI + main() точка входа
├── s2_msgs/            # ROS2 msg/srv определения (colcon workspace)
├── s2_msgs_ws/         # Собранные s2_msgs (build artifacts)
├── s2_config/          # Конфигурационные файлы сцен и роботов
└── build/              # Артефакты сборки CMake (gitignored)
```

---

## workspace/s2_core/ — ядро

Статическая библиотека `s2_core`. Не зависит от ROS2.

### Заголовочные файлы (include/s2/)

```
include/s2/
├── types.hpp               # Базовые типы: Vec3, Pose3D, Velocity, CollisionShape,
│                           #   VisualDesc, ZoneShape, EffectType, ActorState
├── agent.hpp               # struct Agent: id, name, pose, velocity, SharedState,
│                           #   plugins, collision, KinematicTree
├── actor.hpp               # struct Actor: id, name, pose, current_state (FSM)
├── prop.hpp                # struct Prop: id, type, pose, visual, movable
├── world.hpp               # class SimWorld: контейнер агентов/пропов/акторов/геометрии
├── world_snapshot.hpp      # WorldSnapshot, AgentSnapshot, ZoneSnapshot и др. (для viz)
├── sim_engine.hpp          # class SimEngine: тиковый цикл, load_world, step/run/stop
├── sim_bus.hpp             # class SimBus: типизированная шина событий; event:: namespace
├── shared_state.hpp        # class SharedState: contributions + resolver + effective
├── zone.hpp                # struct Zone: shape, effects, inside_agents
├── zone_system.hpp         # class ZoneSystem: tick зон, enter/exit, применение эффектов
├── collision_system.hpp    # class CollisionSystem: sphere vs static geometry
├── raycast_engine.hpp      # class RaycastEngine: raycast в статику и динамических агентов
├── kinematic_tree.hpp      # class KinematicTree: иерархия звеньев (URDF links)
├── plugin_base.hpp         # class IAgentPlugin: базовый интерфейс плагина агента
├── transport_adapter.hpp   # class ITransportAdapter, AgentSensorFrame, TransportEvent
├── geo_origin.hpp          # struct GeoOrigin: широта/долгота для GNSS плагина
├── heightmap.hpp           # class Heightmap: высотная карта мира
├── scene_loader.hpp        # class SceneLoader: загрузка YAML-сцены в SceneData
├── scene_writer.hpp        # class SceneWriter: запись геометрии/агентов в YAML
├── sensor_data.hpp         # SensorData и связанные типы данных сенсоров
├── effect_context.hpp      # struct EffectContext: контекст для плагинов эффектов
├── triple_buffer.hpp       # TripleBuffer<T>: lock-free тройная буферизация
├── urdf_loader.hpp         # class UrdfLoader: парсинг URDF → KinematicTree
│
├── interfaces/
│   └── effect_plugin.hpp   # class EffectPlugin: интерфейс плагина зонального эффекта
│                           #   on_init, apply_modifier, apply_continuous, apply_mutation,
│                           #   on_agent_exit, sensor_mods, visual_hint
│
└── components/             # Компоненты SharedState (single-owner поля)
    ├── tire_puncture_data.hpp      # struct TirePunctureData: punctured bool
    ├── pending_teleport.hpp        # struct PendingTeleport: отложенный телепорт
    └── teleport_target_data.hpp    # struct TeleportTargetData: runtime destination
```

### Исходные файлы (src/)

```
src/
├── placeholder.cpp         # Пустой translation unit (для STATIC lib без .cpp)
├── world_snapshot.cpp      # snapshot_to_json(): сериализация WorldSnapshot → JSON
├── sim_engine_viz.cpp      # SimEngine::publish_viz(): отправка снапшота в VizServer
├── kinematic_tree.cpp      # KinematicTree: FK, compute_world_pose
├── urdf_loader.cpp         # UrdfLoader: парсинг XML → KinematicTree
└── zone_system.cpp         # ZoneSystem::tick(), on_agent_enter/exit, apply_active_effects
```

### Тесты (tests/)

```
tests/
├── test_smoke.cpp
├── test_types.cpp
├── test_shared_state.cpp
├── test_sim_bus.cpp
├── test_sim_engine.cpp
├── test_geometry.cpp
├── test_world_snapshot.cpp
├── test_snapshot_viz.cpp
├── test_plugin_input.cpp
├── test_triple_buffer.cpp
├── test_ros2_transport.cpp
├── test_sim_transport_bridge.cpp
├── test_kinematic_tree.cpp
├── test_urdf_loader.cpp
├── test_urdf.xml           # Тестовый URDF-файл
├── test_joint_vel_plugin.cpp
├── test_color_plugin.cpp
├── test_collision_system.cpp
├── test_gravity_plugin.cpp
├── test_lidar_plugin.cpp
├── test_zone_system.cpp
├── test_effect_modifier.cpp
├── test_effect_velocity_addition.cpp
├── test_effect_charging.cpp
├── test_effect_mutation.cpp
├── test_effect_teleport.cpp
├── test_battery_plugin.cpp
└── test_scene_writer.cpp   # Отдельный target s2_editor_tests (без ROS2)
```

---

## workspace/s2_plugins/ — плагины

Статическая библиотека `s2_plugins`. Зависит от `s2_core`.

### Плагины агентов (include/s2/plugins/)

```
include/s2/plugins/
├── plugin_base.hpp         # Заголовок реестра: create_plugin(), list_plugin_schemas()
├── diff_drive.hpp          # DiffDrivePlugin: cmd_vel → linear/angular velocity
├── gnss.hpp                # GnssPlugin: GPS координаты → ROS2 NavSatFix
├── imu.hpp                 # ImuPlugin: ориентация + угловая скорость → ROS2 Imu
├── lidar.hpp               # LidarPlugin: 2D/3D raycast → LaserScan
├── gravity.hpp             # GravityPlugin: Z-притяжение + поддержка поверхности
├── battery.hpp             # BatteryPlugin: разряд батареи → add_scale/add_lock
├── color.hpp               # ColorPlugin: динамическое изменение цвета агента
├── joint_vel.hpp           # JointVelPlugin: управление суставами кинематического дерева
├── trajectory_recorder.hpp # TrajectoryRecorderPlugin: запись траектории
├── path_display.hpp        # PathDisplayPlugin: подписка на /plan, визуализация пути
└── topic_display.hpp       # TopicDisplayPlugin: отображение произвольного ROS2-топика
```

### Эффекты зон (include/s2/effects/)

```
include/s2/effects/
├── ice_modifier.hpp        # IceModifier: MODIFIER — add_scale(0.2) (скользкость)
├── boost_zone.hpp          # BoostZone: MODIFIER — add_scale(>1.0) (ускорение)
├── motion_lock_zone.hpp    # MotionLockZone: MODIFIER — add_lock(true) (стоп-зона)
├── conveyor_effect.hpp     # ConveyorEffect: MODIFIER — add_velocity_addition(direction)
├── wind_effect.hpp         # WindEffect: MODIFIER — add_velocity_addition(wind)
├── charging_effect.hpp     # ChargingEffect: CONTINUOUS — заряжает BatteryComponent
├── tire_puncture.hpp       # TirePunctureEffect: MUTATION — TirePunctureData.punctured=true
└── teleport_effect.hpp     # TeleportEffect: MUTATION — PendingTeleport с destination
```

### Реестры (src/)

```
src/
├── plugins_registry.cpp    # Фабрика плагинов агентов: create_plugin(type, node)
├── effects_registry.cpp    # Фабрика эффектов зон: create_effect(type, params)
├── color.cpp               # ColorPlugin implementation
└── joint_vel.cpp           # JointVelPlugin implementation
```

---

## workspace/s2_transport/ — транспортный слой

Статическая библиотека `s2_transport`. Опционально зависит от ROS2.

```
include/s2/
├── ros2_transport.hpp          # class Ros2Transport: publisher/subscriber обёртки
├── ros2_transport_adapter.hpp  # class Ros2TransportAdapter: реализует ITransportAdapter
└── sim_transport_bridge.hpp    # class SimTransportBridge: мост SimEngine ↔ адаптер

src/
├── ros2_transport.cpp              # Реализация с rclcpp (под флагом S2_WITH_ROS2)
├── ros2_transport_stub.cpp         # Заглушка без ROS2
├── ros2_transport_adapter.cpp      # Реализация с rclcpp
├── ros2_transport_adapter_stub.cpp # Заглушка без ROS2
└── sim_transport_bridge.cpp        # init(), on_post_tick(), регистрация агентов
```

---

## workspace/s2_visualizer/ — визуализатор и точка входа

Исполняемый файл `s2_sim`.

```
src/
├── main.cpp                # Точка входа: загрузка сцены, создание движка, запуск
│                           #   SimEngineCommandAdapter: VizCommandHandler → SimEngine
├── viz_server.hpp          # class VizServer: HTTP + SSE + WebSocket сервер
├── viz_server.cpp          # Реализация VizServer (на raw sockets, без внешних HTTP-lib)
└── sim_engine_viz_impl.cpp # Реализация SimEngine::publish_viz()

web/
├── index.html              # SPA-оболочка
└── js/
    └── app.js              # Three.js визуализация + SSE-клиент + UI (редактор сцен)
```

---

## workspace/s2_msgs/ — ROS2-сообщения

```
s2_msgs/
├── CMakeLists.txt
├── package.xml
└── srv/
    └── PluginCall.srv      # Сервис для вызова плагина: agent_id, plugin_type, json → json
```

---

## workspace/s2_config/ — конфигурация

```
s2_config/
├── robots/
│   └── dozer.urdf          # URDF-модель робота dozer (кинематическое дерево)
└── scenes/
    ├── test_basic.yaml         # Минимальная сцена с одним агентом
    ├── test_two_robots.yaml    # Дефолтная сцена (два робота) — запускается sim
    ├── test_collision.yaml     # Тест коллизий
    ├── test_geometry.yaml      # Тест статической геометрии
    ├── test_gravity_ramp.yaml  # Тест гравитации на рампе
    ├── test_lidar.yaml         # Тест лидара
    ├── test_dozer.yaml         # Тест URDF-агента dozer
    ├── test_ros2.yaml          # Тест ROS2-транспорта
    ├── test_ros2_full.yaml     # Полный тест ROS2
    ├── test_viz_overlay.yaml   # Тест визуальных оверлеев
    └── test_zones.yaml         # Тест зон и эффектов (используется sim_ros2)
```

---

## docs/ — документация задач

```
docs/
├── PROJECT.md              # Краткий обзор проекта
├── ARCHITECTURE.md         # Архитектура системы
├── QUICKSTART.md           # Быстрый старт
├── known_bugs.md           # Известные баги
├── 00-infrastructure.md    # Задача 00: инфраструктура
├── 01-core-types.md        # Задача 01: базовые типы
├── ...
├── 28-effect-teleport.md   # Задача 28: эффект телепорта
├── 29-zone-ui-editor.md    # Задача 29: редактор зон
├── ...
├── 37.1-zone-flat.md       # Задача 37.1 (текущая работа)
├── 37.2-aruco-detector.md
├── 37.3-agent-detector.md
├── 37.4-actor-detector.md
├── 37.5-zone-detector.md
├── 37.6-ray-zone-transit.md
└── zzz05..zzz08-*.md       # Будущие задачи (ещё не начаты)
```

---

## memory-bank/ — живая память

```
memory-bank/
├── projectbrief.md     # Цели, границы и базовые требования
├── productContext.md   # Зачем проект, какую проблему решает
├── activeContext.md    # Текущая работа, активные решения, ближайшие шаги
├── systemPatterns.md   # Архитектурные паттерны, критические пути
├── techContext.md      # Стек, Docker, зависимости, ограничения
└── progress.md         # Что сделано, что осталось, известные проблемы
```

---

## Ключевые зависимости между пакетами

```
s2_visualizer (s2_sim binary)
    └── s2_core
    └── s2_plugins
    └── s2_transport

s2_transport
    └── s2_core
    └── [rclcpp, s2_msgs] (опционально, S2_WITH_ROS2)

s2_plugins
    └── s2_core

s2_core
    └── Eigen3
    └── yaml-cpp
    └── nlohmann_json
    └── GeographicLib
    └── tinyxml2
```

---

## Внешние зависимости

| Зависимость      | Использование                                          |
|------------------|--------------------------------------------------------|
| Eigen3           | Vec3 (Vector3d), матрицы вращения, KinematicTree       |
| yaml-cpp         | Загрузка YAML-сцен, конфиги плагинов и эффектов        |
| nlohmann/json    | Сериализация снапшотов, схемы плагинов, SSE-протокол   |
| GeographicLib    | Конвертация GPS-координат в метрические (GnssPlugin)   |
| tinyxml2         | Парсинг URDF-файлов (UrdfLoader)                       |
| GTest            | Юнит-тесты (ctest)                                     |
| rclcpp / ROS2    | Транспортный слой (опционально, флаг S2_WITH_ROS2)     |
| Three.js         | 3D-визуализация в браузере (web/js/app.js)             |
