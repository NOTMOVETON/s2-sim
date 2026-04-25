# Phase 1: Zone Visual & Control Layer — Research

**Researched:** 2026-04-26
**Domain:** C++17 · ZoneSystem · EffectPlugin · Three.js UI · SimEngine KernelCommands
**Confidence:** HIGH

---

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions

**UI Zone Inspector (ZONE-01)**
- D-01: Инспектор зон встраивается в существующую панель Edit Scene — отдельный раздел "Zones" рядом с Agents/Props/Primitives. Без нового модального окна.
- D-02: Форма создания/редактирования зоны в Phase 1 — минимальный CRUD: форма (sphere/box/cylinder), позиция, размер, цвет, список эффектов. Параметры конкретного эффекта (config_schema) — Phase 5.
- D-03: Выбор типа эффекта в форме — hardcoded dropdown: ice, boost, lock, charging, conveyor, wind, teleport, fog, emi. В Phase 5 заменяется на динамический список из /api/effects/registry.
- D-04: Удаление зоны — кнопка Delete в панели редактирования → DespawnZone KernelCommand. Без диалога подтверждения.

**VisualHint Pipeline (ZONE-02)**
- D-05: "Анимация активации агента внутри" = только color/opacity зоны в ZoneSnapshot. ZoneSnapshot.color/opacity уже есть. Каждый тип зоны имеет свой цвет по умолчанию.
- D-06: VisualHint типы в app.js — все 4: `glow`, `arrows`, `particles`, `grid`. Нужно реализовать рендеринг всех 4 типов в Three.js.

**Сенсорные эффекты (ZONE-03)**
- D-07: FogEffect и EMIEffect — новые EffectPlugin реализации. Используют существующий механизм sensor_mods() из EffectPlugin интерфейса.
- D-08: FogEffect: required_capabilities: [optical_sensor], ухудшает max_range через SensorMod. EMIEffect: required_capabilities: [gnss_sensor, imu_sensor], добавляет noise_std через SensorMod.

**Zone Lifecycle (ZONE-04)**
- D-09: Поле `strength` (0.0–1.0) добавляется в `struct Zone` и в `ZoneSnapshot`. EffectContext уже имеет zone_strength — поле просто не было заполнено.
- D-10: Lifecycle update (рост, затухание, auto-remove при strength < threshold) выполняется в Phase 0 тика вместе с KernelCommands — до применения эффектов.

**Zone KernelCommands (ZONE-05)**
- D-11: SpawnZone/DespawnZone/ToggleZone уже в KernelCommand variant — нужны только обработчики в SimEngine::phase0_kernel_commands(). ToggleZone при enabled=false отправляет on_exit всем Entity внутри; при enabled=true — on_enter.

**Detection Mode (ZONE-06)**
- D-12: Три режима: CENTER / BOUNDING / PER_LINK. Enum `DetectionMode` в zone.hpp. PER_LINK: итерация kinematic_tree агента, EffectContext.contact_link заполняется именем линка при контакте.

**Self-Destruct Policy (ZONE-07)**
- D-13: Поле `self_destruct_policy` в Zone struct: `on_any_contact` удаляет зону при любом контакте; `on_effect_applied` — только если эффект применился. DespawnZone вызывается из ZoneSystem по итогам тика.

**Spawn Triggers (ZONE-08)**
- D-14: ZoneSpawnSystem реализует все 4 типа триггеров: command, event (EventBus фильтр), timer (N сек от sim_time), state_change (ActorStateChanged EventBus).
- D-15: `event` = любое EventBus событие; `state_change` = конкретно ActorStateChanged с фильтром entity_id + state_value.
- D-16: ZoneSpawnSystem — отдельная система в SimEngine, подписывается на EventBus при инициализации.

**Entity.owned_zones (ZONE-09)**
- D-17: Поле `owned_zones` в YAML сцены и через SpawnZone{attached_to: entity_id} в рантайме. SceneLoader спавнит owned_zones автоматически при загрузке сцены.
- D-18: Per-link attachment в Phase 1: `attached_to_link: <link_name>` в конфиге зоны. ZoneSystem в Phase 6 тика (attachments) обновляет позу зоны из kinematic_frames AgentSnapshot по имени линка.

**Zone Movement via Invisible Prop (ZONE-10)**
- D-19: Паттерн "невидимый проп-носитель": SpawnProp(invisible, position) + SpawnZone(attached_to: prop_id). Никакого специального кода зоны — переиспользует SpawnZone{attached_to} из ZONE-09.

### Claude's Discretion

- Структура ZoneSpawnSystem внутри SimEngine (отдельный класс или метод фазы)
- Формат YAML конфигурации spawn_trigger (точный синтаксис полей)
- Анимационные параметры 4 типов VisualHint в Three.js (цвета, скорости)
- Порядок итерации kinematic_tree при per_link detection

### Deferred Ideas (OUT OF SCOPE)

- Параметры эффектов в UI (config_schema per effect type) — Phase 5 с /api/effects/registry
- Actor FSM state_change триггер в реальных сценариях — Phase 2
- VisualHint Level 2 кастомные JS-модули — Phase 8 (Visualization Overhaul)
- Редактирование spawn_trigger в UI инспекторе — Phase 5 или отдельный backlog
</user_constraints>

---

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|------------------|
| ZONE-01 | Редактор зон в UI с инспектором (форма, параметры, эффекты) | Существующие вкладки editor-tabs в index.html; паттерн switchEditorTab(); SpawnZone/DespawnZone KernelCommands через /command REST endpoint |
| ZONE-02 | VisualHint pipeline для зон (цвет, прозрачность, анимация въезда/выезда) | EffectPlugin.visual_hint() уже в интерфейсе и возвращает VisualHint{type, params}; app.js рендерит зоны по shape_type, но не читает visual_hint |
| ZONE-03 | Сенсорные эффекты: туман (ухудшение видимости), ЭМ помехи (деградация GNSS/IMU) | EffectPlugin.sensor_mods() и SensorMod struct уже в интерфейсе; образцы: IceModifier, ChargingEffect в s2_plugins |
| ZONE-04 | Lifecycle зон: zone.strength (0–1), рост/затухание, auto-remove при strength=0 | struct Zone не имеет поля strength — добавить; EffectContext.zone_strength уже есть; ZoneSnapshot не имеет strength — добавить |
| ZONE-05 | KernelCommands для зон: SpawnZone/RemoveZone/ToggleZone | Структуры cmd::SpawnZone/DespawnZone/ToggleZone уже в kernel_command.hpp; обработчики в apply_kernel_command() — заглушки |
| ZONE-06 | Zone detection_mode — CENTER/BOUNDING/PER_LINK | ZoneSystem::detection_point() — stub (always center); zone.detection_mode — строка "center" |
| ZONE-07 | Zone self_destruct_policy — on_any_contact/on_effect_applied | Поля отсутствуют в struct Zone; ZoneSystem не имеет механизма авто-удаления |
| ZONE-08 | Zone spawn triggers — command/event/timer/state_change | ZoneSpawnSystem не существует; EventBus и SimBus работают (ZoneEntered/ZoneExited уже публикуются) |
| ZONE-09 | Entity.owned_zones — список зон привязанных к Entity + attached_to_link | SceneLoader не парсит owned_zones в агентах; Zone.attached_to_agent существует, но attached_to_link отсутствует; Phase 6 tick пустая заглушка |
| ZONE-10 | Zone movement через invisible prop-носитель | SpawnProp SpawnZone{attached_to} уже достаточно; нужны обработчики SpawnZone (ZONE-05) и SpawnProp в SimEngine |
</phase_requirements>

---

## Summary

Phase 1 строится поверх зрелой архитектуры Phase 0. Фундамент готов: KernelCommand variant содержит SpawnZone/DespawnZone/ToggleZone, EffectPlugin интерфейс имеет sensor_mods() и visual_hint(), ZoneSystem публикует ZoneEntered/ZoneExited на EventBus, app.js рендерит зоны по форме. Однако большинство механизмов Phase 1 — это **нереализованные заглушки**: обработчики в apply_kernel_command() возвращают (void), detection_mode всегда работает как CENTER, strength отсутствует в Zone struct, ZoneSpawnSystem не существует, owned_zones не парсятся.

Работа фазы разбивается на два потока: **backend C++** (расширение Zone struct, ZoneSystem, SimEngine обработчики, новые EffectPlugin, SceneLoader парсинг) и **frontend JS** (новая вкладка Зоны в Edit Scene, VisualHint рендеринг в app.js). Оба потока независимы и могут идти параллельно.

Главный принцип: ничего не ломать в существующих тестах. Изменения в Zone struct, ZoneSnapshot, EffectContext — backward-compatible (добавление полей с дефолтами). Изменения в ZoneSystem — расширяют существующие методы без смены сигнатур.

**Primary recommendation:** Сначала реализовать backend (задачи 1.4→1.5→1.6→1.7→1.8→1.9, добавляя поля и обработчики), параллельно реализовать frontend (1.1→1.2), и финишировать сенсорными эффектами (1.3) и zone movement (1.10).

---

## Architectural Responsibility Map

| Capability | Primary Tier | Secondary Tier | Rationale |
|------------|-------------|----------------|-----------|
| Zone struct (strength, detection_mode enum, self_destruct) | s2_core (zone.hpp) | — | Данные зоны — часть ядра |
| ZoneSnapshot (strength поле) | s2_core (world_snapshot.hpp) | — | Снимок собирается в SimEngine |
| KernelCommand обработчики SpawnZone/DespawnZone/ToggleZone | s2_core (sim_engine.hpp) | — | apply_kernel_command — в SimEngine |
| ZoneSystem детекция (BOUNDING, PER_LINK) | s2_core (zone_system.cpp) | — | Вся детекция в ZoneSystem::tick |
| Zone lifecycle (strength рост/затухание) | s2_core (zone_system.cpp) | — | Lifecycle update в Phase 0 по D-10 |
| Self-destruct policy | s2_core (zone_system.cpp) | — | Удаление через ZoneSystem после apply |
| ZoneSpawnSystem (spawn triggers) | s2_core (sim_engine.hpp) | — | Подсистема SimEngine, подписана на bus_ |
| FogEffect / EMIEffect плагины | s2_plugins (effects/) | — | Все EffectPlugin в s2_plugins |
| SceneLoader (owned_zones парсинг) | s2_core (scene_loader.hpp) | — | Загрузка сцены — ядро |
| UI Zone Inspector (вкладка Zones) | s2_visualizer (index.html + app.js) | — | Все UI — в visualizer |
| VisualHint рендеринг (glow/arrows/particles/grid) | s2_visualizer (app.js) | — | Three.js рендеринг — в visualizer |

---

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| C++17 | — | Язык ядра | Проект использует C++17 (if constexpr, std::variant, std::optional) |
| GTest | latest in Docker | Юнит-тесты | Все 34 существующих теста используют GTest |
| YAML-cpp | via CMake | Парсинг сцен | SceneLoader использует YAML::Node |
| nlohmann/json | via CMake | JSON сериализация | KernelCommand params, VisualHint params |
| Three.js | r160 (CDN в index.html) | 3D визуализация | Весь рендеринг в app.js |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| Eigen3 | via CMake | Vec3, матрицы | Все векторные вычисления (zone.shape.center, bounding) |

**Installation:** Нет новых зависимостей — все уже в Docker образе.

---

## Architecture Patterns

### System Architecture Diagram

```
                    ┌─────────────────────────────────────────────────┐
                    │         Phase 0: KernelCommands                 │
                    │  SpawnZone → ZoneSystem::add_zone()             │
                    │  DespawnZone → zones_.erase()                   │
                    │  ToggleZone → zone.enabled → on_exit/on_enter   │
                    │  Lifecycle update → strength growth/decay        │
                    └────────────────────┬────────────────────────────┘
                                         │
                    ┌────────────────────▼────────────────────────────┐
                    │         Phase 3: ZoneSystem::tick()              │
                    │  1. Обновить позиции attached зон (owned_zones)  │
                    │  2. Detection: CENTER / BOUNDING / PER_LINK      │
                    │     → ZoneEntered / ZoneExited на EventBus       │
                    │  3. Self-destruct check (after enter events)     │
                    │  4. Apply effects (with zone_strength в ctx)     │
                    │     → MODIFIER: add_scale / add_velocity         │
                    │     → CONTINUOUS: battery charge                 │
                    │     → SENSOR: sensor_mods() → SensorMod[]       │
                    └────────────────────┬────────────────────────────┘
                                         │ EventBus
                    ┌────────────────────▼────────────────────────────┐
                    │         ZoneSpawnSystem (event subscriber)       │
                    │  event trigger: EventBus filter → SpawnZone      │
                    │  timer trigger: sim_time >= threshold → SpawnZone│
                    │  state_change: ActorStateChanged → SpawnZone     │
                    └─────────────────────────────────────────────────┘
                                         │
                    ┌────────────────────▼────────────────────────────┐
                    │         Phase 7: WorldSnapshot                   │
                    │  ZoneSnapshot: strength, agents_inside,          │
                    │               visual_hints[]                     │
                    │  → JSON → SSE → Browser                         │
                    └────────────────────┬────────────────────────────┘
                                         │
                    ┌────────────────────▼────────────────────────────┐
                    │         Browser: app.js                          │
                    │  Zones: рендерить sphere/box/cylinder            │
                    │  VisualHint: glow → PointLight; arrows → Arrow3D│
                    │             particles → Points; grid → GridHelper│
                    │  Zone Inspector: вкладка Zones в Edit Scene      │
                    │    → SpawnZone/DespawnZone REST → SimEngine      │
                    └─────────────────────────────────────────────────┘
```

### Recommended Project Structure

Новые файлы Phase 1:

```
workspace/
  s2_core/
    include/s2/
      zone.hpp                    # добавить: strength, DetectionMode enum, self_destruct_policy
      zone_system.hpp             # добавить: remove_zone(), lifecycle методы
      world_snapshot.hpp          # добавить: ZoneSnapshot.strength, visual_hints
      effect_context.hpp          # добавить: contact_link поле
      zone_spawn_system.hpp       # NEW: ZoneSpawnSystem класс
    src/
      zone_system.cpp             # расширить: BOUNDING, PER_LINK, self_destruct
      zone_spawn_system.cpp       # NEW: реализация триггеров
    tests/
      test_zone_lifecycle.cpp     # NEW: strength, decay, auto-remove
      test_zone_detection_mode.cpp # NEW: BOUNDING, PER_LINK
      test_zone_self_destruct.cpp  # NEW: on_any_contact, on_effect_applied
      test_zone_spawn_triggers.cpp # NEW: event, timer, state_change
      test_zone_owned.cpp          # NEW: owned_zones, attached_to_link
  s2_plugins/
    include/s2/effects/
      fog_effect.hpp              # NEW: FogEffect : EffectPlugin
      emi_effect.hpp              # NEW: EMIEffect : EffectPlugin
    src/
      effect_factory.cpp          # добавить: регистрация fog, emi
  s2_visualizer/
    web/
      index.html                  # добавить: вкладка Zones в editor-tabs
      js/
        app.js                    # добавить: VisualHint рендеринг, zone inspector logic
```

### Pattern 1: Расширение Zone struct (добавление полей с дефолтами)

**What:** Добавить поля в struct Zone без нарушения существующего кода.
**When to use:** Для strength, DetectionMode, self_destruct_policy.

```cpp
// workspace/s2_core/include/s2/zone.hpp
// Source: существующий паттерн проекта (backward-compatible additions)

enum class DetectionMode {
    CENTER,   ///< Центр Entity (текущее поведение)
    BOUNDING, ///< Bounding shape Entity пересекает зону
    PER_LINK  ///< Каждый линк kinematic_tree проверяется отдельно
};

struct SelfDestructPolicy {
    enum class Type { NONE, ON_ANY_CONTACT, ON_EFFECT_APPLIED };
    Type type{Type::NONE};
};

struct ZoneLifecycle {
    double initial_strength{1.0};   ///< Начальная сила при спавне
    double growth_rate{0.0};        ///< М/с (если < 0 — уменьшение)
    double max_strength{1.0};       ///< Потолок роста
    double decay_delay{0.0};        ///< Секунд до начала затухания
    double decay_rate{0.0};         ///< Единиц/с (если > 0 — затухание)
    double remove_threshold{0.05};  ///< Удалить если strength < этого
};

struct Zone {
    // ... существующие поля ...
    double strength{1.0};                           ///< Текущая сила (0..1)
    DetectionMode detection_mode_enum{DetectionMode::CENTER};
    SelfDestructPolicy self_destruct;
    ZoneLifecycle lifecycle;
    std::optional<std::string> attached_to_link;   ///< Линк для per-link attachment
    std::string attached_to_entity_id;             ///< Generic EntityId для owned_zones
};
```

### Pattern 2: Обработчик SpawnZone в apply_kernel_command()

**What:** Реализовать заглушку обработчика.
**When to use:** Phase 0 kernel commands.

```cpp
// workspace/s2_core/include/s2/sim_engine.hpp (в apply_kernel_command)
// Source: существующий паттерн Phase 0 в sim_engine.hpp

else if constexpr (std::is_same_v<T, cmd::SpawnZone>) {
    Zone z;
    z.id = cmd.id_hint.empty()
        ? "zone_" + std::to_string(next_zone_id_++)
        : cmd.id_hint;
    z.shape   = cmd.shape;
    z.visible = cmd.visible;
    z.color   = cmd.color;
    z.opacity = cmd.opacity;
    z.label   = cmd.label;
    // effects строки → через effect_factory_ (как в ZoneSystem::add_zone)
    for (const auto& eff_type : cmd.effects) {
        Zone::EffectDesc desc;
        desc.type = eff_type;
        z.effects.push_back(std::move(desc));
    }
    if (cmd.attached_to.has_value()) {
        z.attached_to_entity_id = cmd.attached_to.value();
    }
    zone_system_.add_zone(std::move(z));
}
else if constexpr (std::is_same_v<T, cmd::DespawnZone>) {
    zone_system_.remove_zone(cmd.id);
}
else if constexpr (std::is_same_v<T, cmd::ToggleZone>) {
    // toggle_zone уже существует в ZoneSystem
    zone_system_.toggle_zone_with_events(cmd.id, cmd.enabled, world_.agents(), bus_);
}
```

### Pattern 3: FogEffect / EMIEffect

**What:** Новые EffectPlugin использующие sensor_mods().
**When to use:** ZONE-03.

```cpp
// workspace/s2_plugins/include/s2/effects/fog_effect.hpp
// Source: образец — IceModifier в существующем коде

class FogEffect : public EffectPlugin {
public:
    void on_init(const YAML::Node& params) override {
        range_multiplier_ = params["range_multiplier"].as<double>(0.3);
    }

    EffectType effect_type() const override { return EffectType::SENSOR; }

    std::vector<std::string> required_capabilities() const override {
        return {"optical_sensor"};
    }

    std::vector<SensorMod> sensor_mods(const EffectContext& ctx) const override {
        double mult = range_multiplier_ + (1.0 - range_multiplier_) * (1.0 - ctx.zone_strength);
        return {SensorMod{.param = "max_range", .multiplier = mult}};
    }

    std::optional<VisualHint> visual_hint() const override {
        return VisualHint{"glow", {{"color", "#AADDFF"}, {"pulse_rate", 0.5}, {"intensity", 0.4}}};
    }
private:
    double range_multiplier_{0.3};
};
```

### Pattern 4: VisualHint рендеринг в app.js

**What:** Читать visual_hints из ZoneSnapshot и рисовать в Three.js.
**When to use:** ZONE-02 — 4 типа hints.

```javascript
// workspace/s2_visualizer/web/js/app.js
// Source: существующий паттерн updateOrCreateMesh() в app.js

function renderVisualHints(zoneKey, zone, hints) {
    if (!hints) return;
    hints.forEach((hint, i) => {
        const hintKey = `${zoneKey}_hint_${i}`;
        if (hint.type === 'glow') {
            // PointLight под зоной
            renderGlowHint(hintKey, zone, hint.params);
        } else if (hint.type === 'arrows') {
            // ArrowHelper по направлению
            renderArrowsHint(hintKey, zone, hint.params);
        } else if (hint.type === 'particles') {
            // Points с BufferGeometry
            renderParticlesHint(hintKey, zone, hint.params);
        } else if (hint.type === 'grid') {
            // GridHelper внутри зоны
            renderGridHint(hintKey, zone, hint.params);
        }
    });
}
```

### Pattern 5: Zone Inspector — новая вкладка в Edit Scene

**What:** Добавить вкладку "Зоны" рядом с "Геометрия" и "Агенты".
**When to use:** ZONE-01.

```html
<!-- workspace/s2_visualizer/web/index.html -->
<!-- Source: существующий паттерн editor-tabs в index.html -->

<button class="editor-tab-btn" data-tab="zones" onclick="switchEditorTab('zones')">Зоны</button>

<!-- ... -->

<div id="editor-tab-zones" style="display:none">
    <button class="save" onclick="startAddZone()" style="width:100%;margin-bottom:6px;">+ Добавить зону</button>
    <div id="zone-list"></div>

    <div id="zone-form-view" style="display:none">
        <h5 id="zf-title">Новая зона</h5>
        <label>ID: <input type="text" id="zf-id"></label>
        <label>Форма:
            <select id="zf-shape">
                <option value="sphere">Sphere</option>
                <option value="box">Box</option>
                <option value="cylinder">Cylinder</option>
            </select>
        </label>
        <!-- Параметры формы, позиция, цвет, opacity -->
        <label>Цвет: <input type="color" id="zf-color" value="#4488FF"></label>
        <label>Прозрачность: <input type="range" id="zf-opacity" min="0" max="1" step="0.05" value="0.3"></label>
        <label>Эффекты:
            <select id="zf-effects" multiple>
                <option value="ice">ice</option>
                <option value="boost">boost</option>
                <option value="lock">lock</option>
                <option value="charging">charging</option>
                <option value="conveyor">conveyor</option>
                <option value="wind">wind</option>
                <option value="teleport">teleport</option>
                <option value="fog">fog</option>
                <option value="emi">emi</option>
            </select>
        </label>
        <button class="save" onclick="confirmZoneForm()">Применить</button>
        <button onclick="cancelZoneForm()">Отмена</button>
    </div>
</div>
```

### Anti-Patterns to Avoid

- **Изменение сигнатур существующих методов ZoneSystem::tick():** Все 34 теста пройдут — нельзя менять параметры. Расширять только внутренней логикой.
- **Добавление detection_mode как runtime-параметр плагина:** Detection mode — свойство зоны, не эффекта. В Zone struct, не в EffectDesc.
- **Хранение kinematic_tree в ZoneSystem:** ZoneSystem получает агентов по ссылке каждый тик — читать kinematic_tree из Agent напрямую.
- **Рендеринг VisualHint в фазе обновления снапшота:** Hints собирать в ZoneSnapshot — рендерить в browser. Не хранить Three.js объекты в C++.
- **ZoneSpawnSystem как отдельный поток:** Только подписчик EventBus в основном sim-thread, выдаёт SpawnZone в command_queue_.

---

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Создание зоны в runtime | Прямой push_back в zone_system_.zones_ | cmd::SpawnZone → apply_kernel_command | Thread-safety: mutex защита в command_queue_ |
| Удаление зоны | Прямой erase в zones_ | ZoneSystem::remove_zone() + DespawnZone cmd | Нужно очистить inside_agents и отправить on_exit |
| Lifecycle таймер | Отдельный std::thread | Обновление в phase0 тика (sim_time) | Детерминизм — всё в sim_thread |
| EventBus subscribe | Кастомный callback механизм | SimBus::subscribe\<T\>() | Уже работает, typed events |
| Three.js анимации зон | CSS анимации / WebGL shader | Three.js PointLight/ArrowHelper/Points | Three.js уже подключён, паттерны в app.js |

**Key insight:** Zone lifecycle и spawn triggers должны быть в sim_thread — детерминизм через фиксированный dt и sim_time. Никаких std::thread, никаких async.

---

## Common Pitfalls

### Pitfall 1: EffectContext.zone_strength не заполняется

**What goes wrong:** FogEffect и EMIEffect читают ctx.zone_strength для масштабирования эффекта. Если zone_strength не передаётся в ctx, эффект всегда работает на полную силу.

**Why it happens:** В существующем apply_active_effects() EffectContext создаётся без zone_strength. Поле zone_strength уже есть в EffectContext struct, но строка `ctx.zone_strength = zone.strength;` отсутствует.

**How to avoid:** В ZoneSystem::apply_active_effects() — добавить `ctx.zone_strength = zone.strength;` при создании EffectContext. Аналогично в on_agent_enter().

**Warning signs:** sensor_mods() всегда возвращает одно и то же значение независимо от strength зоны.

### Pitfall 2: toggle_zone() не отправляет on_exit/on_enter события

**What goes wrong:** Существующий ZoneSystem::toggle_zone() просто меняет zone.enabled. Но D-11 требует: при enabled=false все агенты внутри получают on_exit; при enabled=true — on_enter заново.

**Why it happens:** toggle_zone() не принимает agents и bus — не может публиковать события.

**How to avoid:** Добавить новый метод `toggle_zone_with_events(id, enabled, agents, bus)` или передавать bus в ZoneSystem (хранить ссылку). По D-11 ToggleZone обрабатывается в phase0 — можно сделать inline в apply_kernel_command().

**Warning signs:** Агенты внутри зоны не получают on_exit при DespawnZone или ToggleZone.

### Pitfall 3: ZoneSnapshot не содержит visual_hints

**What goes wrong:** EffectPlugin.visual_hint() возвращает данные — но build_snapshot() в sim_engine.hpp не читает visual_hints из зоны. Браузер ничего не рендерит.

**Why it happens:** ZoneSnapshot struct не имеет поля visual_hints. build_snapshot() не итерирует effects и не собирает hints.

**How to avoid:** Добавить `std::vector<ZoneSnapshot::Hint> visual_hints;` в ZoneSnapshot. В build_snapshot() итерировать zone.effects, вызывать effect.plugin->visual_hint(), добавлять в zs.visual_hints. Сериализовать в snapshot_to_json().

**Warning signs:** app.js получает zones без поля visual_hints.

### Pitfall 4: Удаление зоны во время итерации zones_

**What goes wrong:** Self-destruct удаляет зону в момент когда ZoneSystem::tick() итерирует zones_. Итератор становится невалидным.

**Why it happens:** self_destruct срабатывает внутри apply_active_effects() / on_agent_enter(), а zones_ — std::vector. Удаление меняет индексы.

**How to avoid:** Накапливать zones_to_destroy (set ZoneId) в процессе тика. После завершения всех итераций по zones_ — удалять. Или использовать индексную итерацию назад.

**Warning signs:** crash / undefined behavior при срабатывании self_destruct в тике с несколькими зонами.

### Pitfall 5: PER_LINK detection — AgentId не имеет kinematic_tree в ZoneSystem

**What goes wrong:** ZoneSystem::tick() принимает `std::vector<Agent>&` — Agent имеет `std::shared_ptr<KinematicTree> kinematic_tree`. Но если URDF не загружен — kinematic_tree == nullptr.

**Why it happens:** PER_LINK режим итерирует kinematic_tree->links(), но не проверяет nullptr.

**How to avoid:** В PER_LINK ветке: `if (!agent.kinematic_tree) { fallback на CENTER; continue; }`.

**Warning signs:** crash при PER_LINK mode для агента без URDF.

### Pitfall 6: spawn_trigger: state_change — ActorStateChanged не публикуется в Phase 1

**What goes wrong:** ZoneSpawnSystem подписывается на event::ActorStateChanged, но акторов (Phase 2) ещё нет. Триггер state_change не сработает ни разу в Phase 1.

**Why it happens:** D-14 явно указывает: state_change реализуется в Phase 1 но реально используется в Phase 2. Механизм нужен рабочим — просто никто не публикует ActorStateChanged сейчас.

**How to avoid:** Реализовать подписку корректно. Написать тест с ручным bus_.publish(event::ActorStateChanged{...}) для верификации механизма. Не ждать Phase 2 для тестирования.

**Warning signs:** Тест ZoneSpawnSystem для state_change не может проверить срабатывание без ручной публикации события.

---

## Code Examples

### Существующий паттерн EffectPlugin с sensor_mods() (образец для FogEffect/EMIEffect)

```cpp
// Из workspace/s2_core/include/s2/interfaces/effect_plugin.hpp
struct SensorMod {
    std::string param;       ///< "max_range", "noise_std"
    double multiplier{1.0};
    double addend{0.0};
};
virtual std::vector<SensorMod> sensor_mods(const EffectContext& ctx) const { return {}; }
```

### Существующий паттерн EffectPlugin::visual_hint()

```cpp
// Из workspace/s2_plugins/include/s2/effects/ice_modifier.hpp
std::optional<VisualHint> visual_hint() const override {
    return VisualHint{
        "glow",
        {{"color", "#88AAFF"}, {"pulse_rate", 1.5}, {"intensity", 0.6}}
    };
}
```

### Существующий паттерн подписки EventBus в тестах

```cpp
// Из workspace/s2_core/tests/test_zone_system.cpp
bus.subscribe<event::ZoneEntered>([&](const event::ZoneEntered& e) {
    received_zone   = e.zone_id;
    received_entity = e.entity_id;
});
```

### Существующий паттерн zone рендеринга в app.js (строки 1564–1619)

```javascript
// app.js — zones секция рендеринга
if (data.zones) {
    data.zones.forEach(z => {
        const key = `zone_${z.id}`;
        if (z.shape_type === 'sphere') {
            updateOrCreateMesh(key, 'sphere', pose,
                { type: 'sphere', radius: r, color },
                { transparent: true, opacity, depthWrite: false, side: 'double' }
            );
        }
        // + aabb, cylinder
    });
}
```

### Существующий паттерн switchEditorTab() в app.js

```javascript
// app.js ~1152
function switchEditorTab(tab) {
    document.querySelectorAll('.editor-tab-btn').forEach(btn => {
        btn.classList.toggle('active', btn.dataset.tab === tab);
    });
    ['geometry', 'agents'].forEach(t => {
        document.getElementById(`editor-tab-${t}`).style.display = t === tab ? 'block' : 'none';
    });
}
// После добавления Zones: расширить массив на 'zones'
```

### Формат YAML для owned_zones (рекомендуемый синтаксис)

```yaml
# Агент с owned_zones
- type: agent
  name: delivery_robot
  plugins:
    - type: diff_drive
  owned_zones:
    - id: freezer_compartment
      shape: { type: sphere, radius: 0.3 }
      attached_to_link: cargo_link
      color: "#88AAFF"
      effects:
        - type: fog
    - id: detection_zone
      shape: { type: cylinder, radius: 1.5, half_height: 0.5 }
      effects:
        - type: ice

# spawn_trigger формат в zone_templates (для ZoneSpawnSystem)
zone_templates:
  dust_cloud:
    shape: { type: sphere }
    lifecycle:
      initial_strength: 0.1
      growth:
        rate: 1.5
        max: 1.0
      decay:
        delay: 2.0
        rate: 0.2
        remove_at: 0.05
    spawn_trigger:
      type: event                  # event | timer | state_change | command
      event_type: ZoneExited       # для type: event
      source_entity: ""            # фильтр по entity_id (пусто = любой)
    # ИЛИ:
    # spawn_trigger:
    #   type: timer
    #   delay: 5.0
    # ИЛИ:
    # spawn_trigger:
    #   type: state_change
    #   entity_id: door_1
    #   state_value: OPEN
    effects:
      - type: fog
```

---

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| detection_mode как строка "center" | DetectionMode enum {CENTER, BOUNDING, PER_LINK} | Phase 1 | Compile-time safety, switch без строковых сравнений |
| zone.strength — отсутствует | zone.strength (0.0–1.0) + в EffectContext | Phase 1 | Плавное усиление/затухание эффектов |
| ZoneSystem::toggle_zone() без событий | toggle_zone_with_events() с on_exit/on_enter | Phase 1 | Правильный lifecycle агентов при включении/выключении |
| Нет ZoneSpawnSystem | ZoneSpawnSystem с 4 типами триггеров | Phase 1 | Декларативное создание зон по событиям |

**Deprecated/outdated:**
- detection_mode как std::string: заменяется на enum DetectionMode в Phase 1, строка `detection_mode{"center"}` в Zone struct → `DetectionMode detection_mode_enum{DetectionMode::CENTER}`

---

## Assumptions Log

| # | Claim | Section | Risk if Wrong |
|---|-------|---------|---------------|
| A1 | ZoneSnapshot.visual_hints передаётся браузеру через snapshot_to_json() | VisualHint рендеринг | Нужно добавить сериализацию — легко, но не забыть |
| A2 | KinematicTree имеет метод links() возвращающий итерируемую коллекцию | PER_LINK detection | Проверено через test_kinematic_tree.cpp — HIGH |
| A3 | EventBus::subscribe\<T\>() вызывает callback в том же тике что publish() | ZoneSpawnSystem | Проверено в test_event_bus.cpp — HIGH |
| A4 | SpawnZone.effects — список строк типов, не полные EffectDesc | ZONE-05 обработчик | Из kernel_command.hpp: `std::vector<std::string> effects` — VERIFIED |

**Если таблица предположений пуста** — все утверждения подтверждены кодом.

---

## Open Questions

1. **Куда добавить ZoneSpawnSystem — в SimEngine как поле или отдельный файл?**
   - What we know: D-16 говорит "отдельная система в SimEngine". SimEngine уже имеет ZoneSystem как поле.
   - What's unclear: Нужен ли отдельный класс или достаточно метода phase0_spawn_triggers()?
   - Recommendation: Отдельный класс ZoneSpawnSystem в zone_spawn_system.hpp — по аналогии с ZoneSystem. SimEngine хранит `ZoneSpawnSystem zone_spawn_system_`. Класс имеет init(SimBus&) для подписок и tick(sim_time, command_queue&) для timer триггеров.

2. **Как передавать EffectContext.contact_link в apply_active_effects()?**
   - What we know: contact_link заполняется только при PER_LINK mode.
   - What's unclear: apply_active_effects() вызывается после детекции, передать имя линка нужно в EffectContext.
   - Recommendation: Добавить параметр `std::string contact_link = ""` в apply_active_effects(). При CENTER/BOUNDING — пустая строка. При PER_LINK — имя линка из kinematic_tree итерации.

3. **Как хранить zone_templates для ZoneSpawnSystem?**
   - What we know: SpawnZone command содержит полный конфиг зоны. Шаблоны нужны для event/timer/state_change триггеров.
   - What's unclear: Шаблоны в YAML сцены или в отдельной секции?
   - Recommendation: Секция `zone_templates:` в YAML на уровне `s2:`. SceneLoader парсит в `std::vector<ZoneTemplate>`. ZoneSpawnSystem хранит templates и triggered conditions.

---

## Environment Availability

| Dependency | Required By | Available | Version | Fallback |
|------------|------------|-----------|---------|----------|
| Docker | Все сборки | ✓ | docker compose | — |
| GTest | Тесты | ✓ | via CMake in Docker | — |
| YAML-cpp | SceneLoader | ✓ | via CMake | — |
| nlohmann/json | VisualHint params | ✓ | via CMake | — |
| Three.js | VisualHint рендеринг | ✓ | r160 CDN в index.html | — |

**Нет недостающих зависимостей** — все уже в Docker образе.

---

## Validation Architecture

### Test Framework
| Property | Value |
|----------|-------|
| Framework | GTest (Google Test) |
| Config file | CMakeLists.txt в каждом пакете |
| Quick run command | `docker compose --project-directory docker up --build tests` |
| Full suite command | `docker compose --project-directory docker up --build tests` |

### Phase Requirements → Test Map

| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| ZONE-04 | strength рост/затухание, auto-remove | unit | `docker compose ... up --build tests` | ❌ Wave 0: test_zone_lifecycle.cpp |
| ZONE-04 | zone_strength заполняется в EffectContext | unit | `docker compose ... up --build tests` | ❌ Wave 0: test_zone_lifecycle.cpp |
| ZONE-05 | SpawnZone создаёт зону в ZoneSystem | unit | `docker compose ... up --build tests` | ❌ Wave 0: test_zone_commands.cpp |
| ZONE-05 | DespawnZone удаляет зону, on_exit для агентов внутри | unit | `docker compose ... up --build tests` | ❌ Wave 0: test_zone_commands.cpp |
| ZONE-05 | ToggleZone: on_exit при disabled, on_enter при enabled | unit | `docker compose ... up --build tests` | ❌ Wave 0: test_zone_commands.cpp |
| ZONE-06 | BOUNDING detection: bounding shape пересекается с зоной | unit | `docker compose ... up --build tests` | ❌ Wave 0: test_zone_detection_mode.cpp |
| ZONE-06 | PER_LINK detection: contact_link заполнен, fallback для нет URDF | unit | `docker compose ... up --build tests` | ❌ Wave 0: test_zone_detection_mode.cpp |
| ZONE-07 | on_any_contact: зона удаляется при контакте (любой immunity) | unit | `docker compose ... up --build tests` | ❌ Wave 0: test_zone_self_destruct.cpp |
| ZONE-07 | on_effect_applied: зона жива при immunity, удаляется при применении | unit | `docker compose ... up --build tests` | ❌ Wave 0: test_zone_self_destruct.cpp |
| ZONE-08 | timer trigger: зона спавнится через N секунд sim_time | unit | `docker compose ... up --build tests` | ❌ Wave 0: test_zone_spawn_triggers.cpp |
| ZONE-08 | event trigger: зона спавнится по EventBus событию | unit | `docker compose ... up --build tests` | ❌ Wave 0: test_zone_spawn_triggers.cpp |
| ZONE-08 | state_change trigger: зона спавнится по ActorStateChanged | unit | `docker compose ... up --build tests` | ❌ Wave 0: test_zone_spawn_triggers.cpp |
| ZONE-09 | owned_zones агента движутся вместе с агентом | unit | `docker compose ... up --build tests` | ❌ Wave 0: test_zone_owned.cpp |
| ZONE-03 | FogEffect: SensorMod уменьшает max_range при optical_sensor | unit | `docker compose ... up --build tests` | ❌ Wave 0: test_effect_fog_emi.cpp |
| ZONE-03 | EMIEffect: SensorMod добавляет noise_std при gnss_sensor/imu_sensor | unit | `docker compose ... up --build tests` | ❌ Wave 0: test_effect_fog_emi.cpp |

Существующие тесты (не должны регрессировать):
- test_zone_system.cpp — 11 тестов, все должны пройти
- test_kernel_command.cpp — ZoneCommandsCreation test проходит

### Sampling Rate
- **Per task commit:** `docker compose --project-directory docker up --build tests`
- **Per wave merge:** `docker compose --project-directory docker up --build tests`
- **Phase gate:** Full suite green before `/gsd-verify-phase`

### Wave 0 Gaps

- [ ] `workspace/s2_core/tests/test_zone_lifecycle.cpp` — ZONE-04
- [ ] `workspace/s2_core/tests/test_zone_commands.cpp` — ZONE-05
- [ ] `workspace/s2_core/tests/test_zone_detection_mode.cpp` — ZONE-06
- [ ] `workspace/s2_core/tests/test_zone_self_destruct.cpp` — ZONE-07
- [ ] `workspace/s2_core/tests/test_zone_spawn_triggers.cpp` — ZONE-08
- [ ] `workspace/s2_core/tests/test_zone_owned.cpp` — ZONE-09
- [ ] `workspace/s2_plugins/tests/test_effect_fog_emi.cpp` — ZONE-03

---

## Security Domain

`security_enforcement` не установлен в config.json (ключ отсутствует) — применяем как enabled.

### Applicable ASVS Categories

| ASVS Category | Applies | Standard Control |
|---------------|---------|-----------------|
| V2 Authentication | нет | Phase 1 — внутренняя симуляция, нет auth endpoints |
| V3 Session Management | нет | — |
| V4 Access Control | нет | — |
| V5 Input Validation | да (partial) | YAML парсинг: YAML::Node.as\<T\>(default) защищает от отсутствующих полей |
| V6 Cryptography | нет | — |

### Known Threat Patterns

| Pattern | STRIDE | Standard Mitigation |
|---------|--------|---------------------|
| YAML injection через zone id | Tampering | ZoneId — std::string, не исполняется; добавить sanitize при REST input |
| Бесконечный spawn_trigger (event loops) | Denial of Service | ZoneSpawnSystem: ограничить max_zones_per_trigger или cooldown |
| Некорректный attached_to EntityId | Tampering | В apply_kernel_command: проверить что entity с таким id существует |

---

## Sources

### Primary (HIGH confidence)
- `workspace/s2_core/include/s2/zone.hpp` — текущая Zone struct
- `workspace/s2_core/include/s2/zone_system.hpp` и `zone_system.cpp` — реализация ZoneSystem
- `workspace/s2_core/include/s2/sim_engine.hpp` — apply_kernel_command() заглушки, phase0–phase8
- `workspace/s2_core/include/s2/interfaces/effect_plugin.hpp` — SensorMod, VisualHint
- `workspace/s2_plugins/include/s2/effects/ice_modifier.hpp` — образец MODIFIER эффекта
- `workspace/s2_plugins/include/s2/effects/charging_effect.hpp` — образец CONTINUOUS эффекта
- `workspace/s2_core/include/s2/kernel_command.hpp` — SpawnZone/DespawnZone/ToggleZone structs
- `workspace/s2_core/include/s2/world_snapshot.hpp` — ZoneSnapshot struct
- `workspace/s2_core/include/s2/effect_context.hpp` — EffectContext с zone_strength
- `workspace/s2_core/include/s2/scene_loader.hpp` — существующий парсинг зон
- `workspace/s2_core/tests/test_zone_system.cpp` — паттерны тестов
- `workspace/s2_visualizer/web/js/app.js` (строки 1562–1619) — zone рендеринг
- `workspace/s2_visualizer/web/index.html` — editor-tabs структура
- `RESULT_DISCUSS.md` §9.1–9.10 — архитектурный дизайн-документ

### Secondary (MEDIUM confidence)
- `.planning/phases/01-zone-visual-control-layer/01-CONTEXT.md` — D-01 — D-19, locked decisions
- `.planning/REQUIREMENTS.md` §ZONE-01–ZONE-10 — требования
- `.planning/ROADMAP.md` §Phase 1 — задачи 1.1–1.10

---

## Metadata

**Confidence breakdown:**
- Standard Stack: HIGH — все зависимости проверены в codebase
- Architecture: HIGH — код прочитан, точки изменений идентифицированы
- Pitfalls: HIGH — основаны на конкретных gap'ах в существующем коде
- VisualHint rendering: MEDIUM — Three.js паттерны известны, конкретные параметры анимаций на усмотрение планировщика

**Research date:** 2026-04-26
**Valid until:** 2026-05-26 (стабильная архитектура, нет внешних зависимостей)
