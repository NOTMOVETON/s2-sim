# Задача 25 — Эффекты MODIFIER: ConveyorEffect, WindEffect + velocity_addition в кинематике

## Цель

Эффекты, добавляющие внешнюю скорость к агенту (velocity_addition).
Ключевое отличие от scale-эффектов: additive-скорость применяется **после** actuation-плагина,
прямо в кинематике tick(). Агент не может этому противостоять командами.

После задачи: робот на «конвейере» дрейфует в заданном направлении даже при нулевой команде.
Робот в ветровой зоне сносится ветром.

## Зависимости

- Задача 24 (эффекты MODIFIER + фабрика)
- `sim_engine.hpp` — нужно применить velocity_addition в кинематической фазе
- `shared_state.hpp` — add_velocity_addition() (уже реализован)

---

## Что сделать

### 1. Фикс кинематики в SimEngine — применить velocity_addition

**Файл:** `workspace/s2_core/include/s2/sim_engine.hpp`

В методе `tick()`, фаза 3f (Кинематика), после вычисления `world_vel`:

```cpp
// === 3f. Кинематика ===
double local_vx = agent.world_velocity.linear.x();
double local_vy = agent.world_velocity.linear.y();
double wz = agent.world_velocity.angular.z();

Eigen::Matrix3d R = CollisionSystem::rotation_from_pose(agent.world_pose);
Vec3 body_vel{local_vx, local_vy, 0.0};
Vec3 world_vel = R * body_vel;

// ← ДОБАВИТЬ: применить velocity_addition из resolver-а
const Vec3& additive = agent.state.effective().velocity_addition;

agent.world_pose.x += (world_vel.x() + additive.x()) * dt_;
agent.world_pose.y += (world_vel.y() + additive.y()) * dt_;
agent.world_pose.z += agent.world_velocity.linear.z() * dt_;
// Примечание: Z управляется GravityPlugin, additive.z() добавляется для воздушных зон
agent.world_pose.yaw += wz * dt_;
```

### 2. ConveyorEffect

**Файл:** `workspace/s2_plugins/effects/conveyor_effect.hpp` (новый)

```cpp
#pragma once
#include <s2/interfaces/effect_plugin.hpp>

namespace s2::effects {

/// Добавляет постоянную скорость в заданном направлении (движущийся пол).
/// Применяется только к агентам с capability "surface_contact".
/// Направление задаётся в мировых координатах.
class ConveyorEffect : public EffectPlugin {
public:
    void on_init(const YAML::Node& params) override {
        if (params["direction"]) {
            const auto& d = params["direction"];
            direction_.x() = d["x"].as<double>(1.0);
            direction_.y() = d["y"].as<double>(0.0);
            direction_.z() = d["z"].as<double>(0.0);
        }
        speed_ = params["speed"].as<double>(1.0);
    }

    EffectType effect_type() const override { return EffectType::MODIFIER; }

    std::vector<std::string> required_capabilities() const override {
        return {"surface_contact"};
    }

    void apply_modifier(SharedState& state, const EffectContext& ctx) override {
        Vec3 velocity = direction_.normalized() * speed_;
        state.add_velocity_addition(velocity, "conveyor_" + ctx.zone_id);
    }

    std::optional<VisualHint> visual_hint() const override {
        return VisualHint{
            "arrows",
            {
                {"color", "#FF8800"},
                {"direction", {direction_.x(), direction_.y(), direction_.z()}},
                {"speed", speed_},
                {"animated", true}
            }
        };
    }

private:
    Vec3 direction_{1.0, 0.0, 0.0};
    double speed_{1.0};
};

} // namespace s2::effects
```

### 3. WindEffect

**Файл:** `workspace/s2_plugins/effects/wind_effect.hpp` (новый)

```cpp
#pragma once
#include <s2/interfaces/effect_plugin.hpp>
#include <cmath>

namespace s2::effects {

/// Добавляет ветровую скорость.
/// Применяется ко всем агентам (без требований к capabilities).
/// Поддерживает порывы (gusts) через sin-шум по времени.
class WindEffect : public EffectPlugin {
public:
    void on_init(const YAML::Node& params) override {
        if (params["wind_vector"]) {
            const auto& w = params["wind_vector"];
            wind_vector_.x() = w["x"].as<double>(0.5);
            wind_vector_.y() = w["y"].as<double>(0.0);
            wind_vector_.z() = w["z"].as<double>(0.0);
        }
        gust_amplitude_ = params["gust_amplitude"].as<double>(0.0);
        gust_frequency_ = params["gust_frequency"].as<double>(0.5);
    }

    EffectType effect_type() const override { return EffectType::MODIFIER; }

    std::vector<std::string> required_capabilities() const override {
        return {};  // все агенты, включая дроны
    }

    void apply_modifier(SharedState& state, const EffectContext& ctx) override {
        Vec3 v = wind_vector_;

        if (gust_amplitude_ > 0.0) {
            double gust = gust_amplitude_ * std::sin(ctx.sim_time * gust_frequency_ * 2.0 * M_PI);
            v += wind_vector_.normalized() * gust;
        }

        state.add_velocity_addition(v, "wind_" + ctx.zone_id);
    }

    std::optional<VisualHint> visual_hint() const override {
        return VisualHint{
            "particles",
            {
                {"color", "#AADDFF"},
                {"direction", {wind_vector_.x(), wind_vector_.y(), wind_vector_.z()}},
                {"density", 30}
            }
        };
    }

private:
    Vec3 wind_vector_{0.5, 0.0, 0.0};
    double gust_amplitude_{0.0};
    double gust_frequency_{0.5};
};

} // namespace s2::effects
```

### 4. Регистрация в фабрике

**Файл:** `workspace/s2_plugins/src/effects_registry.cpp`

Добавить к существующим регистрациям:

```cpp
else if (type == "conveyor")    plugin = std::make_unique<effects::ConveyorEffect>();
else if (type == "wind")        plugin = std::make_unique<effects::WindEffect>();
```

### 5. Пример YAML

```yaml
zones:
  # Конвейерная лента движется вправо
  - id: "conveyor_main"
    shape:
      type: aabb
      center: {x: 0.0, y: 0.0, z: 0.15}
      half_size: {x: 0.6, y: 3.0, z: 0.3}
    color: "#FF8800"
    opacity: 0.3
    label: "Конвейер"
    effects:
      - type: conveyor
        params:
          direction: {x: 1.0, y: 0.0, z: 0.0}
          speed: 1.5

  # Открытая зона с боковым ветром и порывами
  - id: "outdoor_wind"
    shape:
      type: infinite
    color: "#AADDFF"
    opacity: 0.1
    label: "Ветер"
    effects:
      - type: wind
        params:
          wind_vector: {x: 0.3, y: 0.0, z: 0.0}
          gust_amplitude: 0.2
          gust_frequency: 0.3
```

---

## Тесты

**Файл:** `workspace/s2_core/tests/test_effect_velocity_addition.cpp`

- `ConveyorEffect_DriftsAgent` — агент с нулевой cmd_vel на конвейере: через 10 тиков
  сдвинулся в направлении конвейера на speed × dt × 10 (±5% погрешности)
- `ConveyorEffect_AddsToOwnVelocity` — агент движется в ту же сторону: скорость суммируется
- `ConveyorEffect_RequiresSurfaceContact` — агент без "surface_contact" → нет дрейфа
- `WindEffect_AllAgents` — агент без capabilities в ветровой зоне → дрейфует
- `WindEffect_Gusts` — gust_amplitude > 0 → добавочная скорость колеблется синусоидально
- `VelocityAddition_AppliedAfterActuation` — добавочная скорость применяется после плагина:
  итоговое смещение = (cmd_vel + additive) × dt

---

## Критерии завершения

- [ ] velocity_addition из resolver-а применяется в кинематической фазе tick()
- [ ] ConveyorEffect добавляет дрейф агентам с "surface_contact"
- [ ] WindEffect добавляет дрейф всем агентам, поддерживает порывы
- [ ] Суммирование нескольких velocity_addition работает (конвейер + ветер)
- [ ] Все тесты проходят в Docker
