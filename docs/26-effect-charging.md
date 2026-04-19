# Задача 26 — Эффект CONTINUOUS: ChargingEffect + BatteryComponent

## Цель

Первый CONTINUOUS-эффект. После задачи:
зарядная станция (цилиндрическая зона) заряжает батарею агента,
пока он находится внутри. Уровень батареи хранится в SharedState
и доступен плагинам и визуализатору.

## Зависимости

- Задача 25 (фабрика эффектов, ZoneSystem)
- `shared_state.hpp` — `emplace<T>()`, `get<T>()`

---

## Что сделать

### 1. BatteryComponent

**Файл:** `workspace/s2_core/include/s2/components/battery_component.hpp` (новый)

```cpp
#pragma once

namespace s2 {

/// Минимальный компонент батареи в SharedState.
/// Владелец — BatteryPlugin (задача 32) или инициализируется вручную.
/// Уровень: 0.0 (разряжена) … 1.0 (полная).
struct BatteryComponent {
    double level{1.0};       ///< Текущий уровень [0.0, 1.0]
    bool charging{false};    ///< Флаг: сейчас заряжается (для визуализации)
};

} // namespace s2
```

### 2. ChargingEffect

**Файл:** `workspace/s2_plugins/effects/charging_effect.hpp` (новый)

```cpp
#pragma once
#include <s2/interfaces/effect_plugin.hpp>
#include <s2/components/battery_component.hpp>
#include <algorithm>

namespace s2::effects {

/// Заряжает батарею агента пока он находится в зоне.
/// Требует capability "has_battery" и BatteryComponent в SharedState.
///
/// Алгоритм CONTINUOUS (вызывается каждый тик пока агент внутри):
///   battery.level = min(1.0, battery.level + charge_rate × dt)
///   battery.charging = true
///
/// При выходе из зоны battery.charging сбрасывается в false
/// (обработка в ZoneSystem::on_agent_exit).
class ChargingEffect : public EffectPlugin {
public:
    void on_init(const YAML::Node& params) override {
        charge_rate_ = params["charge_rate"].as<double>(0.1); // 10% в секунду по умолчанию
    }

    EffectType effect_type() const override { return EffectType::CONTINUOUS; }

    std::vector<std::string> required_capabilities() const override {
        return {"has_battery"};
    }

    void apply_continuous(SharedState& state, const EffectContext& ctx) override {
        auto* bat = state.get<BatteryComponent>();
        if (!bat) {
            // Создать компонент если отсутствует (graceful init)
            bat = &state.emplace<BatteryComponent>();
        }
        bat->level = std::min(1.0, bat->level + charge_rate_ * ctx.dt);
        bat->charging = true;
    }

    std::optional<VisualHint> visual_hint() const override {
        return VisualHint{
            "glow",
            {{"color", "#FFDD44"}, {"pulse_rate", 2.0}, {"intensity", 0.8}}
        };
    }

private:
    double charge_rate_{0.1};
};

} // namespace s2::effects
```

### 3. Сброс charging-флага при выходе из зоны

**Файл:** `workspace/s2_core/include/s2/zone_system.hpp`

В `on_agent_exit()`, после публикации события, сбросить charging для ChargingEffect:

```cpp
void on_agent_exit(Agent& agent, Zone& zone, SimBus& bus) {
    bus.publish(AgentExitedZoneEvent{agent.id, zone.id});

    // Сбросить charging флаг если в зоне был ChargingEffect
    for (const auto& desc : zone.effects) {
        if (desc.type == "charging") {
            auto* bat = agent.state.get<BatteryComponent>();
            if (bat) bat->charging = false;
        }
    }
}
```

### 4. BatteryComponent отображается в snapshot

**Файл:** `workspace/s2_core/include/s2/world_snapshot.hpp`

Добавить поле `battery_level` и `battery_charging` в `AgentSnapshot`:

```cpp
struct AgentSnapshot {
    // ... существующие ...
    double battery_level{-1.0};   ///< -1 = нет компонента
    bool battery_charging{false};
};
```

В `SimEngine::build_snapshot()`:

```cpp
const auto* bat = agent.state.get<BatteryComponent>();
if (bat) {
    as.battery_level = bat->level;
    as.battery_charging = bat->charging;
}
```

### 5. Регистрация в фабрике

**Файл:** `workspace/s2_plugins/src/effects_registry.cpp`

```cpp
else if (type == "charging")    plugin = std::make_unique<effects::ChargingEffect>();
```

### 6. Инициализация BatteryComponent в SceneLoader

Агент с capability `has_battery` должен иметь BatteryComponent в SharedState с начала симуляции.
Задача: в `SceneLoader`, после создания агента, если у него есть `has_battery` — инициализировать:

```cpp
if (agent.capabilities.count("has_battery")) {
    agent.state.emplace<BatteryComponent>();
}
```

### 7. Пример YAML

```yaml
agents:
  - name: robot_0
    capabilities: ["surface_contact", "has_battery"]
    # ... остальные параметры ...

zones:
  - id: "charging_pad"
    shape:
      type: cylinder
      center: {x: -5.0, y: 0.0, z: 0.5}
      radius: 1.0
      half_height: 1.0
    color: "#FFDD44"
    opacity: 0.4
    label: "Зарядная станция"
    effects:
      - type: charging
        params:
          charge_rate: 0.05   # 5% в секунду = полный заряд за 20 сек
```

---

## Тесты

**Файл:** `workspace/s2_core/tests/test_effect_charging.cpp`

- `ChargingEffect_IncreasesLevel` — агент с "has_battery" и level=0.5 в зоне: через 10 сек
  уровень = 0.5 + charge_rate × 10 (не выше 1.0)
- `ChargingEffect_CapsAt100Percent` — агент с level=0.95, charge_rate=0.1, 10 тиков:
  level ≤ 1.0
- `ChargingEffect_NoCapability` — агент без "has_battery" → level не меняется
- `ChargingEffect_ChargingFlagSet` — пока агент в зоне: battery.charging = true
- `ChargingEffect_ChargingFlagClearedOnExit` — агент вышел: battery.charging = false
- `ChargingEffect_CreatesComponentIfMissing` — агент без BatteryComponent:
  после первого тика компонент создан с некоторым уровнем
- `BatterySnapshot_ContainsLevel` — snapshot агента содержит battery_level

---

## Критерии завершения

- [ ] BatteryComponent создаётся в SharedState при инициализации агента с capability "has_battery"
- [ ] ChargingEffect увеличивает level каждый тик пока агент в зоне
- [ ] Уровень не превышает 1.0
- [ ] charging = true в зоне, false после выхода
- [ ] battery_level доступен в AgentSnapshot
- [ ] Все тесты проходят в Docker
