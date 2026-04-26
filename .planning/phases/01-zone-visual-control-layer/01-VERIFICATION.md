---
phase: 01-zone-visual-control-layer
verified: 2026-04-26T18:55:00Z
status: human_needed
score: 6/6 must-haves verified
overrides_applied: 0
re_verification:
  previous_status: gaps_found
  previous_score: 4/6
  gaps_closed:
    - "Зону можно создать через UI, она отображается с цветовой индикацией"
    - "SpawnZone/DespawnZone работают через REST API"
  gaps_remaining: []
  regressions: []
  bugfixes_applied:
    - "move_zone z-coordinate: зоны больше не падают на z=0 при перемещении"
    - "resize_zone route: изменение формы зон через UI"
    - "resize_zone preserves center: зоны не прыгают в {0,0,0} при resize"
    - "zone mesh invalidation: визуальное обновление при изменении формы/размеров"
human_verification:
  - test: "Открыть http://localhost:1937, вкладка Зоны, нажать + Добавить зону, заполнить форму (sphere, radius 3, effects=fog, цвет синий), нажать Применить"
    expected: "Зона появляется на сцене как сфера с синим цветом и glow VisualHint"
    why_human: "UI взаимодействие, Three.js рендеринг и REST round-trip проверяются только в браузере"
  - test: "В списке зон нажать Edit на существующей зоне, затем нажать Удалить"
    expected: "Зона исчезает со сцены (DespawnZone через REST)"
    why_human: "Визуальное подтверждение удаления зоны из сцены"
  - test: "Проверить VisualHint рендеринг: загрузить сцену с fog-зоной, посмотреть в 3D"
    expected: "Под зоной видно свечение (glow PointLight) с цветом из hint.params"
    why_human: "Three.js рендеринг проверяется только визуально"
  - test: "Поднять зону гизмо вверх (по Y в Three.js), отпустить, убедиться что зона остаётся на месте"
    expected: "Зона сохраняет высоту (z-координата передаётся через move_zone)"
    why_human: "Визуальная проверка 3D позиционирования"
  - test: "Edit существующую зону, изменить форму (sphere -> box или radius), нажать Применить"
    expected: "Форма/размер зоны обновляются на сцене (mesh пересоздаётся)"
    why_human: "Визуальная проверка resize через UI"
---

# Phase 1: Zone Visual & Control Layer -- Verification Report (Re-verification)

**Phase Goal:** Зоны видны в UI, имеют lifecycle (strength/drift), управляются KernelCommands, поддерживают detection_mode, self_destruct, spawn triggers и owned_zones.
**Verified:** 2026-04-26T18:55:00Z
**Status:** human_needed
**Re-verification:** Да -- после gap closure (Plan 01-07) + bugfixes

## Gap Closure Summary

Предыдущая верификация обнаружила 2 blocker-а и 1 warning. Все закрыты планом 01-07:

| Gap | Previous Status | Current Status | Closure |
|-----|----------------|----------------|---------|
| SC1: UI confirmZoneForm/deleteCurrentZone -- заглушки console.log | FAILED (partial) | VERIFIED | Plan 01-07 Task 2: commit d5f80d6 |
| SC3: viz_server.cpp нет routes spawn_zone/despawn_zone | FAILED (partial) | VERIFIED | Plan 01-07 Task 1: commit 60864a7 |
| SC4 warning: ctx.contact_link не заполняется при PER_LINK | WARNING | VERIFIED | Plan 01-07 Task 3: commit 972bcbf |

## Bugfix Summary

Дополнительные баги обнаружены при UAT:

| Bug | Commit | Fix |
|-----|--------|-----|
| Зоны падают на z=0 при перемещении гизмо | `5a9c28c` | on_move_zone(x,y,z), гизмо передаёт Three.js Y как sim Z |
| Нет route для изменения формы зон | `5a9c28c` | resize_zone route + on_resize_zone + confirmZoneForm resize |
| resize_zone затирает center на {0,0,0} | `723a22e` | ZoneSystem::resize_zone() сохраняет old_center |
| Mesh зоны не обновляется при resize | `cf60d34` | zoneTag в userData, removeMesh при изменении |

## Goal Achievement

### Observable Truths (ROADMAP Success Criteria)

| # | Truth | Status | Evidence |
|---|-------|--------|----------|
| SC1 | Зону можно создать через UI, она отображается с цветовой индикацией | VERIFIED | confirmZoneForm() fetch POST spawn_zone. resize_zone для редактирования. zoneTag mesh invalidation. |
| SC2 | FogEffect ухудшает дальность лидара агента с optical_sensor capability | VERIFIED | FogEffect с required_capabilities: {"optical_sensor"}, sensor_mods() по zone_strength. |
| SC3 | SpawnZone/DespawnZone работают через REST API | VERIFIED | Routes spawn_zone, despawn_zone, resize_zone в viz_server.cpp. Проверено curl. |
| SC4 | detection_mode: per_link корректно определяет через какой линк произошёл контакт | VERIFIED | agent_in_zone_result() возвращает InZoneResult{true, link.name}. ctx.contact_link заполняется. |
| SC5 | Зона с self_destruct_policy: on_any_contact удаляется при первом контакте | VERIFIED | ON_ANY_CONTACT -> zones_to_destroy. |
| SC6 | owned_zone агента двигается вместе с агентом | VERIFIED | update_owned_zones_positions() из phase6_attachments(). |

**Score:** 6/6 truths verified

### Required Artifacts

| Artifact | Expected | Status |
|----------|----------|--------|
| `viz_server.hpp` | on_spawn_zone, on_despawn_zone, on_resize_zone, on_move_zone(x,y,z) | VERIFIED |
| `viz_server.cpp` | Routes spawn_zone, despawn_zone, resize_zone, move_zone с z | VERIFIED |
| `main.cpp` | SimEngineCommandAdapter overrides: spawn/despawn/resize/move(z) | VERIFIED |
| `app.js` | confirmZoneForm (spawn/resize), deleteCurrentZone, гизмо z, zoneTag | VERIFIED |
| `zone_system.hpp` | InZoneResult, agent_in_zone_result() | VERIFIED |
| `zone_system.cpp` | agent_in_zone_result(), ctx.contact_link, resize preserves center | VERIFIED |
| `zone_spawn_system.hpp/cpp` | ZoneSpawnSystem с триггерами | VERIFIED |
| `zone.hpp` | DetectionMode, ZoneLifecycle, SelfDestructPolicy | VERIFIED |
| `effect_context.hpp` | zone_strength, contact_link | VERIFIED |

### Key Link Verification

| From | To | Via | Status |
|------|----|-----|--------|
| app.js confirmZoneForm() | /command?cmd=spawn_zone | fetch POST | WIRED |
| app.js confirmZoneForm() | /command?cmd=resize_zone | fetch POST (edit mode) | WIRED |
| app.js deleteCurrentZone() | /command?cmd=despawn_zone | fetch POST | WIRED |
| app.js gizmo mouseUp | /command?cmd=move_zone&z= | fetch POST с z | WIRED |
| viz_server.cpp | handler->on_resize_zone() | cmd=="resize_zone" | WIRED |
| main.cpp on_resize_zone() | zone_system().resize_zone() | direct call | WIRED |
| zone_system.cpp resize_zone | zone.shape = new_shape; center preserved | old_center | WIRED |
| app.js SSE handler | removeMesh + updateOrCreateMesh | zoneTag changed | WIRED |

### Requirements Coverage

| Requirement | Status | Summary |
|-------------|--------|---------|
| ZONE-01 | SATISFIED | UI редактор зон: CRUD + resize + gizmo 3D |
| ZONE-02 | SATISFIED | VisualHint pipeline (glow/arrows/particles/grid) |
| ZONE-03 | SATISFIED | FogEffect + EMIEffect сенсорные плагины |
| ZONE-04 | SATISFIED | Lifecycle: strength, decay, auto-remove |
| ZONE-05 | SATISFIED | KernelCommands SpawnZone/DespawnZone/ToggleZone + REST routes |
| ZONE-06 | SATISFIED | detection_mode CENTER/BOUNDING/PER_LINK с contact_link |
| ZONE-07 | SATISFIED | self_destruct_policy on_any_contact/on_effect_applied |
| ZONE-08 | SATISFIED | Spawn triggers: event/timer/state_change |
| ZONE-09 | SATISFIED | Entity.owned_zones + attached_to_link |
| ZONE-10 | SATISFIED | Zone movement через invisible prop |

### Known Issues

- Авто-рефреш списка зон при spawn/delete -- отложено (renderZoneList при SSE обновлении ломает onclick кнопок). Workaround: переключить вкладку и вернуться.

### Human Verification Required

1. **Создание зоны через UI** -- Применить в форме новой зоны, зона появляется на сцене
2. **Удаление зоны через UI** -- Edit -> Удалить, зона исчезает
3. **VisualHint glow рендеринг** -- fog-зона со свечением в Three.js
4. **Перемещение зоны по высоте** -- гизмо вверх, зона остаётся на месте (z сохраняется)
5. **Изменение формы зоны** -- Edit, сменить shape/radius, Применить -- mesh обновляется

## Gaps Summary

Все 6 success criteria полностью верифицированы. Дополнительно закрыты 4 бага при UAT.
Статус human_needed -- 5 пунктов требуют визуальной проверки в браузере.

---
_Verified: 2026-04-26T18:55:00Z_
_Verifier: Claude (gsd-verifier) + manual bugfix verification_
