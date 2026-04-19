# Задача 17 — Редактор сцены: edge-snapping примитивов

## Цель

Реализовать привязку рёбер примитивов (edge-snapping): зажав `Shift` и кликнув
левой кнопкой мыши вблизи ребра одного примитива, затем вблизи ребра второго —
второй объект перемещается так, чтобы выбранные рёбра (midpoints) совпали.
Оба объекта сохраняют свою ориентацию, первый не двигается.

## Зависимости

- Требует: задача 15 (примитивы существуют, TransformControls работает)
- Не зависит от: задачи 16 (агенты), 18 (навигация)

## Определение «ребра»

Для **box**: 12 рёбер — по 4 параллельных каждой из осей X, Y, Z.
Midpoint ребра (локальная система): одна координата = 0, две другие = ±half_size.

Для **cylinder**: два круговых ребра — верхний (y = +h/2) и нижний (y = −h/2) торец.
Midpoint = центр окружности.

Для **sphere**: псевдоребро — нормализованная точка попадания × radius.

## Алгоритм

### Определение ближайшего ребра

```js
function findNearestEdge(prim, mesh, hitPointWorld)
```

1. Преобразовать `hitPointWorld` в локальные координаты меша.
2. Для box: перебрать все 12 midpoint-ов рёбер, найти ближайший к localHit.
3. Для cylinder: выбрать топ или бот по расстоянию от localHit.
4. Для sphere: нормализовать localHit × radius.
5. Преобразовать midpoint обратно в мировые координаты через `applyMatrix4(matrixWorld)`.

Возвращает `{ worldMid, worldDir, halfLen, shape }`:
- `shape = 'segment'` — отрезок (box)
- `shape = 'ring'` — окружность (cylinder)
- `shape = 'point'` — точка (sphere)

### Состояние выбора рёбер

```js
let snapEdge1 = null;  // { primId, edgeMid: THREE.Vector3 }
let snapEdge2 = null;
```

Первый Shift+ЛКМ → `snapEdge1`, подсветка ребра.
Второй Shift+ЛКМ → snap: `delta = snapEdge1.edgeMid - edgeMid2`, `mesh2.position.add(delta)`.
Повторный клик на тот же примитив → сброс.

### Вычисление смещения

```js
const delta = snapEdge1.edgeMid.clone().sub(edgeInfo.worldMid);
mesh2.position.add(delta);
syncPrimitiveFromMesh(primId);
sendGeometryToServer();
```

### Подсветка ребра

```js
function showEdgeHighlight(edgeInfo)
```

- `segment`: тонкий жёлтый `CylinderGeometry(0.03, 0.03, halfLen*2)`, повёрнутый по `worldDir`
- `ring`: `TorusGeometry(halfLen, 0.03)`, повёрнутый по `worldDir` (ось цилиндра)
- `point`: `SphereGeometry(0.08)`

Все с `depthTest: false` — виден поверх поверхности.

### Обработка Shift

```js
window.addEventListener('keyup', e => {
    if (e.key === 'Shift') {
        shiftHeld = false;
        if (snapEdge1 && !snapEdge2) clearEdgeSnap();
    }
});
```

## Реализованные функции

Все изменения только в `workspace/s2_visualizer/web/js/app.js`:

- `findNearestEdge(prim, mesh, hitPointWorld)` — ближайшее ребро к точке попадания
- `showEdgeHighlight(edgeInfo)` — жёлтая подсветка (цилиндр / тор / сфера)
- `clearEdgeSnap()` — сброс состояния и удаление подсветки
- `handleEdgeSnapClick(event)` — основной обработчик Shift+ЛКМ
- Глобальные переменные: `snapEdge1`, `snapEdge2`, `edgeHighlightMesh`, `shiftHeld`
- Keydown/keyup обработчики для Shift
- Guard `if (editorMode && shiftHeld)` в click-обработчике

## Критерии завершения

- [x] Shift+ЛКМ вблизи ребра box — ребро подсвечивается жёлтым цилиндром
- [x] Shift+ЛКМ вблизи ребра второго box — второй box перемещается, рёбра совпадают
- [x] Ориентация обоих объектов не меняется
- [x] Первый объект не сдвигается
- [x] Работает для box-box, box-cylinder, cylinder-sphere
- [x] Отпускание Shift сбрасывает незавершённый snap
- [x] После snap объект можно переместить TransformControls

## Известные ограничения

- Snap работает только в режиме редактора (`editorMode = true`)
- Snap только между примитивами, не между примитивом и агентом
- Snap только транслирует (не вращает) примитив 2
- Snap выравнивает midpoint'ы рёбер, но не выравнивает направления рёбер
- В задаче 18 Shift+ЛКМ планируется для pan: нужно учесть, что в editor mode
  Shift+ЛКМ по пустому месту — pan, по примитиву — edge-snap
