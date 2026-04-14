# Active Context — S2

## Текущая работа

Задача 16 завершена. После завершения — мелкие доработки UX редактора агентов:
- Полупрозрачный бокс следует за курсором при размещении агента (`agent_place_preview`) — работает.
- Превью нового агента после клика на сцену (`agent_edit_pending`) — **не работает**, см. known bugs.

Следующая — задача 17 (face-snapping) или 18 (undo/copy-paste).

### Что сделано в последней сессии (задача 16)
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
| 17 | `docs/17-scene-editor-facesnap.md` | Face-snapping примитивов (Shift+LMB) |
| 18 | `docs/18-scene-editor-nav-undo-copy.md` | Shift+LMB pan, Ctrl+Z undo, Ctrl+C/V copy-paste |
| 19 | `docs/19-scene-editor-runtime-load.md` | Браузер сцен, загрузка в рантайме |

### Блок B: Физика

| # | Файл | Описание |
|---|------|----------|
| 20 | `docs/20-collision-system.md` | CollisionSystem, slide-реакция, наклонные плоскости |
| 21 | `docs/21-gravity-plugin.md` | GravityPlugin: свободное падение и опора |
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
