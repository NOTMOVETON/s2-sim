# Задача 15 — Редактор сцены: режим редактирования и CRUD примитивов

## Цель

Добавить в UI режим редактирования сцены, в котором пользователь может:
- переключаться между режимом симуляции и режимом редактора
- добавлять, выбирать, перемещать, вращать, масштабировать и удалять
  статические примитивы (box, cylinder, sphere)
- настраивать цвет и размеры выбранного примитива через панель свойств
- сохранять результат в YAML-файл на сервере

Симуляция при этом может продолжать работать — примитивы сцены статичны.

## Зависимости

- Требует: задача 14 (статическая геометрия отображается корректно)
- Предшествует: задача 16 (редактор агентов), 17 (face-snapping), 19 (runtime load)

## Архитектура изменений

```
[Кнопка "Edit Scene"]
        ↓
  editorMode = true
        ↓
  SimEngine продолжает тикать (статика не влияет на тик)
        ↓
  Пользователь добавляет/двигает примитивы через UI
        ↓
  editorState: { primitives: [...] }  (клиентская копия сцены)
        ↓
  POST /api/scene/geometry → C++ VizServer → обновляет static_geometry_ в SimWorld
        ↓
  При сохранении: POST /api/scene/save → записывает YAML на диск
```

## Изменения на фронтенде

### Глобальное состояние редактора

```js
let editorMode = false;
let editorPrimitives = [];   // массив объектов { id, type, pose, size, radius, height, color }
let selectedPrimitiveId = null;
let nextPrimitiveId = 0;     // локальный счётчик (до сохранения)
```

### Кнопка переключения режима

В `index.html` добавить кнопку рядом с Pause/Reset:
```html
<button id="btn-edit-scene">Edit Scene</button>
```

При включении `editorMode`:
- gridHelper делается ярче (opacity 0.6 → 1.0)
- TransformControls переключается на примитивы (а не только на агентов)
- Появляется панель редактора (`editor-panel`)
- Боковая панель агента прячется

### Панель редактора (`editor-panel`)

Структура панели (HTML, скрытая по умолчанию, показывается в режиме редактора):

```html
<div id="editor-panel">
  <!-- Кнопки добавления -->
  <div class="editor-add-buttons">
    <button id="btn-add-box">+ Box</button>
    <button id="btn-add-cylinder">+ Cylinder</button>
    <button id="btn-add-sphere">+ Sphere</button>
  </div>

  <!-- Трансформ-режим -->
  <div class="editor-transform-mode">
    <button id="btn-mode-translate">Move</button>
    <button id="btn-mode-rotate">Rotate</button>
    <button id="btn-mode-scale">Scale</button>
  </div>

  <!-- Свойства выбранного примитива (показываются при выборе) -->
  <div id="primitive-props" style="display:none">
    <h4 id="primitive-type-label">Box</h4>

    <label>Цвет: <input type="color" id="prop-color" value="#808080"></label>

    <!-- Box -->
    <div id="props-box">
      <label>X: <input type="number" id="prop-sx" step="0.1" value="1"></label>
      <label>Y: <input type="number" id="prop-sy" step="0.1" value="1"></label>
      <label>Z: <input type="number" id="prop-sz" step="0.1" value="1"></label>
    </div>

    <!-- Cylinder -->
    <div id="props-cylinder" style="display:none">
      <label>Radius: <input type="number" id="prop-radius" step="0.1" value="0.5"></label>
      <label>Height: <input type="number" id="prop-height" step="0.1" value="1.0"></label>
    </div>

    <!-- Sphere -->
    <div id="props-sphere" style="display:none">
      <label>Radius: <input type="number" id="prop-radius-sphere" step="0.1" value="0.5"></label>
    </div>

    <button id="btn-delete-primitive">Удалить</button>
  </div>

  <!-- Кнопки сохранения -->
  <div class="editor-save-buttons">
    <button id="btn-save-scene">Сохранить сцену</button>
    <button id="btn-apply-geometry">Применить</button>
  </div>
</div>
```

### Добавление примитива

```js
function addPrimitive(type) {
    const id = `prim_${nextPrimitiveId++}`;
    const prim = {
        id,
        type,
        pose: { x: 0, y: 0, z: 0.5, yaw: 0, pitch: 0, roll: 0 },
        size: { x: 1, y: 1, z: 1 },
        radius: 0.5,
        height: 1.0,
        color: '#808080',
    };
    editorPrimitives.push(prim);
    createPrimitiveMesh(prim);
    selectPrimitive(id);
    sendGeometryToServer();
}
```

### Выбор примитива кликом

В raycaster (уже есть для агентов) — добавить проверку мешей с ключом `static_`:

```js
function onCanvasClick(event) {
    if (!editorMode) return; // только в режиме редактора
    // raycast по static_ мешам
    const hits = raycaster.intersectObjects(
        Object.entries(meshes)
            .filter(([k]) => k.startsWith('static_'))
            .map(([, m]) => m)
    );
    if (hits.length > 0) {
        const key = hits[0].object.userData.key;
        const id = key.replace('static_', '');
        selectPrimitive(id);
    }
}
```

### TransformControls для примитива

При выборе примитива — TransformControls цепляется к его мешу:

```js
function selectPrimitive(id) {
    selectedPrimitiveId = id;
    const mesh = meshes[`static_${id}`];
    if (mesh) {
        transformControls.attach(mesh);
        scene.add(transformControls);
    }
    updatePrimitivePropsPanel(id);
}
```

При отпускании TransformControls (`mouseUp`) — считать новую позу из меша и обновить `editorPrimitives`:

```js
transformControls.addEventListener('mouseUp', () => {
    if (selectedPrimitiveId) {
        syncPrimitiveFromMesh(selectedPrimitiveId);
        sendGeometryToServer();
    }
});
```

### Scale-режим

TransformControls в режиме `scale` изменяет `mesh.scale`. При `mouseUp` пересчитывать
фактические размеры:

```js
function syncPrimitiveFromMesh(id) {
    const prim = editorPrimitives.find(p => p.id === id);
    const mesh = meshes[`static_${id}`];
    // Позиция (Three.js Y-up → sim Z-up)
    prim.pose.x = mesh.position.x;
    prim.pose.y = -mesh.position.z;
    prim.pose.z = mesh.position.y;
    prim.pose.yaw = mesh.rotation.y;
    prim.pose.pitch = -mesh.rotation.z; // YZX order
    prim.pose.roll = mesh.rotation.x;
    // Размеры (с учётом scale)
    if (prim.type === 'box') {
        prim.size.x = (prim.size.x || 1) * mesh.scale.x;
        prim.size.y = (prim.size.y || 1) * mesh.scale.y;
        prim.size.z = (prim.size.z || 1) * mesh.scale.z;
        mesh.scale.set(1, 1, 1); // сбросить scale — размер уже в size
    } else if (prim.type === 'cylinder' || prim.type === 'sphere') {
        prim.radius = (prim.radius || 0.5) * Math.max(mesh.scale.x, mesh.scale.z);
        if (prim.type === 'cylinder') {
            prim.height = (prim.height || 1.0) * mesh.scale.y;
        }
        mesh.scale.set(1, 1, 1);
    }
    // Пересоздать геометрию с новыми размерами
    recreatePrimitiveMesh(prim);
}
```

### Удаление примитива

```js
function deletePrimitive(id) {
    editorPrimitives = editorPrimitives.filter(p => p.id !== id);
    removeMesh(`static_${id}`);
    transformControls.detach();
    selectedPrimitiveId = null;
    document.getElementById('primitive-props').style.display = 'none';
    sendGeometryToServer();
}
```

### Применение и сохранение на сервер

**`sendGeometryToServer()`** — отправляет текущий список примитивов на сервер (применяется сразу):

```js
function sendGeometryToServer() {
    const payload = {
        geometry: editorPrimitives.map(p => ({
            type: p.type,
            x: p.pose.x, y: p.pose.y, z: p.pose.z,
            yaw: p.pose.yaw, pitch: p.pose.pitch, roll: p.pose.roll,
            sx: p.size?.x || 1, sy: p.size?.y || 1, sz: p.size?.z || 1,
            radius: p.radius || 0.5,
            height: p.height || 1.0,
            color: p.color || '#808080',
        }))
    };
    fetch('/api/scene/geometry', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(payload),
    });
}
```

**`saveScene()`** — сохраняет сцену в YAML (POST /api/scene/save):

```js
function saveScene() {
    fetch('/api/scene/save', { method: 'POST' })
        .then(r => r.json())
        .then(d => {
            if (d.ok) showToast(`Сцена сохранена: ${d.path}`);
            else showToast(`Ошибка: ${d.error}`);
        });
}
```

## Изменения на C++ стороне

### VizServer: новые HTTP-эндпоинты

**Файл:** `workspace/s2_visualizer/src/viz_server.cpp`

Добавить обработку в `handle_http_request()`:

```
POST /api/scene/geometry   — обновить static_geometry в SimWorld
POST /api/scene/save       — сохранить текущую сцену в YAML на диск
```

### Формат JSON для /api/scene/geometry

```json
{
  "geometry": [
    { "type": "box", "x": 3.0, "y": 0.0, "z": 0.5,
      "yaw": 0, "pitch": 0, "roll": 0,
      "sx": 2.0, "sy": 1.0, "sz": 1.0,
      "color": "#4444FF" },
    { "type": "cylinder", "x": -2.0, "y": 2.0, "z": 1.0,
      "radius": 0.5, "height": 2.0,
      "color": "#44FF44" }
  ]
}
```

### SimEngineVizImpl: команда обновления геометрии

**Файл:** `workspace/s2_visualizer/src/sim_engine_viz_impl.cpp`

```cpp
void SimEngineVizImpl::on_update_geometry(const std::vector<WorldPrimitive>& prims) {
    // Заменить static_geometry в SimWorld
    world_.static_geometry().clear();
    for (const auto& p : prims)
        world_.add_static_primitive(p);
    // Выставить флаг: следующий снапшот должен включить geometry
    geometry_dirty_ = true;
}
```

### Сохранение сцены в YAML

**Файл:** новый `workspace/s2_core/include/s2/scene_writer.hpp`

```cpp
class SceneWriter {
public:
    /// Сохранить текущее состояние сцены поверх оригинального YAML-файла.
    /// Сохраняет geometry: секцию, остальное оставляет нетронутым.
    static void save_geometry(const std::string& yaml_path,
                              const std::vector<WorldPrimitive>& prims);
};
```

Реализация: загрузить YAML, заменить `s2.world.geometry`, записать обратно.

### Передача пути к файлу сцены

`SimEngine` или `SimEngineVizImpl` должен хранить `scene_yaml_path_` — путь к текущему
YAML-файлу, чтобы знать куда сохранять.

## Сброс флага geometrySent при обновлении геометрии

После `POST /api/scene/geometry` сервер в следующем снапшоте включает `geometry`.
Фронтенд при получении нового снапшота с `data.geometry` должен заменить
существующие `static_` меши на новые (текущий код делает это только один раз).

Исправление: убрать флаг `geometrySent`, вместо этого всегда обновлять static меши
если `data.geometry` присутствует в снапшоте.

## Критерии завершения

- [x] Кнопка "Edit Scene" переключает режим редактора
- [x] Кнопки "+ Box", "+ Cylinder", "+ Sphere" добавляют примитивы в центр сцены
- [x] TransformControls работает для перемещения и вращения примитивов
- [x] Scale-режим изменяет размеры примитива
- [x] Панель свойств показывает текущий цвет и размеры выбранного примитива
- [x] Изменение цвета в панели обновляет цвет меша мгновенно
- [x] Удаление примитива убирает его из сцены
- [x] "Применить" (Apply) — примитивы обновляются на сервере немедленно
- [x] "Сохранить сцену" — YAML-файл обновляется на диске
- [x] В режиме симуляции (не editor) примитивы нельзя выбрать или переместить

## Тесты

- `SceneWriter::save_geometry` — сохраняет и перечитывает geometry секцию без потери остального YAML
- `VizCommandHandler::on_update_geometry` — geometry в SimWorld заменяется корректно

## Статус

✅ Завершено. Все тесты проходят (`s2_editor_tests`).
