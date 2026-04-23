# Active Context — S2

## Текущая работа

### Задача 27 — TirePunctureEffect + TirePunctureData ✅ ЗАВЕРШЕНА

Все тесты проходят (2/2 test suites, 100%).

**Что реализовано:**
- `s2_core/include/s2/components/tire_puncture_data.hpp` — `TirePunctureData { bool punctured{false} }` — единый флаг
- `s2_plugins/include/s2/effects/tire_puncture.hpp` — `TirePunctureEffect`: MUTATION, capability "wheeled", `apply_mutation()` устанавливает `punctured = true`, `visual_hint()` "grid" (#AA2200)
- `diff_drive.hpp` — читает TirePunctureData: `clamped_linear *= 0.5`, `clamped_angular += 0.05*sin(time_acc_*15.0)`; member `time_acc_`
- `world_snapshot.hpp` — поле `tire_punctured{false}` в AgentSnapshot
- `sim_engine.hpp::build_snapshot()` — заполняет `tire_punctured` из SharedState
- `world_snapshot.cpp` — сериализация `tire_punctured` в JSON
- `effects_registry.cpp` — тип `"tire_puncture"` зарегистрирован
- `test_effect_mutation.cpp` — 5 тестов: AppliedOnEntry, PersistsAfterExit, NoCapability, DiffDrivePenalty, DiffDriveNoPenaltyWhenOk

**Архитектурные детали:**
- TirePunctureData в s2_core — DiffDrivePlugin (s2_plugins) читает его напрямую
- Упрощённая модель: binary punctured/not punctured, без отдельных шин — будет усложняться позже
- `on_agent_exit()` не переопределяется — прокол необратим

**Следующий шаг:** задача 28 (TeleportEffect, PendingTeleport, on_agent_exit callback).

### BatteryPlugin: разряд + ограничения скорости ✅ ЗАВЕРШЕНО

Все тесты проходят (2/2 test suites, 100%).

**Что реализовано:**
- `plugin_base.hpp` — новый виртуальный метод `pre_resolve(double dt, Agent&)` — no-op по умолчанию
- `sim_engine.hpp` фаза 3a — вызов `plugin->pre_resolve(dt_, agent)` для каждого плагина до `resolve()`
- `battery.hpp::pre_resolve()`:
  - Разряд: `level -= drain_rate_ * dt` при `!charging` (по умолчанию 1%/с)
  - `level <= 0.05` → `add_lock(true, "battery_critical")`
  - `0.05 < level < 0.20` → `add_scale((level-0.05)/0.15, "battery_low")`
  - `level >= 0.20` → нет contribution
- `battery.hpp::from_config()` — `drain_rate`, `low_level`, `critical_level`
- 8 новых тестов (тест 9–16): drain, no-drain при charging, scale contribution, lock contribution, граница 20%, граница 5%, unlock при восстановлении заряда

**Архитектурное решение: `pre_resolve()` вместо `update()`**
`resolve()` вызывается до `plugin->update()`. Если бы BatteryPlugin добавлял contributions в `update()`, DiffDrive уже прочитал бы effective() без них. Решение — `pre_resolve()` в фазе 3a: вызывается до `resolve()`, contributions учитываются в этом же тике.

**Следующий шаг:** задача 27 (TirePunctureEffect, TirePunctureData).

### BatteryPlugin (задача 32) ✅ ЗАВЕРШЁН

Все тесты проходят (2/2 test suites).

**Что реализовано:**
- `s2_core/include/s2/sensor_data.hpp` — добавлен `BatteryData`
- `s2_core/include/s2/transport_adapter.hpp` — `SensorOutput` получил `std::optional<BatteryData> battery`
- `s2_plugins/include/s2/plugins/battery.hpp` — `BatteryPlugin`: initialize, update, contribute_snapshot, to_json, from_config
- `s2_transport/src/sim_transport_bridge.cpp` — "battery" в `is_sensor_plugin()` и ветка в `on_post_tick()`
- `s2_transport/include/s2/ros2_transport_adapter.hpp` — `battery_pubs` в `NodeInfo`
- `s2_transport/src/ros2_transport_adapter.cpp` — регистрация топика и публикация `BatteryState`
- `s2_plugins/src/plugins_registry.cpp` — тип `"battery"` зарегистрирован
- `s2_core/tests/test_battery_plugin.cpp` — 8+8 тестов
- `test_zones.yaml` — robot_0 получил battery плагин (initial_level: 0.5)

### Рефакторинг AgentSnapshot.extra ✅ ЗАВЕРШЁН

Все тесты проходят (326/326, 2/2 test suites).

**Что изменено:**
- `world_snapshot.hpp`: убраны доменные поля `battery_level`, `battery_charging`, `held_objects`; добавлено `nlohmann::json extra = nlohmann::json::object()`
- `plugin_base.hpp`: добавлен `virtual void contribute_snapshot(nlohmann::json& out, const Agent& agent) const {}` + `#include <nlohmann/json.hpp>`
- `sim_engine.hpp`: в `build_snapshot()` вызывается `plugin->contribute_snapshot(as.extra, agent)` для каждого плагина
- `world_snapshot.cpp`: `j.update(agent.extra)` вместо хардкода `battery_level` и др.
- `test_world_snapshot.cpp`, `test_snapshot_viz.cpp`: доменные поля переведены на `agent.extra["battery_level"] = ...`

**Граница:** в `AgentSnapshot` остаются только core-поля (`effective_speed_scale`, `motion_locked`); доменные данные — через `extra`.

### Задача 26 — ChargingEffect + BatteryComponent ✅ ЗАВЕРШЕНА

Все тесты проходят.

**Что реализовано:**
- `s2_plugins/include/s2/components/battery_component.hpp` — `BatteryComponent {level, charging}` (в plugins, не в core)
- `s2_plugins/include/s2/effects/charging_effect.hpp` — CONTINUOUS эффект; `on_agent_exit` сбрасывает `charging=false`
- `interfaces/effect_plugin.hpp`: новый виртуальный метод `on_agent_exit(SharedState&, ctx)` — no-op в базе
- `zone_system.cpp`: `on_agent_exit` вызывает `plugin->on_agent_exit()` для всех эффектов (без знания о BatteryComponent)
- `effects_registry.cpp`: тип `"charging"` зарегистрирован
- `test_effect_charging.cpp`: 7 тестов; тест 7 проверяет SharedState напрямую, не snapshot
- `test_shared_state.cpp`: локальный `BatteryComponent` перемещён в anonymous namespace (фикс ODR violation)

**Архитектурное решение:** `BatteryComponent` живёт в `s2_plugins` — core не знает о доменных типах. `battery_level` попадает в JSON через `extra` (через `BatteryPlugin::contribute_snapshot`, задача 32).

**Следующий шаг:** задача 27 (TirePunctureEffect, TirePunctureData).

### Задача 25 + пост-релизные улучшения ✅ ЗАВЕРШЕНЫ

Все тесты проходят (2/2 test suites, 100%).

**Задача 25 — ConveyorEffect, WindEffect + velocity_addition:**
- `sim_engine.hpp` фаза 3f: `(world_vel + additive) * dt`, Z тоже поддерживает additive.
- `conveyor_effect.hpp` — MODIFIER, "surface_contact", `direction` + `speed`.
- `wind_effect.hpp` — MODIFIER без capabilities, порывы через sin.
- `effects_registry.cpp` — типы `"conveyor"` и `"wind"`.
- 7 тестов в `test_effect_velocity_addition.cpp`.

**Пост-релизные улучшения (сессия после задачи 25):**

1. **YAML `required_capabilities` имеют приоритет над плагином** (`zone_system.cpp`):
   - Раньше плагин всегда перезаписывал capabilities из YAML.
   - Теперь: если YAML указал `required_capabilities` — берётся из YAML; иначе — дефолт плагина.
   - Позволяет переопределять кому действует эффект прямо в сцене.

2. **Отображение зональных эффектов в боковой панели UI** (`index.html`, `app.js`):
   - `+Vx / +Vy (зона)` — оранжевый, конвейер/ветер. Появляется только при ненулевом additive.
   - `×скорость (зона)` — синий, множитель (лёд/буст). Появляется при отклонении от 1.0.
   - `Движение заблокировано` — красный, `motion_lock`. Появляется при блокировке.

3. **Фикс `AgentSnapshot`** (`world_snapshot.hpp`, `world_snapshot.cpp`, `sim_engine.hpp`):
   - `velocity_addition` — добавлено поле, читается из `agent.state.effective()`.
   - `effective_speed_scale` и `motion_locked` — теперь читаются из реального состояния, а не захардкожены.

4. **Фикс `clear_contributions()`** (`shared_state.hpp`):
   - Раньше сбрасывал `effective_` к дефолтам — снапшот всегда видел нули.
   - Теперь очищает только сырые списки; `effective_` сохраняется до следующего `resolve()`.
   - Обновлены 3 теста: `ClearContributionsKeepsEffective`, `ResolverCalledContributionsCleared`, `ContributionsResolved`.

5. **Тестовая сцена `test_zones.yaml`**:
   - Явные `required_capabilities` на каждом эффекте для самодокументирования.
   - Конвейер расширен по оси движения (half_size.x: 5.0), скорость снижена до 1.0 м/с.

**Следующий шаг:** реализовать задачу 26 (ChargingEffect, BatteryComponent).

### Задача 24 — IceModifier, BoostZone, MotionLockZone ✅ ЗАВЕРШЕНА (включая баг-фиксы)

Все тесты проходят (2/2 test suites, 100%). Задача полностью завершена.

**Что реализовано:**
- `IceModifier` — MODIFIER, требует "surface_contact", `traction_coefficient`, детерминированный шум через sin
- `BoostZone` — MODIFIER, `speed_multiplier`, требует "surface_contact"
- `MotionLockZone` — MODIFIER, `add_lock`, применяется ко всем
- `s2::create_effect()` — фабрика эффектов
- `SimEngine::set_effect_factory()` — фабрика передаётся в zone_system_ при каждом load_world()
- `DiffDrivePlugin::update()` — читает `effective().motion_locked` и `effective().speed_scale`; масштабирует обе компоненты (linear + angular)
- 9 тестов в `test_effect_modifier.cpp`
- Тестовая сцена `test_zones.yaml`

**Баги исправлены при интеграционном тестировании:**
- `diff_drive.hpp`: угловая скорость теперь тоже масштабируется на льду (`clamped_angular *= speed_scale`)
- `app.js`: зоны передавали pre-трансформированные координаты в `updateOrCreateMesh`, которая трансформировала их снова — двойной своп давал неверные позиции. Исправлено: передаём sim-координаты напрямую.
- `app.js`: AABB размеры Y/Z менялись местами дважды (в зоне и в `createGeometry`), из-за чего физическая зона не совпадала с визуальной — агент "проваливался" в зону раньше/позже чем видно. Исправлено: передаём raw sim-размеры, `createGeometry` делает один своп.

**Следующий шаг:** реализовать задачу 25 (ConveyorEffect, WindEffect + velocity_addition в кинематике).

### Задача 23 — Инфраструктура зон ✅ ЗАВЕРШЕНА

ZoneShapeType::CYLINDER, EffectPlugin интерфейс, ZoneSystem с enter/exit detection, ZoneSnapshot, SceneLoader парсинг зон, интеграция в SimEngine. 11 тестов.

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
