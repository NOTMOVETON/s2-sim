# techContext.md

> **Зачем нужен этот файл:** фиксировать стек, Docker-процедуры, зависимости и ограничения среды. Новый агент читает этот файл, чтобы понять, как собирать, запускать и отлаживать проект.

## Стек

| Компонент | Версия | Зачем |
|-----------|--------|-------|
| C++ | 17 | Основной язык |
| Eigen3 | 3.4+ | Линейная алгебра (Vec3, матрицы вращения) |
| yaml-cpp | 0.7+ | Конфигурация сцен |
| GTest | 1.12+ | Unit-тесты |
| nlohmann/json | 3.x | Сериализация снапшотов |
| GeographicLib | 1.52 | Геодезические преобразования ENU <-> LLA (WGS84) |
| uWebSockets | latest | WebSocket-сервер визуализатора |
| CMake | 3.16+ | Система сборки |
| ROS2 Jazzy | Jazzy Jalisco | Транспортный слой (опционально, `-DS2_WITH_ROS2=ON`) |
| rmw_fastrtps_cpp | — | ROS2 middleware (FastRTPS), явно указывается в compose |

## Docker

### Базовые образы
- **Без ROS2:** `ubuntu:24.04` (Noble)
  - Пакеты: `build-essential`, `cmake`, `git`, `libeigen3-dev`, `libyaml-cpp-dev`, `nlohmann-json3-dev`, `libgtest-dev`, `libgmock-dev`, `zlib1g-dev`, `libssl-dev`, `pkg-config`, `libgeographic-dev`, `geographiclib-tools`
- **С ROS2:** `docker/Dockerfile.ros2` — multi-stage, Ubuntu Noble + ROS2 Jazzy
  - Добавляет: `ros-jazzy-ros-base`, `ros-jazzy-geometry-msgs`, `ros-jazzy-fastcdr`, `ros-jazzy-fastrtps`, `ros-jazzy-rmw-fastrtps-cpp`

### Сервисы (docker-compose.yml)
- `build` — сборка проекта (без ROS2)
- `tests` — запуск тестов (с ROS2, `S2_WITH_ROS2=ON`)
- `sim` — симуляция с `test_two_robots.yaml`, порт 1937
- `sim_ros2` — симуляция с `test_ros2.yaml`, с ROS2, `network_mode: host`

### Команды
```bash
# Сборка и тесты (основной цикл разработки)
docker compose --project-directory docker up --build build
docker compose --project-directory docker up --build tests
docker compose --project-directory docker up --build sim

# Симуляция с ROS2
docker compose --project-directory docker up --build sim_ros2

# Логи
docker compose --project-directory docker logs sim
```

## Структура проекта
```
workspace/
  s2_core/           # Ядро симулятора
    include/s2/
      agent.hpp          # Agent с plugins, domain_id
      sim_engine.hpp     # Главный цикл, handle_plugin_input()
      plugin_base.hpp    # IAgentPlugin интерфейс (has_inputs, handle_input)
      geo_origin.hpp     # GeoOrigin (LLA точка отсчёта)
      sensor_data.hpp    # GnssData, ImuData, DiffDriveData
      shared_state.hpp   # SharedState с contributions
      sim_bus.hpp        # Шина событий
      world.hpp          # SimWorld
      world_snapshot.hpp # WorldSnapshot
      triple_buffer.hpp  # Thread-safe triple buffer (sim ↔ transport)
    tests/
  s2_plugins/        # Плагины робота
    include/s2/plugins/
      diff_drive.hpp       # Кинематика диф. привода (latch cmd_vel)
      gnss.hpp             # GNSS (GeographicLib)
      imu.hpp              # IMU
    src/
      plugins_registry.cpp # Реестр плагинов
  s2_transport/      # Транспортный слой (ROS2 или stub)
    include/s2/
      ros2_transport.hpp   # ROS2Transport: isolated contexts per domain
    src/
      ros2_transport.cpp       # Реализация с rclcpp (S2_WITH_ROS2)
      ros2_transport_stub.cpp  # Заглушка без ROS2
  s2_visualizer/     # Веб-визуализатор (порт 1937)
  s2_config/         # Конфигурация сцен
    scenes/
      test_two_robots.yaml  # 2 робота, без ROS2
      test_ros2.yaml        # 3 робота, domain_id 0/1/2
  CMakeLists.txt     # Корневой CMake
```

## Ограничения среды
- `-Wall -Wextra -Werror` — все предупреждения как ошибки
- ROS2 — только в `Dockerfile.ros2`, не является зависимостью ядра
- Ядро (`s2_core`, `s2_plugins`) компилируется без ROS2
- `s2_transport` имеет stub-режим (без ROS2) и полную реализацию (`S2_WITH_ROS2=ON`)

## Сборка
```bash
# Внутри контейнера
cd /workspace/build
cmake .. -DS2_WITH_ROS2=ON  # или без флага для stub
make -j$(nproc)
ctest --output-on-failure
```

## Формат YAML конфигурации
```yaml
s2:
  update_rate: 100.0
  viz_rate: 30.0
  world:
    geo_origin: {lat: 55.75, lon: 37.62, alt: 150.0}
    surface: flat
    geometry: []
  agents:
    - name: "robot_0"
      domain_id: 0          # ROS2 DDS domain (изоляция)
      pose: {x: 0.0, y: 0.0, z: 0.0, yaw: 0.0}
      plugins:
        - type: "diff_drive"
          max_linear: 2.0
          max_angular: 1.5
        - type: "gnss"
          noise_std: 0.5
```

## ROS2 Transport — ключевые детали

### Архитектура изоляции доменов
```
register_agent_cmd_vel(agent_id, domain_id):
  → rclcpp::Context::init() с InitOptions::set_domain_id(domain_id)
  → rclcpp::Node с NodeOptions::context(context)
  → SingleThreadedExecutor с ExecutorOptions::context = context
  → Subscription<Twist> на "/cmd_vel" → cmd_callback_

start():
  → один std::thread на каждый уникальный domain_id
  → spin_some(10ms) в цикле пока running_

stop():
  → running_ = false, join все потоки
  → context->shutdown() для каждого домена
  → очистка domain_nodes_
```

### Важно: без глобального rclcpp::init()
Транспорт сам управляет контекстами. Глобальный `rclcpp::init/shutdown` не вызывается.

### Топик
Каждый агент подписан на `/cmd_vel` (без префикса) в своём изолированном DDS-домене.
Изоляция достигается через разные `domain_id`, а не через разные имена топиков.
