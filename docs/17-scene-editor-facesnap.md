# Задача 17 — Редактор сцены: face-snapping примитивов

## Цель

Реализовать привязку граней примитивов (face-snapping): зажав `Shift` и кликнув
левой кнопкой мыши на грань одного примитива, затем на грань второго — второй
объект перемещается так, чтобы выбранные грани соприкоснулись. Оба объекта
сохраняют свою ориентацию в пространстве, первый объект не двигается.

## Зависимости

- Требует: задача 15 (примитивы существуют, TransformControls работает)
- Не зависит от: задачи 16 (агенты), 18 (навигация)

## Определение «грани»

Для **box**: 6 граней — top (+Z), bottom (-Z), front (+Y), back (-Y), right (+X), left (-X)
в локальной системе координат примитива.

Для **cylinder**: top (+Z), bottom (-Z), side (lateral surface — обрабатывается как
ближайшая точка окружности к курсору).

Для **sphere**: любая точка поверхности — нормаль в точке клика.

## Алгоритм

### Шаг 1: Определение грани при клике

При Shift+LMB в режиме редактора:

1. Raycast по всем `static_` мешам
2. Получить `face` из результата: `intersects[0].face` (THREE.Face — содержит нормаль)
3. Преобразовать нормаль грани из локальной системы объекта в мировую:
   ```js
   const worldNormal = face.normal.clone()
       .transformDirection(mesh.matrixWorld);
   ```
4. Получить точку пересечения (мировые координаты):
   ```js
   const hitPoint = intersects[0].point; // мировая точка на поверхности
   ```
5. Вычислить «позицию грани» — центр грани:
   ```js
   // Для box: центр грани = центр объекта + нормаль * half_extent_в_направлении_нормали
   const faceCenter = computeFaceCenter(prim, worldNormal);
   ```

### Шаг 2: Состояние выбора граней

```js
let snapFace1 = null;  // { primId, faceCenter, faceNormal }
let snapFace2 = null;
```

При первом Shift+LMB → `snapFace1 = { primId, faceCenter, worldNormal }`, подсветить грань.
При втором Shift+LMB → `snapFace2 = { primId, faceCenter, worldNormal }`, выполнить snap.

Если кликнуть туда же (повторный Shift+LMB на ту же грань) → сбросить `snapFace1`.

### Шаг 3: Вычисление смещения для snap

Цель: грань объекта 2 должна соприкоснуться с гранью объекта 1.

```
snapFace1.faceCenter + snapFace1.faceNormal * 0 (объект 1 не двигается)
snapFace2.faceCenter должна совпасть с snapFace1.faceCenter
```

Вектор смещения объекта 2:
```js
function computeSnapOffset(face1, face2) {
    // face2 должна встать туда, где face1
    const delta = face1.faceCenter.clone().sub(face2.faceCenter);
    return delta;
}
```

Новая позиция центра объекта 2:
```js
const prim2 = editorPrimitives.find(p => p.id === face2.primId);
const mesh2 = meshes[`static_${face2.primId}`];

const offset = computeSnapOffset(face1, face2);
mesh2.position.add(offset);

// Синхронизировать pose в editorPrimitives
syncPrimitiveFromMesh(face2.primId);
sendGeometryToServer();
```

### Шаг 4: Вычисление faceCenter для разных типов

```js
function computeFaceCenter(prim, mesh, worldNormal) {
    const localNormal = worldNormal.clone()
        .transformDirection(mesh.matrixWorld.clone().invert());
    // Найти доминирующую ось локальной нормали
    const ax = Math.abs(localNormal.x);
    const ay = Math.abs(localNormal.y);
    const az = Math.abs(localNormal.z);

    let localFaceCenter = new THREE.Vector3();

    if (prim.type === 'box') {
        const hx = (prim.size?.x || 1) / 2;
        const hy = (prim.size?.y || 1) / 2;
        const hz = (prim.size?.z || 1) / 2;
        if (ax > ay && ax > az)
            localFaceCenter.set(Math.sign(localNormal.x) * hx, 0, 0);
        else if (ay > ax && ay > az)
            localFaceCenter.set(0, Math.sign(localNormal.y) * hy, 0);
        else
            localFaceCenter.set(0, 0, Math.sign(localNormal.z) * hz);
    } else if (prim.type === 'cylinder') {
        const h = (prim.height || 1.0) / 2;
        if (az > ax && az > ay)
            localFaceCenter.set(0, 0, Math.sign(localNormal.z) * h);
        else
            localFaceCenter.set(
                Math.sign(localNormal.x) * (prim.radius || 0.5),
                Math.sign(localNormal.y) * (prim.radius || 0.5),
                0
            );
    } else if (prim.type === 'sphere') {
        localFaceCenter = localNormal.clone()
            .multiplyScalar(prim.radius || 0.5);
    }

    // Преобразовать в мировые координаты
    return localFaceCenter.applyMatrix4(mesh.matrixWorld);
}
```

### Шаг 5: Подсветка выбранной грани

После первого клика — отобразить визуальный индикатор:
- Маленький полупрозрачный диск (PlaneGeometry) в точке `faceCenter`, ориентированный
  по `worldNormal`:

```js
function showFaceHighlight(faceCenter, worldNormal) {
    if (faceHighlightMesh) scene.remove(faceHighlightMesh);
    const geo = new THREE.PlaneGeometry(0.3, 0.3);
    const mat = new THREE.MeshBasicMaterial({
        color: 0xFFFF00, transparent: true, opacity: 0.6, side: THREE.DoubleSide
    });
    faceHighlightMesh = new THREE.Mesh(geo, mat);
    faceHighlightMesh.position.copy(faceCenter);
    faceHighlightMesh.lookAt(faceCenter.clone().add(worldNormal));
    scene.add(faceHighlightMesh);
}
```

При втором клике (выполнение snap) — убрать highlight.

### Шаг 6: Обработка клавиши Shift

```js
window.addEventListener('keydown', e => {
    if (e.key === 'Shift') shiftHeld = true;
});
window.addEventListener('keyup', e => {
    if (e.key === 'Shift') {
        shiftHeld = false;
        // Сбросить выбор граней если snap не был завершён
        if (snapFace1 && !snapFace2) {
            snapFace1 = null;
            if (faceHighlightMesh) { scene.remove(faceHighlightMesh); faceHighlightMesh = null; }
        }
    }
});
```

В обработчике клика:
```js
canvas.addEventListener('click', e => {
    if (editorMode && shiftHeld) {
        handleFaceSnapClick(e);
        return; // не обрабатывать как выбор объекта
    }
    // ... обычная обработка
});
```

## Критерии завершения

- [ ] Shift+LMB на грань box — грань подсвечивается жёлтым диском
- [ ] Shift+LMB на грань второго box — второй box перемещается к первому (грани совпадают)
- [ ] Ориентация обоих объектов не меняется
- [ ] Первый объект не сдвигается
- [ ] Работает для box-box, box-cylinder, cylinder-sphere
- [ ] Отпускание Shift сбрасывает незавершённый snap
- [ ] После snap объект можно переместить TransformControls

## Известные упрощения

- Snap работает только в режиме редактора (editorMode = true)
- Snap только между двумя примитивами, не между примитивом и агентом
- Snap не учитывает вращение объекта 2 — только перемещает центр
