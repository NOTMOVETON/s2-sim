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

### Фича 04.10 — ColorPlugin ✅ ЗАВЕРШЕНА

- **`IAgentPlugin`**: добавлен виртуальный метод `initialize(Agent&)` — вызывается до первого `update()` для запоминания начального состояния агента
- **`SimTransportBridge::init()`**: вызов `plugin->initialize(agent)` после регистрации плагинов
- **`ColorPlugin`**: изменяет `agent.visual.color` на заданное время по запросу ROS2 сервиса (`/set_color`)
  - Принимает запрос `{"color": "#FF0000", "duration": 3.0}`
  - По истечении таймера восстанавливает оригинальный цвет
  - `to_json()` отдаёт текущий цвет и оставшееся время
- **Тесты** (`test_color_plugin.cpp`): 5 тестов — инициализация, вызов сервиса, истечение таймера, нулевая длительность, `to_json()`

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

### Фича 12 — Плагины визуализации данных робота ✅ ЗАВЕРШЕНА

- **`TrajectoryRecorderPlugin`**: записывает и отображает собственную траекторию робота
  - `from_config()`: `record_interval_s`, `max_points`, `color`
  - `update()`: каждые `record_interval_s` секунд добавляет позу в кольцевой буфер
  - `to_json()`: `{"type":"trajectory","points":[...],"color":"#FFAA00"}`
  - Работает без ROS2, полностью автономно
- **`PathDisplayPlugin`**: подписывается на `nav_msgs/Path` и отображает планируемый путь
  - `subscribe_topics()` — новый опциональный метод `IAgentPlugin`
  - `handle_subscription(topic, json)` — получает путь в JSON, обновляет буфер точек
  - В stub-режиме ничего не отображает
- **`IAgentPlugin`**: добавлены `subscribe_topics()` и `handle_subscription()`
- **`SimTransportBridge::init()`**: регистрирует подписки для плагинов с `subscribe_topics()`
- **`Ros2TransportAdapter`**: создаёт `rclcpp::Subscription<nav_msgs::msg::Path>`
- **`app.js`**: `renderOverlayLine(id, points, color)` — рендеринг `THREE.Line` поверх сцены; обработка `type:"trajectory"` и `type:"path"` из `plugins_data`
- **Демо-сцена**: `test_viz_overlay.yaml`

### Фича 13 — Полная документация симуляции S2 ✅ ЗАВЕРШЕНА

- **`workspace/README.md`**: единый самодостаточный файл документации
  - Общий обзор системы, ASCII-диаграмма потоков данных, таблица модулей
  - Ядро симуляции: `SimEngine`, тиковый цикл, `SharedState`, объекты мира
  - Система плагинов: полный интерфейс `IAgentPlugin`, жизненный цикл, реестр, типы плагинов
  - Примеры полных реализаций: `range_sensor`, `gnss`, `imu`, `diff_drive`, `ackermann_drive`, `slope_limiter`, `battery`, `trajectory_recorder`, `path_display`
  - Транспортный слой: `ITransportAdapter`, `SimTransportBridge`, `Ros2TransportAdapter`, инструкция по новому транспорту
  - Визуализатор: `VizServer`, формат `WorldSnapshot`, Three.js фронтенд, инструкция по кастомному клиенту
  - YAML-конфиг сцены: полный справочник всех секций

### Фича 11 — Конфигурация транспорта и визуализатора из YAML ✅ ЗАВЕРШЕНА

#### Что сделано
- **`TransportConfig`** и **`VizConfig`** — новые структуры в `SceneData` (`scene_loader.hpp`)
  - `TransportConfig`: `type` ("ros2"|"stub"), `default_domain_id`
  - `VizConfig`: `enabled` (bool), `port` (int, по умолчанию 8080)
- **`SceneLoader::load()`**: парсинг секций `s2.transport` и `s2.visualizer` из YAML
- **`main.cpp`**: адаптер создаётся по `transport_config.type`; `VizServer` создаётся только при `viz_config.enabled == true`, порт берётся из конфига
- **YAML-сцены** обновлены (test_basic, test_dozer, test_ros2_full): добавлены секции `transport:` и `visualizer:`

## Известные проблемы

### [BUG] Задача 19: reload не переинициализирует ROS2 транспорт

**Симптом:** После загрузки новой сцены через "Scenes → Load" ROS2 топики и ноды не пересоздаются. Если новая сцена имеет других агентов или другие domain_id — транспортный слой остаётся от предыдущей сцены.

**Когда проявляется:** Только при использовании ROS2 транспорта (не stub). При `transport: type: stub` не заметно.

**Обходной путь:** Полный рестарт Docker-контейнера.

**Где искать:** `workspace/s2_visualizer/src/main.cpp` — `SimEngineCommandAdapter::on_load_scene()`. После `engine_->load_world()` нужно вызвать `transport_bridge_->reinit(engine_->world())`, но `transport_bridge_` недоступен в адаптере.

---

### [BUG] Задача 19: resetEditorState() не очищает TF-frames и overlay-линии

**Симптом:** После загрузки новой сцены могут оставаться видимыми TF-frames и trajectory/path линии от агентов предыдущей сцены — до первого SSE-обновления, которое их перезапишет.

**Когда проявляется:** При переключении между сценами с разными агентами.

**Где искать:** `workspace/s2_visualizer/web/js/app.js` — функция `resetEditorState()`. Нужно добавить очистку `tfFrames` и вызов `clearOverlayLines()`.

---

### [BUG] Задача 19: активная сцена не помечается в списке

**Симптом:** В панели "Scenes" нет визуального индикатора текущей загруженной сцены.

**Где искать:** `workspace/s2_visualizer/web/js/app.js` — функция `loadSceneList()`. Нужен эндпоинт или параметр в `/api/scenes`, возвращающий имя активной сцены.

---

### [BUG] Превью нового агента не отображается после размещения в редакторе

**Симптом:** Полупрозрачный бокс нового агента появляется только при изменении любого поля в форме, но не сразу после клика на сцену (после шага размещения).

**Ожидаемое поведение:** После клика на сцену форма открывается с видимым превью-боксом на выбранной позиции. Превью обновляется при изменении полей формы (x, y, yaw, размер, цвет).

**Что было попробовано:**
- Исключение `agent_edit_*` из SSE-очистки мешей в `updateScene` — не помогло.
- Первоначальный вызов `refreshNewAgentPreview()` в конце `openAgentForm` — меш создаётся, но не отображается.

**Предположительная причина:** `refreshNewAgentPreview()` вызывается до того как браузер отрисовывает фрейм, либо что-то в цикле обновления сцены скрывает/удаляет меш раньше первого рендера. Точная причина не установлена.

**Где искать:** [app.js](workspace/s2_visualizer/web/js/app.js) — функции `openAgentForm`, `refreshNewAgentPreview`, `updateScene` (строка ~1050).

### Фича 14 — Патч визуализации статической геометрии ✅ ЗАВЕРШЕНА

- Корректная передача roll/pitch/yaw в GeometrySnapshot
- Параметры cylinder (radius/height) в JSON снапшоте
- Переподключение клиента: `geometrySent` сбрасывается в `onopen`

### Фича 15 — Редактор сцены: CRUD примитивов ✅ ЗАВЕРШЕНА

- **Фронтенд**: кнопка "Edit Scene", panel редактора (box/cylinder/sphere), TransformControls
  для примитивов, panель свойств (цвет, размеры), raycaster по static_ мешам в editor mode,
  `sendGeometryToServer()` и `saveScene()` (POST-запросы)
- **Бэкенд**: `POST /api/scene/geometry` и `POST /api/scene/save` в `VizServer`
- **`SimEngineCommandAdapter`**: `on_update_geometry()` и `on_save_scene()` подключены
- **`SceneWriter::save_geometry()`**: сохранение геометрии в YAML, остальные секции не трогаются
- **Тесты**: `test_scene_writer.cpp` (SceneWriter + SimWorld geometry update), `s2_editor_tests` в CMake

### Фича 16 — Редактор агентов в UI ✅ ЗАВЕРШЕНА

- **Без хардкодов для UI**: каждый плагин объявляет свою схему через `config_schema()`, фронтенд строит формы динамически из `GET /api/plugins/registry`
- **`IAgentPlugin`**: добавлены `display_label()` и `config_schema()` с дефолтными реализациями
- **Все плагины**: реализован `config_schema()` (diff_drive, gnss, imu, trajectory_recorder, path_display, topic_display, joint_vel, color)
- **`list_plugin_schemas()`** в `plugins_registry.cpp` — собирает схемы всех плагинов в JSON
- **`SceneWriter::save_agents()`** — сохранение JSON-массива агентов в YAML с рекурсивным `json_to_yaml()`
- **Новые HTTP-эндпоинты**: `GET /api/plugins/registry`, `GET /api/scene/state`, `GET /api/scene/urdf-list`, `POST /api/scene/agents`
- **`SimEngineCommandAdapter`**: `on_get_scene_state()`, `on_get_urdf_list()`, `on_update_agents()`
- **Фронтенд (index.html + app.js)**: вкладки в editor panel, форма с preview-мешами, raycast в плоскость земли для размещения агентов
- **Тесты**: `SceneWriterAgents` (3 теста), все проходят

### Фича 17 — Edge-snapping примитивов ✅ ЗАВЕРШЕНА

- **`app.js`**: 4 новые функции — `findNearestEdge`, `showEdgeHighlight`, `clearEdgeSnap`, `handleEdgeSnapClick`
- Состояние: `snapEdge1`, `snapEdge2`, `edgeHighlightMesh`, `shiftHeld`
- `findNearestEdge`: для box — 12 рёбер, выбирается ближайший midpoint; для cylinder — top/bottom ring; для sphere — точка на поверхности
- Подсветка: `segment` → жёлтый CylinderGeometry; `ring` → TorusGeometry; `point` → SphereGeometry; всё с `depthTest: false`
- Shift+ЛКМ на ребро → подсветка; второй клик → `delta = edgeMid1 - edgeMid2`, `mesh2.position.add(delta)`
- После snap: `clearEdgeSnap` → `syncPrimitiveFromMesh` → `sendGeometryToServer`
- Бэкенд не изменён — всё на фронтенде

### Фича 18 — Навигация, Undo, Copy/Paste в редакторе ✅ ЗАВЕРШЕНА

- **Shift+LMB pan**: mousedown capture phase; проверяет примитив под курсором; пустое место → ручной pan; примитив → edge-snap (задача 17) без изменений
- **MMB отключён**: `controls.mouseButtons.MIDDLE = null`
- **Undo-стек**: `pushUndoSnapshot` / `undo`, MAX_UNDO=50; точки вызова — `addPrimitive`, `deletePrimitive`, `transformControls.mouseDown`, color/size input
- **Copy/Paste**: `copySelected` (один примитив), `pasteSelected` (смещение +0.5 x/y), `deleteSelected`
- **Клавиши**: Ctrl+Z, Ctrl+C, Ctrl+V, Delete/Backspace — только в editorMode
- Мультиселект (Shift+клик) не реализован — конфликт с edge-snap
- Файл: `workspace/s2_visualizer/web/js/app.js`

### Фича 20 — CollisionSystem ✅ ЗАВЕРШЕНА

- **`collision_system.hpp`**: sphere vs box/sphere/cylinder, ZYX-ротация, `apply_slide`, `find_support_surface`
- **`agent.hpp`**: `has_collision`, `max_slope_rad`, `max_step_height`
- **`scene_loader.hpp`**: парсинг `collision:`, `max_slope_deg:`, `max_step_height:`; URDF collision hierarchy
- **`urdf_loader.hpp/cpp`**: `load_urdf_collision()` — из `<link><collision><geometry>`
- **`sim_engine.hpp`**: фаза 3h — multi-contact, walkable/wall/step_height логика
- **`test_collision.yaml`**: тестовая сцена с пандусами и двумя агентами (с коллизией / без)
- **22 теста**, 254/254 всего

#### Баг-фикс: `obstacle_top_z` для наклонных поверхностей

**Проблема**: `check_sphere_vs_box()` вычислял `obstacle_top_z = primitive_top_z(box)` — глобальный максимум Z всех восьми углов box. Для рампы 18.4° это всегда ~1.05м, хотя реальная высота поверхности у основания рампы ~0.05м. Агент с `max_step_height=0.2` не мог въехать снизу: `1.05 - 0 > 0.2`.

**Исправление** (`check_sphere_vs_box` в `collision_system.hpp`): `obstacle_top_z` теперь вычисляется как Z верхней грани box в проекции центра сферы — transform `(clamp_x, clamp_y, +half_z)` из локальных в мировые координаты. Для горизонтальных box результат идентичен прежнему, для наклонных — корректная локальная высота.

### Фича 20.1 — Выравнивание ориентации агента по поверхности ✅ ЗАВЕРШЕНА

- **`sim_engine.hpp`**: фаза 3h — после обработки коллизий вычисляет `roll` и `pitch` агента из нормали первого walkable-контакта
- Математика: нормаль сначала приводится в тело робота (поворот на -yaw), затем `pitch = atan2(nx_body, nz)`, `roll = atan2(-ny_body, nz)`
- Учёт yaw при вычислении pitch/roll: при вращении на наклонной плоскости pitch и roll корректно меняются местами
- При отсутствии walkable-контакта (воздух, только стены): `roll = pitch = 0`
- Фронтенд уже применял все три угла (`rotation.set(pose.roll, pose.yaw, -pose.pitch, 'YZX')`) — без изменений
- **Тесты**: `SurfaceAlignment_Yaw0_PitchOnly` и `SurfaceAlignment_Yaw90_RollOnly` в `test_sim_engine.cpp`

### Фича 21 — GravityPlugin ✅ ЗАВЕРШЕНА

- **`workspace/s2_plugins/include/s2/plugins/gravity.hpp`**: новый плагин типа Resource
  - `find_support_surface()` каждый тик → grounded/falling
  - Grounded: `pose.z = ground_z`, `fall_velocity = 0`
  - Falling: `fall_velocity -= g * dt`, clamp к `max_fall_speed`, `pose.z += fall_velocity * dt`
  - Защита от проваливания при большом dt: snap к `ground_z` если ушли ниже
  - Всегда: `world_velocity.linear.z() = 0` — запрещает фазе 3f повторно применять Z
  - `to_json()`: `{"grounded": bool, "fall_velocity": double}`
  - `config_schema()`: три параметра — `gravity_accel`, `max_fall_speed`, `grounded_epsilon`
- **`plugin_base.hpp`**: добавлен `set_collision_system(const CollisionSystem*)` — no-op в базе
- **`sim_engine.hpp`**: фаза 3e — `plugin->set_collision_system(&collision_system_)` перед `plugin->update()`
- **`plugins_registry.cpp`**: регистрация `GravityPlugin`
- **`test_gravity_plugin.cpp`**: 5 тестов — FreeFall, LandsOnFloor, StaysOnGround, FallsOffPlatform, MaxFallSpeed
- **`test_gravity_ramp.yaml`**: тестовая сцена — пол, восходящий пандус, платформа на 1м, нисходящий пандус; агент стартует в воздухе и падает на платформу

#### Особенности реализации

- GravityPlugin работает в фазе **3e** (до коллизий 3h) — это позиционный контроллер, не симулятор сил
- Кратковременные мигания `fall_velocity != 0` на переходах геометрий (рампа→пол) — штатный артефакт: raycast в 3e видит геометрию, коллизия в 3h корректирует позу на том же тике
- Тангенциальная составляющая гравитации (ускорение/торможение на склоне) реализована в задаче 20.2

### Фича 20.1 + 20.2 — Выравнивание по поверхности + физика на склоне ✅ ЗАВЕРШЕНА

- **`collision_system.hpp`**: новые структуры `RayHit{z, normal}` и `SupportInfo{ground_z, normal}`
  - `ray_down_vs_box`: slab-метод расширен — отслеживание оси tmin для нормали хитовой грани
  - `ray_down_vs_cylinder`: нормаль крышки или боковой поверхности
  - `ray_down_vs_sphere`: нормаль = `(hit - center).normalized()`
  - `find_support_surface` → `std::optional<SupportInfo>`
  - `rotation_from_pose` перенесён в public (используется SimEngine)
- **`sim_engine.hpp`**: фаза 3f — полная ZYX-ротация body→world вместо yaw-only; DiffDrive двигает вдоль поверхности
- **`sim_engine.hpp`**: фаза 3h — выравнивание roll/pitch из нормали первого walkable-контакта
- **`sim_engine.hpp`**: фаза 3h — walkable контакты: только Z push-out (предотвращает проваливание через платформу), XY push-out пропускается (мешал заезду на рампу на малой скорости)
- **`gravity.hpp`**: линейная модель трения/скольжения с двухрежимным капом
  - `friction_coef_` (0..1, default 0.0): 0=лёд, 1=полное сцепление
  - `slide_velocity_` (Vec3, мировые координаты): накапливается как `g_tangential * (1 - friction) * dt`
  - Скольжение всегда в направлении склона (независимо от yaw робота)
  - Slide добавляется к body velocity через `R^T * slide_world` (поверх DiffDrive)
  - **Два режима капа**: при движении — `drive_speed * (1-friction)`, стоя — `max_fall_speed`
  - Гарантия: при friction > 0 робот ВСЕГДА поднимается (net >= drive * friction)
  - Сброс на плоском полу и в воздухе
  - `to_json()`: `slide_speed`, `friction_coef`
  - `config_schema()`: поле `friction_coef`
- **`test_collision_system.cpp`**: 3 теста (нормали: горизонт, рампа, пустая геометрия)
- **`test_gravity_plugin.cpp`**: 4 новых теста (скольжение с разными friction, плоский пол)
- **`test_gravity_ramp.yaml`**: `friction_coef: 0.5`
- Все тесты проходят (2/2 test suites)

#### Баг-фикс: робот не мог заехать на рампу на малой скорости

**Проблема**: Walkable-контакт с наклонной поверхностью генерировал push-out по нормали (3D). Горизонтальная компонента нормали отталкивала робота назад. При малой скорости (0.1 м/с) откат push-out (0.01м) превышал перемещение за тик (0.002м).

**Первоначальное исправление**: Для walkable-контактов collision response пропускался полностью (`continue`).

#### Баг-фикс: робот проваливался через второй этаж при переходе с рампы

**Проблема**: Полный `continue` для walkable-контактов пропускал в том числе Z push-out. При переходе рампа->платформа сфера проникала в платформу, `find_support_surface` находил первый этаж вместо платформы, GravityPlugin snap'ил робота вниз.

**Исправление**: Для walkable-контактов применяется **только Z push-out**, XY push-out пропускается:
```cpp
agent.world_pose.z += contact.contact_normal.z() * contact.penetration;
continue;
```
Z push-out предотвращает проваливание, XY push-out не мешает заезду на рампу.

#### Эволюция модели трения

**Было (статическое трение Кулона)**: `tan(theta) > friction_coef` → скользит, иначе обнуляет XY-velocity. Баг: на плоском полу `friction > 0` обнуляло velocity DiffDrive (else-блок).

**v2 (линейная модель)**: `slide_accel = g_tangential * (1 - friction)`. Slide накапливается отдельно от DiffDrive velocity, добавляется через `R^T * slide_world`. На плоском полу `g_tangential=0` → slide=0 → привод не затронут.

**v3 (двухрежимный кап)**: При движении slide ограничен `drive_speed * (1-friction)` — робот всегда поднимается если friction > 0. Стоя — кап `max_fall_speed`. Решает проблему накопления slide при длительном подъёме.

### Фича 19 — Браузер сцен и runtime reload ✅ ЗАВЕРШЕНА

- **Bug fix**: `SimEngine::update_static_geometry()` — новый метод, атомарно обновляет `world_` и `collision_system_`. Ранее редактор обновлял только world, коллизии оставались старыми.
- **Bug fix**: `findNearestEdge()` в app.js для box — исправлен маппинг осей (`hy = size.z/2`, `hz = size.y/2`). Позиция edge-snapping подсветки теперь совпадает с реальными рёбрами.
- **HTTP-эндпоинты**: `GET /api/scenes`, `POST /api/scene/load`, `POST /api/scene/save-as`, `POST /api/scene/new`
- **`SimEngineCommandAdapter`**: `on_get_scene_list()`, `on_load_scene()`, `on_save_scene_as()`, `on_new_scene()`; хранит `plugin_factory_` и `scenes_dir_`; reload через `engine_->load_world()` после `SceneLoader::load()`
- **Frontend**: кнопка "Scenes", панель браузера, loading overlay, `loadSceneList()`, `loadScene()`, `saveSceneAs()`, `newScene()`, `resetEditorState()`
- Тесты: 254/254

### Задача 23 — ZoneSystem инфраструктура ✅ ЗАВЕРШЕНА

ZoneShapeType::CYLINDER, EffectPlugin интерфейс, ZoneSystem, ZoneSnapshot, SceneLoader парсинг зон, интеграция в SimEngine. 11 тестов.

### Задача 26 — ChargingEffect + BatteryComponent ✅ ЗАВЕРШЕНА (с рефакторингом)

- `s2_plugins/include/s2/components/battery_component.hpp` — доменный тип в plugins, не в core
- `s2_plugins/include/s2/effects/charging_effect.hpp` — CONTINUOUS; `on_agent_exit` сбрасывает `charging`
- `interfaces/effect_plugin.hpp`: виртуальный `on_agent_exit(SharedState&, ctx)` — no-op в базе
- `zone_system.cpp`: `on_agent_exit` делегирует плагинам, не знает о доменных типах
- `world_snapshot.hpp/cpp`: `battery_level{-1.0}`, `battery_charging{false}` — заполняет BatteryPlugin (задача 32)
- `effects_registry.cpp`: `"charging"` зарегистрирован
- `test_effect_charging.cpp`: 7 тестов; тест 7 — SharedState напрямую
- **Фикс ODR**: локальный `BatteryComponent` в `test_shared_state.cpp` → anonymous namespace

### Задача 25 + пост-релизные улучшения ✅ ЗАВЕРШЕНЫ

**Задача 25 — ConveyorEffect, WindEffect + velocity_addition:**
- `sim_engine.hpp` фаза 3f: `(world_vel + additive) * dt`, Z тоже
- `conveyor_effect.hpp` — MODIFIER, "surface_contact", `direction`+`speed`
- `wind_effect.hpp` — MODIFIER без capabilities, порывы через sin
- Зарегистрированы в `effects_registry.cpp`
- 7 тестов в `test_effect_velocity_addition.cpp`

**Пост-релизные улучшения:**
- `zone_system.cpp`: YAML `required_capabilities` имеют приоритет над дефолтом плагина (раньше плагин всегда перезаписывал)
- `shared_state.hpp`: `clear_contributions()` больше не сбрасывает `effective_` — снапшот теперь видит реальные значения между тиками
- `world_snapshot.hpp/cpp`: поле `velocity_addition` в `AgentSnapshot`; `effective_speed_scale` и `motion_locked` читаются из реального состояния
- `sim_engine.hpp build_snapshot()`: заполняет все три поля из `agent.state.effective()`
- `index.html` + `app.js`: три индикатора в боковой панели — `+Vx/Vy (зона)` (оранжевый), `×скорость (зона)` (синий), `Движение заблокировано` (красный); отображаются только при активном эффекте
- `test_zones.yaml`: явные `required_capabilities` на каждом эффекте, расширенная зона конвейера
- Обновлены 3 теста под новую семантику `clear_contributions()`

### Задача 24 — IceModifier, BoostZone, MotionLockZone ✅ ЗАВЕРШЕНА (включая баг-фиксы)

- **`workspace/s2_plugins/include/s2/effects/ice_modifier.hpp`** — IceModifier (замедление через traction_coefficient, детерминированный шум)
- **`workspace/s2_plugins/include/s2/effects/boost_zone.hpp`** — BoostZone (ускорение через speed_multiplier)
- **`workspace/s2_plugins/include/s2/effects/motion_lock_zone.hpp`** — MotionLockZone (add_lock для любых агентов)
- **`workspace/s2_plugins/include/s2/effects_registry.hpp`** + **`src/effects_registry.cpp`** — фабрика `s2::create_effect(type, params)`
- **`sim_engine.hpp`**: `set_effect_factory()` + `effect_factory_` поле; в `load_world()` вызывается `zone_system_.set_effect_factory(effect_factory_)` до `add_zone()`
- **`main.cpp`**: `engine.set_effect_factory(s2::create_effect)` до `load_world()`
- **`diff_drive.hpp`**: читает `effective().motion_locked` и `effective().speed_scale`; при lock → `Vec3::Zero()` и ранний return; иначе `clamped_linear *= speed_scale`, `clamped_angular *= speed_scale`
- **`test_effect_modifier.cpp`**: 9 тестов (IceModifier_SlowsAgent, NoCapability, NoiseAmplitude, BoostZone_SpeedsUpAgent, MotionLock_BlocksMovement, TwoModifiers_Combined, MotionLock_StopsRegardlessOfBoost, DiffDrive_ReadsEffectiveScale, DiffDrive_MotionLocked)
- **`workspace/s2_config/scenes/test_zones.yaml`**: тестовая сцена (transport: stub, два робота — robot_0 с surface_contact, robot_1 без; зоны ice/boost/lock/charging)
- **2/2 test suites, 100% тестов проходят**

#### Баги, найденные при интеграционном тестировании (пост-релиз)

**Угловая скорость не масштабировалась на льду**: в `diff_drive.hpp` `speed_scale` применялся только к `clamped_linear`. Исправлено: добавлено `clamped_angular *= eff.speed_scale` + повторный clamp.

**Зоны отображались в неверных позициях по высоте**: в `app.js` зоны строили `pose = {x:cx, y:cz, z:-cy}` (pre-трансформация sim→Three.js), но `updateOrCreateMesh` уже делает ту же трансформацию (`position.set(pose.x, pose.z, -pose.y)`). Двойная трансформация давала неверные координаты. Исправлено: зоны теперь передают sim-координаты напрямую `{x:cx, y:cy, z:cz}`.

**Зоны AABB срабатывали не там где видно / скорость долго восстанавливалась после выхода**: в `app.js` при рендере AABB размеры Y и Z переставлялись местами дважды: сначала в коде зоны (`sy=hs[2], sz=hs[1]`), затем в `createGeometry` (`BoxGeometry(size.x, size.z, size.y)`). Итог: физическая зона и визуальная имели разные пропорции. Исправлено: передаём `[hs[0], hs[1], hs[2]]` напрямую, `createGeometry` делает один правильный своп.

### Фича 22 — LidarPlugin ✅ ЗАВЕРШЕНА (включая баг-фиксы 22.2)

- **`LidarScanData`** в `sensor_data.hpp`: seq, angle_min/max, angle_increment, time_increment, scan_time, range_min/max, ranges
- **`SensorOutput`** в `transport_adapter.hpp`: добавлен `std::optional<LidarScanData> lidar_scan` + поле `frame_id` в `SensorRegistration`
- **`RaycastEngine`**: `set_dynamic_agents()` + `dynamic_prims_` для видимости агентов с коллизией
- **`plugin_base.hpp`**: virtual no-op `set_raycast_engine()`, virtual `sensor_frame_id()` (по умолчанию `""`)
- **`sim_engine.hpp`**: поле `raycast_engine_`, dynamic_agents per-tick (исключая текущего), передача в плагины; `handle_plugin_input` матчит по `plugin_key` ИЛИ по `type`
- **`world_snapshot.hpp`** + `world_snapshot.cpp`: `has_collision`, `bounding_type/radius/size` в AgentSnapshot + JSON
- **`lidar.hpp`**: LidarPlugin — raycast 2D, publish timer, mount_link, `sensor_frame_id()` (mount_link или "base_link"), to_json() с lidar_points, has_inputs()/inputs_schema() для управления visible
- **`plugins_registry.cpp`**: регистрация "lidar"
- **`sim_transport_bridge.cpp`**: routing LidarScanData, topic = "/<sensor_name>", `reg.frame_id = plugin->sensor_frame_id()`
- **`ros2_transport_adapter.hpp/cpp`**: `lidar_pubs` + `lidar_frames`, register + publish `sensor_msgs/LaserScan` с корректным frame_id
- **`app.js`**: `renderLidarPoints()` (PointsMaterial), `updateCollisionMesh()` (полупрозрачный MeshBasic), кнопка "Collisions: ON/OFF"
- **`index.html`**: кнопка "Collisions: OFF" в toolbar
- **`test_lidar.yaml`**: тестовая сцена — robot_0 с лидаром, robot_1 с коллизией, 3 стены, цилиндр
- **`test_lidar_plugin.cpp`**: 12 тестов (RaycastEngine dynamic, LidarPlugin hits/miss/minrange/rate/toJson/mountLink/fields)
- Все тесты: 2/2 test suites, 0 failures

#### Баги, найденные в интеграционном тестировании (22.2)

**frame_id LaserScan**: адаптер генерировал `front_lidar_link` вместо `base_link`. Причина: нет передачи mount_link из плагина в адаптер. Исправлено через `SensorRegistration.frame_id` + `IAgentPlugin::sensor_frame_id()`.

**Кнопка "Показывать лучи" не работала**: `plugin_key` для именованного плагина = `"lidar_front_lidar"`, а `handle_plugin_input` матчил только по `type() == plugin_type`. Исправлено: добавлен матч по `plugin_key(*plugin) == plugin_type`.

**Самостоятельное движение робота**: не баг — latch-поведение DiffDrive (стандарт cmd_vel). Явный Stop отправляет нули и сбрасывает флаг.

## Следующие задачи

### Задачи 23–36: Зоны, эффекты, акторы

Написаны подробные task-файлы в `docs/`. Порядок реализации:

```
23 → 24 → 25 → 26 → 27 → 28 → 29 → 30 → 31
                                 ↓
32 → 33 → 34 → 35 → 36
```

- `docs/23-zone-infrastructure.md` — ZoneShape CYLINDER, EffectPlugin, ZoneSystem, ZoneSnapshot ✅ РЕАЛИЗОВАНО
- `docs/24-effect-ice-boost-lock.md` — IceModifier, BoostZone, MotionLockZone + фабрика ✅ РЕАЛИЗОВАНО
- `docs/25-effect-conveyor-wind.md` — ConveyorEffect, WindEffect + velocity_addition в кинематике ✅ РЕАЛИЗОВАНО
- `docs/26-effect-charging.md` — ChargingEffect, BatteryComponent ✅ РЕАЛИЗОВАНО
- `docs/27-effect-tire-puncture.md` — TirePunctureEffect, TirePunctureData
- `docs/28-effect-teleport.md` — TeleportEffect, PendingTeleport, on_agent_exit callback
- `docs/29-zone-ui-editor.md` — Kernel Commands для зон, Three.js ZoneManager, панель инспектора
- `docs/30-zone-visual-effects.md` — VisualHint pipeline, arrows/particles/glow/grid
- `docs/31-sensor-effects.md` — FogEffect, EMInterference, SensorModResolver
- `docs/32-actor-base-door.md` — IActorBehavior, DoorBehavior FSM, DoorOpenerPlugin (proximity)
- `docs/33-pedestrian.md` — PedestrianBehavior, attached presence zone
- `docs/34-conveyor-actor.md` — ConveyorActor с attached ConveyorEffect зоной
- `docs/35-props-attachment.md` — Prop, AttachObjectCommand, GrabberPlugin
- `docs/36-elevator-actor.md` — ElevatorBehavior, ElevatorUserPlugin, Agent Attachment
