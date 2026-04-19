# Task 05 — Зоны + эффекты + contribution/resolver в действии

> **Предыдущий шаг:** `04-agents-basic.md` (агент двигается)
> **Следующий шаг:** `06-raycast-lidar.md`
> **Контекст:** `ARCHITECTURE.md` разделы 7, 8, 9

## Цель

Зоны и эффекты работают. После этого шага: агент въезжает в зону льда — замедляется. Выезжает — скорость возвращается. Зона зарядки заряжает батарею. Мутация (прокол) — необратима.

## Что сделать

### 1. Zone struct

```
s2_core/include/s2/zone.hpp
```

```cpp
struct Zone {
    ZoneId id;
    bool enabled{true};

    ZoneShape shape;

    struct EffectDesc {
        std::string type;     // "ice_modifier", "charging", ...
        EffectType effect_type;
        std::vector<std::string> required_capabilities;
        YAML::Node params;
    };
    std::vector<EffectDesc> effects;

    std::unordered_set<AgentId> inside_agents;
};
```

### 2. EffectPlugin интерфейс

```
s2_core/include/s2/interfaces/effect_plugin.hpp
```

```cpp
class EffectPlugin {
public:
    virtual ~EffectPlugin() = default;
    virtual void on_init(const YAML::Node& params) = 0;
    virtual EffectType effect_type() const = 0;
    virtual std::vector<std::string> required_capabilities() const = 0;

    // MODIFIER: возвращает contributions
    virtual void apply_modifier(SharedState& state, double dt) {}

    // CONTINUOUS: модифицирует single-owner field
    virtual void apply_continuous(SharedState& state, double dt) {}

    // MUTATION: одноразово
    virtual void apply_mutation(SharedState& state) {}

    // SENSOR: возвращает параметры модификации
    struct SensorMod { std::string param; double multiplier{1.0}; };
    virtual std::vector<SensorMod> sensor_mods() const { return {}; }
};
```

### 3. Встроенные эффекты (s2_plugins/effects/)

**IceModifier** (MODIFIER):
- required_capabilities: ["surface_contact"]
- apply_modifier: `state.add_scale(traction_coeff, "ice_zone")`
- Параметр: traction_coefficient (default 0.2)

**ChargingEffect** (CONTINUOUS):
- required_capabilities: ["has_battery"]
- apply_continuous: увеличивает battery_level
- Параметр: charge_rate

**TirePunctureEffect** (MUTATION):
- required_capabilities: ["wheeled"]
- apply_mutation: tire_ok["fl"] = false
- Параметр: affected_tires

**FogSensorEffect** (SENSOR):
- required_capabilities: ["optical"]
- sensor_mods: [{"max_range", 0.6}]

Для CONTINUOUS (ChargingEffect) нужен BatteryComponent в SharedState. Пока создаём вручную:

```cpp
struct BatteryComponent {
    double level{1.0};
    double drain_rate{0.001};
};
```

### 4. ZoneSystem

```
s2_core/include/s2/zone_system.hpp
```

```cpp
class ZoneSystem {
public:
    void add_zone(Zone zone);

    void tick(std::vector<Agent>& agents, SimBus& bus, double dt);

    // Для World Query API
    std::vector<ZoneId> zones_containing(const Vec3& point) const;

private:
    void on_agent_enter(Agent& agent, Zone& zone, SimBus& bus);
    void on_agent_exit(Agent& agent, Zone& zone, SimBus& bus);

    std::vector<Zone> zones_;

    // Активные эффекты на агентах (zone_id → agent_id → effects)
    struct ActiveEffect {
        ZoneId zone_id;
        std::unique_ptr<EffectPlugin> plugin;
    };
    std::unordered_map<AgentId, std::vector<ActiveEffect>> active_effects_;
};
```

tick():
1. Для каждой зоны:
   - Если !enabled: убрать всех inside, вызвать exit
   - Проверить каждого агента: inside?
   - Если вошёл: on_agent_enter (создать эффекты, capabilities matching)
   - Если вышел: on_agent_exit (удалить MODIFIER/CONTINUOUS, оставить MUTATION)
2. Для каждого агента с active effects:
   - MODIFIER → apply_modifier (пишет contributions)
   - CONTINUOUS → apply_continuous (модифицирует single-owner fields)

**[СПРОСИТЬ]** Сейчас эффекты создаются при каждом входе. Для v1 это ок (десятки агентов, десятки зон). Нужна ли оптимизация (пул эффектов)?

### 5. Агентные (собственные) эффекты

Добавить в Agent:

```cpp
struct Agent {
    // ... existing ...
    std::vector<std::unique_ptr<EffectPlugin>> own_effects;
};
```

Собственные эффекты применяются каждый тик, всегда, до зональных:

```cpp
// В tick(), фаза 3b:
for (auto& eff : agent.own_effects) {
    if (eff->effect_type() == EffectType::MODIFIER)
        eff->apply_modifier(agent.state, dt_);
    else if (eff->effect_type() == EffectType::CONTINUOUS)
        eff->apply_continuous(agent.state, dt_);
}
```

### 6. Обновить тиковую петлю

```
tick():
  // Фаза 2: Зоны
  zone_system_.tick(world_.agents(), bus_, dt_);

  for (auto& agent : world_.agents()) {
      // 3a-3c: Resource + own effects + zone effects уже сделали contributions
      // 3d: RESOLVER
      agent.state.resolve();
      // 3e: Actuation (читает effective)
      // ... остальное как было
  }
```

### 7. Обновить сцену

```yaml
# В test_basic.yaml добавить:
zones:
  - id: "ice_patch"
    shape:
      type: "aabb"
      center: {x: 3.0, y: 0.0, z: 0.0}
      half_size: {x: 2.0, y: 2.0, z: 1.0}
    effects:
      - type: "ice_modifier"
        required_capabilities: ["surface_contact"]
        params:
          traction_coefficient: 0.2
```

## Тесты

```
s2_core/tests/test_zones.cpp
s2_core/tests/test_effects.cpp
```

### test_zones.cpp
- Агент вне зоны → event не публикуется
- Агент входит в зону → AgentEnteredZone event
- Агент выходит → AgentExitedZone event
- zones_containing() возвращает правильные зоны
- Зона disabled → агенты внутри получают exit event

### test_effects.cpp
- IceModifier: агент в зоне льда, speed_scale=0.2 → за 100 тиков проехал ~0.2 вместо ~1.0
- IceModifier: агент без capability "surface_contact" → эффект не применяется
- ChargingEffect: battery_level растёт пока агент в зоне
- TirePuncture: tire_ok["fl"] = false после входа. Остаётся false после выхода.
- Два MODIFIER одновременно (ice 0.2 + boost 1.5): effective = 0.2 × 1.5 = 0.3
- Own effect: BatteryModifier scale 0.8 + zone ice 0.2 → effective = 0.8 × 0.2 = 0.16

## Критерии приёмки

- Зоны detect enter/exit
- MODIFIER → contribution → resolver → actuation видит результат
- CONTINUOUS модифицирует single-owner state
- MUTATION необратима
- Capabilities matching работает
- Несколько эффектов компонуются через resolver (multiply)
