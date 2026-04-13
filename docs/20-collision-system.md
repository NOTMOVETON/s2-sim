# Задача 20 — Система коллизий

## Цель

Реализовать систему коллизий на основе статических примитивов сцены (задача 14).
Коллизии вычисляются в тиковом цикле и влияют на движение агентов.

## Принятые решения

| Вопрос | Решение |
|--------|---------|
| Тип реакции | Slide: убрать нормальную компоненту скорости, оставить тангенциальную |
| Пол | Явный примитив (box/plane) — нет неявного глобального пола |
| Наклонные плоскости | Агент движется вдоль поверхности; угол подъёма ограничен `max_slope_deg` |
| Коллизия агента | Иерархия: URDF `<collision>` → конфиг `collision:` → нет коллизии |
| Невидимые агенты | Если у агента нет коллизии — он не участвует в системе (проезжает сквозь) |
| Другие агенты для лидара | Агенты с коллизией видимы для лидара (задача 22) |

## Архитектура

### Фаза 3h тикового цикла

В `sim_engine.hpp` фаза 3h сейчас пустая заглушка:
```cpp
// 3h. Collision detection — пока пусто
```

После реализации эта фаза будет:
```
3h. Для каждого агента с коллизией:
    - Предсказать следующую позицию (pose + velocity * dt)
    - Проверить коллизию bounding shape с static_geometry
    - Если коллизия: применить slide-реакцию к velocity
    - Обновить pose с откорректированной velocity
```

### CollisionSystem

**Новый файл:** `workspace/s2_core/include/s2/collision_system.hpp`

```cpp
namespace s2 {

/// Результат проверки коллизии одного агента с геометрией.
struct CollisionContact {
    bool has_contact{false};
    Vec3 contact_normal{0, 0, 1};  // нормаль поверхности (от поверхности к агенту)
    double penetration{0.0};        // глубина проникновения (метры)
};

class CollisionSystem {
public:
    /// Установить статическую геометрию сцены.
    void set_static_geometry(const std::vector<WorldPrimitive>& prims);

    /// Проверить коллизию bounding shape агента с геометрией.
    /// Возвращает первый (ближайший) контакт или has_contact=false.
    CollisionContact check(const Pose3D& pose,
                           const CollisionShape& shape) const;

    /// Применить slide-реакцию: убрать нормальную компоненту velocity.
    /// contact_normal должна быть нормализована.
    static Velocity apply_slide(const Velocity& vel,
                                const Vec3& contact_normal);

    /// Проверить, стоит ли агент на поверхности снизу (для гравитации).
    /// Бросает луч вниз (-Z) из позиции агента.
    /// Возвращает высоту поверхности или nullopt если нет поверхности.
    std::optional<double> find_support_surface(const Pose3D& pose,
                                                double bounding_radius) const;

private:
    std::vector<WorldPrimitive> static_prims_;

    CollisionContact check_sphere_vs_box(const Vec3& center,
                                         double radius,
                                         const WorldPrimitive& box) const;

    CollisionContact check_sphere_vs_sphere(const Vec3& center,
                                             double radius,
                                             const WorldPrimitive& sphere) const;

    CollisionContact check_sphere_vs_cylinder(const Vec3& center,
                                               double radius,
                                               const WorldPrimitive& cyl) const;
};

} // namespace s2
```

### Slide-реакция

```cpp
inline Velocity CollisionSystem::apply_slide(const Velocity& vel,
                                              const Vec3& contact_normal) {
    Velocity result = vel;
    // Проекция линейной скорости на нормаль контакта
    double proj = vel.linear.dot(contact_normal);
    if (proj < 0.0) {
        // Убрать компоненту скорости, направленную в поверхность
        result.linear -= contact_normal * proj;
    }
    return result;
}
```

### Интеграция в SimEngine::tick()

**Файл:** `workspace/s2_core/include/s2/sim_engine.hpp`

```cpp
// === Фаза 3h: Коллизии ===
for (auto& agent : world_.agents()) {
    if (!agent.bounding.type != CollisionShape_none) {
        // Предсказать позицию следующего тика
        Vec3 next_pos = agent.world_pose.position()
            + agent.world_velocity.linear * dt_;

        // Проверить коллизию
        auto contact = collision_system_.check(
            Pose3D{next_pos.x(), next_pos.y(), next_pos.z(),
                   agent.world_pose.roll, agent.world_pose.pitch, agent.world_pose.yaw},
            agent.bounding
        );

        if (contact.has_contact) {
            // Slide: откорректировать скорость
            agent.world_velocity = CollisionSystem::apply_slide(
                agent.world_velocity, contact.contact_normal
            );

            // Push-out: вытолкнуть из поверхности
            agent.world_pose.x += contact.contact_normal.x() * contact.penetration;
            agent.world_pose.y += contact.contact_normal.y() * contact.penetration;
            agent.world_pose.z += contact.contact_normal.z() * contact.penetration;
        }
    }
}
```

## Определение коллизионного шейпа агента

### Иерархия источников

1. **URDF `<collision>`** — если URDF загружен и содержит `<collision>` теги, извлечь
   описание шейпа из первого колизионного элемента базового линка (в v1 — один шейп).

2. **YAML `collision:`** — если URDF не задан или не содержит `<collision>`:
   ```yaml
   agents:
     - name: robot_0
       collision:
         bounding:
           type: sphere
           radius: 0.35
   ```

3. **Нет коллизии** — агент невидим для системы коллизий и проезжает сквозь всё.

### Признак отсутствия коллизии

Добавить в `Agent`:
```cpp
struct Agent {
    // ...
    bool has_collision = false;  // false = агент не участвует в коллизиях
    CollisionShape bounding;
};
```

`SceneLoader` устанавливает `has_collision = true` если нашёл `collision:` в YAML или
`<collision>` в URDF.

## Наклонные плоскости

### Алгоритм

При обнаружении коллизии снизу (нормаль контакта направлена вверх под углом `α` к Z):

1. Вычислить угол наклона: `slope_angle = acos(contact_normal.dot({0,0,1}))`
2. Если `slope_angle > agent.max_slope_rad`:
   - Treat как стена — полный slide (убрать нормальную компоненту)
3. Если `slope_angle <= agent.max_slope_rad`:
   - Агент движется по поверхности наклона:
     - Проецировать вектор движения на касательную плоскость наклона
     - Обновить Z-координату агента: `z = surface_z + bounding_radius`
     - Это означает что агент "едет по наклону" — его Z меняется вместе с поверхностью

### Параметр агента max_slope_deg

```yaml
agents:
  - name: robot_0
    max_slope_deg: 20.0   # робот может заезжать на наклоны до 20 градусов
    collision:
      bounding:
        type: sphere
        radius: 0.35
```

По умолчанию `max_slope_deg = 0.0` — робот не может заезжать на наклоны (горизонтальная плоскость).

### Специальный случай: пол

Коллизия снизу (`contact_normal.z > 0.7`) — агент "стоит" на поверхности:
- `velocity.z = 0` (не проваливается)
- `pose.z = surface_z + bounding_radius`

Это базовое поведение без плагина гравитации. С плагином гравитации (задача 21)
логика дополняется свободным падением.

## check_sphere_vs_box: алгоритм

```cpp
inline CollisionContact CollisionSystem::check_sphere_vs_box(
    const Vec3& center, double radius, const WorldPrimitive& box) const
{
    // Учитываем вращение box через Pose3D (yaw)
    // Для v1: только yaw, без pitch/roll (оси-выровненный box в горизонтальной плоскости)
    double cos_yaw = std::cos(-box.pose.yaw);
    double sin_yaw = std::sin(-box.pose.yaw);

    // Перевести center в локальную систему box
    double dx = center.x() - box.pose.x;
    double dy = center.y() - box.pose.y;
    double dz = center.z() - box.pose.z;

    double local_x =  cos_yaw * dx + sin_yaw * dy;
    double local_y = -sin_yaw * dx + cos_yaw * dy;
    double local_z = dz;

    double hx = box.size.x() / 2.0;
    double hy = box.size.y() / 2.0;
    double hz = box.size.z() / 2.0;

    // Ближайшая точка на AABB (в локальных координатах)
    double cx = std::clamp(local_x, -hx, hx);
    double cy = std::clamp(local_y, -hy, hy);
    double cz = std::clamp(local_z, -hz, hz);

    double ex = local_x - cx;
    double ey = local_y - cy;
    double ez = local_z - cz;
    double dist2 = ex*ex + ey*ey + ez*ez;

    if (dist2 >= radius * radius) return {};  // нет коллизии

    double dist = std::sqrt(dist2);
    CollisionContact c;
    c.has_contact = true;
    c.penetration = radius - dist;

    // Нормаль в мировых координатах (из локальной)
    Vec3 local_normal{dist > 1e-6 ? ex/dist : 0.0,
                      dist > 1e-6 ? ey/dist : 1.0,
                      dist > 1e-6 ? ez/dist : 0.0};

    // Вращение обратно в мировые координаты
    c.contact_normal = Vec3{
        cos_yaw * local_normal.x() - sin_yaw * local_normal.y(),
        sin_yaw * local_normal.x() + cos_yaw * local_normal.y(),
        local_normal.z()
    };

    return c;
}
```

## Тестовая сцена с коллизиями

**Файл:** `workspace/s2_config/scenes/test_collision.yaml`

```yaml
s2:
  update_rate: 50
  visualizer:
    enabled: true
    port: 1937
  transport:
    type: stub

  world:
    geometry:
      # Пол
      - type: box
        pose: { x: 0.0, y: 0.0, z: -0.025 }
        size: [40.0, 40.0, 0.05]
        color: "#333333"

      # Стена слева
      - type: box
        pose: { x: -5.0, y: 0.0, z: 0.5 }
        size: [0.2, 6.0, 1.0]
        color: "#4444AA"

      # Цилиндр в центре
      - type: cylinder
        pose: { x: 2.0, y: 2.0, z: 0.5 }
        radius: 0.4
        height: 1.0
        color: "#AA4444"

  agents:
    - name: robot_0
      pose: { x: 0.0, y: 0.0 }
      max_slope_deg: 0.0
      collision:
        bounding:
          type: sphere
          radius: 0.35
      visual:
        type: box
        size: [0.6, 0.4, 0.3]
        color: "#FF6B35"
      plugins:
        - type: diff_drive
          wheel_base: 0.4
          max_linear_vel: 1.5
          max_angular_vel: 2.0
```

## Критерии завершения

- [ ] Агент с `collision: bounding: sphere` не проходит сквозь box-стену
- [ ] Реакция slide: агент скользит вдоль стены (не останавливается резко)
- [ ] Агент без `collision:` проходит сквозь стены (нет регрессии)
- [ ] Агент не проваливается сквозь пол (box z=-0.025, толщина 0.05)
- [ ] Агент скользит по наклонной плоскости при `slope_angle <= max_slope_deg`
- [ ] При `slope_angle > max_slope_deg` — реакция как стена
- [ ] `find_support_surface` используется плагином Gravity (задача 21)

## Тесты

- `CollisionSystem::check` sphere vs box — hit и miss
- `CollisionSystem::check` sphere vs cylinder — hit и miss
- `CollisionSystem::apply_slide` — нормальная компонента убирается, тангенциальная остаётся
- `CollisionSystem::find_support_surface` — есть поверхность / нет поверхности
- Интеграционный тест: агент со скоростью {1,0} движется к стене → скользит вдоль неё

## Зависимости на другие задачи

- Задача 21 (Gravity) использует `find_support_surface()` из этой задачи
- Задача 22 (Lidar) использует `CollisionShape` агента для добавления в raycast-сцену
