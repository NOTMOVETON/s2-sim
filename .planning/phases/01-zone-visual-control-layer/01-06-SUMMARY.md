---
plan: 06
phase: 01-zone-visual-control-layer
status: done
commit: 57af6cb
---

# Plan 01-06 Summary — Zone Inspector UI

## Выполнено

**index.html:**
- Добавлена кнопка вкладки "Зоны" (data-tab="zones") рядом с "Геометрия" и "Агенты"
- Добавлен блок `#editor-tab-zones` с формой CRUD:
  - Тип формы: sphere / box / cylinder с соответствующими параметрами
  - Поля: ID, X, Y, цвет, прозрачность (range)
  - Dropdown эффектов (multiple): ice, boost, lock, charging, conveyor, wind, teleport, fog, emi
  - Кнопки: Применить, Отмена, Удалить (скрыта при создании)

**app.js:**
- `switchEditorTab()` расширен для 'zones' → вызывает `renderZoneList()`
- `editorZones` обновляется из `data.zones` каждый тик в `updateWorld()`
- Zone Inspector функции: `startAddZone`, `startEditZone`, `cancelZoneForm`, `confirmZoneForm`, `deleteCurrentZone`, `onZoneShapeChange`, `renderZoneList`
- `confirmZoneForm()` → POST SpawnZone с shape/effects/color/opacity/id_hint
- `deleteCurrentZone()` → POST DespawnZone с id
- VisualHint рендеринг: `renderVisualHints()` + 4 рендерера:
  - `renderGlowHint` — THREE.PointLight под центром зоны
  - `renderArrowsHint` — THREE.ArrowHelper направлением вверх
  - `renderParticlesHint` — THREE.Points (20 точек вокруг центра)
  - `renderGridHint` — THREE.GridHelper внутри зоны
- Cleanup hints при удалении зоны: `removeVisualHint()` + цикл в зональном cleanup

## Acceptance criteria

- [x] `grep "editor-tab-zones" index.html` — найдено
- [x] `grep 'data-tab="zones"' index.html` — найдено
- [x] `grep "fog" index.html` — найдено (option в zf-effects)
- [x] `grep "emi" index.html` — найдено (option в zf-effects)
- [x] `grep "confirmZoneForm" app.js` — найдено
- [x] `grep "deleteCurrentZone" app.js` — найдено
- [x] `grep "DespawnZone" app.js` — найдено
- [x] `grep "renderVisualHints" app.js` — найдено
- [x] Docker build sim — успех (код 0)
