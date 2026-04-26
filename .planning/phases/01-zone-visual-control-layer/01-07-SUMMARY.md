---
phase: 01-zone-visual-control-layer
plan: 07
subsystem: s2_visualizer, s2_core
tags: [zone, rest-api, spawn, despawn, resize, contact-link, per-link, viz-server, ui]
dependency_graph:
  requires:
    - "01-04: KernelCommand SpawnZone/DespawnZone structs, SimEngine command_queue_"
    - "01-06: Zone Inspector UI (confirmZoneForm, deleteCurrentZone stubs)"
  provides:
    - "REST routes POST /command?cmd=spawn_zone, despawn_zone, resize_zone в viz_server"
    - "VizCommandHandler::on_spawn_zone(), on_despawn_zone(), on_resize_zone(), on_move_zone(x,y,z)"
    - "SimEngineCommandAdapter overrides -> push_command / zone_system().resize_zone()"
    - "UI confirmZoneForm() fetch POST spawn_zone (новая) или move+visual+resize (редактирование)"
    - "UI deleteCurrentZone() fetch POST despawn_zone с id зоны"
    - "InZoneResult struct с contact_link для PER_LINK detection"
    - "ctx.contact_link заполняется в apply_active_effects() и on_agent_enter()"
    - "Zone mesh пересоздаётся при изменении формы/размеров (zoneTag в userData)"
  affects:
    - "workspace/s2_visualizer/src/viz_server.hpp"
    - "workspace/s2_visualizer/src/viz_server.cpp"
    - "workspace/s2_visualizer/src/main.cpp"
    - "workspace/s2_visualizer/web/js/app.js"
    - "workspace/s2_visualizer/web/index.html"
    - "workspace/s2_core/include/s2/zone_system.hpp"
    - "workspace/s2_core/src/zone_system.cpp"
tech_stack:
  added: []
  patterns:
    - "REST route -> VizCommandHandler virtual -> SimEngineCommandAdapter override -> push_command(KernelCommand) / zone_system()"
    - "InZoneResult: расширенный результат agent_in_zone с contact_link для PER_LINK"
    - "zoneTag в mesh.userData: пересоздание mesh при изменении shape_type или размеров"
key_files:
  created: []
  modified:
    - workspace/s2_visualizer/src/viz_server.hpp
    - workspace/s2_visualizer/src/viz_server.cpp
    - workspace/s2_visualizer/src/main.cpp
    - workspace/s2_visualizer/web/js/app.js
    - workspace/s2_visualizer/web/index.html
    - workspace/s2_core/include/s2/zone_system.hpp
    - workspace/s2_core/src/zone_system.cpp
key_decisions:
  - "on_spawn_zone принимает все shape параметры отдельно (radius, hx/hy/hz, cyl_r/cyl_h) -- универсальный маршрут для всех типов форм"
  - "agent_in_zone() остаётся inline делегатом к agent_in_zone_result().inside -- полная backward compat"
  - "box half_size: форма UI вводит полные W/D/H, деление на 2 происходит в JS при отправке"
  - "on_move_zone передаёт z-координату (Three.js Y -> sim Z) -- зоны не физические объекты, позиция фиксируется"
  - "resize_zone() сохраняет center зоны -- resize != move"
  - "zone mesh пересоздаётся через zoneTag (type:dims) в userData -- updateOrCreateMesh не обновляет геометрию"
patterns_established:
  - "REST command routing: cmd param -> handler->on_xxx() -> push_command(KernelCommand{cmd::Xxx{}}) / zone_system().method()"
  - "InZoneResult pattern: agent_in_zone_result() для расширенных данных, agent_in_zone() для bool"
  - "Zone mesh invalidation: zoneTag в userData для определения необходимости пересоздания mesh"
requirements_completed: [ZONE-01, ZONE-05, ZONE-06]
metrics:
  duration: "5 min + bugfixes"
  completed_date: "2026-04-26"
  tasks_completed: 3
  tasks_total: 3
  files_modified: 7
  files_created: 0
---

# Phase 01 Plan 07: Gap Closure -- REST SpawnZone/DespawnZone + UI Fetch + contact_link Summary

**REST endpoints spawn_zone/despawn_zone/resize_zone в viz_server, UI кнопки Применить/Удалить реально создают, редактируют и удаляют зоны через fetch POST, ctx.contact_link заполняется при PER_LINK detection.**

## Performance

- **Duration:** 5 min + bugfixes
- **Started:** 2026-04-26T16:35:57Z
- **Completed:** 2026-04-26T16:41:28Z
- **Tasks:** 3/3
- **Files modified:** 7

## Accomplishments
- REST routes spawn_zone, despawn_zone и resize_zone в viz_server handle_command() с полным парсингом параметров
- VizCommandHandler расширен: on_spawn_zone, on_despawn_zone, on_resize_zone, on_move_zone(x,y,z)
- SimEngineCommandAdapter реализует все overrides: push_command для spawn/despawn, zone_system().resize_zone() для resize
- UI confirmZoneForm() отправляет POST spawn_zone (новая зона) или move+visual+resize (редактирование)
- UI deleteCurrentZone() отправляет POST despawn_zone
- InZoneResult struct и agent_in_zone_result() возвращает имя первого линка при PER_LINK detection
- ctx.contact_link заполняется в apply_active_effects() и on_agent_enter()

## Task Commits

1. **Task 1: REST routes spawn_zone/despawn_zone + VizCommandHandler + SimEngineCommandAdapter** -- `60864a7` (feat)
2. **Task 2: UI confirmZoneForm() и deleteCurrentZone() -- замена заглушек на реальные fetch POST** -- `d5f80d6` (feat)
3. **Task 3: InZoneResult + ctx.contact_link при PER_LINK detection** -- `972bcbf` (feat)

## Bugfix Commits

4. **move_zone z-coordinate + resize_zone route** -- `5a9c28c` (fix)
   - on_move_zone теперь принимает x,y,z (было x,y с z=0)
   - Гизмо в app.js передаёт Three.js Y как sim Z
   - Новый route resize_zone + on_resize_zone в VizCommandHandler/SimEngineCommandAdapter
   - confirmZoneForm в режиме редактирования отправляет resize_zone
5. **resize_zone сохраняет center** -- `723a22e` (fix)
   - ZoneSystem::resize_zone() перезаписывал весь shape включая center -- зона прыгала в {0,0,0}
   - Теперь center сохраняется при resize
6. **Zone mesh пересоздание при изменении формы/размеров** -- `cf60d34` (fix)
   - updateOrCreateMesh создавал геометрию один раз, не обновляя при resize
   - Теперь zoneTag (type:dims) в userData -- при изменении mesh удаляется и пересоздаётся

## Files Created/Modified
- `workspace/s2_visualizer/src/viz_server.hpp` -- on_spawn_zone, on_despawn_zone, on_resize_zone, on_move_zone(x,y,z)
- `workspace/s2_visualizer/src/viz_server.cpp` -- routes spawn_zone, despawn_zone, resize_zone, move_zone с z
- `workspace/s2_visualizer/src/main.cpp` -- SimEngineCommandAdapter overrides: spawn/despawn/resize/move(z)
- `workspace/s2_visualizer/web/js/app.js` -- confirmZoneForm fetch POST (spawn/move+visual+resize), гизмо z, zoneTag mesh invalidation
- `workspace/s2_visualizer/web/index.html` -- обновлён hint текст
- `workspace/s2_core/include/s2/zone_system.hpp` -- InZoneResult struct, agent_in_zone_result()
- `workspace/s2_core/src/zone_system.cpp` -- agent_in_zone_result(), ctx.contact_link, resize_zone preserves center

## Decisions Made
- on_spawn_zone() принимает все shape параметры отдельно вместо сериализованного ZoneShape -- проще для URL query params
- agent_in_zone() остаётся inline делегатом: нулевой breaking change
- Box half_size: форма UI вводит W/D/H (полные размеры), деление на 2 в JS при отправке
- on_move_zone передаёт z-координату -- зоны не физические объекты, остаются где поставлены
- resize_zone() сохраняет center -- resize != move
- zone mesh invalidation через zoneTag в userData -- единственный способ обновить геометрию в updateOrCreateMesh

## Deviations from Plan

- Добавлен route resize_zone (не в исходном плане) -- без него редактирование формы зон невозможно
- on_move_zone расширен z-координатой (в плане была только 2D позиция)
- Zone mesh invalidation через zoneTag -- обнаружено при тестировании UI

## Issues Encountered

- updateOrCreateMesh создавал геометрию один раз и не обновлял -- потребовался zoneTag invalidation
- resize_zone перезаписывал center на {0,0,0} -- исправлено сохранением old_center
- Авто-рефреш renderZoneList на каждом SSE фрейме ломал кнопку Edit -- отложено на потом

## Known Stubs

- Авто-рефреш списка зон при spawn/delete -- отложено (renderZoneList при SSE обновлении ломает onclick)

## Next Phase Readiness
- Gap closure Phase 1 завершён: SC1 (UI fetch), SC3 (REST routes), SC4 (contact_link) -- все закрыты
- Дополнительно: resize_zone, move_zone z-coord, mesh invalidation
- Фаза 1 готова к transition

---
*Phase: 01-zone-visual-control-layer*
*Completed: 2026-04-26*
