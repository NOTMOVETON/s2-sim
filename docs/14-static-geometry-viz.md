# Задача 14 — Визуализация статической геометрии сцены

## Цель

Исправить и довести до рабочего состояния уже частично реализованный механизм отображения
статических примитивов сцены (box, cylinder, sphere) в Three.js визуализаторе.
Примитивы задаются в YAML через секцию `geometry:` и отображаются в браузере как
непроходимая визуальная геометрия. На данном этапе физика коллизий не реализуется —
агенты проезжают сквозь примитивы.

## Текущее состояние

Большинство кода уже написано, но имеет несколько дефектов:

| Компонент | Статус |
|-----------|--------|
| `WorldPrimitive` в `world.hpp` | ✓ реализован |
| `SceneLoader::parse_geometry()` | ✓ парсит YAML |
| `GeometrySnapshot` в `world_snapshot.hpp` | ✓ структура есть |
| `snapshot_to_json()` с `include_geometry` | ✓ сериализует |
| Фронтенд: создание мешей из `data.geometry` | ✓ базово работает |
| **Передача rotation (roll/pitch) в снапшоте** | ✓ исправлено |
| **Переподключение клиента** | ✓ `geometrySent` сбрасывается в `onopen` |
| **Параметры cylinder (radius/height)** | ✓ передаются в JSON |

## Архитектура

```
YAML geometry:          SceneLoader::parse_geometry()
    ↓                           ↓
WorldPrimitive[]   →   SimWorld::static_geometry_
                               ↓
                    build_snapshot() (первый тик)
                               ↓
              WorldSnapshot::geometry (Vec<GeometrySnapshot>)
                               ↓
                    snapshot_to_json(include_geometry=true)
                               ↓
                        SSE → Browser
                               ↓
               app.js: data.geometry → THREE.Mesh
```

## Реализация

### Шаг 1: GeometrySnapshot — добавить rotation ✓

**Файл:** `workspace/s2_core/include/s2/world_snapshot.hpp`

Добавлены поля `yaw`, `pitch`, `roll`:

```cpp
struct GeometrySnapshot {
    std::string type;
    double x{0}, y{0}, z{0};
    double yaw{0}, pitch{0}, roll{0};
    double sx{1}, sy{1}, sz{1};
    double radius{0.5};
    double height{1.0};
    std::string color{"#808080"};
};
```

### Шаг 2: parse_pose() — добавить pitch/roll из YAML ✓

**Файл:** `workspace/s2_core/include/s2/scene_loader.hpp`

`parse_pose()` теперь читает `pitch` и `roll` из YAML-узла (ранее только `yaw`).

### Шаг 3: build_snapshot() — заполнить rotation из WorldPrimitive ✓

**Файл:** `workspace/s2_core/include/s2/sim_engine.hpp`

В `build_snapshot()` заполняются `gs.yaw`, `gs.pitch`, `gs.roll` из `prim.pose`.

### Шаг 4: snapshot_to_json() — добавить roll/pitch в сериализацию ✓

**Файл:** `workspace/s2_core/src/world_snapshot.cpp`

```cpp
j["yaw"]   = geom.yaw;
j["pitch"] = geom.pitch;
j["roll"]  = geom.roll;
j["radius"] = geom.radius;
j["height"] = geom.height;
```

### Шаг 5: Фронтенд — применять rotation и параметры форм ✓

**Файл:** `workspace/s2_visualizer/web/js/app.js`

В блоке `data.geometry.forEach((geom, i) => {...})` передаётся полная поза и параметры форм.

`updateOrCreateMesh` применяет Euler rotation в системе координат Three.js (Y-up):
```js
mesh.rotation.set(pose.roll || 0, pose.yaw || 0, -(pose.pitch || 0), 'YZX');
```

### Шаг 6: Сброс geometrySent при переподключении ✓

**Файл:** `workspace/s2_visualizer/web/js/app.js`

В `evtSource.onopen` сбрасывается `geometrySent = false` и удаляются старые статические меши.

### Шаг 7: Тест сцены с примитивами ✓

**Файл:** `workspace/s2_config/scenes/test_geometry.yaml` — создан.

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
      - type: box
        pose: { x: 3.0, y: 0.0, z: 0.5 }
        size: [2.0, 1.0, 1.0]
        color: "#4444FF"

      - type: cylinder
        pose: { x: -2.0, y: 2.0, z: 1.0 }
        radius: 0.5
        height: 2.0
        color: "#44FF44"

      - type: sphere
        pose: { x: 0.0, y: -3.0, z: 0.5 }
        radius: 0.5
        color: "#FF4444"

      - type: box
        pose: { x: 1.0, y: 1.0, z: 0.25, pitch: 0.3 }
        size: [3.0, 0.5, 0.5]
        color: "#FF8800"

  agents:
    - name: robot_0
      pose: { x: 0.0, y: 0.0 }
      visual:
        type: box
        size: [0.6, 0.4, 0.3]
        color: "#FF6B35"
      plugins:
        - type: diff_drive
          wheel_base: 0.4
          max_linear_vel: 1.0
          max_angular_vel: 1.5
```

## Критерии завершения

- [x] Запустить симуляцию с `test_geometry.yaml`
- [x] В браузере видны box, cylinder, sphere с правильными цветами
- [x] Cylinder отображается с правильным radius и height (не как куб 1×1×1)
- [x] Box с pitch — наклонён в пространстве
- [x] После закрытия и повторного открытия вкладки геометрия снова отображается
- [x] Агент проезжает сквозь примитивы (коллизий пока нет)

## Зависимости

- Предшествует: задача 20 (коллизии), задача 15 (редактор сцены)
- Требует: ничего нового

## Тесты

Юнит-тесты для `snapshot_to_json` с `include_geometry=true`:
- Проверить что box сериализует sx/sy/sz корректно
- Проверить что cylinder сериализует radius/height корректно
- Проверить что sphere сериализует radius корректно
- Проверить что поля yaw/pitch/roll присутствуют в JSON
