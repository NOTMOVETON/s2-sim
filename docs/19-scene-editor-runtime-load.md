# Задача 19 — Редактор сцены: управление сценами и загрузка в рантайме

## Цель

Реализовать управление несколькими сценами:
- браузер сцен в UI (список `.yaml` файлов из `s2_config/scenes/`)
- загрузка новой сцены через кнопку в UI
- при загрузке — симуляция перезапускается с нуля (полный сброс)
- кнопка "Сохранить как..." — сохранить текущую сцену под новым именем

## Зависимости

- Требует: задача 15 (SceneWriter, сохранение YAML)
- Требует: задача 16 (agents JSON формат для сохранения)

## Поведение при загрузке сцены

При загрузке новой сцены через UI:
1. Отправить `POST /api/scene/load` с именем файла
2. C++ сторона:
   a. Остановить тиковый цикл
   b. Очистить `SimWorld` (агенты, примитивы, props, actors)
   c. Загрузить новый YAML через `SceneLoader::load()`
   d. Инициализировать новые транспортные соединения (если изменились domain_id)
   e. Перезапустить тиковый цикл
   f. Ответить `{"ok": true}` (или `{"ok": false, "error": "..."}`)
3. Фронтенд:
   a. Очистить все меши агентов (`agent_*`)
   b. Очистить все статические меши (`static_*`)
   c. Сбросить `geometrySent = false` (будет передана с первым снапшотом новой сцены)
   d. Сбросить `editorPrimitives`, `editorAgents`, undo-стек
   e. Показать overlay "Загрузка..." до получения первого снапшота

Симуляция стартует с нуля — позиции агентов, время, скорости — всё сбрасывается.

## UI — Браузер сцен

### Кнопка открытия

Добавить в header-toolbar:
```html
<button id="btn-scenes">Scenes</button>
```

### Панель браузера сцен

```html
<div id="scenes-panel" style="display:none">
  <h3>Сцены</h3>
  <div id="scenes-list">
    <!-- заполняется динамически -->
  </div>
  <hr>
  <button id="btn-new-scene">Новая сцена</button>
  <button id="btn-saveas-scene">Сохранить как...</button>
  <input type="text" id="saveas-name" placeholder="имя файла (без .yaml)">
</div>
```

### Список сцен

```js
function loadSceneList() {
    fetch('/api/scenes')
        .then(r => r.json())
        .then(data => {
            const list = document.getElementById('scenes-list');
            list.innerHTML = '';
            data.scenes.forEach(s => {
                const item = document.createElement('div');
                item.className = 'scene-item' + (s.active ? ' active' : '');
                item.innerHTML = `
                    <span>${s.name}</span>
                    <button onclick="loadScene('${s.name}')">Загрузить</button>
                `;
                list.appendChild(item);
            });
        });
}
```

### Загрузка сцены

```js
function loadScene(name) {
    if (!confirm(`Загрузить "${name}"? Симуляция будет перезапущена.`)) return;

    showLoadingOverlay('Загрузка сцены...');

    fetch('/api/scene/load', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ scene: name }),
    })
    .then(r => r.json())
    .then(d => {
        if (d.ok) {
            // Очистить всё
            clearAllMeshes();
            resetEditorState();
            document.getElementById('scenes-panel').style.display = 'none';
            // Дождаться первого снапшота (overlay скроется автоматически)
        } else {
            hideLoadingOverlay();
            alert(`Ошибка загрузки: ${d.error}`);
        }
    });
}

function clearAllMeshes() {
    Object.keys(meshes).forEach(k => removeMesh(k));
    clearOverlayLines();
    geometrySent = false;
    lastAgentData = {};
}

function resetEditorState() {
    editorPrimitives = [];
    editorAgents = [];
    undoStack.length = 0;
    clipboardPrimitives = [];
    selectedPrimitiveId = null;
    selectedPrimitiveIds.clear();
    if (editorMode) toggleEditorMode(false);
}
```

### Overlay загрузки

```js
function showLoadingOverlay(msg) {
    let overlay = document.getElementById('loading-overlay');
    if (!overlay) {
        overlay = document.createElement('div');
        overlay.id = 'loading-overlay';
        overlay.style.cssText = `
            position:fixed; top:0; left:0; width:100%; height:100%;
            background:rgba(0,0,0,0.7); color:#fff;
            display:flex; align-items:center; justify-content:center;
            font-size:1.5em; z-index:9999;
        `;
        document.body.appendChild(overlay);
    }
    overlay.textContent = msg;
    overlay.style.display = 'flex';
}

function hideLoadingOverlay() {
    const overlay = document.getElementById('loading-overlay');
    if (overlay) overlay.style.display = 'none';
}
```

Overlay скрывается при получении первого снапшота:
```js
function updateScene(data) {
    hideLoadingOverlay(); // первый снапшот = симуляция запустилась
    // ...
}
```

### Сохранить как...

```js
function saveSceneAs() {
    const name = document.getElementById('saveas-name').value.trim();
    if (!name) { alert('Введите имя файла'); return; }

    fetch('/api/scene/save-as', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ name }),
    })
    .then(r => r.json())
    .then(d => {
        if (d.ok) {
            showToast(`Сцена сохранена: ${d.path}`);
            loadSceneList(); // обновить список
        } else {
            alert(`Ошибка: ${d.error}`);
        }
    });
}
```

### Новая сцена (пустая)

Создать минимальный YAML с одним агентом, загрузить его:

```js
function newScene() {
    const name = prompt('Имя новой сцены:');
    if (!name) return;
    fetch('/api/scene/new', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ name }),
    })
    .then(r => r.json())
    .then(d => {
        if (d.ok) loadScene(d.name);
        else alert(`Ошибка: ${d.error}`);
    });
}
```

## Изменения на C++ стороне

### Новые HTTP-эндпоинты

```
GET  /api/scenes             — список .yaml файлов в s2_config/scenes/
POST /api/scene/load         — загрузить сцену (перезапуск симуляции)
POST /api/scene/save-as      — сохранить копию под новым именем
POST /api/scene/new          — создать пустую сцену и загрузить её
```

### GET /api/scenes

```cpp
// Сканировать папку scenes_dir_ (s2_config/scenes/)
// Вернуть JSON:
// { "scenes": [
//     { "name": "test_basic",      "active": false },
//     { "name": "test_two_robots", "active": true  }
// ]}
```

### POST /api/scene/load

Самая сложная операция. В `SimEngine` нужен метод `reload(new_yaml_path)`:

```cpp
// В SimEngineVizImpl или SimEngine:
void reload_scene(const std::string& yaml_path) {
    // 1. Pause тик
    pause();

    // 2. Сбросить мир
    world_ = SimWorld{};

    // 3. Загрузить новую сцену
    auto scene_data = SceneLoader::load(yaml_path, plugin_factory_);

    // 4. Наполнить мир
    for (auto& agent : scene_data.agents)
        world_.add_agent(std::move(agent));
    for (auto& prim : scene_data.geometry)
        world_.add_static_primitive(std::move(prim));

    // 5. Переинициализировать транспорт (новые domain_id)
    if (transport_bridge_)
        transport_bridge_->reinit(world_);

    // 6. Сбросить sim_time
    sim_time_ = 0.0;

    // 7. Обновить scene_yaml_path_
    scene_yaml_path_ = yaml_path;

    // 8. Разослать первый снапшот с geometry
    geometry_dirty_ = true;

    // 9. Возобновить тик
    unpause();
}
```

### POST /api/scene/save-as

```cpp
// Скопировать текущий yaml_path в новый файл
// Заменить geometry секцию актуальными примитивами
// Заменить agents секцию актуальными агентами
// Вернуть { "ok": true, "path": "s2_config/scenes/<name>.yaml" }
```

### Минимальный шаблон для новой сцены

```yaml
s2:
  update_rate: 50
  visualizer:
    enabled: true
    port: 1937
  transport:
    type: stub

  world:
    geometry: []

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
          max_linear_vel: 1.5
          max_angular_vel: 2.0
```

## Критерии завершения

- [x] `GET /api/scenes` возвращает список `.yaml` файлов из `s2_config/scenes/`
- [x] Панель "Scenes" в UI показывает список с кнопками "Загрузить"
- [x] При загрузке сцены показывается overlay "Загрузка..."
- [x] После загрузки симуляция стартует с нуля (время = 0, агенты на начальных позициях)
- [x] Старые меши очищены, новые геометрия и агенты отображаются
- [x] "Сохранить как..." создаёт новый YAML-файл в `s2_config/scenes/`
- [x] "Новая сцена" создаёт пустую сцену с одним агентом и загружает её
- [ ] Активная сцена помечается в списке *(не реализовано — список не отмечает текущую)*

## Тесты

- `SimEngine::reload_scene` — тест: загрузить сцену А, reload на сцену Б, агенты Б присутствуют, агенты А отсутствуют *(reload реализован в SimEngineCommandAdapter, не в SimEngine — unit-тест не написан)*
- `SceneWriter::save_as` — создаёт файл с правильным содержимым *(не написан)*

## Архитектурные решения (принятые при реализации)

- **Reload в адаптере, не в SimEngine**: `SimEngine` не зависит от `SceneLoader` и `PluginFactory`. Логика reload живёт в `SimEngineCommandAdapter::on_load_scene()` в `main.cpp`. Вызывается `engine_->load_world()` с новым `SimWorld`, что автоматически обновляет коллизии и сохраняет начальные состояния.
- **plugin_factory_ в адаптере**: конструктор `SimEngineCommandAdapter` принимает `SceneLoader::PluginFactory` для использования при reload.
- **scenes_dir_**: вычисляется как `parent_path(scene_path_)` — всегда совпадает с директорией текущей сцены.
- **Порядок эндпоинтов**: `/api/scene/save-as`, `/api/scene/load`, `/api/scene/new` стоят ДО `/api/scene/save` в `viz_server.cpp` — иначе `url.find("/api/scene/save")` даёт ложное совпадение.
- **update_static_geometry()**: добавлен метод `SimEngine::update_static_geometry()`, который синхронно обновляет `world_` и `collision_system_`. Использован в `on_update_geometry()` вместо ручного обновления — теперь коллизии применяются сразу после правок в редакторе.

## Известные ограничения

- **Thread safety**: `on_load_scene()` вызывается из HTTP-потока, тиковый цикл работает в главном потоке. Мьютекса нет — теоретическая гонка данных на `world_` и `collision_system_`. На практике `pause()` минимизирует окно гонки.
- **Транспорт не переинициализируется**: при загрузке сцены с другими агентами/domain_id ROS2-ноды не пересоздаются. Изменение транспортной конфигурации вступает в силу только после полного рестарта Docker.
- **resetEditorState() неполный**: не очищает TF-frames overlay и trajectory/path линии — они могут остаться видимыми от старой сцены до первого SSE-обновления.
- **Активная сцена не помечена**: в списке сцен нет визуального индикатора текущей сцены.
