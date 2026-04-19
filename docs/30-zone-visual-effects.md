# Задача 30 — Zone Visual Effects: анимации зон в визуализаторе

## Цель

Зоны с разными эффектами выглядят по-разному в браузерном визуализаторе.
Каждый плагин эффекта описывает `VisualHint` — визуальный намёк для UI.
Типы анимаций: `arrows` (направленные стрелки), `particles` (частицы),
`glow` (пульсирующее свечение), `grid` (сетка/решётка).

После задачи: конвейер отображает движущиеся стрелки; ветровая зона — частицы;
ледяная и зарядная зоны — мерцающий ореол; запретная зона — красную решётку.

## Зависимости

- Задача 23 (инфраструктура зон, ZoneSnapshot)
- Задача 24 (IceModifier, BoostZone, MotionLockZone — уже имеют VisualHint)
- Задача 25 (ConveyorEffect, WindEffect — уже имеют VisualHint)
- Задача 26 (ChargingEffect — уже имеет VisualHint)
- Задача 29 (Three.js zone rendering)

---

## Что сделать

### 1. VisualHint в ZoneSnapshot

**Файл:** `workspace/s2_core/include/s2/world_snapshot.hpp`

Добавить VisualHintSnapshot в ZoneSnapshot:

```cpp
struct VisualHintSnapshot {
    std::string type;           ///< "arrows", "particles", "glow", "grid"
    nlohmann::json params;      ///< {"color": "#...", "speed": 1.0, ...}
};

struct ZoneSnapshot {
    // ... существующие поля ...
    std::vector<VisualHintSnapshot> visual_hints; ///< По одному на каждый эффект с hint
};
```

### 2. Передача VisualHint из плагинов в snapshot

**Файл:** `workspace/s2_core/include/s2/zone_system.hpp`

ZoneSystem хранит инстанции плагинов эффектов:

```cpp
struct ZoneRuntime {
    Zone zone;
    std::vector<std::unique_ptr<EffectPlugin>> plugins;
};
```

В `SimEngine::build_snapshot()`, для каждой зоны собрать visual_hints из плагинов:

```cpp
for (const auto& zr : zone_system_.zones()) {
    // ... заполнить основные поля ZoneSnapshot ...

    for (const auto& plugin : zr.plugins) {
        if (!plugin) continue;
        auto hint = plugin->visual_hint();
        if (hint) {
            VisualHintSnapshot vhs;
            vhs.type   = hint->type;
            vhs.params = hint->params;
            zs.visual_hints.push_back(std::move(vhs));
        }
    }
}
```

### 3. Передача visual_hints через WebSocket

**Файл:** `workspace/s2_core/src/sim_ws_server.cpp`

В сериализации ZoneSnapshot добавить hints:

```cpp
nlohmann::json hints_json = nlohmann::json::array();
for (const auto& vh : z.visual_hints) {
    hints_json.push_back({{"type", vh.type}, {"params", vh.params}});
}
zj["visual_hints"] = hints_json;
```

### 4. Three.js: анимация типа `glow`

**Файл:** `workspace/visualizer/src/zone_effects/glow.js` (новый)

Пульсирующее свечение через изменение opacity и emissive материала.

```javascript
export class GlowEffect {
    constructor(mesh, params) {
        this.mesh     = mesh;
        this.color    = params.color || '#88AAFF';
        this.rate     = params.pulse_rate || 1.5;
        this.baseOpacity = mesh.material.opacity;
        this.time     = 0;
    }

    tick(dt) {
        this.time += dt;
        // Плавное мерцание: opacity ± 30% от базового
        const factor = 1.0 + 0.3 * Math.sin(this.time * this.rate * Math.PI * 2);
        this.mesh.material.opacity = Math.min(0.9, this.baseOpacity * factor);
    }

    dispose() {}
}
```

### 5. Three.js: анимация типа `arrows`

**Файл:** `workspace/visualizer/src/zone_effects/arrows.js` (новый)

Стрелки из инстанций ArrowHelper, движущиеся в заданном направлении.

```javascript
import * as THREE from 'three';

const ARROW_COUNT = 6;

export class ArrowsEffect {
    constructor(scene, zoneMesh, params) {
        this.scene    = scene;
        this.params   = params;
        this.time     = 0;
        this.arrows   = [];

        const dir = new THREE.Vector3(
            params.direction?.[0] ?? 1,
            params.direction?.[1] ?? 0,
            params.direction?.[2] ?? 0
        ).normalize();
        const color = new THREE.Color(params.color || '#FF8800');

        // Создать несколько стрелок в пределах зоны
        for (let i = 0; i < ARROW_COUNT; i++) {
            const arrow = new THREE.ArrowHelper(dir, new THREE.Vector3(), 0.3, color, 0.12, 0.08);
            arrow.userData.phase = (i / ARROW_COUNT) * Math.PI * 2;
            scene.add(arrow);
            this.arrows.push(arrow);
        }

        this.dir   = dir;
        this.zone  = zoneMesh;
        this.speed = params.speed || 1.0;
    }

    tick(dt) {
        this.time += dt;
        const pos = this.zone.position;

        // Разместить стрелки случайно внутри зоны, анимировать вдоль direction
        this.arrows.forEach((arrow, i) => {
            const phase = arrow.userData.phase;
            const t = ((this.time * this.speed * 0.5) + phase / (Math.PI * 2)) % 1.0;

            // Позиция: линейно вдоль dir с зацикливанием
            const offset = new THREE.Vector3()
                .copy(this.dir)
                .multiplyScalar((t - 0.5) * 2.0);

            // Небольшой разброс перпендикулярно
            const perp1 = new THREE.Vector3(this.dir.y, -this.dir.x, 0).normalize();
            const perp2 = new THREE.Vector3(0, this.dir.z, -this.dir.y).normalize();
            const spread1 = Math.sin(phase * 1.7 + 0.3) * 0.3;
            const spread2 = Math.cos(phase * 2.1 + 0.7) * 0.3;

            arrow.position.set(
                pos.x + offset.x + perp1.x * spread1 + perp2.x * spread2,
                pos.y + offset.y + perp1.y * spread1 + perp2.y * spread2,
                pos.z + offset.z + perp1.z * spread1 + perp2.z * spread2
            );

            // Прозрачность нарастает/спадает у краёв
            const fade = Math.max(0, 1.0 - Math.abs(t - 0.5) * 3.5);
            arrow.line.material.opacity = fade * 0.85;
            arrow.cone.material.opacity = fade * 0.85;
        });
    }

    dispose() {
        this.arrows.forEach(a => {
            this.scene.remove(a);
            a.line.geometry.dispose();
            a.cone.geometry.dispose();
        });
        this.arrows = [];
    }
}
```

### 6. Three.js: анимация типа `particles`

**Файл:** `workspace/visualizer/src/zone_effects/particles.js` (новый)

Points-система с движением в заданном направлении.

```javascript
import * as THREE from 'three';

const DEFAULT_DENSITY = 20;

export class ParticlesEffect {
    constructor(scene, zoneMesh, params) {
        this.scene = scene;
        this.zone  = zoneMesh;
        this.time  = 0;

        const count = params.density || DEFAULT_DENSITY;
        const color = new THREE.Color(params.color || '#AADDFF');
        const dir   = new THREE.Vector3(
            params.direction?.[0] ?? 1,
            params.direction?.[1] ?? 0,
            params.direction?.[2] ?? 0
        ).normalize();

        this.dir = dir;
        this.speed = params.speed || 0.5;

        // Инициализировать случайные позиции внутри единичной сферы
        this.positions   = new Float32Array(count * 3);
        this.phases      = new Float32Array(count);
        for (let i = 0; i < count; i++) {
            this.positions[i * 3]     = (Math.random() - 0.5);
            this.positions[i * 3 + 1] = (Math.random() - 0.5);
            this.positions[i * 3 + 2] = (Math.random() - 0.5);
            this.phases[i] = Math.random();
        }

        const geometry = new THREE.BufferGeometry();
        geometry.setAttribute('position',
            new THREE.BufferAttribute(this.positions.slice(), 3));

        const material = new THREE.PointsMaterial({
            color,
            size: 0.06,
            transparent: true,
            opacity: 0.7,
            depthWrite: false,
        });

        this.points = new THREE.Points(geometry, material);
        scene.add(this.points);
    }

    tick(dt) {
        this.time += dt;
        const pos  = this.zone.position;
        const attr = this.points.geometry.attributes.position;

        for (let i = 0; i < this.phases.length; i++) {
            // Частицы движутся вдоль direction, при выходе за [-0.5, 0.5] — телепорт
            let t = (this.phases[i] + this.time * this.speed * 0.3) % 1.0;

            attr.array[i * 3]     = pos.x + (t - 0.5) * 2 * this.dir.x
                                    + this.positions[i * 3]     * (1 - Math.abs(this.dir.x));
            attr.array[i * 3 + 1] = pos.y + (t - 0.5) * 2 * this.dir.y
                                    + this.positions[i * 3 + 1] * (1 - Math.abs(this.dir.y));
            attr.array[i * 3 + 2] = pos.z + (t - 0.5) * 2 * this.dir.z
                                    + this.positions[i * 3 + 2] * (1 - Math.abs(this.dir.z));
        }
        attr.needsUpdate = true;
    }

    dispose() {
        this.scene.remove(this.points);
        this.points.geometry.dispose();
        this.points.material.dispose();
    }
}
```

### 7. Three.js: анимация типа `grid`

**Файл:** `workspace/visualizer/src/zone_effects/grid.js` (новый)

Решётка линий поверх зоны.

```javascript
import * as THREE from 'three';

export class GridEffect {
    constructor(scene, zoneMesh, params) {
        this.scene = scene;
        this.zone  = zoneMesh;

        const color     = new THREE.Color(params.color || '#FF2222');
        const spacing   = params.spacing   || 0.5;
        const lineWidth = params.line_width || 1.0;

        // Горизонтальная сетка 2×2м
        const gridHelper = new THREE.GridHelper(2, Math.round(2 / spacing), color, color);
        gridHelper.material.opacity     = 0.5;
        gridHelper.material.transparent = true;
        gridHelper.material.linewidth   = lineWidth;
        gridHelper.position.copy(zoneMesh.position);

        this.grid = gridHelper;
        scene.add(gridHelper);
    }

    tick(dt) {
        // Решётка следует за зоной
        this.grid.position.copy(this.zone.position);
    }

    dispose() {
        this.scene.remove(this.grid);
        this.grid.geometry.dispose();
        this.grid.material.dispose();
    }
}
```

### 8. Менеджер визуальных эффектов зон

**Файл:** `workspace/visualizer/src/zone_effects/zone_fx_manager.js` (новый)

```javascript
import { GlowEffect }      from './glow.js';
import { ArrowsEffect }    from './arrows.js';
import { ParticlesEffect } from './particles.js';
import { GridEffect }      from './grid.js';

export class ZoneFxManager {
    constructor(scene) {
        this.scene   = scene;
        this.effects = new Map(); // zone_id → Effect[]
    }

    update(zones, meshes) {
        const activeIds = new Set(zones.map(z => z.id));

        // Удалить эффекты для несуществующих зон
        for (const [id, fxList] of this.effects) {
            if (!activeIds.has(id)) {
                fxList.forEach(fx => fx.dispose());
                this.effects.delete(id);
            }
        }

        for (const zone of zones) {
            if (!zone.enabled || !zone.visible) continue;

            const mesh = meshes.get(zone.id);
            if (!mesh) continue;

            // Пересоздать эффекты при изменении подсказок
            const hintsKey = JSON.stringify(zone.visual_hints);
            if (this.effects.has(zone.id)) {
                if (this.effects.get(zone.id)._hintsKey === hintsKey) continue;
                this.effects.get(zone.id).forEach(fx => fx.dispose());
            }

            const fxList = [];
            for (const hint of (zone.visual_hints || [])) {
                const fx = this._createEffect(hint, mesh);
                if (fx) fxList.push(fx);
            }
            fxList._hintsKey = hintsKey;
            this.effects.set(zone.id, fxList);
        }
    }

    tick(dt) {
        for (const fxList of this.effects.values()) {
            fxList.forEach(fx => fx.tick(dt));
        }
    }

    _createEffect(hint, mesh) {
        switch (hint.type) {
            case 'glow':      return new GlowEffect(mesh, hint.params);
            case 'arrows':    return new ArrowsEffect(this.scene, mesh, hint.params);
            case 'particles': return new ParticlesEffect(this.scene, mesh, hint.params);
            case 'grid':      return new GridEffect(this.scene, mesh, hint.params);
            default:          return null;
        }
    }

    dispose() {
        for (const fxList of this.effects.values()) {
            fxList.forEach(fx => fx.dispose());
        }
        this.effects.clear();
    }
}
```

### 9. Интеграция в главный рендер-цикл

**Файл:** `workspace/visualizer/src/main.js`

```javascript
import { ZoneManager }    from './zones.js';
import { ZoneFxManager }  from './zone_effects/zone_fx_manager.js';

const zoneManager = new ZoneManager(scene);
const zoneFxMgr   = new ZoneFxManager(scene);

// В обработчике WebSocket snapshot:
function onSnapshot(data) {
    // ... существующая обработка агентов, акторов ...

    if (data.zones) {
        zoneManager.update(data.zones);
        zoneFxMgr.update(data.zones, zoneManager.meshes);
    }
}

// В анимационном цикле:
function animate(ts) {
    const dt = (ts - lastTs) / 1000;
    lastTs = ts;

    // ... существующие обновления ...
    zoneFxMgr.tick(dt);

    renderer.render(scene, camera);
    requestAnimationFrame(animate);
}
```

---

## Тесты

Визуальные эффекты тестируются вручную в браузере. Автоматические тесты покрывают
только подготовку данных на стороне C++:

**Файл:** `workspace/s2_core/tests/test_zone_visual_hints.cpp`

- `VisualHint_GlowEffect_PresentInSnapshot` — ChargingEffect в зоне:
  snapshot.visual_hints содержит запись с type="glow"
- `VisualHint_ArrowsEffect_HasDirection` — ConveyorEffect:
  visual_hints[0].params["direction"] совпадает с direction конвейера
- `VisualHint_MultipleEffects_AllHints` — зона с IceModifier + ConveyorEffect:
  visual_hints содержит оба hint
- `VisualHint_NoHint_EmptyList` — эффект без visual_hint():
  visual_hints пустой

---

## Критерии завершения

- [ ] VisualHint из плагинов попадает в ZoneSnapshot
- [ ] Snapshot передаётся клиенту с visual_hints
- [ ] `glow` анимирует прозрачность зоны (пульсация)
- [ ] `arrows` отображает движущиеся стрелки в направлении direction
- [ ] `particles` отображает плывущие частицы
- [ ] `grid` отображает статичную решётку поверх зоны
- [ ] Эффекты удаляются при исчезновении зоны или снятии галки visibility
- [ ] Все C++ тесты проходят в Docker
