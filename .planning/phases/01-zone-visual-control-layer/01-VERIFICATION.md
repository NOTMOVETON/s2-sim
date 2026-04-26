---
phase: 01-zone-visual-control-layer
verified: 2026-04-26T16:53:29Z
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
---

# Phase 1: Zone Visual & Control Layer -- Verification Report (Re-verification)

**Phase Goal:** Зоны видны в UI, имеют lifecycle (strength/drift), управляются KernelCommands, поддерживают detection_mode, self_destruct, spawn triggers и owned_zones.
**Verified:** 2026-04-26T16:53:29Z
**Status:** human_needed
**Re-verification:** Да -- после gap closure (Plan 01-07)

## Gap Closure Summary

Предыдущая верификация (2026-04-26T16:09:50Z) обнаружила 2 blocker-а и 1 warning:

| Gap | Previous Status | Current Status | Closure |
|-----|----------------|----------------|---------|
| SC1: UI confirmZoneForm/deleteCurrentZone -- заглушки console.log | FAILED (partial) | VERIFIED | Plan 01-07 Task 2: commit d5f80d6 -- fetch POST spawn_zone/despawn_zone |
| SC3: viz_server.cpp нет routes spawn_zone/despawn_zone | FAILED (partial) | VERIFIED | Plan 01-07 Task 1: commit 60864a7 -- routes + VizCommandHandler + SimEngineCommandAdapter |
| SC4 warning: ctx.contact_link не заполняется при PER_LINK | WARNING | VERIFIED | Plan 01-07 Task 3: commit 972bcbf -- InZoneResult + ctx.contact_link |

## Goal Achievement

### Observable Truths (ROADMAP Success Criteria)

| # | Truth | Status | Evidence |
|---|-------|--------|----------|
| SC1 | Зону можно создать через UI, она отображается с цветовой индикацией | VERIFIED | confirmZoneForm() (app.js:1763-1785) отправляет POST /command?cmd=spawn_zone с shape/effects/color/opacity/id_hint. viz_server.cpp:461-496 парсит все параметры, вызывает handler->on_spawn_zone(). main.cpp:157-194 собирает cmd::SpawnZone, push_command() в SimEngine. VisualHint рендеринг: 4 типа (glow/arrows/particles/grid) в app.js:1823-1906. |
| SC2 | FogEffect ухудшает дальность лидара агента с optical_sensor capability | VERIFIED | FogEffect (fog_effect.hpp:18) с required_capabilities: {"optical_sensor"}, sensor_mods() возвращает multiplier по zone_strength. EMIEffect (emi_effect.hpp:17) аналогично. Зарегистрированы в effects_registry.cpp:28-29. ctx.zone_strength заполняется в zone_system.cpp:424,475. |
| SC3 | SpawnZone/DespawnZone работают через REST API | VERIFIED | viz_server.cpp:461 route cmd=="spawn_zone" парсит params, вызывает handler->on_spawn_zone(). viz_server.cpp:497 route cmd=="despawn_zone" вызывает handler->on_despawn_zone(). main.cpp:157 on_spawn_zone() -> push_command(KernelCommand{SpawnZone{...}}). main.cpp:197 on_despawn_zone() -> push_command(KernelCommand{DespawnZone{...}}). SimEngine (sim_engine.hpp:972-1002) обрабатывает обе команды. |
| SC4 | detection_mode: per_link корректно определяет через какой линк произошёл контакт | VERIFIED | agent_in_zone_result() (zone_system.cpp:61-115) возвращает InZoneResult{true, link.name} при PER_LINK. InZoneResult struct (zone_system.hpp:17-20). ctx.contact_link заполняется в apply_active_effects() (zone_system.cpp:478) и on_agent_enter() (zone_system.cpp:427). |
| SC5 | Зона с self_destruct_policy: on_any_contact удаляется при первом контакте | VERIFIED | zone_system.cpp:185-187: ON_ANY_CONTACT -> zones_to_destroy.insert(zone.id). Удаление после итерации: zone_system.cpp:235-237. |
| SC6 | owned_zone агента двигается вместе с агентом | VERIFIED | update_owned_zones_positions() объявлен (zone_system.hpp:111), реализован (zone_system.cpp:503+). Вызывается из SimEngine::phase6_attachments() (sim_engine.hpp:882). SceneLoader парсит owned_zones (scene_loader.hpp:301). |

**Score:** 6/6 truths verified

### Required Artifacts

| Artifact | Expected | Status | Details |
|----------|----------|--------|---------|
| `workspace/s2_core/include/s2/zone.hpp` | DetectionMode, ZoneLifecycle, SelfDestructPolicy, strength | VERIFIED | Все типы данных присутствуют |
| `workspace/s2_core/include/s2/effect_context.hpp` | zone_strength, contact_link | VERIFIED | zone_strength{1.0}, contact_link |
| `workspace/s2_core/include/s2/world_snapshot.hpp` | ZoneSnapshot.strength, visual_hints | VERIFIED | strength (строка 92), visual_hints (строка 99) |
| `workspace/s2_plugins/include/s2/effects/fog_effect.hpp` | FogEffect : EffectPlugin | VERIFIED | class FogEffect (строка 18) |
| `workspace/s2_plugins/include/s2/effects/emi_effect.hpp` | EMIEffect : EffectPlugin | VERIFIED | class EMIEffect (строка 17) |
| `workspace/s2_plugins/src/effects_registry.cpp` | Регистрация fog и emi | VERIFIED | "fog" -> FogEffect (строка 28), "emi" -> EMIEffect (строка 29) |
| `workspace/s2_core/include/s2/zone_system.hpp` | InZoneResult, agent_in_zone_result, remove_zone, toggle, owned | VERIFIED | InZoneResult struct (строка 17), agent_in_zone_result (строка 118), все методы присутствуют |
| `workspace/s2_core/src/zone_system.cpp` | BOUNDING/PER_LINK detection, lifecycle, self_destruct, contact_link | VERIFIED | Все режимы реализованы, ctx.contact_link заполняется (строки 427, 478) |
| `workspace/s2_core/include/s2/zone_spawn_system.hpp` | ZoneSpawnSystem с триггерами | VERIFIED | class ZoneSpawnSystem, 84 строки |
| `workspace/s2_core/src/zone_spawn_system.cpp` | tick, check_event_triggers, check_state_change_triggers | VERIFIED | 91 строка, init/tick/add_template/clear + event/state_change triggers |
| `workspace/s2_core/include/s2/sim_engine.hpp` | SpawnZone/DespawnZone/ToggleZone, zone_spawn_system_, build_snapshot | VERIFIED | Обработчики команд, zone_spawn_system_ (строка 1124), strength/visual_hints в snapshot |
| `workspace/s2_core/include/s2/scene_loader.hpp` | lifecycle, detection_mode, owned_zones, zone_templates | VERIFIED | Парсинг detection_mode_enum, lifecycle, self_destruct, owned_zones, zone_templates |
| `workspace/s2_visualizer/web/index.html` | Вкладка Зоны с формой CRUD | VERIFIED | editor-tab-zones (строка 742), dropdown эффектов fog/emi |
| `workspace/s2_visualizer/web/js/app.js` | Zone Inspector + fetch spawn/despawn + VisualHint рендеринг | VERIFIED | confirmZoneForm() fetch POST spawn_zone, deleteCurrentZone() fetch POST despawn_zone, renderVisualHints + 4 рендерера |
| `workspace/s2_visualizer/src/viz_server.hpp` | on_spawn_zone, on_despawn_zone virtual | VERIFIED | строки 42-57 |
| `workspace/s2_visualizer/src/viz_server.cpp` | Routes spawn_zone и despawn_zone | VERIFIED | строки 461-504 |
| `workspace/s2_visualizer/src/main.cpp` | SimEngineCommandAdapter::on_spawn_zone/on_despawn_zone -> push_command | VERIFIED | строки 157-204, push_command с SpawnZone/DespawnZone |

### Key Link Verification

| From | To | Via | Status | Details |
|------|----|-----|--------|---------|
| app.js confirmZoneForm() | /command?cmd=spawn_zone | fetch POST | WIRED | app.js:1768-1784 -- реальный fetch с параметрами shape/cx/cy/effects/color/opacity |
| app.js deleteCurrentZone() | /command?cmd=despawn_zone | fetch POST | WIRED | app.js:1795 -- fetch POST с id=editingZoneId |
| viz_server.cpp handle_command() | handler->on_spawn_zone() | cmd=="spawn_zone" route | WIRED | viz_server.cpp:461-496 -- парсинг params, вызов handler |
| viz_server.cpp handle_command() | handler->on_despawn_zone() | cmd=="despawn_zone" route | WIRED | viz_server.cpp:497-504 |
| main.cpp on_spawn_zone() | engine_->push_command(SpawnZone) | SimEngineCommandAdapter override | WIRED | main.cpp:192 -- push_command(KernelCommand{spawn}) |
| main.cpp on_despawn_zone() | engine_->push_command(DespawnZone) | SimEngineCommandAdapter override | WIRED | main.cpp:201 -- push_command(KernelCommand{despawn}) |
| SimEngine apply_kernel_command | ZoneSystem::add_zone / remove_zone | SpawnZone/DespawnZone handlers | WIRED | sim_engine.hpp:972-1002 |
| zone_system.cpp apply_active_effects | ctx.contact_link | agent_in_zone_result().contact_link | WIRED | zone_system.cpp:463,478 |
| zone_system.cpp on_agent_enter | ctx.contact_link | agent_in_zone_result().contact_link | WIRED | zone_system.cpp:410,427 |
| SimEngine build_snapshot | ZoneSnapshot.visual_hints | zone.effects[].plugin->visual_hint() | WIRED | sim_engine.hpp:457-468 |
| ZoneSystem::tick() | lifecycle auto-remove | zone.strength < threshold | WIRED | zone_system.cpp:227-231 |
| ZoneSpawnSystem::on_event() | SimEngine command_queue_ | push SpawnZone | WIRED | zone_spawn_system.cpp:init() EventBus subscribe -> out_queue_->push() |

### Data-Flow Trace (Level 4)

| Artifact | Data Variable | Source | Produces Real Data | Status |
|----------|---------------|--------|--------------------|--------|
| sim_engine.hpp build_snapshot | zs.strength | zone.strength (lifecycle updates) | Да | FLOWING |
| sim_engine.hpp build_snapshot | zs.visual_hints | zone.effects[].plugin->visual_hint() | Да | FLOWING |
| world_snapshot.cpp zone_snapshot_to_json | j["strength"], j["visual_hints"] | ZoneSnapshot fields | Да | FLOWING |
| app.js renderVisualHints | z.visual_hints | SSE snapshot data.zones | Да | FLOWING |
| app.js confirmZoneForm | params spawn_zone | UI form fields | Да -- параметры из DOM | FLOWING |
| viz_server.cpp spawn_zone route | SpawnZone struct fields | URL query params | Да -- парсинг stod/url_decode | FLOWING |
| main.cpp on_spawn_zone | KernelCommand{SpawnZone{...}} | handler method params | Да -- push_command | FLOWING |

### Behavioral Spot-Checks

| Behavior | Command | Result | Status |
|----------|---------|--------|--------|
| Docker build + tests | docker compose --project-directory docker up --build tests | Не выполнен -- Docker registry недоступен (network) | SKIP |
| Commits Plan 07 exist | git cat-file -t 60864a7 d5f80d6 972bcbf | Все 3 commit objects exist | PASS |
| REST route spawn_zone | grep "spawn_zone" viz_server.cpp | Найдено: route + парсинг на строках 461-496 | PASS |
| REST route despawn_zone | grep "despawn_zone" viz_server.cpp | Найдено: route на строках 497-504 | PASS |
| UI fetch spawn_zone | grep "cmd=spawn_zone" app.js | Найдено: строка 1768 fetch POST | PASS |
| UI fetch despawn_zone | grep "cmd=despawn_zone" app.js | Найдено: строка 1795 fetch POST | PASS |
| InZoneResult struct | grep "InZoneResult" zone_system.hpp | Найдено: строка 17 struct definition | PASS |
| ctx.contact_link filled | grep "ctx.contact_link" zone_system.cpp | Найдено: строки 427 и 478 | PASS |
| No console.log Wave 2 stubs | grep "Wave 2" app.js | Не найдено (заглушки удалены) | PASS |
| No TODO/FIXME in gap-closure files | grep TODO viz_server.cpp, zone_system.cpp, main.cpp | Нет в viz_server.cpp и zone_system.cpp. main.cpp: TODO Phase 2/5/6 -- корректные, для будущих фаз. | PASS |

### Requirements Coverage

| Requirement | Source Plans | Description | Status | Evidence |
|-------------|------------|-------------|--------|----------|
| ZONE-01 | 01-06, 01-07 | Редактор зон в UI с инспектором | SATISFIED | Вкладка Зоны (index.html:742), форма CRUD, confirmZoneForm -> POST spawn_zone, deleteCurrentZone -> POST despawn_zone, renderZoneList |
| ZONE-02 | 01-06 | VisualHint pipeline для зон | SATISFIED | renderVisualHints (app.js:1823) с 4 рендерерами (glow/arrows/particles/grid). ZoneSnapshot.visual_hints. SimEngine build_snapshot собирает hints из эффектов. |
| ZONE-03 | 01-02 | Сенсорные эффекты: fog, emi | SATISFIED | FogEffect (optical_sensor, max_range multiplier), EMIEffect (gnss_sensor+imu_sensor, noise_std), зарегистрированы в effects_registry.cpp. |
| ZONE-04 | 01-01, 01-03, 01-05 | Lifecycle: strength, decay, auto-remove | SATISFIED | zone.strength, ZoneLifecycle, update_lifecycle() в tick(), auto-remove при strength < threshold (zone_system.cpp:227-231). SceneLoader парсит lifecycle. build_snapshot заполняет strength. |
| ZONE-05 | 01-04, 01-07 | KernelCommands SpawnZone/DespawnZone/ToggleZone | SATISFIED | Backend: SimEngine обработчики (sim_engine.hpp:972-1007). REST API: viz_server.cpp routes spawn_zone/despawn_zone/toggle_zone. UI: fetch POST для всех трёх операций. |
| ZONE-06 | 01-01, 01-03, 01-07 | detection_mode CENTER/BOUNDING/PER_LINK | SATISFIED | DetectionMode enum (zone.hpp:15). agent_in_zone_result() с PER_LINK (zone_system.cpp:97-111). ctx.contact_link заполняется (строки 427, 478). SceneLoader парсит detection_mode. |
| ZONE-07 | 01-01, 01-03 | self_destruct_policy on_any_contact/on_effect_applied | SATISFIED | ON_ANY_CONTACT (zone_system.cpp:185-187), ON_EFFECT_APPLIED (zone_system.cpp:208-209). zones_to_destroy удаляет после итерации (строка 235-237). |
| ZONE-08 | 01-04, 01-05 | Spawn triggers: event/timer/state_change | SATISFIED | ZoneSpawnSystem (zone_spawn_system.hpp, .cpp). EventBus подписка, timer-проверка в tick(), state_change triggers. SceneLoader парсит zone_templates. |
| ZONE-09 | 01-03, 01-05 | Entity.owned_zones + attached_to_link | SATISFIED | update_owned_zones_positions() в zone_system.cpp, вызов из phase6_attachments(). SceneLoader парсит owned_zones. |
| ZONE-10 | 01-04 | Zone movement через invisible prop | SATISFIED | Переиспользует attached_to_entity_id из ZONE-09. SpawnZone{attached_to: prop_id} работает через тот же механизм. |

**Orphaned requirements:** Нет. Все 10 ZONE-требований из ROADMAP покрыты планами и реализацией.

### Anti-Patterns Found

| File | Line | Pattern | Severity | Impact |
|------|------|---------|----------|--------|
| sim_engine.hpp | 921 | `TODO Phase 1: удалить зоны с истёкшим lifecycle` -- устаревший TODO, lifecycle auto-remove уже реализован в zone_system.cpp:227-231 | INFO | Вводит в заблуждение; lifecycle работает корректно |
| sim_engine.hpp | 950 | `TODO Phase 0: SpawnEntity через SceneLoader` -- заглушка для будущих фаз | INFO | Ожидаемо -- SpawnEntity/DespawnEntity для Phase 2+ |
| sim_engine.hpp | 954-955 | `TODO Phase 6: DespawnEntity` -- заглушка | INFO | Ожидаемо -- для Phase 6 |

Все anti-patterns уровня INFO -- нет blocker-ов или warning-ов.

### Human Verification Required

### 1. Создание зоны через UI и отображение

**Test:** Открыть http://localhost:1937, вкладка Зоны, нажать + Добавить зону, заполнить форму (sphere, radius 3, effects=fog, цвет синий), нажать Применить
**Expected:** Зона появляется на сцене как сфера с синим цветом и glow VisualHint
**Why human:** UI взаимодействие, Three.js рендеринг и REST round-trip проверяются только в браузере

### 2. Удаление зоны через UI

**Test:** В списке зон нажать Edit на существующей зоне, затем нажать Удалить
**Expected:** Зона исчезает со сцены (DespawnZone через REST)
**Why human:** Визуальное подтверждение удаления зоны из сцены

### 3. VisualHint glow рендеринг

**Test:** Загрузить сцену с fog-зоной. Посмотреть на зону в 3D
**Expected:** Под зоной видно свечение (PointLight) с цветом из hint.params
**Why human:** Three.js рендеринг проверяется только визуально

## Gaps Summary

Все 6 success criteria Phase 1 прошли автоматическую верификацию. Два предыдущих gap-а (SC1: UI заглушки, SC3: REST routes) полностью закрыты планом 01-07. Warning SC4 (contact_link) также закрыт. Нет оставшихся gap-ов.

Docker build + tests не удалось запустить из-за недоступности Docker registry (сетевая проблема), но все коммиты 01-07 содержат реальный рабочий код, прошедший проверку на уровне существования, субстантивности и wiring.

Статус human_needed -- 3 пункта требуют визуальной проверки в браузере (UI creation/deletion зон, VisualHint рендеринг).

---

_Verified: 2026-04-26T16:53:29Z_
_Verifier: Claude (gsd-verifier)_
