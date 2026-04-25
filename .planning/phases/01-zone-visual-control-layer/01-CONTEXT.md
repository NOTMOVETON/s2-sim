# Phase 1: Zone Visual & Control Layer - Context

**Gathered:** 2026-04-26
**Status:** Ready for planning

<domain>
## Phase Boundary

Зоны видны в UI, имеют lifecycle (strength/drift), управляются KernelCommands,
поддерживают detection_mode, self_destruct, spawn triggers и owned_zones.

Требования: ZONE-01, ZONE-02, ZONE-03, ZONE-04, ZONE-05, ZONE-06, ZONE-07,
ZONE-08, ZONE-09, ZONE-10.

Scope фиксирован задачами 1.1–1.10 из ROADMAP.md. Phase 2 (Actor FSM) и
Phase 5 (/api/effects/registry) — зависимости, но Phase 1 работает без них.

</domain>

<decisions>
## Implementation Decisions

### UI Zone Inspector (ZONE-01)

- **D-01:** Инспектор зон встраивается в **существующую панель Edit Scene** — отдельный
  раздел "Zones" рядом с Agents/Props/Primitives. Без нового модального окна.
- **D-02:** Форма создания/редактирования зоны в Phase 1 — **минимальный CRUD**:
  форма (sphere/box/cylinder), позиция, размер, цвет, список эффектов.
  Параметры конкретного эффекта (config_schema) — Phase 5 когда появится /api/effects/registry.
- **D-03:** Выбор типа эффекта в форме — **hardcoded dropdown**: ice, boost, lock,
  charging, conveyor, wind, teleport, fog, emi. В Phase 5 dropdown заменяется
  на динамический список из /api/effects/registry без изменения контракта.
- **D-04:** Удаление зоны — кнопка **Delete в панели редактирования** → DespawnZone
  KernelCommand. Без диалога подтверждения.

### VisualHint Pipeline (ZONE-02)

- **D-05:** "Анимация активации агента внутри" = **только color/opacity зоны** в
  ZoneSnapshot. Отдельная анимация на агенте или пульс границы — не нужны.
  ZoneSnapshot.color/opacity уже есть — достаточно чтобы каждый тип зоны имел
  свой цвет по умолчанию.
- **D-06:** VisualHint типы в app.js — **все 4**: `glow`, `arrows`, `particles`, `grid`.
  EffectPlugin.visual_hint() уже возвращает VisualHint{type, params}. Нужно
  реализовать рендеринг всех 4 типов в Three.js (сейчас app.js не рендерит их).

### Сенсорные эффекты (ZONE-03)

- **D-07:** FogEffect и EMIEffect — новые EffectPlugin реализации. Используют
  **существующий механизм sensor_mods()** из EffectPlugin интерфейса — возвращают
  SensorMod (параметр + коэффициент). LidarPlugin читает sensor_mods в Phase 4
  тика. Никаких изменений в интерфейсе EffectPlugin не требуется.
- **D-08:** FogEffect: required_capabilities: [optical_sensor], ухудшает max_range
  через SensorMod. EMIEffect: required_capabilities: [gnss_sensor, imu_sensor],
  добавляет noise_std через SensorMod.

### Zone Lifecycle (ZONE-04)

- **D-09:** Поле `strength` (0.0–1.0) добавляется в `struct Zone` и в `ZoneSnapshot`.
  EffectContext уже имеет zone_strength — плагины уже могут использовать его,
  поле просто не было заполнено.
- **D-10:** Lifecycle update (рост, затухание, auto-remove при strength < threshold)
  выполняется в **Phase 0 тика** вместе с KernelCommands — до применения эффектов.
  Так агенты видят актуальный strength уже в текущем тике.

### Zone KernelCommands (ZONE-05)

- **D-11:** SpawnZone/DespawnZone/ToggleZone уже в KernelCommand variant — нужны только
  **обработчики в SimEngine::phase0_kernel_commands()**. ToggleZone при enabled=false
  отправляет on_exit всем Entity внутри; при enabled=true — on_enter.

### Detection Mode (ZONE-06)

- **D-12:** Три режима: CENTER / BOUNDING / PER_LINK. Enum `DetectionMode` в zone.hpp.
  PER_LINK: итерация kinematic_tree агента, EffectContext.contact_link заполняется
  именем линка при контакте.

### Self-Destruct Policy (ZONE-07)

- **D-13:** Поле `self_destruct_policy` в Zone struct: `on_any_contact` удаляет зону
  при любом контакте; `on_effect_applied` — только если эффект применился
  (не поглощён immunity). DespawnZone вызывается из ZoneSystem по итогам тика.

### Spawn Triggers (ZONE-08)

- **D-14:** ZoneSpawnSystem реализует **все 4 типа триггеров** в Phase 1:
  - `command` — уже есть (SpawnZone KernelCommand)
  - `event` — EventBus filter по типу + source_entity (любое EventBus событие:
    SignalActivated, GrabSucceeded, ZoneEntered, ...)
  - `timer` — N секунд от старта симуляции (sim_time)
  - `state_change` — подписка на `ActorStateChanged` EventBus с фильтром
    entity_id + state_value; реализуется в Phase 1, реально используется с Phase 2
    когда появятся Actor FSM
- **D-15:** Разграничение `event` vs `state_change` триггера: `event` = любое
  EventBus событие (плагин агента публикует что-то → зона спавнится); `state_change` =
  конкретно ActorStateChanged (FSM переход конкретного актора в конкретное состояние).
- **D-16:** ZoneSpawnSystem — отдельная система в SimEngine, подписывается на EventBus
  при инициализации, регистрирует pending zone templates.

### Entity.owned_zones (ZONE-09)

- **D-17:** Поле `owned_zones` в YAML сцены **и** через SpawnZone{attached_to: entity_id}
  в рантайме. SceneLoader спавнит owned_zones автоматически при загрузке сцены.
- **D-18:** **Per-link attachment** реализуется в Phase 1: `attached_to_link: <link_name>`
  в конфиге зоны. ZoneSystem в Phase 6 тика (attachments) обновляет позу зоны из
  kinematic_frames AgentSnapshot по имени линка.

### Zone Movement via Invisible Prop (ZONE-10)

- **D-19:** Паттерн "невидимый проп-носитель": SpawnProp(invisible, position) +
  SpawnZone(attached_to: prop_id). Prop может иметь плагин drift_behavior.
  Никакого специального кода зоны — переиспользует SpawnZone{attached_to} из ZONE-09.

### Claude's Discretion

- Структура ZoneSpawnSystem внутри SimEngine (отдельный класс или метод фазы)
- Формат YAML конфигурации spawn_trigger (точный синтаксис полей)
- Анимационные параметры 4 типов VisualHint в Three.js (цвета, скорости)
- Порядок итерации kinematic_tree при per_link detection

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### Основной дизайн-документ (главный источник правды)
- `RESULT_DISCUSS.md` §9 — Система зон: detection_mode, effect model, lifecycle,
  self_destruct, spawn triggers, owned_zones, movement pattern
- `RESULT_DISCUSS.md` §9.4 — Единый интерфейс EffectPlugin (apply + capabilities matching)
- `RESULT_DISCUSS.md` §9.6 — Lifecycle зоны: strength, growth/decay, spawn triggers
- `RESULT_DISCUSS.md` §9.7 — Self-destruct policy
- `RESULT_DISCUSS.md` §9.8 — Toggle и enabled: семантика on_exit/on_enter
- `RESULT_DISCUSS.md` §9.9 — Movement зоны через invisible prop
- `RESULT_DISCUSS.md` §9.10 — Owned zones: YAML объявление + attached_to_link

### Требования
- `.planning/REQUIREMENTS.md` §ZONE-01–ZONE-10 — Полные спецификации Phase 1
- `.planning/ROADMAP.md` §Phase 1 — 10 задач (1.1–1.10) с деталями реализации

### Существующий код (точки изменений и переиспользования)
- `workspace/s2_core/include/s2/zone.hpp` — Zone struct: добавить strength, detection_mode enum, self_destruct_policy
- `workspace/s2_core/include/s2/zone_system.hpp` — ZoneSystem: detection, per_link, owned_zones
- `workspace/s2_core/include/s2/world_snapshot.hpp` — ZoneSnapshot: добавить strength
- `workspace/s2_core/include/s2/kernel_command.hpp` — SpawnZone/DespawnZone/ToggleZone уже есть
- `workspace/s2_core/include/s2/interfaces/effect_plugin.hpp` — VisualHint struct, sensor_mods() уже есть
- `workspace/s2_plugins/include/s2/effects/` — существующие эффекты (ice, boost, conveyor...) как образцы для FogEffect/EMIEffect
- `workspace/s2_visualizer/web/js/app.js` — рендеринг зон (zones array), добавить VisualHint rendering

</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets
- `Zone.color/opacity/visible` + `ZoneSnapshot.color/opacity` — визуальные поля уже есть, рендеринг в app.js уже рисует зоны с цветом
- `EffectPlugin.visual_hint()` — метод уже в интерфейсе, эффекты (ice, charging, conveyor...) уже возвращают VisualHint; нужен только рендеринг в Three.js
- `EffectPlugin.sensor_mods()` — метод и SensorMod struct уже в интерфейсе; FogEffect/EMIEffect реализуют его без изменения интерфейса
- `cmd::SpawnZone/DespawnZone/ToggleZone` — уже в KernelCommand variant; нужны обработчики в phase0_kernel_commands()
- `Zone.detection_mode` — поле `std::string detection_mode{"center"}` уже есть в Zone struct
- `EffectContext.zone_strength` — поле уже в EffectContext, просто не заполнялось

### Established Patterns
- EffectPlugin наследование — образцы: IceModifier, ChargingEffect, ConveyorEffect в `s2_plugins/include/s2/effects/`
- KernelCommand обработчик в phase0 — образец из Plans 00-03/00-04
- Phase 6 (attachments) — уже в тиковом цикле, сюда добавляется обновление owned_zones поз

### Integration Points
- SimEngine::phase0_kernel_commands() — добавить обработчики SpawnZone/DespawnZone/ToggleZone
- SimEngine::phase6_attachments() — добавить обновление позиций owned_zones / per-link zones
- ZoneSystem (zone_system.cpp) — расширить: detection_mode enum, per_link iteration, strength в EffectContext
- SceneLoader — парсинг owned_zones из YAML, парсинг spawn_trigger секции
- app.js zones rendering section (строки ~1562–1619) — добавить VisualHint рендеринг

</code_context>

<specifics>
## Specific Ideas

- Инспектор зон — в Edit Scene панели, раздел Zones. Dropdown типов эффектов
  hardcoded в Phase 1, заменяется на /api/effects/registry в Phase 5.
- `state_change` spawn trigger реализуется через подписку на EventBus.ActorStateChanged;
  в Phase 1 тестируется только как механизм, реальные сценарии появятся в Phase 2.
- VisualHint.type: "glow" — для fog/emi/charging зон; "arrows" — для conveyor/wind;
  "particles" — для телепорта/пыли; "grid" — для зон с детекцией по сетке.

</specifics>

<deferred>
## Deferred Ideas

- Параметры эффектов в UI (config_schema per effect type) — Phase 5 с /api/effects/registry
- Actor FSM state_change триггер в реальных сценариях — Phase 2 (DoorBehavior, ConveyorActor)
- VisualHint Level 2 кастомные JS-модули (отдельные .js файлы per hint type) — Phase 8 (Visualization Overhaul)
- Редактирование spawn_trigger в UI инспекторе — Phase 5 (registry) или отдельный backlog

</deferred>

---

*Phase: 01-zone-visual-control-layer*
*Context gathered: 2026-04-26*
