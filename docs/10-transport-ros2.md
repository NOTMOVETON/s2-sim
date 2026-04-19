# Task 10 — Полный ROS2 транспорт ✅ ЗАВЕРШЕНО

> **Предыдущий шаг:** `09-visualizer.md` (визуализатор работает)
> **MVP:** `10.1-transport-ros2-mvp.md` (подписка /cmd_vel)
> **Статус:** Реализовано и протестировано. Все тесты проходят.

## Что реализовано

### Архитектура

```
SimEngine::tick() (100 Hz)
  └─ transport_timer >= 1/transport_rate:
       post_tick_cb_(world_, sim_time_)
         └─ SimTransportBridge::on_post_tick()
              └─ per agent: build AgentSensorFrame
                   └─ ITransportAdapter::publish_agent_frame(frame)
                        └─ Ros2TransportAdapter (per domain):
                             ├─ tf_broadcaster → odom→base_link (каждый кадр)
                             ├─ gnss_pub → /gnss/fix  (если новый seq)
                             ├─ imu_pub  → /imu/data  (если новый seq)
                             └─ odom_pub → /odom      (если новый seq)

Static TF (1 Hz таймер, per domain):
  earth→map  (ECEF позиция точки спавна робота)
  map→odom   (identity — odom совпадает с map в точке спавна)

Inbound:
  /cmd_vel sub → handle_plugin_input(agent_id, "diff_drive", json)
```

### Transport-agnostic интерфейс — `ITransportAdapter`

Файл: `workspace/s2_core/include/s2/transport_adapter.hpp`

Ключевые структуры:
- `AgentSensorFrame` — кадр данных агента (поза + сенсоры с новым seq)
- `SensorOutput` — данные одного сенсора (`std::optional<GnssData/ImuData/DiffDriveData>`)
- `InputTopicDesc` — описание входного топика с callback
- `ServiceDesc` — описание сервиса с handler
- `TransportEvent` — событие от плагина (ArUco, зона и т.д.)

Интерфейс `ITransportAdapter`:
```cpp
set_geo_origin(GeoOrigin)
register_agent(id, domain_id, name, initial_pose)  // initial_pose → earth→map
register_sensor(SensorRegistration)
register_input_topic(InputTopicDesc)
register_service(ServiceDesc)
publish_agent_frame(AgentSensorFrame)
emit_event(TransportEvent)
```

### `SimTransportBridge`

Файл: `workspace/s2_transport/include/s2/sim_transport_bridge.hpp`
Файл: `workspace/s2_transport/src/sim_transport_bridge.cpp`

- `init()` — итерирует агентов, вызывает `register_agent` с начальной позой,
  регистрирует сенсоры, топики команд и сервисы через плагины
- `on_post_tick()` — строит `AgentSensorFrame` (только сенсоры с изменившимся seq),
  вызывает `publish_agent_frame` и `emit_event`

### TF-дерево — привязка к точке спавна

Каждый робот получает `map` и `odom` фреймы, привязанные к его точке спавна:

```
earth → map  (ECEF позиция точки спавна, уникальная для каждого робота)
map   → odom (identity — odom = map в точке спавна)
odom  → base_link (относительное смещение от старта, динамический TF)
```

**earth→map** вычисляется как:
```
ECEF_spawn = ECEF(geo_origin) + R_enu^T * (spawn_x, spawn_y, spawn_z)
```
где `R_enu` — матрица поворота ENU→ECEF при `geo_origin`.

**odom→base_link** публикуется относительно начальной позы:
```
translation = world_pose - initial_pose
rotation    = q_init^-1 * q_current
```
Робот стартует в `(0, 0, 0)` своего odom-фрейма независимо от позиции в сцене.

Изоляция: каждый домен публикует своё `/tf` дерево, Nav2 стек видит только своего робота.

### Публикация сенсоров

- **GNSS** (`/gnss/fix`, `sensor_msgs/NavSatFix`) — lat/lon/alt/covariance, по умолчанию 10 Гц
- **IMU** (`/imu/data`, `sensor_msgs/Imu`) — gyro/accel/orientation, по умолчанию 100 Гц
- **DiffDrive** (`/odom`, `nav_msgs/Odometry`) — pose + twist, публикуется при каждом тике

Частота сенсоров контролируется полем `publish_rate_hz` в YAML-конфиге плагина.
Дублирование подавляется через `seq` — публикуется только при изменении данных.

### FastDDS транспорт

Файл: `docker/fastdds.xml`

Оба контейнера (`sim_ros2` и внешние клиенты) используют UDP-only конфигурацию:
```xml
<type>UDPv4</type>
<useBuiltinTransports>false</useBuiltinTransports>
```
Это устраняет конфликт SHM (shared memory) vs UDP при работе из разных контейнеров.

### Расширение `IAgentPlugin`

Файл: `workspace/s2_core/include/s2/plugin_base.hpp`

Добавлены default no-op методы:
```cpp
virtual std::vector<std::string> command_topics() const { return {}; }
virtual std::vector<std::string> service_names()  const { return {}; }
virtual std::string handle_service(const std::string& svc, const std::string& req) { return "{}"; }
virtual std::vector<TransportEvent> poll_events() { return {}; }
```

### Расширение `SimEngine`

Файл: `workspace/s2_core/include/s2/sim_engine.hpp`

- `Config::transport_rate` (по умолчанию 30 Гц)
- `set_post_tick_callback(PostTickCallback)` — вызывается с частотой transport_rate
- Таймер с epsilon-допуском (`- 1e-9`) для корректной работы при накоплении float

## Тестовая сцена

Файл: `workspace/s2_config/scenes/test_ros2_full.yaml`

3 робота, domain_id 50/51/52, Москва (55.75°N, 37.62°E):
- `robot_0` — pose (0, 0, 0)
- `robot_1` — pose (3, 0, 0), earth→map смещён на ~3м на Восток
- `robot_2` — pose (-3, 0, 0), earth→map смещён на ~3м на Запад

## Запуск и проверка

```bash
# Сборка и запуск
docker compose --project-directory docker up --build sim_ros2

# Проверка TF дерева (domain 50)
ROS_DOMAIN_ID=50 ros2 run tf2_ros tf2_echo earth map
# → нетривиальный трансформ (ECEF), разный для каждого домена

ROS_DOMAIN_ID=50 ros2 run tf2_ros tf2_echo odom base_link
# → (0, 0, 0) в начале, меняется при движении

# Список топиков
ROS_DOMAIN_ID=50 ros2 topic list
# → /gnss/fix, /imu/data, /odom, /tf, /cmd_vel

# Частота сенсоров
ROS_DOMAIN_ID=50 ros2 topic hz /gnss/fix    # → ~10 Hz
ROS_DOMAIN_ID=50 ros2 topic hz /imu/data    # → ~100 Hz

# Координаты GNSS (должны быть ~55.75°N, 37.62°E для robot_0)
ROS_DOMAIN_ID=50 ros2 topic echo /gnss/fix

# Управление роботом
ROS_DOMAIN_ID=50 ros2 topic pub /cmd_vel geometry_msgs/msg/Twist \
  "{linear: {x: 0.5}, angular: {z: 0.0}}"
```

## Файлы

| Файл | Описание |
|------|----------|
| `s2_core/include/s2/transport_adapter.hpp` | ITransportAdapter + структуры данных |
| `s2_core/include/s2/plugin_base.hpp` | command_topics, service_names, poll_events |
| `s2_core/include/s2/sim_engine.hpp` | PostTickCallback + transport_rate |
| `s2_transport/include/s2/sim_transport_bridge.hpp` | SimTransportBridge |
| `s2_transport/src/sim_transport_bridge.cpp` | Реализация |
| `s2_transport/include/s2/ros2_transport_adapter.hpp` | Ros2TransportAdapter |
| `s2_transport/src/ros2_transport_adapter.cpp` | ROS2 реализация |
| `s2_transport/src/ros2_transport_adapter_stub.cpp` | Stub без ROS2 |
| `s2_transport/CMakeLists.txt` | sensor_msgs, nav_msgs, tf2_ros, std_srvs |
| `s2_msgs/` | amen_cmake пакет с PluginCall.srv |
| `docker/fastdds.xml` | UDP-only FastDDS конфигурация |
| `docker/Dockerfile.ros2` | nav_msgs, tf2_ros, std_srvs, std_msgs |
| `docker/docker-compose.yml` | sim_ros2 с fastdds.xml mount |
| `s2_config/scenes/test_ros2_full.yaml` | 3 робота, domain 50/51/52, Москва |
| `s2_visualizer/src/main.cpp` | SimTransportBridge вместо ручного transport |
