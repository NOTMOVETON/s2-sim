# Задача 21 — Плагин гравитации (GravityPlugin)

## Цель

Реализовать плагин `gravity` типа Resource, который симулирует гравитацию для агента.
При отсутствии опоры снизу агент падает вертикально вниз с ускорением `g` до тех пор,
пока не достигнет поверхности (статического примитива снизу).

Дополнительно (задача 20.2): линейная модель трения/скольжения на наклонных поверхностях
с двухрежимным капом.

## Зависимости

- Требует: задача 20 (CollisionSystem, `find_support_surface()`)
- Требует: задача 14 (примитивы сцены существуют)

## Статус: ЗАВЕРШЕНА

## Поведение

```
Каждый тик:
  1. Запросить find_support_surface(agent.pose, bounding_radius)
  2. Вычислить ground_z = support.ground_z + bounding_radius
  3. Если grounded (z <= ground_z + epsilon):
     a. Snap: pose.z = ground_z, fall_velocity = 0
     b. Скольжение по склону (задача 20.2):
        - Вычислить g_tangential из нормали поверхности
        - Накопить slide_velocity += g_tangential * (1 - friction) * dt
        - Кап: при движении — drive*(1-friction), стоя — max_fall_speed
        - Добавить slide к body velocity через R^T
     c. Обнулить world_velocity.z (запретить фазе 3f двигать по Z)
  4. Если не grounded:
     a. fall_velocity -= g * dt (с капом max_fall_speed)
     b. pose.z += fall_velocity * dt
     c. slide_velocity = 0
     d. Защита от проваливания: snap к ground_z если ушли ниже
```

## Конфиг YAML

```yaml
plugins:
  - type: gravity
    gravity_accel: 9.81    # ускорение свободного падения, м/с2 (по умолчанию 9.81)
    max_fall_speed: 20.0   # максимальная скорость падения, м/с (clamp)
    grounded_epsilon: 0.02 # допуск "на поверхности", метры
    friction_coef: 0.5     # коэффициент трения: 0=лёд, 1=полное сцепление (по умолчанию 0.0)
```

## Реализация

**Файл:** `workspace/s2_plugins/include/s2/plugins/gravity.hpp`

### Ключевые поля

```cpp
double gravity_accel_  = 9.81;
double max_fall_speed_ = 20.0;
double epsilon_        = 0.02;
double friction_coef_  = 0.0;          // 0=лёд, 1=полное сцепление
double fall_velocity_  = 0.0;          // вертикальная скорость (< 0 = вниз)
Vec3   slide_velocity_ = Vec3::Zero(); // скорость скольжения (мировые координаты)

const CollisionSystem* collision_ = nullptr;
```

### Модель скольжения (задача 20.2)

Линейная модель с двухрежимным капом:

- `friction_coef` (0..1): линейно масштабирует эффект гравитации на склоне
- `slide_velocity_` в мировых координатах — всегда вдоль склона, независимо от yaw
- Slide добавляется к body velocity: `vel += R^T * slide_world` (поверх DiffDrive)
- **При движении**: slide <= drive_speed * (1 - friction) => net >= drive * friction
- **Стоя**: slide <= max_fall_speed (свободное скатывание)
- На плоском полу: `g_tangential = 0` => slide = 0 => привод работает нормально

### to_json

```cpp
j["grounded"]       = (fall_velocity_ == 0.0);
j["fall_velocity"]  = fall_velocity_;
j["slide_speed"]    = slide_velocity_.norm();
j["friction_coef"]  = friction_coef_;
```

## Инжекция CollisionSystem

**Файл:** `workspace/s2_core/include/s2/plugin_base.hpp`

Метод `set_collision_system(const CollisionSystem*)` добавлен в `IAgentPlugin` как no-op.
GravityPlugin переопределяет его. SimEngine вызывает перед `plugin->update()`.

```cpp
// plugin_base.hpp
virtual void set_collision_system(const CollisionSystem*) {}

// sim_engine.hpp, фаза 3e:
plugin->set_collision_system(&collision_system_);
plugin->update(dt_, agent);
```

## Порядок фаз при наличии гравитации

```
3e. plugin->update() — GravityPlugin: snap Z + скольжение
3f. Кинематика — полная ZYX-ротация body->world (DiffDrive двигает вдоль поверхности)
3h. Коллизии — walkable: Z push-out; стены: горизонтальный slide + push-out
```

GravityPlugin (3e) управляет Z-позицией через snap к поверхности.
Collision (3h) обеспечивает Z push-out для walkable контактов — предотвращает
проваливание через платформу при переходе между поверхностями (рампа->этаж).

## Collision: walkable-контакты

**Файл:** `workspace/s2_core/include/s2/sim_engine.hpp`, фаза 3h

Для walkable-контактов (angle < max_slope_rad) применяется **только Z push-out**:
```cpp
if (walkable)
{
    agent.world_pose.z += contact.contact_normal.z() * contact.penetration;
    continue;  // XY push-out пропускаем: мешает заезду на рампу
}
```

**Зачем Z push-out**: без него при переходе рампа->платформа `find_support_surface`
может не найти платформу (луч стартует ниже поверхности) и snap'ить робота на
первый этаж.

**Зачем пропуск XY push-out**: push-out по нормали наклонной поверхности имеет
горизонтальную компоненту, которая отталкивает робота назад — не позволяет
заехать на рампу на малой скорости.

## Тестовая сцена

**Файл:** `workspace/s2_config/scenes/test_gravity_ramp.yaml`

Сцена: пол + восходящий пандус (0->1м) + платформа (1м) + нисходящий пандус (1->0м).
Агент стартует в воздухе (z=3) над платформой, падает, можно управлять движением.

## Критерии завершения

- [x] Агент с плагином `gravity` падает вниз при z > floor
- [x] Агент приземляется на пол (box z~0) и не проваливается
- [x] Агент приземляется на платформу и стоит на ней
- [x] Если агент съезжает с платформы — падает на пол
- [x] Вертикальная скорость не превышает `max_fall_speed`
- [x] `to_json()` возвращает `grounded`, `fall_velocity`, `slide_speed`, `friction_coef`
- [x] Плагин не влияет на горизонтальное движение на плоском полу
- [x] Агент без плагина `gravity` — поведение не меняется
- [x] Скольжение по склону работает с двухрежимным капом (задача 20.2)
- [x] При friction > 0 робот всегда поднимается в горку
- [x] Переход рампа->платформа без проваливания (Z push-out)

## Тесты

**Файл:** `workspace/s2_core/tests/test_gravity_plugin.cpp`

### Базовые (задача 21)
- `FreeFall` — нет поверхности -> z уменьшается
- `LandsOnFloor` — пол на z=0 -> приземление на z=bounding_radius
- `StaysOnGround` — на поверхности -> z не меняется
- `FallsOffPlatform` — за краем платформы -> начинает падать
- `MaxFallSpeed` — скорость падения ограничена max_fall_speed

### Скольжение (задача 20.2)
- `SlidingOnRamp_ZeroFriction` — friction=0, стоит -> slide нарастает
- `SlidingOnRamp_FullFriction` — friction=1, стоит -> slide=0
- `SlidingOnRamp_PartialFriction` — friction=0.5, стоит -> скользит медленнее
- `FlatFloor_NoSliding_AnyFriction` — плоский пол -> slide=0

### Двухрежимный кап (задача 20.2)
- `DrivingUphill_AlwaysClimbs_WithFriction` — friction=0.5, drive=0.1 -> net > 0
- `DrivingUphill_NoClimb_ZeroFriction` — friction=0, drive=1.0 -> slide поглощает скорость
