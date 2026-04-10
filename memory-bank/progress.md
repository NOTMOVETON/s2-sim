# Progress — S2

## Что работает ✓

### Фича 3.5 — Базовый визуализатор с SSE
- VizServer на POSIX сокетах
- SSE streaming в браузер
- Three.js 3D-сцена с роботами
- Кинематика: локальные скорости преобразуются в мировые с учётом yaw

### Фича 04 — Плагины робота
- IAgentPlugin интерфейс в s2_core
- DiffDrivePlugin — кинематика дифференциального привода
- GnssPlugin — GNSS с GeographicLib::LocalCartesian (WGS84)
- ImuPlugin — гироскоп/акселерометр/yaw
- Реестр плагинов в s2_plugins
- GeoOrigin — единая LLA точка отсчёта на всю сцену
- SceneLoader поддерживает plugins: и geo_origin: из YAML

### Фича 04.5 — Интерактивный визуализатор
- Play/Pause/Reset кнопки
- Клик по роботу → боковая панель с метаданными
- Режим слежения (Follow/Unfollow)
- Перемещение робота мышкой (TransformControls)
- Визуализация выходов плагинов в JSON (аккордеон)
- force_broadcast_latest — мгновенная отправка снапшота после команд

### Фича 04.6 — Управление плагинами из симуляции ✅
- IAgentPlugin: 3 новых метода — has_inputs(), inputs_schema(), handle_input()
- SimEngine: handle_plugin_input() — единая точка входа для любых транспортов
- VizCommandHandler: on_plugin_input() — мост от VizServer к SimEngine
- DiffDrivePlugin: принимает JSON {linear_velocity, angular_velocity}
- viz_server.cpp: обработка command?cmd=plugin_input с URL decode
- WorldSnapshot: plugin_inputs_schemas передаётся в JSON снапшоте
- Фронтенд: кнопка в аккордеоне, динамическая форма по JSON Schema
- TF Frames: checkbox "Show TF frames" в боковой панели, AxesHelper(2.0) привязан к мешу

### Фича 04.7 — Исправления багов плагина управления ✅ ЗАВЕРШЕНО
Все 3 проблемы исправлены, все тесты проходят.

#### Проблема 1: Конфликт ID форм плагинов во фронтенде — ИСПРАВЛЕНО
**Файл:** `workspace/s2_visualizer/web/js/app.js`
**Изменение:** ID формы изменён с `plugin-form-${pluginName}` на `plugin-form-${agentId}-${pluginName}`

#### Проблема 2: Персистентность скорости через SharedState — ИСПРАВЛЕНО
**Файл:** `workspace/s2_plugins/include/s2/plugins/diff_drive.hpp`
**Корень бага:** При external input записывалось `current_data.desired_linear = external_linear_velocity_` в SharedState. На следующий тик `update()` читал `desired_linear` из SharedState и получал external velocity — робот двигался бесконечно.
**Изменение:** При external input НЕ обновляем `current_data.desired_linear / desired_angular`. Только НЕ-external тики обновляют desired из SharedState.

#### Проблема 3: Debug-логи — УДАЛЕНО
**Файлы:** `sim_engine.hpp`, `diff_drive.hpp`
**Изменение:** Удалены все `std::cout` логи

#### Новые тесты
- `TwoAgentsWithZeroAndOneIds` — два агента с ID 0 и 1, команда только robot_1 → robot_0 стоит, robot_1 двигается
- `InputToAgentZero` — команда robot_0 → robot_0 двигается, robot_1 стоит

### UI управления плагином — Send + Stop ✅
**Файл:** `workspace/s2_visualizer/web/js/app.js`, `web/index.html`
- Кнопка-переключатель Start/Stop заменена на две отдельные кнопки: **Send** и **Stop**
- Send — запускает непрерывную отправку команд (20 Гц), перезапускает с новыми значениями если уже запущен
- Stop — немедленно останавливает интервал и отправляет нулевые скорости

### Фича 10.1 — ROS2 транспорт MVP ✅ ЗАВЕРШЕНО
165 тестов. Подписка на `/cmd_vel` с domain isolation. Подробности: `docs/10.1-transport-ros2-mvp.md`.

### Фича 10 — Полный ROS2 транспорт ✅ ЗАВЕРШЕНО
181 тест проходит. Полная интеграция: публикация сенсоров, TF-дерево, per-robot earth→map.

#### Что сделано
- **ITransportAdapter** (`s2_core/include/s2/transport_adapter.hpp`) — transport-agnostic интерфейс
- **SimTransportBridge** (`s2_transport/`) — мост между SimEngine и адаптером
- **Ros2TransportAdapter** (`s2_transport/`) — ROS2 реализация с per-domain нодами
- **TF-дерево с привязкой к точке спавна:**
  - `earth→map` — ECEF позиция точки спавна робота (уникальная для каждого домена)
  - `map→odom` — identity
  - `odom→base_link` — смещение от стартовой позы (робот стартует в 0,0,0 своего odom)
- **Публикация сенсоров:** GNSS @ 10 Гц, IMU @ 100 Гц, Odometry при каждом тике
- **PostTickCallback** в SimEngine @ `transport_rate` (по умолчанию 30 Гц)
- **FastDDS UDP-only конфиг** (`docker/fastdds.xml`) — решает SHM/UDP конфликт между контейнерами
- **Тестовая сцена** `test_ros2_full.yaml` — 3 робота, domain_id 50/51/52, Москва

#### Баги исправлены
- `EXPECT_NE(out->imu, nullptr)` → `EXPECT_TRUE(out->imu.has_value())` (SensorOutput использует optional)
- Transport timer epsilon (`- 1e-9`) — IEEE 754 накопление `0.01 * 10` < `0.1`
- FastDDS SHM vs UDP мисматч — оба контейнера теперь UDP-only

### Фича 04.9 — URDF Loader + JointVelPlugin + Topic/Rate Config + Kinematic Frames Viz ✅ ЗАВЕРШЕНА

#### Что сделано
- **IAgentPlugin**: `output_topic_`, `base_rate_hz_`, `default_publish_rate_hz()`, `set_base_rate()`, `set_output_topic()` — конфигурируемые топик и частота
- **SensorRegistration**: добавлен `topic_override`
- **GnssPlugin**: адаптирован под `default_publish_rate_hz()`
- **SceneLoader**: `urdf:` (загрузка кинематики из URDF с приоритетом над `links:`), `topic:`, `publish_rate_hz:`
- **load_urdf()**: новый модуль `urdf_loader.hpp/cpp` на tinyxml2, BFS обход
- **LinkFrameSnapshot**: в AgentSnapshot, сериализация в JSON, заполнение в build_snapshot()
- **JointVelPlugin**: управление джоинтами через Twist-подобный JSON, реализован `inputs_schema()` для UI
- **app.js**: `updateOrCreateKinematicFrame()`, TF-overlay для всех звеньев kinematic_tree
- **Тесты**: `test_urdf_loader.cpp` (8 тестов), `test_joint_vel_plugin.cpp` (8 тестов)
- **Docker**: `libtinyxml2-dev` в обоих Dockerfile
- **UI**: добавлена поддержка `inputs_schema()` для `JointVelPlugin`
- **Исправления (задача 4.10):**
  - `app.js`: исправлен баг загрузки схем плагинов (не пропускает агентов)
  - `JointVelPlugin`: `inputs_schema()` теперь динамический (по именам джоинтов)
  - `JointVelPlugin`: `handle_input()` поддерживает именованные поля джоинтов
  - Тесты: обновлены тесты `JointVelPlugin`

### Фича 04.9.1 — JointVel UI: корректный поворот базы и рабочее управление скоростями фреймов ✅ ЗАВЕРШЕНО

#### Проблема A: Неверная ориентация кинематических фреймов при вращении
**Файл:** `workspace/s2_visualizer/web/js/app.js`
**Корень бага:** `updateOrCreateKinematicFrame` и `updateOrCreateLinkMesh` использовали `rotation.set(roll, yaw, pitch)` с дефолтным Euler-порядком XYZ → `Rx(roll)*Ry(yaw)*Rz(pitch)`. Правильное преобразование Z-up (sim) → Y-up (Three.js): `Ry(yaw)*Rz(-pitch)*Rx(roll)` — порядок YZX, pitch с инвертированным знаком (т.к. sim Y-ось → Three.js -Z-ось). На агентах без URDF (roll=0, pitch=0) ошибка не проявлялась.
**Изменение:**
```js
// было:
axes.rotation.set(pose.roll || 0, pose.yaw || 0, pose.pitch || 0);
// стало:
axes.rotation.set(pose.roll || 0, pose.yaw || 0, -(pose.pitch || 0), 'YZX');
```
То же изменение в `updateOrCreateLinkMesh`.

#### Проблема B: Не работает управление joint_vel из UI
**Файл:** `workspace/s2_config/scenes/test_dozer.yaml`
**Корень бага:** В конфиге `joint_vel` использовались имена джоинтов (`arm_joint`, `bucket_joint`), тогда как `KinematicTree` хранит звенья по именам link (`arm`, `bucket`). Плагин не находил звенья и команды уходили в никуда.
**Изменение:** Заменены `arm_joint` → `arm`, `bucket_joint` → `bucket` в конфигурации сцены.

#### Новый тест
- `JointNamesMatchUrdfLinks` — регрессионный тест: проверяет что конфигурация с link-name (не joint-name) корректно двигает оба звена.

### Фича 11 — Конфигурация транспорта и визуализатора из YAML ✅ ЗАВЕРШЕНА

#### Что сделано
- **`TransportConfig`** и **`VizConfig`** — новые структуры в `SceneData` (`scene_loader.hpp`)
  - `TransportConfig`: `type` ("ros2"|"stub"), `default_domain_id`
  - `VizConfig`: `enabled` (bool), `port` (int, по умолчанию 8080)
- **`SceneLoader::load()`**: парсинг секций `s2.transport` и `s2.visualizer` из YAML
- **`main.cpp`**: адаптер создаётся по `transport_config.type`; `VizServer` создаётся только при `viz_config.enabled == true`, порт берётся из конфига
- **YAML-сцены** обновлены (test_basic, test_dozer, test_ros2_full): добавлены секции `transport:` и `visualizer:`

## Известные проблемы
Нет открытых критических проблем.

## Следующие задачи
- Фича 05 — Зоны и эффекты (docs/05-zones-effects.md)
