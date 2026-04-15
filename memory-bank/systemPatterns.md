# System Patterns — S2

## Архитектура компонентов

### Слой ядра (s2_core)
- **World** — контейнер всех сущностей (агенты, пропы, акторы)
- **SimEngine** — главный цикл симуляции с фиксированным шагом dt
- **SimWorld** — контейнер сущностей для симуляции
- **Agent** — агент с plugins, state, pose, velocity, domain_id
- **WorldSnapshot** — снимок состояния для передачи визуализатору
- **TripleBuffer** — lock-free triple buffer для передачи данных между потоками

### Слой плагинов (s2_plugins)
- **IAgentPlugin** — интерфейс плагина агента (update, from_config, to_json, has_inputs, handle_input)
- **DiffDrivePlugin** — дифференциальный привод; поддерживает latch cmd_vel (команда сохраняется до явного {0,0})
- **GnssPlugin** — GNSS с поддержкой LLA координат (GeographicLib)
- **ImuPlugin** — инерциальный модуль (гироскоп + акселерометр)
- **Реестр плагинов** — создание плагинов по имени типа

### Слой визуализатора (s2_visualizer)
- **VizServer** — HTTP + SSE сервер для передачи данных в браузер (порт 1937)
- **SimEngineVizImpl** — мост между SimEngine и VizServer
- **Web UI** — Three.js сцена с OrbitControls и TransformControls

### Слой транспорта (s2_transport)
- **ROS2Transport** — подписка на `/cmd_vel` в изолированных ROS2-доменах
  - Один `rclcpp::Context` + `SingleThreadedExecutor` + spin-поток на каждый `domain_id`
  - Без глобального `rclcpp::init()` — транспорт сам управляет контекстами
  - Топик: `/cmd_vel` (без префикса), изоляция через DDS domain_id
- **Stub-режим** — заглушка без ROS2 для базовой сборки

## Потоки данных

### UI → Робот
```
[VizServer UI] → POST /command → [SimEngineCommandAdapter]
                                            ↓
                                [SimEngine::handle_plugin_input]
                                            ↓
                                [IAgentPlugin::handle_input]
                                            ↓
                                [DiffDrivePlugin::update]
                                            ↓
                                    [WorldSnapshot]
                                            ↓
                                [VizServer::publish] → SSE → [Browser]
```

### ROS2 → Робот
```
[ROS2 Publisher] → /cmd_vel (в domain X)
                         ↓
              [rclcpp::Context, domain X]
                         ↓
              [rclcpp::Subscription callback]
                         ↓
              [CmdVelCallback → SimEngine::handle_plugin_input]
                         ↓
              [DiffDrivePlugin::handle_input (latch)]
                         ↓
              [DiffDrivePlugin::update → world_velocity]
```

## Критические паттерны

### DiffDrive: latch vs one-shot
External input (от ROS2 или UI) сохраняется до получения новой команды (latch). Сброс флага `has_external_input_` намеренно НЕ производится в `update()`. Для остановки нужно явно отправить `{linear_velocity: 0, angular_velocity: 0}`.

**Почему:** при публикации 30 Гц и sim 100 Гц без latch 2/3 тиков без команды → мерцание скорости.

### DiffDrive: SharedState обратная связь
При external input `current_data.desired_linear/angular` НЕ обновляются из external velocity. Только не-external тики обновляют desired из SharedState. Иначе на следующем тике `update()` прочитает external velocity как desired и снова применит его — бесконечное движение.

### ROS2 Domain isolation
Физическая изоляция через отдельные `rclcpp::Context` с разными `domain_id` (не логическая через имена топиков). Каждый контекст — отдельный DDS participant. Агент в domain 1 физически не видит публикации в domain 0.

### ID конфликт во фронтенде
При нескольких агентах с одинаковыми плагинами ID формы включает agentId: `plugin-form-${agentId}-${pluginName}`.

### Редактор сцены (задачи 14–19)
- Editor mode — клиентский режим, симуляция продолжает тикать
- `editorPrimitives` — клиентская копия геометрии; изменения отправляются через `POST /api/scene/geometry`
- `undoStack` — массив JSON-снапшотов `editorPrimitives` (до 50 операций)
- Shift+LMB для pan вместо средней кнопки (OrbitControls переопределён через capture phase)
- Загрузка сцены = полный перезапуск SimEngine (SimWorld очищается, время сбрасывается)

### CollisionSystem (задача 20)
- Только агенты с `has_collision = true` участвуют в коллизиях
- Иерархия источников коллизии: URDF `<collision>` → YAML `collision:` → нет коллизии
- Slide-реакция: убрать нормальную компоненту velocity, оставить тангенциальную
- Пол = явный box-примитив (нет неявного глобального пола)
- `find_support_surface()` используется GravityPlugin

### GravityPlugin (задачи 21 + 20.2)
- Тип Resource, инжекция CollisionSystem через `set_collision_system` в SimEngine
- Управляет Z-координатой (позиционный snap) и скольжением по склону
- `find_support_surface` → `SupportInfo{ground_z, normal}` — Z и нормаль поверхности
- Линейная модель трения: `slide_accel = g_tangential * (1 - friction_coef) * dt`
- Двухрежимный кап: при движении `slide <= drive_speed * (1-friction)`, стоя `slide <= max_fall_speed`
- `slide_velocity_` в мировых координатах — всегда вдоль склона, независимо от yaw
- Slide добавляется к body velocity: `vel += R^T * slide_world` (поверх DiffDrive)
- `friction_coef = 0`: полное скольжение (лёд); `= 1`: нет скольжения (полное сцепление)
- На плоском полу: `g_tangential = 0` → slide=0 → привод работает нормально
- `max_slope_rad` = проходимость (collision), `friction_coef` = скольжение (gravity) — разные концепции

### CollisionSystem: walkable vs non-walkable (задача 20 + баг-фиксы)
- Walkable: `contact.normal.z >= cos(max_slope_rad)` → только Z push-out, XY push-out пропускается. Без XY push-out робот может заезжать на рампу.
- **Правильная формула Z push-out**: `delta_z = penetration / contact_normal.z` (НЕ `normal.z * penetration`)
  - `nz * p` — только Z-проекция полного вектора. После применения сфера остаётся в рампе на `p * sin²(θ)`.
  - `p / nz` — точный Z-сдвиг для полного снятия проникновения при фиксированных XY.
  - При flat (nz=1): `p / 1 = p * 1` — одинаково. При рампе 18° (nz=0.95): `p/0.95 ≈ 1.05p` vs `0.95p`.
  - Без правильной формулы: остаточное проникновение каждый тик → осцилляция с GravityPlugin snap → шок/дрожание на рампе.
- Non-walkable: стены, крутые склоны → горизонтальный slide + push-out. `max_step_height` позволяет переезжать малые препятствия

### Выравнивание по поверхности (задача 20.1 + bugfix)
- Фаза 3h: roll/pitch из нормали `find_support_surface()` — НЕ из collision contacts.
- **Почему не contacts:** GravityPlugin снапит z точно на поверхность (penetration≈0) → check_sphere_all() возвращает пустой список → alignment видит нет walkable-контактов → pitch=roll=0. Это вызывало фликер (рампа <-> горизонт) каждый тик.
- `find_support_surface` использует down-raycast, надёжен при penetration=0.
- Проверка: `z <= surface_z + 0.05` (допуск) — чтобы не выравниваться в полёте.
- Фаза 3f: полная ZYX-ротация body→world (через `CollisionSystem::rotation_from_pose`)
- DiffDrive двигает робота вдоль поверхности, а не горизонтально

### RaycastEngine: OBB intersection (задача 22.3)

- `build_rotation_transpose(Pose3D, rt[3][3])` — строит матрицу R^T из ZYX-вращения (yaw/pitch/roll)
- `intersect_box`: трансформирует луч в локальное пространство box через R^T → slab-тест по ±half_extent
- `intersect_cylinder`: то же — луч в локальном пространстве цилиндра, тест боковой поверхности вдоль локальной оси Z
- Сфера не требует OBB (инвариантна к вращению)
- Без OBB рампы (pitch≠0) давали AABB z=[center±half], лучи с z за пределами диапазона не попадали

### LidarPlugin (задача 22)
- Инжекция `RaycastEngine` через `IAgentPlugin::set_raycast_engine()` — вызывается SimEngine перед каждым `update()`
- Видит: статика (`static_geometry`) + агенты с `has_collision = true` (текущий агент исключён в SimEngine)
- Динамические агенты: `RaycastEngine::set_dynamic_agents()` каждый тик, заменяет предыдущий набор
- `sensor_frame_id()` — новый виртуальный метод; LidarPlugin возвращает `mount_link_` если задан, иначе `"base_link"`
- TF-фрейм прокидывается через `SensorRegistration.frame_id` → `NodeInfo.lidar_frames[sname]` → `LaserScan.header.frame_id`
- ROS2: `sensor_msgs/LaserScan` на топик `/<sensor_name>` (каждый агент в своём domain_id)
- Визуализация: `THREE.Points` в браузере, управляется полем `visible` через `has_inputs()`/`inputs_schema()`
- Кнопка "Collisions": глобальный тоггл, рисует полупрозрачный `MeshBasicMaterial` по данным `agent.bounding` из снапшота

### plugin_key и handle_plugin_input (паттерн)
- `plugin_key(plugin)` = `type` если sensor_name пустой, иначе `type + "_" + sensor_name`
  (пример: diff_drive → `"diff_drive"`, lidar с name=front_lidar → `"lidar_front_lidar"`)
- Ключ используется в `plugins_data` и `plugin_inputs_schemas` снапшота — гарантирует уникальность при нескольких плагинах одного типа
- `handle_plugin_input(agent_id, plugin_type, json)` матчит по `plugin->type() == plugin_type` **ИЛИ** `plugin_key(*plugin) == plugin_type` — UI отправляет полный ключ, прямые вызовы могут использовать короткий тип

### sensor_frame_id и SensorRegistration.frame_id (паттерн)
- Путь: `IAgentPlugin::sensor_frame_id()` → `sim_transport_bridge` → `SensorRegistration.frame_id` → адаптер
- Дефолт в базе: `""` (адаптер сам решает, обычно fallback `"base_link"`)
- Переопределяется плагинами со специфическим монтажным фреймом
