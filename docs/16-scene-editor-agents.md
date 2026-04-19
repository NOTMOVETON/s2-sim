# Задача 16 — Редактор сцены: добавление и настройка агентов

## Цель

Расширить режим редактора сцены (задача 15) возможностью добавлять, настраивать
и удалять агентов. Пользователь должен иметь возможность:

- добавить нового агента кликом в сцену (начальная позиция)
- задать имя, domain_id, URDF-модель
- добавить/убрать плагины из предопределённого списка
- задать параметры каждого плагина
- видеть предпросмотр агента в реальном времени (без перезапуска симуляции)
- удалить агента из сцены
- при сохранении — обновить YAML с новым списком агентов

## Зависимости

- Требует: задача 15 (режим редактора, сохранение YAML)
- Предшествует: задача 19 (runtime load полностью перезапускает сцену)

## Ограничение

Добавление/удаление агентов **вступает в силу только после перезапуска симуляции**.
Симулятор не поддерживает горячее добавление агентов в тиковый цикл без перезагрузки.
Это ограничение фиксируется явно в UI ("Перезапустите симуляцию для применения изменений к агентам").

Предпросмотр агентов — только визуальный, на клиентской стороне. Реальные агенты
в симуляции не добавляются до перезапуска.

## Известные плагины (хардкод для UI)

```js
const KNOWN_PLUGINS = [
    {
        type: 'diff_drive',
        label: 'DiffDrive (привод)',
        params: [
            { key: 'wheel_base',      label: 'База колёс (м)',    type: 'number', default: 0.4 },
            { key: 'max_linear_vel',  label: 'Макс линейная (м/с)', type: 'number', default: 1.5 },
            { key: 'max_angular_vel', label: 'Макс угловая (рад/с)', type: 'number', default: 2.0 },
        ]
    },
    {
        type: 'gnss',
        label: 'GNSS',
        params: [
            { key: 'publish_rate_hz', label: 'Частота (Гц)', type: 'number', default: 10 },
            { key: 'noise_stddev',    label: 'Шум (м)',      type: 'number', default: 0.5 },
        ]
    },
    {
        type: 'imu',
        label: 'IMU',
        params: [
            { key: 'publish_rate_hz', label: 'Частота (Гц)', type: 'number', default: 100 },
        ]
    },
    {
        type: 'trajectory_recorder',
        label: 'Запись траектории',
        params: [
            { key: 'record_interval_s', label: 'Интервал (с)', type: 'number', default: 0.5 },
            { key: 'max_points',        label: 'Макс точек',   type: 'number', default: 200 },
            { key: 'color',             label: 'Цвет',         type: 'color',  default: '#FFAA00' },
        ]
    },
    {
        type: 'path_display',
        label: 'Отображение пути',
        params: [
            { key: 'color', label: 'Цвет', type: 'color', default: '#00FF88' },
        ]
    },
    {
        type: 'topic_display',
        label: 'Топик Display',
        params: [
            { key: 'topic',        label: 'Топик ROS2', type: 'text', default: '/status' },
            { key: 'display_key',  label: 'Ключ поля',  type: 'text', default: 'status' },
        ]
    },
    {
        type: 'joint_vel',
        label: 'Joint Velocity',
        params: []   // динамически — по джоинтам URDF
    },
    {
        type: 'color',
        label: 'Color Service',
        params: []
    },
    {
        type: 'gravity',   // задача 21
        label: 'Gravity',
        params: [
            { key: 'gravity_accel', label: 'Ускорение (м/с²)', type: 'number', default: 9.81 },
        ]
    },
    {
        type: 'lidar',     // задача 22
        label: 'Lidar',
        params: [
            { key: 'num_rays',        label: 'Лучей',           type: 'number', default: 360 },
            { key: 'max_range',       label: 'Макс дальность',  type: 'number', default: 10.0 },
            { key: 'min_range',       label: 'Мин дальность',   type: 'number', default: 0.1 },
            { key: 'height_offset',   label: 'Высота сенсора',  type: 'number', default: 0.2 },
            { key: 'publish_rate_hz', label: 'Частота (Гц)',     type: 'number', default: 10 },
        ]
    },
];
```

## UI — Вкладка Agents в Editor Panel

В панели редактора добавить таб "Agents":

```html
<div id="editor-agents-tab">
  <button id="btn-place-agent">+ Добавить агента</button>
  <div id="agent-editor-list"></div>
  <p class="editor-hint">Агенты применяются после перезапуска симуляции</p>
</div>
```

### Режим размещения агента

1. Клик "+ Добавить агента" → `placingAgentMode = true`, курсор меняется на crosshair
2. Клик в сцену → raycaster вычисляет позицию на плоскости Z=0
3. Создаётся preview-меш агента (бокс с цветом #FF6B35)
4. Открывается форма настройки нового агента

### Форма настройки агента

```html
<div class="agent-editor-form">
  <label>Имя: <input type="text" id="agent-name" value="robot_X"></label>
  <label>Domain ID: <input type="number" id="agent-domain" value="0"></label>
  <label>URDF: <select id="agent-urdf">
    <option value="">— нет —</option>
    <!-- заполняется из GET /api/scene/urdf-list -->
  </select></label>
  <label>Цвет: <input type="color" id="agent-color" value="#FF6B35"></label>
  <label>Размер (ширина/длина/высота):
    <input type="number" id="agent-sx" value="0.6">
    <input type="number" id="agent-sy" value="0.4">
    <input type="number" id="agent-sz" value="0.3">
  </label>

  <h5>Плагины</h5>
  <div id="agent-plugins-list">
    <!-- чекбокс для каждого плагина из KNOWN_PLUGINS -->
  </div>

  <!-- Раскрывающиеся параметры выбранного плагина -->
  <div id="agent-plugin-params"></div>

  <button id="btn-agent-confirm">Добавить в сцену</button>
  <button id="btn-agent-cancel">Отмена</button>
</div>
```

### Список URDF-файлов

Новый эндпоинт на сервере: `GET /api/scene/urdf-list`
Возвращает список `.urdf` файлов из папки `s2_config/robots/`:

```json
{ "files": ["turtlebot3.urdf", "dozer.urdf"] }
```

На фронтенде `<select>` заполняется при открытии формы.

### Предпросмотр агента (клиентский)

При выборе URDF и изменении размеров — preview-меш пересоздаётся:
- Без URDF: просто бокс
- С URDF: тот же бокс (полный рендер URDF требует отдельного парсинга в JS, в v1 пропускаем)

### Редактирование существующего агента

В списке `editor-agents-tab` отображаются все агенты из текущей сцены:

```
[robot_0] DiffDrive + GNSS + IMU     [✎] [✗]
[robot_1] DiffDrive + TrajectoryRec  [✎] [✗]
```

Кнопка ✎ — открыть форму настройки с текущими значениями
Кнопка ✗ — удалить агента (с предупреждением)

### Состояние редактора агентов

```js
let editorAgents = [];  // копия агентов из текущей сцены (загружается при открытии редактора)
/*
  {
    id: 0,
    name: "robot_0",
    domain_id: 0,
    pose: { x, y, z, yaw },
    visual: { type: "box", size: [0.6, 0.4, 0.3], color: "#FF6B35" },
    urdf: "turtlebot3.urdf",  // или ""
    plugins: [
      { type: "diff_drive", params: { wheel_base: 0.4, ... } },
      ...
    ]
  }
*/
```

### Загрузка текущих агентов в редактор

При открытии editor mode — запросить `GET /api/scene/state` который возвращает
текущее состояние сцены (агенты + геометрия) в формате, совместимом с YAML.

## Изменения на C++ стороне

### Новые HTTP-эндпоинты

```
GET  /api/scene/state        — текущее состояние сцены (agents + geometry) в JSON
GET  /api/scene/urdf-list    — список URDF-файлов в s2_config/robots/
POST /api/scene/agents       — обновить список агентов (применится после перезапуска,
                                только записывает в YAML)
```

### /api/scene/state

Формат ответа:
```json
{
  "yaml_path": "s2_config/scenes/my_scene.yaml",
  "agents": [
    {
      "name": "robot_0",
      "domain_id": 0,
      "pose": { "x": 0, "y": 0, "z": 0, "yaw": 0 },
      "visual": { "type": "box", "size": [0.6, 0.4, 0.3], "color": "#FF6B35" },
      "urdf": "",
      "plugins": [
        { "type": "diff_drive", "wheel_base": 0.4, "max_linear_vel": 1.5 }
      ]
    }
  ],
  "geometry": [ ... ]
}
```

### SceneWriter: сохранение агентов

Расширить `SceneWriter` (задача 15) методом:

```cpp
static void save_agents(const std::string& yaml_path,
                         const nlohmann::json& agents_json);
```

## Критерии завершения

- [ ] Кнопка "+ Добавить агента" + клик в сцену создаёт preview-меш
- [ ] Форма настройки: имя, domain_id, цвет, URDF, плагины с параметрами
- [ ] Список URDF заполняется с сервера
- [ ] "Добавить в сцену" — агент появляется в списке редактора
- [ ] Существующие агенты отображаются в списке с кнопками редактирования и удаления
- [ ] "Сохранить сцену" — YAML обновляется с новым списком агентов
- [ ] UI показывает подсказку "перезапустите симуляцию"
