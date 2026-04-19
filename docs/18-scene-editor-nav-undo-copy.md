# Задача 18 — Редактор сцены: навигация, Undo, Copy/Paste

## Цель

1. **Shift+LMB → Pan** — перемещение камеры (pan) теперь работает через Shift+LMB вместо средней кнопки мыши
2. **Убрать MMB** — средняя кнопка мыши больше не делает pan
3. **Ctrl+Z → Undo** — отменить последнее действие в редакторе (добавление, удаление, перемещение, изменение параметров примитива)
4. **Ctrl+C / Ctrl+V → Copy/Paste** — копировать и вставлять выбранные примитивы (не агентов)
5. **Мультиселект** — Shift+клик на примитив добавляет его к выделению; операции Copy/Delete работают для всей группы

## Зависимости

- Требует: задача 15 (редактор и примитивы)
- Не зависит от: задач 16, 17

---

## Часть 1: Изменение навигации

### OrbitControls: перенастройка кнопок

**Файл:** `workspace/s2_visualizer/web/js/app.js`

OrbitControls поддерживает переопределение кнопок через `.mouseButtons`:

```js
controls.mouseButtons = {
    LEFT:   THREE.MOUSE.ROTATE,   // LMB: вращение (без Shift)
    MIDDLE: null,                  // MMB: ничего
    RIGHT:  THREE.MOUSE.PAN,      // RMB: pan (оставляем)
};
```

Shift+LMB для pan реализуем через кастомный обработчик, так как OrbitControls
не поддерживает модификаторы напрямую:

```js
// Перехватить mousedown до OrbitControls
renderer.domElement.addEventListener('mousedown', e => {
    if (e.button === 0 && e.shiftKey) {
        // Симулировать нажатие средней кнопки для OrbitControls
        e.stopImmediatePropagation();
        controls.enabled = false; // отключить OrbitControls
        startManualPan(e);
    }
}, true); // capture phase

renderer.domElement.addEventListener('mouseup', e => {
    if (manualPanning) {
        stopManualPan();
        controls.enabled = true;
    }
});

renderer.domElement.addEventListener('mousemove', e => {
    if (manualPanning) {
        updateManualPan(e);
    }
});
```

Реализация ручного pan через изменение `controls.target` и `camera.position`:

```js
let manualPanning = false;
let panStartMouse = new THREE.Vector2();
let panStartTarget = new THREE.Vector3();
let panStartCameraPos = new THREE.Vector3();

function startManualPan(e) {
    manualPanning = true;
    panStartMouse.set(e.clientX, e.clientY);
    panStartTarget.copy(controls.target);
    panStartCameraPos.copy(camera.position);
}

function updateManualPan(e) {
    if (!manualPanning) return;
    const dx = (e.clientX - panStartMouse.x) / window.innerWidth;
    const dy = (e.clientY - panStartMouse.y) / window.innerHeight;

    // Вектор "вправо" и "вверх" в плоскости экрана
    const right = new THREE.Vector3();
    const up = new THREE.Vector3();
    camera.getWorldDirection(new THREE.Vector3()); // dummy
    right.crossVectors(camera.getWorldDirection(new THREE.Vector3()), camera.up).normalize();
    up.copy(camera.up);

    const panScale = camera.position.distanceTo(controls.target) * 1.0;
    const panDelta = right.multiplyScalar(-dx * panScale)
                         .add(up.clone().multiplyScalar(dy * panScale));

    controls.target.copy(panStartTarget).add(panDelta);
    camera.position.copy(panStartCameraPos).add(panDelta);
    controls.update();
}

function stopManualPan() {
    manualPanning = false;
}
```

---

## Часть 2: Undo (Ctrl+Z)

### Стек операций

```js
const undoStack = [];     // массив снапшотов состояния (до операции)
const MAX_UNDO = 50;

function pushUndoSnapshot() {
    // Сохранить глубокую копию editorPrimitives перед изменением
    const snapshot = JSON.parse(JSON.stringify(editorPrimitives));
    undoStack.push(snapshot);
    if (undoStack.length > MAX_UNDO) undoStack.shift();
}

function undo() {
    if (undoStack.length === 0) return;
    const snapshot = undoStack.pop();
    // Очистить все static_ меши
    Object.keys(meshes).filter(k => k.startsWith('static_')).forEach(k => removeMesh(k));
    // Восстановить примитивы
    editorPrimitives = snapshot;
    editorPrimitives.forEach(p => createPrimitiveMesh(p));
    // Снять выделение
    selectedPrimitiveId = null;
    selectedPrimitiveIds.clear();
    transformControls.detach();
    document.getElementById('primitive-props').style.display = 'none';
    // Отправить на сервер
    sendGeometryToServer();
}
```

### Куда вставить pushUndoSnapshot()

Снапшот нужно делать **до** любого изменяющего действия:
- `addPrimitive()` — в начале функции
- `deletePrimitive()` — в начале функции
- `transformControls` `'mouseDown'` — в обработчике начала перетаскивания
- Изменение параметров в панели свойств (color, size) — при `input` с debounce 300 мс

```js
// В обработчике начала перетаскивания TransformControls:
transformControls.addEventListener('mouseDown', () => {
    if (selectedPrimitiveId || selectedPrimitiveIds.size > 0) {
        pushUndoSnapshot();
    }
});
```

### Клавиатурный обработчик

```js
window.addEventListener('keydown', e => {
    if ((e.ctrlKey || e.metaKey) && e.key === 'z' && !e.shiftKey) {
        e.preventDefault();
        if (editorMode) undo();
    }
});
```

---

## Часть 3: Мультиселект

### Состояние

```js
const selectedPrimitiveIds = new Set();  // мульти-выделение
// selectedPrimitiveId остаётся для одиночного выделения (TransformControls)
```

### Shift+клик на примитив (без face-snap)

```js
function onPrimitiveClick(event) {
    if (shiftHeld && !facesnapMode) {
        // Мультиселект
        const id = getClickedPrimitiveId(event);
        if (id) {
            if (selectedPrimitiveIds.has(id)) {
                selectedPrimitiveIds.delete(id);
                unhighlightMesh(`static_${id}`);
            } else {
                selectedPrimitiveIds.add(id);
                highlightMesh(`static_${id}`);
            }
        }
    } else {
        // Одиночный выбор
        // Снять мульти-выделение
        selectedPrimitiveIds.forEach(id => unhighlightMesh(`static_${id}`));
        selectedPrimitiveIds.clear();
        // ...обычный выбор...
    }
}
```

Подсветка мультиселекта: изменить `emissive` материала:
```js
function highlightMesh(key) {
    const mesh = meshes[key];
    if (mesh) mesh.material.emissive = new THREE.Color(0x333300);
}
function unhighlightMesh(key) {
    const mesh = meshes[key];
    if (mesh) mesh.material.emissive = new THREE.Color(0x000000);
}
```

---

## Часть 4: Copy/Paste

### Буфер обмена

```js
let clipboardPrimitives = [];  // скопированные примитивы (по значению)
```

### Ctrl+C

```js
function copySelected() {
    clipboardPrimitives = [];

    const ids = selectedPrimitiveIds.size > 0
        ? [...selectedPrimitiveIds]
        : (selectedPrimitiveId ? [selectedPrimitiveId] : []);

    ids.forEach(id => {
        const prim = editorPrimitives.find(p => p.id === id);
        if (prim) clipboardPrimitives.push(JSON.parse(JSON.stringify(prim)));
    });
}
```

### Ctrl+V

```js
function pasteSelected() {
    if (clipboardPrimitives.length === 0) return;
    pushUndoSnapshot();

    // Снять текущее выделение
    selectedPrimitiveIds.forEach(id => unhighlightMesh(`static_${id}`));
    selectedPrimitiveIds.clear();
    selectedPrimitiveId = null;
    transformControls.detach();

    // Вставить со смещением +0.5 по X и Y
    const PASTE_OFFSET = 0.5;
    clipboardPrimitives.forEach(orig => {
        const copy = JSON.parse(JSON.stringify(orig));
        copy.id = `prim_${nextPrimitiveId++}`;
        copy.pose.x += PASTE_OFFSET;
        copy.pose.y += PASTE_OFFSET;
        editorPrimitives.push(copy);
        createPrimitiveMesh(copy);
        selectedPrimitiveIds.add(copy.id);
        highlightMesh(`static_${copy.id}`);
    });

    sendGeometryToServer();
}
```

### Клавиатурные обработчики

```js
window.addEventListener('keydown', e => {
    if (!editorMode) return;

    if ((e.ctrlKey || e.metaKey) && !e.shiftKey) {
        switch (e.key) {
            case 'z':
                e.preventDefault();
                undo();
                break;
            case 'c':
                e.preventDefault();
                copySelected();
                break;
            case 'v':
                e.preventDefault();
                pasteSelected();
                break;
        }
    }

    // Delete / Backspace — удалить выделенные
    if (e.key === 'Delete' || e.key === 'Backspace') {
        if (document.activeElement === document.body ||
            document.activeElement === renderer.domElement) {
            e.preventDefault();
            deleteSelected();
        }
    }
});
```

### Удаление группы

```js
function deleteSelected() {
    const ids = selectedPrimitiveIds.size > 0
        ? [...selectedPrimitiveIds]
        : (selectedPrimitiveId ? [selectedPrimitiveId] : []);

    if (ids.length === 0) return;
    pushUndoSnapshot();
    ids.forEach(id => {
        editorPrimitives = editorPrimitives.filter(p => p.id !== id);
        removeMesh(`static_${id}`);
    });
    selectedPrimitiveIds.clear();
    selectedPrimitiveId = null;
    transformControls.detach();
    document.getElementById('primitive-props').style.display = 'none';
    sendGeometryToServer();
}
```

---

## Критерии завершения

- [x] Shift+LMB перемещает камеру (pan) — только по пустому месту
- [x] MMB больше не делает pan (ничего не делает или zoom остаётся)
- [x] Ctrl+Z отменяет последнее действие (добавление, удаление, перемещение)
- [x] До 50 операций в стеке undo
- [N/A] Shift+клик на примитив добавляет его к выделению — не реализован (конфликт с edge-snap задачи 17)
- [x] Ctrl+C копирует текущий выбранный примитив
- [x] Ctrl+V вставляет со смещением +0.5 по X и Y
- [x] Delete / Backspace удаляет выбранный примитив
- [x] Copy/Paste/Delete не работает с агентами
- [x] После undo геометрия на сервере обновляется (sendGeometryToServer)

## Итог реализации

Реализовано в `workspace/s2_visualizer/web/js/app.js` (единственный изменённый файл).

**Отличия от исходной спецификации:**
- Мультиселект (Shift+клик) **не реализован** — оставлен конфликт с edge-snap задачи 17 (Shift+ЛКМ по примитиву → edge-snap). Pan активируется только по пустому месту сцены.
- Copy/Paste работает с одним (`selectedPrimitiveId`), не с группой.
