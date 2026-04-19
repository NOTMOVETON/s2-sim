# Задача 4.13 — Полная документация симуляции S2

## Цель

Написать исчерпывающую документацию всей системы S2 в виде **одного файла** `README.md` в корне репозитория (`workspace/README.md`). Документация должна быть самодостаточной: читатель, не знакомый с кодовой базой, должен понять как устроена система и как её расширять.

---

## Требования к содержанию

### 1. Общий обзор системы

- Что такое S2, для чего предназначен
- ASCII-диаграмма потоков данных: `main.cpp` → `SimEngine` → `SimWorld` → `VizServer` / `SimTransportBridge` → браузер / ROS2
- Таблица модулей: `s2_core`, `s2_plugins`, `s2_transport`, `s2_visualizer`, `s2_msgs`, `s2_config` — что за что отвечает
- Жизненный цикл запуска от `SceneLoader::load()` до `SimEngine::run()`

### 2. Ядро симуляции (`s2_core`)

#### SimEngine и тиковый цикл

- Конфиг: `update_rate`, `viz_rate`, `transport_rate`
- Точный порядок фаз каждого тика (акторы FSM → зоны → для каждого агента: resolver → плагины → кинематика → очистка → снапшот → транспорт)
- Кинематика: формула перевода локальной скорости в мировые координаты через yaw
- `pause()` / `resume()` / `reset()` / `stop()` — семантика каждого
- `PostTickCallback` — как транспортный мост подключается
- `SimBus` — шина событий, стандартные события

#### SharedState и Resolver

- Три вида вкладов: `add_scale()`, `add_lock()`, `add_velocity_addition()`
- `resolve()` — как вычисляется `effective_speed_scale`, `motion_locked`, `velocity_addition`
- Хранение пользовательских данных: `emplace<T>()` / `get<T>()` — типобезопасный словарь для данных сенсоров

#### Объекты мира

- `SimWorld` — контейнер всех сущностей
- `Agent` — поля: `id`, `name`, `domain_id`, `world_pose`, `world_velocity`, `state`, `plugins`, `kinematic_tree`
- `Pose3D`, `Velocity`, `Vec3` — базовые типы
- `KinematicTree` — дерево суставов из URDF, обновляется каждый тик
- `Prop` — пассивный объект, поля, `attached_to`
- `Actor` — FSM-автомат, поля, примеры переходов
- `Zone` — форма (`SPHERE` / `AABB` / `INFINITE`), список эффектов
- `WorldPrimitive` — статическая геометрия (только визуализация)

### 3. Система плагинов

#### Интерфейс `IAgentPlugin`

Описать все методы с сигнатурами и пояснениями:

- Обязательные: `type()`, `update(dt, agent)`, `from_config(node)`, `to_json()`
- Управление входами: `has_inputs()`, `inputs_schema()`, `handle_input(json)`
- Транспорт: `command_topics()`, `service_names()`, `handle_service()`, `poll_events()`
- Метаданные: `set_sensor_name()`, `set_output_topic()`, `set_base_rate()`, `set_mount_pose()`, `default_publish_rate_hz()`

#### Жизненный цикл плагина

Последовательность вызовов от `SceneLoader::load()` через `SimTransportBridge::init()` до `SimEngine::tick()`.

#### Реестр плагинов

- Паттерн `PluginRegistrar` — статическая регистрация
- `create_plugin(type, node)` — как работает фабрика
- Файл `plugins_registry.cpp`

#### Типы плагинов

Для каждого типа: назначение, что пишет в агента / что читает, когда использовать.

- **Actuator** — пишет в `agent.world_velocity`, читает команды через `handle_input()`
- **Sensor** — читает состояние, пишет в `agent.state.emplace<T>()`, управляет частотой через внутренний таймер
- **Resource** — добавляет вклады в `SharedState` (`add_scale`, `add_lock`)
- **Interaction** — взаимодействует с Prop/Actor через `SimBus`
- **Viz overlay** — только `to_json()`, не влияет на физику

### 4. Примеры плагинов — полные реализации

Для **каждого** примера: полный `.hpp`-файл с кодом, строка регистрации в `plugins_registry.cpp`, фрагмент YAML-конфига, пояснение ключевых моментов.

#### 4.1 Sensor: дальномер `range_sensor`

Ультразвуковой дальномер с гауссовым шумом.

- `from_config()`: `max_range`, `noise_std`
- `update()`: таймер + гауссовый шум + запись в `agent.state.emplace<RangeData>()`
- `to_json()`: JSON с текущим измерением
- Структура `RangeData` с полем `seq` для дедупликации

#### 4.2 Sensor: GNSS-приёмник `gnss`

GPS с шумом и конвертацией в WGS84 (lat/lon).

- Конвертация из метрических координат мира через `GeoOrigin`
- Отдельная частота публикации через таймер
- Публикация через `poll_events()` или прямо в `agent.state`

#### 4.3 Sensor: IMU `imu`

Акселерометр + гироскоп + компас.

- Вычисление линейного ускорения из разницы скоростей за `dt`
- Шум для каждой оси отдельно
- Структура `ImuData` с полями `linear_accel`, `angular_vel`, `orientation`

#### 4.4 Actuator: дифференциальный привод `diff_drive`

- `handle_input(json)`: парсит `{"linear_velocity": x, "angular_velocity": z}`, атомарно сохраняет
- `update()`: читает `agent.state.effective().speed_scale`, пишет в `agent.world_velocity`
- `has_inputs()` → `true`, `inputs_schema()` → JSON Schema для браузера
- `command_topics()` → `{"/cmd_vel"}`

#### 4.5 Actuator: привод Акерманна `ackermann_drive`

Модель рулевого управления (автомобильная кинематика).

- Параметры: `wheelbase` (база), `max_steer_angle`
- Команда: `{"speed": v, "steer_angle": delta}`
- Кинематика: `angular = speed * tan(steer_angle) / wheelbase`
- `update()`: применяет ограничения + пишет в `agent.world_velocity`

#### 4.6 Resource: ограничитель скорости на уклоне `slope_limiter`

- `from_config()`: читает `max_slope_deg`, `min_speed_factor`
- `update()`: вычисляет уклон из `agent.world_pose.pitch`, вычисляет `speed_factor`
- Вызов `agent.state.add_scale(speed_factor, "slope_limiter")`
- Не публикует ничего в транспорт, `to_json()` → пустой объект

#### 4.7 Resource: батарея `battery`

- `from_config()`: `capacity_wh`, `discharge_rate_w`
- `update()`: `charge_ -= discharge_rate_ * dt`, вычисляет `speed_factor = charge_ / capacity_`
- `agent.state.add_scale(speed_factor, "battery")`
- При `charge_ <= 0`: `agent.state.add_lock(true, "battery_dead")`
- `to_json()`: `{"charge_percent": 87.3}`

#### 4.8 Viz overlay: записыватель траектории `trajectory_recorder`

- `from_config()`: `record_interval_s`, `max_points`, `color`
- `update()`: каждые `record_interval_s` секунд добавляет `{agent.world_pose.x, .y, .z}` в кольцевой буфер
- `to_json()`: `{"type":"trajectory","points":[...],"color":"#FFAA00"}`
- Не нужен транспорт, работает автономно

#### 4.9 Viz overlay: отображение пути `path_display`

- `from_config()`: `topic`, `color`, `max_points`
- `subscribe_topics()` → `{topic_}` — новый опциональный метод интерфейса
- `handle_subscription(topic, json)` — получает `nav_msgs/Path` в JSON, обновляет буфер точек
- `to_json()`: `{"type":"path","points":[...],"color":"#00FF00"}`
- `update()` — ничего не делает

### 5. Транспортный слой

#### `ITransportAdapter` — интерфейс

Описать все методы с сигнатурами:

- `start()` / `stop()`
- `set_geo_origin(lat, lon, alt)`
- `register_agent(id, name, domain_id)`
- `register_sensor(agent_id, sensor_type, sensor_name, topic)`
- `register_input_topic(agent_id, topic, callback)`
- `register_service(agent_id, service_name, handler)`
- `register_static_transforms(agent_id, transforms)`
- `publish_agent_frame(frame)`
- `emit_event(event)`

#### `SimTransportBridge`

- `init(geo_origin)`: обходит агентов, регистрирует сенсоры/топики/сервисы
- `on_post_tick(world, sim_time)`: строит `AgentSensorFrame` для каждого агента, публикует через адаптер
- Дедупликация по `seq` — почему важна

#### `Ros2TransportAdapter`

- По одному `rclcpp::Context` + `Node` + `Executor` на каждый `domain_id`
- Потоки: один на домен, выполняет `rclcpp::spin_some()`
- Публикаторы создаются лениво при первом `publish_agent_frame`

#### Как написать новый транспорт

Пошаговая инструкция:

1. Создать класс, унаследованный от `ITransportAdapter`
2. Реализовать все методы интерфейса (минимум — `start/stop`, `publish_agent_frame`)
3. Зарегистрировать в `main.cpp` по типу из `TransportConfig`
4. Краткий пример `StubTransportAdapter` (логирует в stdout)

### 6. Визуализатор

#### `VizServer`

- HTTP + SSE сервер на C++ (без внешних зависимостей)
- Порт, путь к статике, частота обновления
- `/snapshot` SSE-стрим — что отправляется и как часто
- `/command` HTTP endpoint — как браузер отправляет команды (pause/resume/reset, plugin input)

#### `WorldSnapshot` — формат данных

Полное описание JSON-структуры снапшота:

```json
{
  "sim_time": 1.23,
  "agents": [
    {
      "id": 0, "name": "robot_0",
      "pose": {"x":0,"y":0,"z":0,"yaw":0},
      "velocity": {"vx":0,"vy":0,"vz":0,"wz":0},
      "visual": {"type":"box","size":[1,1,0.5],"color":"#FF6B35"},
      "plugins_data": {
        "diff_drive": "{\"has_inputs\":true,...}",
        "gnss": "{\"lat\":55.75,...}"
      },
      "frames": [{"name":"base_link","pose":{...}}]
    }
  ],
  "props": [...],
  "actors": [...],
  "static_geometry": [...]
}
```

#### Three.js фронтенд

- Как `app.js` разбирает снапшот
- Создание/обновление мешей агентов, пропов, геометрии
- Отображение данных плагинов в боковой панели (через `plugins_data`)
- Обработка `type:"trajectory"` и `type:"path"` из плагинов viz overlay — рендеринг `THREE.Line`
- Отправка команд на агента через HTTP POST `/command`

#### Как написать свой клиент визуализации

1. Подключиться к SSE: `EventSource('http://localhost:8080/snapshot')`
2. Парсить JSON снапшоты
3. Рендерить агентов по `pose` и `visual`
4. Отображать данные плагинов из `plugins_data`
5. Отправлять команды через POST `/command?agent_id=0&plugin=diff_drive&data={...}`
6. Пример минимального HTML/JS клиента (~30 строк)

### 7. YAML-конфиг сцены — полный справочник

Полное описание всех секций с примерами:

```yaml
s2:
  update_rate: 100        # Гц, физический шаг
  viz_rate: 30            # Гц, браузер
  transport_rate: 100     # Гц, транспорт

  transport:
    type: ros2            # ros2 | stub
    default_domain_id: 0

  visualizer:
    enabled: true
    port: 8080

  world:
    geo_origin: {lat: 55.75, lon: 37.61, alt: 150.0}
    heightmap: ...
    static_geometry: [...]
    props: [...]
    actors: [...]
    zones: [...]

  agents:
    - name: "robot_0"
      pose: {x: 0, y: 0, yaw: 0}
      domain_id: 0
      urdf: "robot.urdf"
      visual: {type: "box", size: [1, 0.6, 0.4], color: "#FF6B35"}
      plugins:
        - type: "diff_drive"
          max_linear: 1.5
          max_angular: 1.0
        - type: "gnss"
          name: "front"
          publish_rate_hz: 5
          noise_std: 0.1
          mount: {x: 0.2, y: 0, z: 0.3}
        - type: "trajectory_recorder"
          record_interval_s: 0.5
          max_points: 200
          color: "#FFAA00"
```

Для каждой секции: все поля, типы, значения по умолчанию, обязательность.

---

## Требования к оформлению

- **Язык**: русский
- **Файл**: один файл `workspace/README.md`
- **Структура**: заголовки уровня `##` для разделов, `###` для подразделов
- **Код**: все примеры в блоках ` ```cpp ` или ` ```yaml `
- **Диаграммы**: ASCII-art или Mermaid (на выбор автора)
- **Стиль**: концепция → структуры/интерфейс → пример кода → ключевые моменты

---

## Что читать перед написанием

Все исходные файлы находятся в `workspace/`. Ключевые для понимания:

| Файл | Что там |
|------|---------|
| `s2_core/include/s2/plugin_base.hpp` | Полный интерфейс IAgentPlugin |
| `s2_core/include/s2/sim_engine.hpp` | Тиковый цикл, build_snapshot() |
| `s2_core/include/s2/shared_state.hpp` | SharedState, contributions, resolver |
| `s2_core/include/s2/agent.hpp` | Структура Agent, Pose3D, Velocity |
| `s2_core/include/s2/world.hpp` | SimWorld, все сущности |
| `s2_core/include/s2/transport_adapter.hpp` | ITransportAdapter |
| `s2_core/include/s2/scene_loader.hpp` | SceneData, SceneLoader::load() |
| `s2_plugins/src/plugins_registry.cpp` | Как регистрируются плагины |
| `s2_plugins/include/s2/plugins/diff_drive.hpp` | Пример actuator-плагина |
| `s2_plugins/include/s2/plugins/gnss.hpp` | Пример sensor-плагина |
| `s2_transport/include/s2/sim_transport_bridge.hpp` | Транспортный мост |
| `s2_visualizer/src/main.cpp` | Точка сборки системы |
| `s2_visualizer/web/js/app.js` | Three.js фронтенд |
| `s2_config/scenes/test_ros2_full.yaml` | Пример полной конфигурации сцены |
