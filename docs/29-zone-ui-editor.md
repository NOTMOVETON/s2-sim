# Задача 29 — Zone UI: редактор зон в визуализаторе

## Цель

Зоны, созданные в сцене, должны отображаться и редактироваться в браузерном визуализаторе.
После задачи:
- Зоны видны в 3D-сцене с цветом, прозрачностью и меткой.
- Зону можно выбрать и редактировать через TransformControls (перемещение + масштаб).
- Можно включить/выключить зону и отдельные эффекты.
- Attached-зоны видны в панели актора/агента как раздел «Прикреплённые зоны».

## Зависимости

- Задача 23 (инфраструктура зон, ZoneSnapshot в WorldSnapshot)
- Задача 24–28 (конкретные эффекты, VisualHint)
- Three.js TransformControls (уже используется для других объектов)
- Kernel Commands: ZoneResizeCommand, ZoneMoveCommand, ZoneToggleCommand, ZoneToggleEffectCommand

---

## Что сделать

### 1. Kernel Commands для управления зоной

**Файл:** `workspace/s2_core/include/s2/sim_bus.hpp`

```cpp
/// Изменить форму/размер зоны в рантайме.
struct ZoneResizeCommand {
    ZoneId zone_id;
    ZoneShape new_shape;
};

/// Переместить центр зоны (только если не привязана к объекту).
struct ZoneMoveCommand {
    ZoneId zone_id;
    Vec3 new_center;
};

/// Включить или выключить зону целиком.
struct ZoneToggleCommand {
    ZoneId zone_id;
    bool enabled;
};

/// Включить или выключить конкретный эффект зоны.
struct ZoneToggleEffectCommand {
    ZoneId zone_id;
    int effect_index;   ///< Индекс в zone.effects
    bool enabled;
};
```

### 2. SimEngine: обработка Kernel Commands для зон

**Файл:** `workspace/s2_core/include/s2/sim_engine.hpp`

В фазе 1 (обработка входящих команд), добавить подписку на зоновые команды:

```cpp
// --- Зоновые команды ---
bus_.subscribe<ZoneMoveCommand>([this](const ZoneMoveCommand& cmd) {
    auto* zone = world_.find_zone(cmd.zone_id);
    if (!zone || zone->attached_to_actor || zone->attached_to_agent) return;
    zone->shape.center = cmd.new_center;
});

bus_.subscribe<ZoneResizeCommand>([this](const ZoneResizeCommand& cmd) {
    auto* zone = world_.find_zone(cmd.zone_id);
    if (!zone) return;
    zone->shape = cmd.new_shape;
});

bus_.subscribe<ZoneToggleCommand>([this](const ZoneToggleCommand& cmd) {
    auto* zone = world_.find_zone(cmd.zone_id);
    if (!zone) return;
    zone->enabled = cmd.enabled;
});

bus_.subscribe<ZoneToggleEffectCommand>([this](const ZoneToggleEffectCommand& cmd) {
    auto* zone = world_.find_zone(cmd.zone_id);
    if (!zone) return;
    if (cmd.effect_index >= 0 && cmd.effect_index < (int)zone->effects.size()) {
        zone->effects[cmd.effect_index].enabled = cmd.enabled;
    }
});
```

В `SimWorld` добавить вспомогательный метод:

```cpp
Zone* find_zone(const ZoneId& id) {
    auto it = std::find_if(zones_.begin(), zones_.end(),
        [&id](const Zone& z){ return z.id == id; });
    return it != zones_.end() ? &(*it) : nullptr;
}
```

### 3. ZoneSnapshot — расширение для UI

**Файл:** `workspace/s2_core/include/s2/world_snapshot.hpp`

Расширить ZoneSnapshot данными, необходимыми для UI:

```cpp
struct ZoneEffectSnapshot {
    std::string type;
    bool enabled{true};
};

struct ZoneSnapshot {
    std::string id;
    bool enabled{true};
    bool visible{true};

    // Геометрия
    std::string shape_type;   // "sphere", "aabb", "cylinder", "infinite"
    double cx{0}, cy{0}, cz{0};        // center
    double sx{1}, sy{1}, sz{1};        // half_size / (radius, half_height, unused)

    // Визуальные параметры
    std::string color{"#4488FF"};
    double opacity{0.3};
    std::string label;

    // Привязка
    bool attached{false};
    std::string attached_to;    // ID агента или актора (если attached)

    // Агенты внутри
    std::vector<AgentId> agents_inside;

    // Эффекты (для отображения в панели)
    std::vector<ZoneEffectSnapshot> effects;
};
```

В `SimEngine::build_snapshot()` заполнить ZoneSnapshot для каждой зоны в мире:

```cpp
for (const auto& zone : world_.zones()) {
    ZoneSnapshot zs;
    zs.id      = zone.id;
    zs.enabled = zone.enabled;
    zs.visible = zone.visible;
    zs.color   = zone.color;
    zs.opacity = zone.opacity;
    zs.label   = zone.label;

    // Геометрия
    switch (zone.shape.type) {
        case ZoneShapeType::SPHERE:
            zs.shape_type = "sphere";
            zs.cx = zone.shape.center.x();
            zs.cy = zone.shape.center.y();
            zs.cz = zone.shape.center.z();
            zs.sx = zone.shape.radius;
            break;
        case ZoneShapeType::AABB:
            zs.shape_type = "aabb";
            zs.cx = zone.shape.center.x();
            zs.cy = zone.shape.center.y();
            zs.cz = zone.shape.center.z();
            zs.sx = zone.shape.half_size.x();
            zs.sy = zone.shape.half_size.y();
            zs.sz = zone.shape.half_size.z();
            break;
        case ZoneShapeType::CYLINDER:
            zs.shape_type = "cylinder";
            zs.cx = zone.shape.center.x();
            zs.cy = zone.shape.center.y();
            zs.cz = zone.shape.center.z();
            zs.sx = zone.shape.radius;
            zs.sz = zone.shape.half_height;
            break;
        case ZoneShapeType::INFINITE:
            zs.shape_type = "infinite";
            break;
    }

    // Привязка
    if (zone.attached_to_actor) {
        zs.attached = true;
        zs.attached_to = std::to_string(*zone.attached_to_actor);
    } else if (zone.attached_to_agent) {
        zs.attached = true;
        zs.attached_to = std::to_string(*zone.attached_to_agent);
    }

    // Агенты внутри
    zs.agents_inside = std::vector<AgentId>(
        zone.inside_agents.begin(), zone.inside_agents.end());

    // Эффекты
    for (const auto& eff : zone.effects) {
        zs.effects.push_back({eff.type, eff.enabled});
    }

    snap.zones.push_back(std::move(zs));
}
```

### 4. WebSocket: передача зон в snapshot JSON

**Файл:** `workspace/s2_core/src/sim_ws_server.cpp` (или аналогичный)

В сериализации snapshot добавить зоны:

```cpp
// Зоны
nlohmann::json zones_json = nlohmann::json::array();
for (const auto& z : snap.zones) {
    nlohmann::json zj;
    zj["id"]         = z.id;
    zj["enabled"]    = z.enabled;
    zj["visible"]    = z.visible;
    zj["color"]      = z.color;
    zj["opacity"]    = z.opacity;
    zj["label"]      = z.label;
    zj["shape_type"] = z.shape_type;
    zj["cx"] = z.cx; zj["cy"] = z.cy; zj["cz"] = z.cz;
    zj["sx"] = z.sx; zj["sy"] = z.sy; zj["sz"] = z.sz;
    zj["attached"]    = z.attached;
    zj["attached_to"] = z.attached_to;
    zj["agents_inside"] = z.agents_inside;

    nlohmann::json eff_arr = nlohmann::json::array();
    for (const auto& e : z.effects) {
        eff_arr.push_back({{"type", e.type}, {"enabled", e.enabled}});
    }
    zj["effects"] = eff_arr;
    zones_json.push_back(zj);
}
json_snap["zones"] = zones_json;
```

### 5. Three.js: отрисовка зон

**Файл:** `workspace/visualizer/src/zones.js` (новый)

```javascript
import * as THREE from 'three';

// Создать mesh зоны по её ZoneSnapshot
export function createZoneMesh(zone) {
    let geometry;

    if (zone.shape_type === 'sphere') {
        geometry = new THREE.SphereGeometry(zone.sx, 24, 16);
    } else if (zone.shape_type === 'aabb') {
        geometry = new THREE.BoxGeometry(zone.sx * 2, zone.sy * 2, zone.sz * 2);
    } else if (zone.shape_type === 'cylinder') {
        geometry = new THREE.CylinderGeometry(zone.sx, zone.sx, zone.sz * 2, 24);
    } else if (zone.shape_type === 'infinite') {
        geometry = new THREE.BoxGeometry(200, 200, 200);
    } else {
        return null;
    }

    const color = new THREE.Color(zone.color);
    const material = new THREE.MeshBasicMaterial({
        color,
        transparent: true,
        opacity: zone.opacity,
        side: THREE.DoubleSide,
        depthWrite: false,
    });

    const mesh = new THREE.Mesh(geometry, material);

    // Обводка
    const edges = new THREE.EdgesGeometry(geometry);
    const lineMat = new THREE.LineBasicMaterial({ color, opacity: 0.7, transparent: true });
    const wireframe = new THREE.LineSegments(edges, lineMat);
    mesh.add(wireframe);

    updateZoneMeshPose(mesh, zone);
    return mesh;
}

export function updateZoneMeshPose(mesh, zone) {
    if (zone.shape_type === 'sphere' || zone.shape_type === 'infinite') {
        mesh.position.set(zone.cx, zone.cy, zone.cz);
    } else if (zone.shape_type === 'aabb') {
        mesh.position.set(zone.cx, zone.cy, zone.cz);
    } else if (zone.shape_type === 'cylinder') {
        mesh.position.set(zone.cx, zone.cy, zone.cz);
        // Three.js цилиндр по умолчанию вертикальный (ось Y), в S2 ось Z
        mesh.rotation.x = Math.PI / 2;
    }
}

// Управление коллекцией зон
export class ZoneManager {
    constructor(scene) {
        this.scene = scene;
        this.meshes = new Map(); // zone_id → mesh
    }

    update(zones) {
        const activeIds = new Set(zones.map(z => z.id));

        // Удалить зоны, которых больше нет
        for (const [id, mesh] of this.meshes) {
            if (!activeIds.has(id)) {
                this.scene.remove(mesh);
                mesh.geometry.dispose();
                mesh.material.dispose();
                this.meshes.delete(id);
            }
        }

        for (const zone of zones) {
            if (!zone.visible || !zone.enabled) {
                if (this.meshes.has(zone.id)) {
                    this.meshes.get(zone.id).visible = false;
                }
                continue;
            }

            if (!this.meshes.has(zone.id)) {
                const mesh = createZoneMesh(zone);
                if (mesh) {
                    this.meshes.set(zone.id, mesh);
                    this.scene.add(mesh);
                }
            } else {
                const mesh = this.meshes.get(zone.id);
                mesh.visible = true;
                updateZoneMeshPose(mesh, zone);
                // Обновить цвет/прозрачность если изменились
                const color = new THREE.Color(zone.color);
                mesh.material.color = color;
                mesh.material.opacity = zone.opacity;
            }
        }
    }

    getMesh(zoneId) {
        return this.meshes.get(zoneId) || null;
    }
}
```

### 6. Three.js: метка зоны

**Файл:** `workspace/visualizer/src/zone_labels.js` (новый)

```javascript
import { CSS2DObject } from 'three/addons/renderers/CSS2DRenderer.js';

export function createZoneLabel(text) {
    const div = document.createElement('div');
    div.className = 'zone-label';
    div.textContent = text;
    div.style.cssText =
        'background: rgba(0,0,0,0.55); color: #fff; font-size: 11px; ' +
        'padding: 2px 6px; border-radius: 3px; pointer-events: none; ' +
        'white-space: nowrap;';
    return new CSS2DObject(div);
}
```

CSS (добавить в основной stylesheet):

```css
.zone-label {
    user-select: none;
    opacity: 0.85;
}
```

### 7. UI-панель: инспектор зоны

**Файл:** `workspace/visualizer/src/ui/zone_panel.js` (новый)

```javascript
// Панель инспектора для выбранной зоны
export function renderZonePanel(zone, sendCommand) {
    const panel = document.getElementById('zone-panel');
    if (!zone) { panel.innerHTML = ''; return; }

    panel.innerHTML = `
        <div class="panel-header">
            <span class="zone-id">${zone.id}</span>
            <label class="toggle">
                <input type="checkbox" id="zone-enabled"
                    ${zone.enabled ? 'checked' : ''}> Включена
            </label>
        </div>
        <div class="panel-row">
            <label>Цвет</label>
            <input type="color" id="zone-color" value="${zone.color}">
        </div>
        <div class="panel-row">
            <label>Прозрачность</label>
            <input type="range" id="zone-opacity" min="0" max="1" step="0.05"
                value="${zone.opacity}">
        </div>
        <div class="panel-section">
            <h4>Эффекты</h4>
            ${zone.effects.map((e, i) => `
                <div class="effect-row">
                    <label class="toggle">
                        <input type="checkbox" class="effect-toggle"
                            data-index="${i}" ${e.enabled ? 'checked' : ''}>
                        ${e.type}
                    </label>
                </div>
            `).join('')}
        </div>
        <div class="panel-section">
            <h4>Агентов внутри: ${zone.agents_inside.length}</h4>
        </div>
    `;

    // Подписать обработчики
    document.getElementById('zone-enabled').addEventListener('change', (ev) => {
        sendCommand({ type: 'ZoneToggle', zone_id: zone.id, enabled: ev.target.checked });
    });

    panel.querySelectorAll('.effect-toggle').forEach(cb => {
        cb.addEventListener('change', (ev) => {
            const idx = parseInt(ev.target.dataset.index);
            sendCommand({ type: 'ZoneToggleEffect', zone_id: zone.id,
                          effect_index: idx, enabled: ev.target.checked });
        });
    });
}
```

### 8. TransformControls для зон

**Файл:** `workspace/visualizer/src/zone_transform.js` (новый)

```javascript
import { TransformControls } from 'three/addons/controls/TransformControls.js';

export class ZoneTransformController {
    constructor(camera, renderer, scene, sendCommand) {
        this.controls = new TransformControls(camera, renderer.domElement);
        this.controls.setMode('translate');
        scene.add(this.controls);
        this.sendCommand = sendCommand;
        this.selectedZoneId = null;

        // После окончания перемещения — отправить команду
        this.controls.addEventListener('mouseUp', () => this._onTransformEnd());
    }

    attach(zoneId, mesh) {
        this.selectedZoneId = zoneId;
        this.controls.attach(mesh);
    }

    detach() {
        this.selectedZoneId = null;
        this.controls.detach();
    }

    _onTransformEnd() {
        if (!this.selectedZoneId || !this.controls.object) return;
        const pos = this.controls.object.position;
        this.sendCommand({
            type: 'ZoneMove',
            zone_id: this.selectedZoneId,
            x: pos.x, y: pos.y, z: pos.z,
        });
    }
}
```

### 9. Attached-зоны в панели агента/актора

Если зона привязана (`zone.attached_to` совпадает с ID агента/актора),
в UI-панели этого объекта отображается раздел «Прикреплённые зоны»:

```javascript
// В renderAgentPanel()
const attachedZones = snapshot.zones.filter(
    z => z.attached && z.attached_to === String(agent.id));

if (attachedZones.length > 0) {
    html += `<div class="panel-section"><h4>Прикреплённые зоны</h4>`;
    for (const az of attachedZones) {
        html += `<div class="attached-zone-row">
            <span>${az.label || az.id}</span>
            <label class="toggle">
                <input type="checkbox" class="attached-zone-enable"
                    data-zoneid="${az.id}" ${az.enabled ? 'checked' : ''}> вкл
            </label>
        </div>`;
    }
    html += `</div>`;
}
```

### 10. CSS для панели зон

**Файл:** `workspace/visualizer/src/styles/zones.css` (новый)

```css
.zone-label {
    user-select: none;
}

#zone-panel {
    min-width: 240px;
    background: rgba(20, 20, 30, 0.92);
    border-left: 1px solid rgba(255,255,255,0.1);
    padding: 12px;
    color: #ddd;
    font-size: 13px;
}

#zone-panel .panel-header {
    display: flex;
    justify-content: space-between;
    align-items: center;
    margin-bottom: 12px;
    font-weight: bold;
}

#zone-panel .panel-row {
    display: flex;
    justify-content: space-between;
    align-items: center;
    margin-bottom: 8px;
}

#zone-panel .effect-row {
    margin: 4px 0;
    padding: 4px 8px;
    background: rgba(255,255,255,0.05);
    border-radius: 4px;
}

#zone-panel .panel-section {
    margin-top: 12px;
    border-top: 1px solid rgba(255,255,255,0.08);
    padding-top: 8px;
}

#zone-panel h4 {
    margin: 0 0 6px 0;
    font-size: 11px;
    text-transform: uppercase;
    color: #888;
    letter-spacing: 0.5px;
}

.attached-zone-row {
    display: flex;
    justify-content: space-between;
    align-items: center;
    padding: 3px 0;
}
```

---

## Тесты

**Файл:** `workspace/s2_core/tests/test_zone_commands.cpp`

- `ZoneMoveCommand_UpdatesCenter` — отправить ZoneMoveCommand: zone.shape.center обновилась
- `ZoneMoveCommand_AttachedZone_Ignored` — attached-зону нельзя переместить командой
- `ZoneResizeCommand_UpdatesShape` — форма зоны изменилась
- `ZoneToggleCommand_DisablesZone` — enabled = false → ZoneSystem не применяет эффекты
- `ZoneToggleEffectCommand_DisablesEffect` — effect.enabled = false → эффект не срабатывает
- `ZoneToggleEffectCommand_OutOfBounds_Ignored` — индекс > effects.size() → без краша
- `ZoneSnapshot_ContainsAllZones` — snapshot содержит все зоны из мира
- `ZoneSnapshot_AgentsInsideList` — агент в зоне → присутствует в agents_inside

---

## Критерии завершения

- [ ] ZoneMoveCommand, ZoneResizeCommand, ZoneToggleCommand, ZoneToggleEffectCommand обрабатываются в SimEngine
- [ ] ZoneSnapshot содержит форму, цвет, эффекты, список агентов внутри
- [ ] Зоны передаются клиенту через WebSocket
- [ ] Three.js отображает SPHERE, AABB, CYLINDER
- [ ] Attached-зоны видны в панели агента/актора
- [ ] TransformControls отправляет ZoneMoveCommand при перемещении
- [ ] Все тесты проходят в Docker
