# Active Context — S2

## Текущая работа

Задачи 20, 20.1, 20.2 и 21 завершены. Следующая — задача 22 (LidarPlugin).

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
