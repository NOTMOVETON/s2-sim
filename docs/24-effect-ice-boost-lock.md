# Задача 24 — Эффекты MODIFIER: IceModifier, BoostZone, MotionLockZone

## Цель

Первые конкретные эффекты типа MODIFIER. После задачи:
- Агент в ледяной зоне замедляется (traction_coefficient)
- Агент в boost-зоне ускоряется
- Агент в danger-зоне блокируется
- DiffDrivePlugin читает `effective().speed_scale` и применяет к своей скорости

## Зависимости

- Задача 23 (Zone infrastructure, EffectPlugin interface)
- `diff_drive.hpp` — плагин актуации (нужно добавить чтение effective_speed_scale)
- `shared_state.hpp` — add_scale(), add_lock() (уже реализованы)

---

## Что сделать

### 1. Фикс DiffDrivePlugin — читать effective_speed_scale

**Файл:** `workspace/s2_plugins/include/s2/plugins/diff_drive.hpp`

В методе `update()`, перед установкой `world_velocity`, умножить на scale:

```cpp
void update(double dt, Agent& agent) override {
    // ... получить cmd_vel из SharedState ...
    double desired_linear = /* из cmd_vel */;
    double desired_angular = /* из cmd_vel */;

    const auto& eff = agent.state.effective();

    // Если движение заблокировано — не двигаемся
    if (eff.motion_locked) {
        agent.world_velocity.linear = Vec3::Zero();
        agent.world_velocity.angular = Vec3::Zero();
        return;
    }

    // Применить scale к линейной скорости
    desired_linear *= eff.speed_scale;

    // Ограничить максимальными значениями
    desired_linear = std::clamp(desired_linear, -max_linear_, max_linear_);
    desired_angular = std::clamp(desired_angular, -max_angular_, max_angular_);

    agent.world_velocity.linear.x() = desired_linear;
    agent.world_velocity.linear.y() = 0.0;
    agent.world_velocity.linear.z() = 0.0;
    agent.world_velocity.angular.z() = desired_angular;
}
```

### 2. IceModifier

**Файл:** `workspace/s2_plugins/effects/ice_modifier.hpp` (новый)

```cpp
#pragma once
#include <s2/interfaces/effect_plugin.hpp>
#include <cmath>

namespace s2::effects {

/// Замедляет агентов с capability "surface_contact".
/// Публикует add_scale(traction_coefficient, "ice").
/// Поддерживает noise_amplitude для рандомизированного скольжения.
class IceModifier : public EffectPlugin {
public:
    void on_init(const YAML::Node& params) override {
        traction_coeff_ = params["traction_coefficient"].as<double>(0.2);
        noise_amplitude_ = params["noise_amplitude"].as<double>(0.0);
    }

    EffectType effect_type() const override { return EffectType::MODIFIER; }

    std::vector<std::string> required_capabilities() const override {
        return {"surface_contact"};
    }

    void apply_modifier(SharedState& state, const EffectContext& ctx) override {
        double scale = traction_coeff_;

        if (noise_amplitude_ > 0.0) {
            // Детерминированный шум через агента и время
            // Простой псевдо-шум: sin(sim_time * frequency + agent_id * offset)
            double noise = noise_amplitude_ *
                std::sin(ctx.sim_time * 7.3 + ctx.agent_id * 1.7);
            scale = std::clamp(traction_coeff_ + noise, 0.01, 1.0);
        }

        state.add_scale(scale, "ice_zone_" + ctx.zone_id);
    }

    std::optional<VisualHint> visual_hint() const override {
        return VisualHint{
            "glow",
            {{"color", "#88AAFF"}, {"pulse_rate", 1.5}, {"intensity", 0.6}}
        };
    }

private:
    double traction_coeff_{0.2};
    double noise_amplitude_{0.0};
};

} // namespace s2::effects
```

### 3. BoostZone

**Файл:** `workspace/s2_plugins/effects/boost_zone.hpp` (новый)

```cpp
#pragma once
#include <s2/interfaces/effect_plugin.hpp>

namespace s2::effects {

/// Ускоряет агентов с capability "surface_contact".
/// speed_multiplier > 1.0 — ускорение.
class BoostZone : public EffectPlugin {
public:
    void on_init(const YAML::Node& params) override {
        speed_multiplier_ = params["speed_multiplier"].as<double>(1.5);
    }

    EffectType effect_type() const override { return EffectType::MODIFIER; }

    std::vector<std::string> required_capabilities() const override {
        return {"surface_contact"};
    }

    void apply_modifier(SharedState& state, const EffectContext& ctx) override {
        state.add_scale(speed_multiplier_, "boost_zone_" + ctx.zone_id);
    }

    std::optional<VisualHint> visual_hint() const override {
        return VisualHint{
            "arrows",
            {{"color", "#44FF88"}, {"direction", {0, 0, 1}}, {"speed", 2.0}}
        };
    }

private:
    double speed_multiplier_{1.5};
};

} // namespace s2::effects
```

### 4. MotionLockZone

**Файл:** `workspace/s2_plugins/effects/motion_lock_zone.hpp` (новый)

```cpp
#pragma once
#include <s2/interfaces/effect_plugin.hpp>

namespace s2::effects {

/// Блокирует движение любого агента в зоне (add_lock).
/// Используется для запретных зон, опасных областей.
class MotionLockZone : public EffectPlugin {
public:
    void on_init(const YAML::Node& params) override {
        source_label_ = params["source_label"].as<std::string>("forbidden_zone");
    }

    EffectType effect_type() const override { return EffectType::MODIFIER; }

    std::vector<std::string> required_capabilities() const override {
        return {};  // применяется ко всем
    }

    void apply_modifier(SharedState& state, const EffectContext& ctx) override {
        state.add_lock(true, source_label_ + "_" + ctx.zone_id);
    }

    std::optional<VisualHint> visual_hint() const override {
        return VisualHint{
            "grid",
            {{"color", "#FF2222"}, {"line_width", 2.0}, {"spacing", 0.5}}
        };
    }

private:
    std::string source_label_{"forbidden_zone"};
};

} // namespace s2::effects
```

### 5. Регистрация в фабрике эффектов

**Файл:** `workspace/s2_plugins/src/effects_registry.cpp` (новый)

```cpp
#include <s2/effects/ice_modifier.hpp>
#include <s2/effects/boost_zone.hpp>
#include <s2/effects/motion_lock_zone.hpp>
#include <s2/interfaces/effect_plugin.hpp>
#include <memory>
#include <string>

namespace s2 {

std::unique_ptr<EffectPlugin> create_effect(
    const std::string& type, const YAML::Node& params)
{
    std::unique_ptr<EffectPlugin> plugin;

    if (type == "ice_modifier")      plugin = std::make_unique<effects::IceModifier>();
    else if (type == "boost_zone")   plugin = std::make_unique<effects::BoostZone>();
    else if (type == "motion_lock")  plugin = std::make_unique<effects::MotionLockZone>();
    // Новые эффекты добавляются здесь по мере реализации задач 25–29.

    if (plugin) {
        plugin->on_init(params);
    }
    return plugin;
}

} // namespace s2
```

Этот `create_effect` передаётся в `ZoneSystem::set_effect_factory()` при инициализации.

### 6. Пример YAML

```yaml
zones:
  - id: "ice_patch"
    shape:
      type: aabb
      center: {x: 3.0, y: 0.0, z: 0.5}
      half_size: {x: 2.0, y: 2.0, z: 1.0}
    color: "#4488FF"
    label: "Лёд"
    effects:
      - type: ice_modifier
        params:
          traction_coefficient: 0.2
          noise_amplitude: 0.05

  - id: "speed_corridor"
    shape:
      type: aabb
      center: {x: -3.0, y: 0.0, z: 0.5}
      half_size: {x: 1.0, y: 5.0, z: 1.0}
    color: "#44FF88"
    label: "Буст"
    effects:
      - type: boost_zone
        params:
          speed_multiplier: 1.8

  - id: "danger_zone"
    shape:
      type: sphere
      center: {x: 0.0, y: 5.0, z: 0.0}
      radius: 1.5
    color: "#FF2222"
    label: "Опасно"
    effects:
      - type: motion_lock
        params:
          source_label: "danger"
```

---

## Тесты

**Файл:** `workspace/s2_core/tests/test_effect_modifier.cpp`

- `IceModifier_SlowsAgent` — агент с capability "surface_contact" в зоне льда:
  через 100 тиков с cmd_vel=1.0 проехал ~0.2 расстояния вместо ~1.0
- `IceModifier_NoCapability` — агент без "surface_contact" → скорость не изменилась
- `IceModifier_NoiseAmplitude` — noise_amplitude > 0 → scale колеблется, но не выходит за [0.01, 1.0]
- `BoostZone_SpeedsUpAgent` — агент в boost зоне: скорость умножена на speed_multiplier
- `MotionLock_BlocksMovement` — агент в motion_lock зоне: motion_locked = true → агент стоит
- `TwoModifiers_Combined` — ice(0.2) + boost(1.5) в двух разных зонах одновременно:
  effective_speed_scale ≈ 0.3 (произведение)
- `MotionLock_StopsRegardlessOfBoost` — motion_lock + boost: агент стоит (lock приоритетнее)
- `DiffDrive_ReadsEffectiveScale` — DiffDrivePlugin: при speed_scale=0.5 итоговая скорость = 0.5 × cmd_vel

---

## Критерии завершения

- [ ] DiffDrivePlugin читает effective().speed_scale и motion_locked
- [ ] IceModifier: агент в зоне льда замедляется, без нужного capability — нет
- [ ] BoostZone: агент ускоряется
- [ ] MotionLockZone: агент блокируется
- [ ] Несколько эффектов перемножаются через resolver
- [ ] Фабрика эффектов создаёт плагины по строковому типу
- [ ] Все тесты проходят в Docker
