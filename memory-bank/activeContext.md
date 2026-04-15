# Active Context — S2

## Текущая работа

Задачи 20 (CollisionSystem + баг-фикс `obstacle_top_z`), 20.1 (выравнивание по поверхности) и 21 (GravityPlugin) завершены. Написана спецификация задачи 20.2 (физика на склоне).

Следующая — задача 20.2 (тангенциальная гравитация + статическое трение) или задача 22 (LidarPlugin).

### Что сделано в текущей сессии (задачи 20–21)

**Баг-фикс `obstacle_top_z` (задача 20)**:
- `check_sphere_vs_box()` вычислял глобальный максимум Z box. Для рампы 18.4° это ~1.05м, хотя реальная высота у основания ~0.05м. При низкой скорости сфера касалась торца рампы → контакт не walkable → `step_height` check провален → агент заблокирован.
- Исправлено: `obstacle_top_z` = Z верхней грани в проекции XY центра сферы. `(clamp_x, clamp_y, +half_z)` из локальных → мировые. Горизонтальные box — результат прежний; наклонные — корректная локальная высота.

**Задача 20.1 (выравнивание по поверхности)**:
- `sim_engine.hpp` фаза 3h: после цикла коллизий — `roll = atan2(-n.y, n.z)`, `pitch = atan2(n.x, n.z)` из нормали первого walkable-контакта. При отсутствии опоры → `roll = pitch = 0`.

**Задача 21 (GravityPlugin)**:
- `gravity.hpp`: позиционный контроллер по Z. `find_support_surface()` → grounded/falling. Grounded: snap к `ground_z`, `fall_velocity=0`. Falling: `fall_velocity -= g*dt`, clamp, `pose.z += fall_velocity*dt`. Всегда: `world_velocity.linear.z()=0`.
- `plugin_base.hpp`: виртуальный no-op `set_collision_system()`.
- `sim_engine.hpp` фаза 3e: инжекция `collision_system_` перед `update()` каждого плагина.
- `plugins_registry.cpp`: регистрация GravityPlugin. 5 тестов. `test_gravity_ramp.yaml`.

**Задача 20.2 (спецификация написана)**:
- `docs/20.2-slope-physics.md`: расширить `find_support_surface` → `SupportInfo{ground_z, normal}`; тангенциальная сила `g_tangential` к `world_velocity.xy`; статическое трение (`friction_coef`).

### Архитектурные детали задачи 21

- GravityPlugin — **позиционный контроллер**, не симулятор сил.
- `world_velocity.linear.z() = 0` в каждом тике — блокирует double-apply в фазе 3f.
- Мигания `fall_velocity != 0` на переходах геометрий — штатный артефакт: raycast 3e опережает коллизию 3h.
- Тангенциальная сила (ускорение/торможение на склоне) — **не реализована**, см. задачу 20.2.

### Что сделано в предпоследней сессии (визуальные баги после задачи 20)

- **`app.js`**: исправлен `createGeometry` для box — `BoxGeometry(size.x, size.z, size.y)` вместо `(size.x, size.y, size.z)`.
  Sim Z (высота) → Three.js Y (height), sim Y (глубина) → Three.js Z (depth).
  Баг приводил к тому, что пол (x=40, y=40, z=0.05) стоял вертикально.
- **`app.js`**: исправлен `syncPrimitiveFromMesh` для box — `scale.y → size.z`, `scale.z → size.y`.
- **`test_collision.yaml`**: перестроена сцена с нуля:
  - Пол исправлен: y=0 (вместо y=8.3), размер 20×20
  - Стены исправлены: убраны ошибочные pitch (pitch=-1.38 у левой стены)
  - Пандусы переделаны: `roll` вместо `pitch` (roll наклоняет в плоскости YZ, робот едет вдоль Y)
  - Нижний конец пандусов вровень с полом (z≈0), робот при движении от y=0 касается верхней грани
  - max_slope_deg=20 (вместо 60), max_step_height=0.02

**Архитектурное замечание по пандусам:**
- Для пандусов, на которые робот ВЪЕЗЖАЕТ с уровня пола, нужен `roll` (вокруг X), а не `pitch` (вокруг Y).
- `roll` наклоняет поверхность в YZ-плоскости → робот едет по Y и поднимается по Z.
- `pitch` наклоняет в XZ-плоскости → для въезда вдоль X.
- Нижний торец пандуса должен быть вровень с полом (z≈0) чтобы сфера первой касалась верхней грани, а не торца.

### Что сделано в предпоследней сессии (задача 20)

- **`collision_system.hpp`**: новый inline-заголовок, полная реализация:
  - `check_sphere_all()` — все контакты (sphere vs box/sphere/cylinder), сортировка по penetration
  - `apply_slide()` — static, убирает нормальную компоненту velocity
  - `find_support_surface()` — луч вниз, для GravityPlugin (задача 21)
  - Полная ZYX-ротация для box: поддерживает pitch/roll (наклонные плоскости)
- **`agent.hpp`**: добавлены `has_collision`, `max_slope_rad`, `max_step_height`
- **`scene_loader.hpp`**: парсинг `collision:`, `max_slope_deg:`, `max_step_height:`; URDF collision extraction
- **`urdf_loader.hpp/cpp`**: новая функция `load_urdf_collision()` — из `<link><collision><geometry>`
- **`sim_engine.hpp`**: фаза 3h реализована; `CollisionSystem collision_system_` как член; `set_static_geometry` в `load_world()`
- **`test_collision.yaml`**: тестовая сцена с полом, стенами, цилиндром, пандусами, robot_1 без коллизии
- **`test_collision_system.cpp`**: 22 теста — unit + интеграционные, все проходят
- **Архитектурные решения задачи:**
  - Нет типа "пол" — walkability по нормали грани (`contact_normal.z >= cos(max_slope_rad)`)
  - `max_step_height` — порог ступеньки/стыка поверхностей (по умолчанию 0.0)
  - Стыки одного уровня: obstacle_top == agent_bottom → 0.0 ≤ 0.0 → проезжаем автоматически
  - Провалы без гравитации: агент летит, для падения нужен GravityPlugin (задача 21)
  - Multi-contact: все контакты, сортировка, применение slide последовательно

### Что сделано в предпоследней сессии (задача 18)

- **`app.js`**: Shift+LMB по пустому месту → ручной pan (capture phase, с проверкой примитивов под курсором)
- **`app.js`**: MMB отключён (`controls.mouseButtons.MIDDLE = null`)
- **`app.js`**: Undo-стек — `pushUndoSnapshot()` / `undo()`, MAX_UNDO=50; вызывается в `addPrimitive`, `deletePrimitive`, `transformControls.mouseDown`, `onPrimColorChange`, `onPrimSizeChange`
- **`app.js`**: `copySelected()` / `pasteSelected()` (paste offset +0.5 x/y) / `deleteSelected()`
- **`app.js`**: keydown handler расширен — Ctrl+Z, Ctrl+C, Ctrl+V, Delete/Backspace (только в editorMode)
- Мультиселект через Shift+клик НЕ реализован — Shift+клик на примитив остаётся edge-snap (задача 17)
- Единственный файл изменён: `workspace/s2_visualizer/web/js/app.js`

### Что сделано в предпоследней сессии (задача 17)

Изначально была реализована face-snapping (привязка граней), затем переработана
в edge-snapping (привязка рёбер) по уточнению требований.

- `app.js`: 4 функции edge-snapping:
  - `findNearestEdge` — ближайший midpoint ребра к точке клика (box: 12 рёбер; cylinder: top/bottom ring; sphere: точка поверхности)
  - `showEdgeHighlight` — жёлтая подсветка: segment → CylinderGeometry; ring → TorusGeometry; point → SphereGeometry
  - `clearEdgeSnap` — сброс состояния и dispose подсветки
  - `handleEdgeSnapClick` — основной обработчик Shift+ЛКМ
- Глобальные переменные: `snapEdge1`, `snapEdge2`, `edgeHighlightMesh`, `shiftHeld`
- Обработчики `keydown/keyup` для Shift
- Guard `if (editorMode && shiftHeld)` в click-обработчике
- Бэкенд не изменён

**Важное замечание для задачи 18**: В задаче 18 Shift+LMB планируется для pan.
Конфликт с edge-snapping нужно разрешить: Shift+ЛКМ по пустому месту → pan,
Shift+ЛКМ по примитиву → edge-snap.

### Что сделано в предпоследней сессии (задача 16)
- **IAgentPlugin**: `display_label()` и `config_schema()` — виртуальные методы с дефолтами
- **Все плагины**: реализован `config_schema()` (diff_drive, gnss, imu, trajectory_recorder, path_display, topic_display, joint_vel, color)
- **`list_plugin_schemas()`** в реестре плагинов — создаёт временные экземпляры и собирает схемы
- **`SceneWriter::save_agents()`** — сохранение JSON-массива агентов в YAML, c рекурсивным `json_to_yaml()`
- **Новые HTTP-эндпоинты** в VizServer: `GET /api/plugins/registry`, `GET /api/scene/state`, `GET /api/scene/urdf-list`, `POST /api/scene/agents`
- **`SimEngineCommandAdapter`**: `on_get_scene_state()`, `on_get_urdf_list()`, `on_update_agents()`
- **Фронтенд**: вкладки "Геометрия" / "Агенты" в editor panel; форма добавления агента с динамическими полями плагинов из реестра; preview-меши `agent_edit_*`; размещение кликом по сцене
- **Исправлен баг**: сырые строки `R"([...)"` с `)"` внутри заменены на конкатенацию строк
- Тесты `SceneWriterAgents` (3 теста): SaveAndReload, PreserveGeometry, SaveEmpty — все проходят
- 100% тесты (2/2 test suites)

## Следующие задачи (по порядку реализации)

### Блок A: Визуал и редактор сцены

| # | Файл | Описание |
|---|------|----------|
| 14 | `docs/14-static-geometry-viz.md` | ✅ Патч: корректный рендер примитивов (rotation, cylinder, reconnect) |
| 15 | `docs/15-scene-editor-primitives.md` | ✅ Editor mode, CRUD примитивов, сохранение YAML |
| 16 | `docs/16-scene-editor-agents.md` | Редактор агентов в UI |
| 17 | `docs/17-scene-editor-facesnap.md` | ✅ Edge-snapping примитивов (Shift+LMB) |
| 18 | `docs/18-scene-editor-nav-undo-copy.md` | Shift+LMB pan, Ctrl+Z undo, Ctrl+C/V copy-paste |
| 19 | `docs/19-scene-editor-runtime-load.md` | Браузер сцен, загрузка в рантайме |

### Блок B: Физика

| # | Файл | Описание |
|---|------|----------|
| 20 | `docs/20-collision-system.md` | ✅ CollisionSystem, slide-реакция, наклонные плоскости |
| 20.1 | `docs/20.1-surface-alignment.md` | ✅ Выравнивание roll/pitch агента по нормали поверхности |
| 21 | `docs/21-gravity-plugin.md` | ✅ GravityPlugin: свободное падение и опора |
| 20.2 | `docs/20.2-slope-physics.md` | Тангенциальная гравитация + статическое трение |
| 22 | `docs/22-lidar-plugin.md` | LidarPlugin: 2D raycast, LaserScan, визуализация точек |

### Порядок зависимостей

```
14 → 15 → 16, 17, 18 (параллельно) → 19
14 → 20 → 21
14 → 20 → 22
```

## Открытые архитектурные решения (закрыты)

- Пол = явный примитив box (не heightmap) — для многоуровневых структур
- Collision response = slide (убрать нормальную компоненту velocity)
- Лидар видит всё с коллизией: статику + других агентов
- Загрузка сцены = полный перезапуск симуляции (не горячая замена)
- Gravity = плагин (опционально для каждого агента)
