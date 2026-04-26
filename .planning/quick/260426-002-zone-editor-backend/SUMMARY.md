---
quick_id: 260426-002
status: complete
commit: 4f5577e
date: 2026-04-26
---

# Summary: zone-editor-backend

## Реализовано

Полноценное редактирование существующих зон через UI визуализатора:

**Backend (C++):**
- `ZoneSystem::update_zone_visual(id, color, opacity)` — новый метод
- `VizCommandHandler`: 3 новых виртуальных метода (move_zone, toggle_zone, update_zone_visual)
- `viz_server.cpp handle_command`: маршрутизация cmd=move_zone/toggle_zone/update_zone_visual с query params
- `SimEngineCommandAdapter`: реализация через `engine_->zone_system()`

**Frontend (JS):**
- `startEditZone()` заполняет ВСЕ поля формы из zone snapshot
- `confirmZoneForm()` отправляет move_zone + update_zone_visual
- Гизмо (TransformControls) прикрепляется к zone mesh при Edit
- Drag → mouseUp → move_zone + обновление полей формы X/Y
- Drag grace: SSE snapshot не перезаписывает позицию во время перетаскивания

**Ограничения (Wave 2):**
- Создание новых зон (SpawnZone) — не реализовано
- Удаление зон (DespawnZone) — не реализовано
- Изменение формы/размера зоны — частично (resize_zone API существует, UI отправка — Wave 2)
