# Active Context — S2

## Текущая работа

### Задача-дизайн: зоны, эффекты, акторы (задачи 23–36) — документация написана

Написаны 14 подробных task-файлов в `docs/23-*.md` — `docs/36-*.md`.
Покрыты: инфраструктура зон, 6 типов эффектов, UI зон, визуальные FX,
сенсорные эффекты, 4 типа акторов, пропы и лифт. Ни один из этих файлов
ещё **не реализован** — это только проектная документация.

Следующий шаг: реализовать задачу 23 (Zone Infrastructure) как первый шаг.

---

Предыдущий контекст (задачи 20–22): LidarPlugin реализован, пофикшены три бага задачи 22.2.

### OBB-фикс RaycastEngine + наклонные лучи лидара (задача 22.3)

**Проблема 1:** `intersect_box` — AABB, рампы невидимы.
**Исправление:** `build_rotation_transpose()` + трансформация луча в локальное пространство box через R^T → slab-тест по ±half_extent. Аналогично для cylinder.

**Проблема 2:** Лидар на наклонном роботе бросал горизонтальные лучи (Z=0) — пол не виден при подъёме/спуске.
**Исправление** (`lidar.hpp`): локальный угол `a = start_angle + i*step` трансформируется в мировое направление через `R = Rz(yaw)*Ry(pitch)*Rx(roll)`:
```cpp
ray.direction = Vec3{r00 * lx + r01 * ly,
                     r10 * lx + r11 * ly,
                     r20 * lx + r21 * ly};
```
При pitch=roll=0 результат совпадает с прежним поведением.

**Ограничение по URDF:** `dynamic_prims` для агентов — единственный bounding shape (не per-link URDF). Отдельная задача.

### Исправление выравнивания pitch/roll при вращении на склоне (задача 20.1 bugfix)

**Проблема:** pitch и roll не менялись при вращении робота на наклонной плоскости — нормаль поверхности была в мировых координатах, yaw игнорировался.

**Исправление** (`sim_engine.hpp`, фаза 3h): нормаль приводится в тело робота перед вычислением pitch/roll:
```cpp
const double yaw = agent.world_pose.yaw;
const double nx_body =  std::cos(yaw) * n.x() + std::sin(yaw) * n.y();
const double ny_body = -std::sin(yaw) * n.x() + std::cos(yaw) * n.y();
agent.world_pose.pitch = std::atan2( nx_body, n.z());
agent.world_pose.roll  = std::atan2(-ny_body, n.z());
```

**Тесты:** `SurfaceAlignment_Yaw0_PitchOnly` и `SurfaceAlignment_Yaw90_RollOnly` в `test_sim_engine.cpp`.

### Что сделано в текущей сессии (Z push-out для walkable контактов)

**Проблема:** робот проваливался сквозь второй этаж при переходе с рампы на платформу.

**Причина:** walkable-контакты обрабатывались через `continue` — полный пропуск collision response, включая Z push-out. При переходе рампа->платформа:
1. Сфера проникает в платформу — контакт walkable (нормаль вверх)
2. `continue` пропускает весь push-out
3. `find_support_surface` бросает луч из нижней точки сферы, которая оказывается ниже платформы
4. Луч находит первый этаж -> GravityPlugin snap'ит робота вниз

**Исправление:** для walkable-контактов применяется **только Z push-out**, XY push-out пропускается:
```cpp
if (walkable)
{
    agent.world_pose.z += contact.contact_normal.z() * contact.penetration;
    continue;
}
```

- Z push-out поднимает робота над поверхностью при переходе между поверхностями
- XY push-out по-прежнему пропущен — не мешает заезду на рампу на малой скорости
- На плоском полу (нормаль строго вверх) Z push-out эквивалентен полному push-out
- На рампе push-out минимален, т.к. GravityPlugin уже snap'ит Z правильно

### Что сделано в предыдущей сессии (двухрежимное скольжение)

**Проблема:** при движении в гору `slide_velocity_` накапливался без ограничения и рано или поздно превышал скорость привода — робот скатывался назад, даже когда активно ехал.

**Решение: два режима капа скольжения в `gravity.hpp`:**
- **Едет** (`drive_speed > 0`): кап `slide_velocity_` на `drive_speed * (1 - friction_coef)`. Гарантия: `net_speed >= drive_speed * friction_coef`. Робот ВСЕГДА поднимается если friction > 0, даже на скорости 0.1 м/с.
- **Стоит** (`drive_speed ~ 0`): кап на `max_fall_speed` как раньше.

### Архитектурные детали

- GravityPlugin — **позиционный контроллер по Z** + скольжение по склону.
- `max_slope_rad` = проходимость (collision), `friction_coef` = скольжение (gravity). Разные концепции.
- `slide_velocity_` в мировых координатах -> скольжение всегда вдоль склона, независимо от yaw робота.
- DiffDrive перезаписывает velocity каждый тик, slide ДОБАВЛЯЕТСЯ поверх -> нет конфликта.
- `world_velocity.linear.z() = 0` в каждом тике — блокирует double-apply в фазе 3f.
- Фаза 3f использует полную ZYX-ротацию (body->world); при roll=pitch=0 результат идентичен прежнему.
- Выравнивание roll/pitch использует нормаль из коллизий (фаза 3h).
- Walkable-контакты: Z push-out (предотвращает проваливание), XY push-out пропущен (заезд на рампу).

## Что сделано в задаче 22 и баг-фиксах

### Задача 22 — LidarPlugin (реализация)

- `LidarScanData` в SharedState, `lidar_scan` в SensorOutput
- `RaycastEngine::set_dynamic_agents()` — видимость других агентов с коллизией
- `LidarPlugin`: num_rays, min_range, max_range, start_angle, end_angle, mount_link, viz_color; управляющая кнопка visible через `has_inputs()`
- ROS2: LaserScan publisher на топике `/<sensor_name>`
- app.js: `renderLidarPoints()`, `updateCollisionMesh()`, кнопка "Collisions"
- Тестовая сцена `test_lidar.yaml`, 12 новых тестов

### Задача 22.2 — Баг-фиксы после интеграционного тестирования

**Баг 1: frame_id в LaserScan был `front_lidar_link` вместо `base_link`**

Причина: `ros2_transport_adapter.cpp` не знал о mount_link плагина, формировал `frame_id` из sensor_name.

Исправление:
- `SensorRegistration.frame_id` — новое поле, прокидывается от плагина до адаптера
- `IAgentPlugin::sensor_frame_id()` — новый виртуальный метод (по умолчанию `""`)
- `LidarPlugin::sensor_frame_id()` — возвращает `mount_link_` если задан, иначе `"base_link"`
- `sim_transport_bridge.cpp`: `reg.frame_id = plugin->sensor_frame_id()`
- `ros2_transport_adapter`: хранит `lidar_frames[sname]`, использует при публикации

**Баг 2: кнопка "Показывать лучи" не работала**

Причина: `plugin_key("lidar", "front_lidar") = "lidar_front_lidar"`. UI отправлял `plugin=lidar_front_lidar`. `handle_plugin_input` матчил только по `plugin->type() == "lidar"` — не совпадало.

Исправление (`sim_engine.hpp`):
```cpp
if (plugin->type() == plugin_type || plugin_key(*plugin) == plugin_type)
```
Теперь матчит и по типу, и по полному ключу.

**Поведение 3: робот самостоятельно движется**

Не баг — штатное latch-поведение DiffDrive (стандарт ROS2 cmd_vel). После первого `Send` с ненулевой скоростью команда держится навсегда. Сброс — кнопкой "Stop" (отправляет нули).

## Следующие задачи (по порядку реализации)

### Блок A: Визуал и редактор сцены

| # | Файл | Описание |
|---|------|----------|
| 14 | `docs/14-static-geometry-viz.md` | Патч: корректный рендер примитивов (rotation, cylinder, reconnect) |
| 15 | `docs/15-scene-editor-primitives.md` | Editor mode, CRUD примитивов, сохранение YAML |
| 16 | `docs/16-scene-editor-agents.md` | Редактор агентов в UI |
| 17 | `docs/17-scene-editor-facesnap.md` | Edge-snapping примитивов (Shift+LMB) |
| 18 | `docs/18-scene-editor-nav-undo-copy.md` | Shift+LMB pan, Ctrl+Z undo, Ctrl+C/V copy-paste |
| 19 | `docs/19-scene-editor-runtime-load.md` | Браузер сцен, загрузка в рантайме |

### Блок B: Физика

| # | Файл | Описание |
|---|------|----------|
| 20 | `docs/20-collision-system.md` | CollisionSystem, slide-реакция, наклонные плоскости |
| 20.1 | `docs/20.1-surface-alignment.md` | Выравнивание roll/pitch агента по нормали поверхности |
| 21 | `docs/21-gravity-plugin.md` | GravityPlugin: свободное падение, опора, трение, скольжение |
| 20.2 | `docs/20.2-slope-physics.md` | Тангенциальная гравитация + линейное трение + двухрежимный кап |
| 22 | `docs/22-lidar-plugin.md` | LidarPlugin: 2D raycast, LaserScan, визуализация точек |

### Порядок зависимостей

```
14 -> 15 -> 16, 17, 18 (параллельно) -> 19
14 -> 20 -> 21
14 -> 20 -> 22
```

## Открытые архитектурные решения (закрыты)

- Пол = явный примитив box (не heightmap) — для многоуровневых структур
- Collision response = slide (убрать нормальную компоненту velocity)
- Лидар видит всё с коллизией: статику + других агентов
- Загрузка сцены = полный перезапуск симуляции (не горячая замена)
- Gravity = плагин (опционально для каждого агента)
- Walkable collision: Z push-out only (XY мешает заезду на рампу)
