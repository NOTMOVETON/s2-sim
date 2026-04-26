# Phase 1: Zone Visual & Control Layer — Pattern Map

**Mapped:** 2026-04-26
**Files analyzed:** 19 (новые + модифицируемые)
**Analogs found:** 17 / 19

---

## File Classification

| Новый / модифицируемый файл | Роль | Data Flow | Ближайший аналог | Качество совпадения |
|-----------------------------|------|-----------|------------------|---------------------|
| `workspace/s2_core/include/s2/zone.hpp` | model | CRUD | `workspace/s2_core/include/s2/zone.hpp` (сам себя расширяет) | exact |
| `workspace/s2_core/include/s2/effect_context.hpp` | model | CRUD | `workspace/s2_core/include/s2/effect_context.hpp` | exact |
| `workspace/s2_core/include/s2/world_snapshot.hpp` | model | CRUD | `workspace/s2_core/include/s2/world_snapshot.hpp` | exact |
| `workspace/s2_core/include/s2/zone_system.hpp` | service | event-driven | `workspace/s2_core/include/s2/zone_system.hpp` | exact |
| `workspace/s2_core/src/zone_system.cpp` | service | event-driven | `workspace/s2_core/src/zone_system.cpp` | exact |
| `workspace/s2_core/include/s2/zone_spawn_system.hpp` | service | event-driven | `workspace/s2_core/include/s2/zone_system.hpp` | role-match |
| `workspace/s2_core/src/zone_spawn_system.cpp` | service | event-driven | `workspace/s2_core/src/zone_system.cpp` | role-match |
| `workspace/s2_core/include/s2/sim_engine.hpp` | controller | request-response | `workspace/s2_core/include/s2/sim_engine.hpp` | exact |
| `workspace/s2_core/include/s2/scene_loader.hpp` | utility | CRUD | `workspace/s2_core/include/s2/scene_loader.hpp` | exact |
| `workspace/s2_plugins/include/s2/effects/fog_effect.hpp` | plugin | request-response | `workspace/s2_plugins/include/s2/effects/ice_modifier.hpp` | role-match |
| `workspace/s2_plugins/include/s2/effects/emi_effect.hpp` | plugin | request-response | `workspace/s2_plugins/include/s2/effects/charging_effect.hpp` | role-match |
| `workspace/s2_plugins/src/effects_registry.cpp` | config | CRUD | `workspace/s2_plugins/src/effects_registry.cpp` | exact |
| `workspace/s2_visualizer/web/index.html` | component | request-response | `workspace/s2_visualizer/web/index.html` | exact |
| `workspace/s2_visualizer/web/js/app.js` | component | event-driven | `workspace/s2_visualizer/web/js/app.js` | exact |
| `workspace/s2_core/tests/test_zone_lifecycle.cpp` | test | CRUD | `workspace/s2_core/tests/test_zone_system.cpp` | role-match |
| `workspace/s2_core/tests/test_zone_commands.cpp` | test | CRUD | `workspace/s2_core/tests/test_zone_system.cpp` | role-match |
| `workspace/s2_core/tests/test_zone_detection_mode.cpp` | test | CRUD | `workspace/s2_core/tests/test_zone_system.cpp` | role-match |
| `workspace/s2_core/tests/test_zone_self_destruct.cpp` | test | CRUD | `workspace/s2_core/tests/test_zone_system.cpp` | role-match |
| `workspace/s2_core/tests/test_zone_spawn_triggers.cpp` | test | event-driven | `workspace/s2_core/tests/test_zone_system.cpp` | role-match |
| `workspace/s2_core/tests/test_zone_owned.cpp` | test | CRUD | `workspace/s2_core/tests/test_zone_system.cpp` | role-match |
| `workspace/s2_plugins/tests/test_effect_fog_emi.cpp` | test | CRUD | `workspace/s2_core/tests/test_zone_system.cpp` | partial |

---

## Pattern Assignments

### `workspace/s2_core/include/s2/zone.hpp` (model, CRUD)

**Аналог:** сам себя расширяет — добавление полей с default-значениями.

**Паттерн расширения struct без breaking changes** (строки 22–68 текущего файла):
```cpp
// Текущий паттерн добавления поля с дефолтом (например detection_mode):
std::string detection_mode{"center"};

// Phase 1 расширяет тем же способом — все новые поля с инлайн-дефолтами:
enum class DetectionMode { CENTER, BOUNDING, PER_LINK };

struct SelfDestructPolicy {
    enum class Type { NONE, ON_ANY_CONTACT, ON_EFFECT_APPLIED };
    Type type{Type::NONE};
};

struct ZoneLifecycle {
    double initial_strength{1.0};
    double growth_rate{0.0};
    double max_strength{1.0};
    double decay_delay{0.0};
    double decay_rate{0.0};
    double remove_threshold{0.05};
};

// В struct Zone:
double            strength{1.0};
DetectionMode     detection_mode_enum{DetectionMode::CENTER};
SelfDestructPolicy self_destruct;
ZoneLifecycle     lifecycle;
std::optional<std::string> attached_to_link;
std::string       attached_to_entity_id;  // EntityId для owned_zones
```

**Существующий move-only паттерн EffectDesc** (строки 43–62):
```cpp
struct EffectDesc {
    std::unique_ptr<EffectPlugin> plugin;  // move-only
    EffectDesc() = default;
    EffectDesc(const EffectDesc&) = delete;
    EffectDesc& operator=(const EffectDesc&) = delete;
    EffectDesc(EffectDesc&&) = default;
    EffectDesc& operator=(EffectDesc&&) = default;
};
```

---

### `workspace/s2_core/include/s2/effect_context.hpp` (model, CRUD)

**Аналог:** сам себя расширяет.

**Существующий паттерн полей с дефолтами** (строки 15–26):
```cpp
// Все поля — plain values без указателей, с инлайн-дефолтами:
double sim_time{0.0};
ZoneId zone_id;
Vec3   zone_center{Vec3::Zero()};
AgentId agent_id{0};
```

**Добавить по тому же паттерну:**
```cpp
double      zone_strength{1.0};   // заполняется ZoneSystem::apply_active_effects()
std::string contact_link;         // для PER_LINK detection (пусто = CENTER/BOUNDING)
```

---

### `workspace/s2_core/include/s2/world_snapshot.hpp` (model, CRUD)

**Аналог:** сам себя расширяет; паттерн — struct ZoneSnapshot (строки 72–90).

**Существующий ZoneSnapshot** (строки 72–90):
```cpp
struct ZoneSnapshot {
    ZoneId id;
    bool enabled{true};
    ZoneShapeType shape_type{ZoneShapeType::SPHERE};
    Vec3 center{Vec3::Zero()};
    double radius{1.0};
    std::string color{"#4488FF"};
    double opacity{0.3};
    std::vector<AgentId> agents_inside;
};
```

**Добавить поля и вложенный тип:**
```cpp
// Добавить в ZoneSnapshot:
double strength{1.0};  // поле zone_strength для браузера

struct Hint {
    std::string         type;    // "glow", "arrows", "particles", "grid"
    nlohmann::json      params;  // {"color":"#88AAFF", "pulse_rate":1.5}
};
std::vector<Hint> visual_hints;  // собирается в SimEngine::build_snapshot()
```

**Паттерн build_snapshot() для зон** (sim_engine.hpp строки 421–437):
```cpp
// Источник: workspace/s2_core/include/s2/sim_engine.hpp, строки 421–437
for (const auto& zone : zone_system_.all_zones()) {
    if (!zone.visible) continue;
    ZoneSnapshot zs;
    zs.id          = zone.id;
    zs.enabled     = zone.enabled;
    // ... копировать поля ...
    zs.agents_inside.assign(zone.inside_agents.begin(), zone.inside_agents.end());
    snap.zones.push_back(std::move(zs));
}
// Phase 1 добавить ВО ВНЕ ЭТОГО ЦИКЛА строку:
// zs.strength = zone.strength;
// + итерацию эффектов для сбора visual_hints:
// for (auto& desc : zone.effects) {
//     if (desc.plugin) {
//         auto hint = desc.plugin->visual_hint();
//         if (hint) zs.visual_hints.push_back({hint->type, hint->params});
//     }
// }
```

---

### `workspace/s2_core/src/zone_system.cpp` (service, event-driven) — расширение

**Аналог:** сам себя расширяет.

**Паттерн итерации зон с накоплением для удаления** (строки 62–105):
```cpp
// ВАЖНО: удалять зоны ПОСЛЕ итерации, чтобы не инвалидировать итератор
// Источник: workspace/s2_core/src/zone_system.cpp, строки 62–105

// Текущий паттерн в tick():
for (auto& zone : zones_) {
    // ... детекция, вызов on_agent_enter / on_agent_exit ...
}
// Phase 1: накапливать zones_to_remove, удалять после цикла:
// std::vector<ZoneId> zones_to_remove;
// for (auto& zone : zones_) { ... если self_destruct → zones_to_remove.push_back(zone.id); }
// for (const auto& id : zones_to_remove) { remove_zone(id); }
```

**Паттерн detection_point()** (строки 182–187):
```cpp
// Источник: workspace/s2_core/src/zone_system.cpp, строки 182–187
Vec3 ZoneSystem::detection_point(const Agent& agent, const std::string& mode)
{
    // "bounding" — TODO: bounding overlap в будущей задаче. Fallback на center.
    (void)mode;
    return agent.world_pose.position();
}
// Phase 1: заменить на switch по DetectionMode enum:
// static Vec3 detection_point(const Agent& agent, DetectionMode mode);
// case DetectionMode::CENTER → agent.world_pose.position()
// case DetectionMode::BOUNDING → bounding sphere overlap → fallback CENTER
// case DetectionMode::PER_LINK → вызывающий код итерирует kinematic_tree
```

**Паттерн создания EffectContext** (строки 210–221):
```cpp
// Источник: workspace/s2_core/src/zone_system.cpp, строки 210–221
EffectContext ctx;
ctx.sim_time       = sim_time;
ctx.dt             = dt;
ctx.zone_id        = zone.id;
ctx.zone_center    = zone.shape.center;
ctx.zone_half_size = zone.shape.half_size;
ctx.agent_id       = agent.id;
ctx.agent_position = agent.world_pose.position();
// Phase 1 добавить:
// ctx.zone_strength = zone.strength;
// ctx.contact_link  = contact_link;  // параметр из PER_LINK итерации
```

**Паттерн toggle_zone()** (строки 146–155):
```cpp
// Источник: workspace/s2_core/src/zone_system.cpp, строки 146–155
bool ZoneSystem::toggle_zone(const ZoneId& id, bool enabled)
{
    for (auto& zone : zones_) {
        if (zone.id == id) {
            zone.enabled = enabled;
            return true;
        }
    }
    return false;
}
// Phase 1: добавить новый метод toggle_zone_with_events(id, enabled, agents, bus)
// чтобы отправлять on_exit / on_enter — аналогично on_agent_exit/on_agent_enter выше.
```

**Паттерн remove_zone()** — нового метода нет, добавить по паттерну toggle_zone():
```cpp
// Скопировать сигнатуру из toggle_zone(), очистить inside_agents, отправить ZoneExited:
void ZoneSystem::remove_zone(const ZoneId& id)
{
    for (auto it = zones_.begin(); it != zones_.end(); ++it) {
        if (it->id == id) {
            // Уведомить агентов внутри (аналог on_agent_exit без bus — добавить параметр)
            zones_.erase(it);
            return;
        }
    }
}
```

---

### `workspace/s2_core/include/s2/zone_spawn_system.hpp` (service, event-driven) — НОВЫЙ

**Аналог:** `workspace/s2_core/include/s2/zone_system.hpp` (роль — подсистема SimEngine).

**Паттерн заголовка подсистемы** (zone_system.hpp строки 1–112):
```cpp
#pragma once

#include <s2/sim_bus.hpp>
#include <s2/zone.hpp>
#include <s2/kernel_command.hpp>
#include <functional>
#include <string>
#include <vector>

namespace s2 {

/**
 * @brief Система триггерного спавна зон.
 *
 * Подписывается на EventBus при инициализации.
 * Каждый тик проверяет timer-триггеры.
 * При срабатывании триггера добавляет SpawnZone в command_queue.
 */
class ZoneSpawnSystem
{
public:
    struct ZoneTemplate {
        Zone proto;          // шаблон зоны (без plugin, только описание)
        // ... тип триггера и его параметры ...
    };

    /// Инициализация — подписка на EventBus.
    void init(SimBus& bus);

    /// Тик — проверка timer-триггеров.
    /// @param sim_time текущее симуляционное время
    /// @param queue    очередь команд (push SpawnZone при срабатывании)
    void tick(double sim_time, KernelCommandQueue& queue);

    /// Добавить шаблон зоны с триггером (из SceneLoader).
    void add_template(ZoneTemplate tmpl);

private:
    std::vector<ZoneTemplate> templates_;
};

} // namespace s2
```

---

### `workspace/s2_core/include/s2/sim_engine.hpp` — расширение apply_kernel_command()

**Аналог:** строки 871–915 (шаблонный метод apply_kernel_command).

**Существующий паттерн обработчика** (строки 874–914):
```cpp
// Источник: workspace/s2_core/include/s2/sim_engine.hpp, строки 871–915
template <typename T>
void apply_kernel_command(const T& cmd)
{
    if constexpr (std::is_same_v<T, cmd::SetPose>)
    {
        for (auto& agent : world_.agents())
            if (agent.id == cmd.id) { agent.world_pose = cmd.pose; break; }
    }
    // ... другие if constexpr ...
    else
    {
        (void)cmd;  // молча игнорируем неизвестные команды
    }
}
```

**Phase 1 добавляет ветки в тот же шаблон:**
```cpp
// Добавить ПЕРЕД финальным else:
else if constexpr (std::is_same_v<T, cmd::SpawnZone>)
{
    Zone z;
    z.id = cmd.id_hint.empty()
        ? "zone_" + std::to_string(next_zone_id_++)
        : cmd.id_hint;
    z.shape   = cmd.shape;
    z.visible = cmd.visible;
    z.color   = cmd.color;
    z.opacity = cmd.opacity;
    z.label   = cmd.label;
    for (const auto& eff_type : cmd.effects) {
        Zone::EffectDesc desc;
        desc.type = eff_type;
        z.effects.push_back(std::move(desc));
    }
    if (cmd.attached_to.has_value())
        z.attached_to_entity_id = cmd.attached_to.value();
    zone_system_.add_zone(std::move(z));
}
else if constexpr (std::is_same_v<T, cmd::DespawnZone>)
{
    zone_system_.remove_zone(cmd.id);
}
else if constexpr (std::is_same_v<T, cmd::ToggleZone>)
{
    zone_system_.toggle_zone_with_events(cmd.id, cmd.enabled, world_.agents(), bus_);
}
```

**Паттерн полей SimEngine** (строки 1003–1041): поле `next_zone_id_` добавить рядом с другими полями:
```cpp
// Добавить в секцию полей:
int next_zone_id_{0};
ZoneSpawnSystem zone_spawn_system_;  // новая подсистема
```

---

### `workspace/s2_core/include/s2/scene_loader.hpp` — расширение парсинга зон

**Аналог:** сам себя расширяет. Паттерн — блок парсинга зон (строки 356–413).

**Существующий паттерн парсинга зоны из YAML** (строки 356–413):
```cpp
// Источник: workspace/s2_core/include/s2/scene_loader.hpp, строки 356–413
if (const auto& zones_node = root["s2"]["zones"]) {
    for (const auto& zn : zones_node) {
        Zone z;
        z.id      = zn["id"].as<std::string>("");
        z.enabled = zn["enabled"].as<bool>(true);
        z.color   = zn["color"].as<std::string>("#4488FF");
        // ... parse_zone_shape, effects ... (тот же паттерн .as<T>(default))
        scene.zones.push_back(std::move(z));
    }
}
```

**Phase 1 добавляет в том же блоке:**
```cpp
// Добавить после z.label = ...:
z.strength = zn["strength"].as<double>(1.0);
// detection_mode → enum:
{
    std::string dm = zn["detection_mode"].as<std::string>("center");
    if (dm == "bounding") z.detection_mode_enum = Zone::DetectionMode::BOUNDING;
    else if (dm == "per_link") z.detection_mode_enum = Zone::DetectionMode::PER_LINK;
    else z.detection_mode_enum = Zone::DetectionMode::CENTER;
}
// self_destruct:
if (zn["self_destruct"]) {
    std::string sd = zn["self_destruct"].as<std::string>("none");
    if      (sd == "on_any_contact")    z.self_destruct.type = Zone::SelfDestructPolicy::Type::ON_ANY_CONTACT;
    else if (sd == "on_effect_applied") z.self_destruct.type = Zone::SelfDestructPolicy::Type::ON_EFFECT_APPLIED;
}
// lifecycle:
if (zn["lifecycle"]) {
    const auto& lc = zn["lifecycle"];
    z.lifecycle.initial_strength  = lc["initial_strength"].as<double>(1.0);
    z.lifecycle.growth_rate       = lc["growth_rate"].as<double>(0.0);
    z.lifecycle.max_strength      = lc["max_strength"].as<double>(1.0);
    z.lifecycle.decay_delay       = lc["decay_delay"].as<double>(0.0);
    z.lifecycle.decay_rate        = lc["decay_rate"].as<double>(0.0);
    z.lifecycle.remove_threshold  = lc["remove_threshold"].as<double>(0.05);
}
// attached_to_link:
if (zn["attached_to_link"])
    z.attached_to_link = zn["attached_to_link"].as<std::string>();
```

**Паттерн owned_zones** — добавить внутри блока парсинга агентов (строки 122–300), после парсинга plugins:
```cpp
// Добавить после блока plugins (строки 184–214):
if (agent_node["owned_zones"]) {
    for (const auto& oz_node : agent_node["owned_zones"]) {
        Zone oz;
        oz.id = oz_node["id"].as<std::string>("owned_" + agent.name + "_" + std::to_string(scene.zones.size()));
        oz.attached_to_entity_id = agent.name;  // резолвится в SimEngine
        if (oz_node["attached_to_link"])
            oz.attached_to_link = oz_node["attached_to_link"].as<std::string>();
        // color, opacity, shape, effects — тем же паттерном что и зоны выше
        scene.zones.push_back(std::move(oz));
    }
}
```

---

### `workspace/s2_plugins/include/s2/effects/fog_effect.hpp` (plugin, request-response) — НОВЫЙ

**Аналог:** `workspace/s2_plugins/include/s2/effects/ice_modifier.hpp` (SENSOR-эффект через sensor_mods).

**Паттерн EffectPlugin заголовка** (ice_modifier.hpp строки 1–51):
```cpp
#pragma once
#include <s2/interfaces/effect_plugin.hpp>
#include <cmath>
#include <algorithm>

namespace s2::effects {

/// Ухудшает дальность оптических сенсоров агента в зоне тумана.
/// Требует capability "optical_sensor".
/// Возвращает SensorMod{param="max_range", multiplier=range_multiplier}.
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
        // Умножитель масштабируется силой зоны: при strength=0 → range_multiplier,
        // при strength=1 → полный эффект.
        double mult = range_multiplier_ + (1.0 - range_multiplier_) * (1.0 - ctx.zone_strength);
        return {SensorMod{.param = "max_range", .multiplier = mult}};
    }

    std::optional<VisualHint> visual_hint() const override {
        // Паттерн скопирован из ice_modifier.hpp строки 39–44
        return VisualHint{
            "glow",
            {{"color", "#AADDFF"}, {"pulse_rate", 0.5}, {"intensity", 0.4}}
        };
    }

private:
    double range_multiplier_{0.3};
};

} // namespace s2::effects
```

---

### `workspace/s2_plugins/include/s2/effects/emi_effect.hpp` (plugin, request-response) — НОВЫЙ

**Аналог:** `workspace/s2_plugins/include/s2/effects/charging_effect.hpp` (CONTINUOUS) + паттерн sensor_mods из ice_modifier.

**Паттерн:** аналогично FogEffect, required_capabilities: [gnss_sensor, imu_sensor], возвращает два SensorMod:
```cpp
#pragma once
#include <s2/interfaces/effect_plugin.hpp>

namespace s2::effects {

/// Добавляет шум к GNSS и IMU сенсорам агента в зоне ЭМ-помех.
/// Требует capability "gnss_sensor" или "imu_sensor".
class EMIEffect : public EffectPlugin {
public:
    void on_init(const YAML::Node& params) override {
        noise_std_ = params["noise_std"].as<double>(2.0);
    }

    EffectType effect_type() const override { return EffectType::SENSOR; }

    std::vector<std::string> required_capabilities() const override {
        return {"gnss_sensor", "imu_sensor"};  // любой из них (capabilities_match — all)
    }

    std::vector<SensorMod> sensor_mods(const EffectContext& ctx) const override {
        double noise = noise_std_ * ctx.zone_strength;
        return {
            SensorMod{.param = "noise_std", .addend = noise},
        };
    }

    std::optional<VisualHint> visual_hint() const override {
        return VisualHint{
            "glow",
            {{"color", "#FF4444"}, {"pulse_rate", 3.0}, {"intensity", 0.7}}
        };
    }

private:
    double noise_std_{2.0};
};

} // namespace s2::effects
```

---

### `workspace/s2_plugins/src/effects_registry.cpp` — расширение

**Аналог:** сам себя расширяет (строки 1–35).

**Паттерн регистрации** (строки 13–33):
```cpp
// Источник: workspace/s2_plugins/src/effects_registry.cpp, строки 1–35
#include <s2/effects/fog_effect.hpp>  // добавить
#include <s2/effects/emi_effect.hpp>  // добавить

// В теле create_effect(), добавить перед закрывающим if (plugin):
else if (type == "fog") plugin = std::make_unique<effects::FogEffect>();
else if (type == "emi") plugin = std::make_unique<effects::EMIEffect>();
```

---

### `workspace/s2_visualizer/web/index.html` — расширение вкладок редактора

**Аналог:** сам себя расширяет; паттерн — editor-tabs (строки 633–694 index.html).

**Существующий паттерн вкладок** (строки 636–694):
```html
<!-- Источник: workspace/s2_visualizer/web/index.html, строки 636–694 -->
<div class="editor-tabs">
    <button class="editor-tab-btn active" data-tab="geometry"
            onclick="switchEditorTab('geometry')">Геометрия</button>
    <button class="editor-tab-btn" data-tab="agents"
            onclick="switchEditorTab('agents')">Агенты</button>
</div>

<div id="editor-tab-geometry"> ... </div>
<div id="editor-tab-agents" style="display:none"> ... </div>
```

**Phase 1 добавляет третью кнопку и блок содержимого:**
```html
<!-- Добавить кнопку в .editor-tabs: -->
<button class="editor-tab-btn" data-tab="zones"
        onclick="switchEditorTab('zones')">Зоны</button>

<!-- Добавить блок вкладки после editor-tab-agents: -->
<div id="editor-tab-zones" style="display:none">
    <button class="save" onclick="startAddZone()"
            style="width:100%;margin-bottom:6px;">+ Добавить зону</button>
    <div id="zone-list"></div>

    <div id="zone-form-view" style="display:none">
        <h5 id="zf-title">Новая зона</h5>
        <label>ID: <input type="text" id="zf-id"></label>
        <label>Форма:
            <select id="zf-shape">
                <option value="sphere">Sphere</option>
                <option value="aabb">Box</option>
                <option value="cylinder">Cylinder</option>
            </select>
        </label>
        <label>Цвет: <input type="color" id="zf-color" value="#4488FF"></label>
        <label>Прозрачность:
            <input type="range" id="zf-opacity" min="0" max="1" step="0.05" value="0.3">
        </label>
        <label>Эффекты:
            <select id="zf-effects" multiple>
                <option value="ice_modifier">ice</option>
                <option value="boost_zone">boost</option>
                <option value="motion_lock">lock</option>
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

---

### `workspace/s2_visualizer/web/js/app.js` — расширение

**Аналог:** сам себя расширяет.

**Паттерн switchEditorTab()** (строки 2181–2190):
```javascript
// Источник: workspace/s2_visualizer/web/js/app.js, строки 2181–2190
window.switchEditorTab = function(tab) {
    activeEditorTab = tab;
    document.getElementById('editor-tab-geometry').style.display =
        tab === 'geometry' ? '' : 'none';
    document.getElementById('editor-tab-agents').style.display =
        tab === 'agents' ? '' : 'none';
    document.querySelectorAll('.editor-tab-btn').forEach(b => {
        b.classList.toggle('active', b.dataset.tab === tab);
    });
    // ...
};
// Phase 1: добавить в массив управляемых вкладок 'zones':
// document.getElementById('editor-tab-zones').style.display = tab === 'zones' ? '' : 'none';
```

**Паттерн zones rendering** (строки 1562–1620):
```javascript
// Источник: workspace/s2_visualizer/web/js/app.js, строки 1562–1620
const currentZoneKeys = new Set();
if (data.zones) {
    data.zones.forEach(z => {
        const key = `zone_${z.id}`;
        currentZoneKeys.add(key);
        if (!z.visible || !z.enabled) { removeMesh(key); return; }
        const pose = { x: cx, y: cy, z: cz, yaw: 0, pitch: 0, roll: 0 };
        const color = z.color || '#4488FF';
        const opacity = (z.opacity !== undefined) ? z.opacity : 0.3;

        if (z.shape_type === 'sphere') {
            updateOrCreateMesh(key, 'sphere', pose,
                { type: 'sphere', radius: z.radius || 1, color },
                { transparent: true, opacity, depthWrite: false, side: 'double' }
            );
        }
        // + aabb, cylinder
    });
}
// Phase 1: добавить после обновления mesh зоны — вызов renderVisualHints(key, z, z.visual_hints)
```

**Паттерн VisualHint рендеринга** — новые вспомогательные функции, скопировать стиль updateOrCreateMesh():
```javascript
// Источник для стиля: updateOrCreateMesh() строки 235+
function renderVisualHints(zoneKey, zone, hints) {
    if (!hints) return;
    hints.forEach((hint, i) => {
        const hintKey = `${zoneKey}_hint_${i}`;
        if      (hint.type === 'glow')      renderGlowHint(hintKey, zone, hint.params);
        else if (hint.type === 'arrows')    renderArrowsHint(hintKey, zone, hint.params);
        else if (hint.type === 'particles') renderParticlesHint(hintKey, zone, hint.params);
        else if (hint.type === 'grid')      renderGridHint(hintKey, zone, hint.params);
    });
}
// Каждая renderXxxHint() создаёт Three.js объект (PointLight, ArrowHelper, Points, GridHelper)
// и добавляет в meshes[hintKey] — тем же паттерном что meshes в updateOrCreateMesh().
```

**Паттерн отправки SpawnZone/DespawnZone через /command REST** (строки 1626–1633):
```javascript
// Источник: workspace/s2_visualizer/web/js/app.js, строки 1626–1633
function sendCommand(cmd) {
    const host = window.location.hostname || 'localhost';
    const port = window.location.port || '1937';
    fetch(`http://${host}:${port}/command?cmd=${cmd}`, { method: 'POST' })
        .then(r => r.json())
        .then(data => console.log('[Command]', data))
        .catch(err => console.error('[Command error]', err));
}
// Phase 1: zone inspector использует POST /command с JSON body,
// аналогично существующему паттерну агентов (смотри строки ~1650+)
```

---

### Тесты (все файлы) (test, CRUD/event-driven)

**Аналог:** `workspace/s2_core/tests/test_zone_system.cpp`

**Паттерн структуры теста** (строки 1–55):
```cpp
// Источник: workspace/s2_core/tests/test_zone_system.cpp, строки 1–55
#include <gtest/gtest.h>
#include <s2/zone_system.hpp>
#include <s2/sim_bus.hpp>
#include <s2/agent.hpp>
#include <s2/actor.hpp>
#include <s2/types.hpp>

namespace s2 {

static Agent make_agent(AgentId id, double x, double y, double z = 0.0)
{
    Agent a;
    a.id = id;
    a.world_pose.x = x;
    a.world_pose.y = y;
    a.world_pose.z = z;
    return a;
}

static Zone make_sphere_zone(const ZoneId& id, Vec3 center, double radius)
{
    Zone z;
    z.id           = id;
    z.enabled      = true;
    z.shape.type   = ZoneShapeType::SPHERE;
    z.shape.center = center;
    z.shape.radius = radius;
    return z;
}
```

**Паттерн подписки на EventBus в тесте** (строки 80+):
```cpp
// Источник: workspace/s2_core/tests/test_zone_system.cpp
bus.subscribe<event::ZoneEntered>([&](const event::ZoneEntered& e) {
    received_zone   = e.zone_id;
    received_entity = e.entity_id;
});
```

**Паттерн вызова tick()** в тестах:
```cpp
// zone_system.tick(agents, actors, bus, sim_time, dt);
// actors = пустой вектор если не нужен
std::vector<Actor> actors;
zone_system.tick(agents, actors, bus, 0.0, 0.1);
```

---

## Shared Patterns

### EventBus subscribe/publish
**Источник:** `workspace/s2_core/include/s2/event_bus.hpp`, строки 154–160
**Применять к:** ZoneSpawnSystem, тестам spawn triggers
```cpp
// Подписка:
bus.subscribe<event::ZoneEntered>([](const event::ZoneEntered& e) { ... });
bus.subscribe<event::ActorStateChanged>([](const event::ActorStateChanged& e) { ... });

// Публикация:
bus.publish(event::ZoneEntered{.zone_id = "z1", .entity_id = 42});
```

### EffectContext construction
**Источник:** `workspace/s2_core/src/zone_system.cpp`, строки 210–221 (on_agent_enter) и 249–264 (apply_active_effects)
**Применять к:** всем местам создания EffectContext в ZoneSystem
```cpp
EffectContext ctx;
ctx.sim_time       = sim_time;
ctx.dt             = dt;
ctx.zone_id        = zone.id;
ctx.zone_center    = zone.shape.center;
ctx.zone_half_size = zone.shape.half_size;
ctx.agent_id       = agent.id;
ctx.agent_position = agent.world_pose.position();
// Phase 1 добавить:
ctx.zone_strength  = zone.strength;
ctx.contact_link   = contact_link;  // "" если не PER_LINK
```

### KernelCommand push (из плагинов)
**Источник:** `workspace/s2_core/include/s2/kernel_command.hpp`, строки 216–225 (комментарий-пример)
**Применять к:** ZoneSpawnSystem когда триггер срабатывает
```cpp
queue.push_back(cmd::SpawnZone{
    .shape    = tmpl.proto.shape,
    .effects  = { "fog" },
    .id_hint  = tmpl.proto.id,
    .visible  = true,
    .color    = tmpl.proto.color,
    .opacity  = tmpl.proto.opacity,
});
```

### YAML парсинг с дефолтом
**Источник:** `workspace/s2_core/include/s2/scene_loader.hpp`, строки 360–412
**Применять к:** всем новым полям в SceneLoader
```cpp
// Паттерн: node["field"].as<Type>(default_value)
z.strength = zn["strength"].as<double>(1.0);
z.enabled  = zn["enabled"].as<bool>(true);
```

### EffectPlugin on_init паттерн
**Источник:** `workspace/s2_plugins/include/s2/effects/ice_modifier.hpp`, строки 14–16
**Применять к:** FogEffect, EMIEffect
```cpp
void on_init(const YAML::Node& params) override {
    field_ = params["field_name"].as<double>(default_value);
}
```

---

## No Analog Found

| Файл | Роль | Data Flow | Причина |
|------|------|-----------|---------|
| (нет) | — | — | Все новые файлы имеют близкие аналоги в кодовой базе |

Примечание: ZoneSpawnSystem и Zone lifecycle — новые механизмы, но строятся на уже существующих EventBus и ZoneSystem паттернах.

---

## Metadata

**Scope поиска аналогов:** workspace/s2_core, workspace/s2_plugins, workspace/s2_visualizer
**Файлов просканировано:** ~20 файлов исходного кода
**Дата извлечения паттернов:** 2026-04-26

**Ключевые ограничения для планировщика:**
1. Нельзя менять сигнатуры `ZoneSystem::tick()` — 11 тестов зависят от неё.
2. Удаление зон только через накопление zones_to_remove, не в процессе итерации.
3. ctx.zone_strength заполняется из zone.strength — не забыть в apply_active_effects() и on_agent_enter().
4. EffectContext — только значения, без указателей (правило из комментария effect_context.hpp).
5. ZoneSpawnSystem работает только в sim_thread — без std::thread, без async.
