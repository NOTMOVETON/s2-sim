# Задача 27 — Эффект MUTATION: TirePunctureEffect

## Цель

Первый MUTATION-эффект. После задачи:
агент, въехавший в зону прокола, получает необратимое повреждение шин.
Эффект остаётся после выхода из зоны. DiffDrivePlugin читает состояние шин
и адаптирует поведение (снижение max_speed, добавление drift).

## Зависимости

- Задача 25 (фабрика эффектов, ZoneSystem)
- `diff_drive.hpp` — нужно добавить чтение TirePunctureData

---

## Что сделать

### 1. TirePunctureData

**Файл:** `workspace/s2_core/include/s2/components/tire_puncture_data.hpp` (новый)

```cpp
#pragma once

namespace s2 {

/// Состояние шин агента.
/// Хранится в SharedState как single-owner (MUTATION записывает, DiffDrivePlugin читает).
struct TirePunctureData {
    bool fl_ok{true};   ///< front-left
    bool fr_ok{true};   ///< front-right
    bool rl_ok{true};   ///< rear-left
    bool rr_ok{true};   ///< rear-right

    /// Количество проколотых шин (0–4).
    int punctured_count() const {
        return (!fl_ok ? 1 : 0) + (!fr_ok ? 1 : 0)
             + (!rl_ok ? 1 : 0) + (!rr_ok ? 1 : 0);
    }

    /// true если хотя бы одна шина спущена.
    bool has_puncture() const { return punctured_count() > 0; }
};

} // namespace s2
```

### 2. TirePunctureEffect

**Файл:** `workspace/s2_plugins/effects/tire_puncture.hpp` (новый)

```cpp
#pragma once
#include <s2/interfaces/effect_plugin.hpp>
#include <s2/components/tire_puncture_data.hpp>
#include <string>
#include <vector>

namespace s2::effects {

/// Однократно прокалывает указанные шины при входе агента в зону.
/// Требует capability "wheeled".
/// Необратимо — не исчезает при выходе.
class TirePunctureEffect : public EffectPlugin {
public:
    void on_init(const YAML::Node& params) override {
        if (params["affected_tires"]) {
            for (const auto& t : params["affected_tires"]) {
                affected_tires_.push_back(t.as<std::string>());
            }
        } else {
            // По умолчанию — одна передняя левая
            affected_tires_ = {"fl"};
        }
    }

    EffectType effect_type() const override { return EffectType::MUTATION; }

    std::vector<std::string> required_capabilities() const override {
        return {"wheeled"};
    }

    void apply_mutation(SharedState& state, const EffectContext& ctx) override {
        auto* data = state.get<TirePunctureData>();
        if (!data) {
            data = &state.emplace<TirePunctureData>();
        }

        for (const auto& tire : affected_tires_) {
            if (tire == "fl") data->fl_ok = false;
            else if (tire == "fr") data->fr_ok = false;
            else if (tire == "rl") data->rl_ok = false;
            else if (tire == "rr") data->rr_ok = false;
        }
    }

    std::optional<VisualHint> visual_hint() const override {
        return VisualHint{
            "grid",
            {{"color", "#AA2200"}, {"spacing", 0.3}, {"line_width", 1.0}}
        };
    }

private:
    std::vector<std::string> affected_tires_;
};

} // namespace s2::effects
```

### 3. DiffDrivePlugin — учитывать TirePunctureData

**Файл:** `workspace/s2_plugins/include/s2/plugins/diff_drive.hpp`

В `update()`, после проверки motion_locked, добавить чтение TirePunctureData:

```cpp
// Учёт проколотых шин
const auto* tire_data = agent.state.get<TirePunctureData>();
if (tire_data && tire_data->has_puncture()) {
    int n = tire_data->punctured_count();
    // Каждый прокол снижает max скорость на 30% и добавляет небольшой drift
    double penalty = std::pow(0.7, n);
    desired_linear *= penalty;
    // Небольшой случайный drift: sin(sim_time × high_freq)
    // Получаем sim_time из контекста — пока не доступен в плагине,
    // поэтому используем внутренний счётчик
    time_acc_ += dt;
    double drift = 0.05 * n * std::sin(time_acc_ * 15.0);
    desired_angular += drift;
}
```

Добавить член `double time_acc_{0.0}` в DiffDrivePlugin.

### 4. TirePunctureData в snapshot

**Файл:** `workspace/s2_core/include/s2/world_snapshot.hpp`

```cpp
struct AgentSnapshot {
    // ... существующие ...
    int tire_puncture_count{0};  ///< Количество проколотых шин (0–4)
};
```

В `SimEngine::build_snapshot()`:

```cpp
const auto* tire = agent.state.get<TirePunctureData>();
if (tire) as.tire_puncture_count = tire->punctured_count();
```

### 5. Регистрация в фабрике

```cpp
else if (type == "tire_puncture") plugin = std::make_unique<effects::TirePunctureEffect>();
```

### 6. Пример YAML

```yaml
zones:
  - id: "nail_strip"
    shape:
      type: aabb
      center: {x: 5.0, y: 0.0, z: 0.1}
      half_size: {x: 0.3, y: 2.0, z: 0.2}
    color: "#AA2200"
    opacity: 0.5
    label: "Полоса шипов"
    effects:
      - type: tire_puncture
        params:
          affected_tires: ["fl", "fr"]   # оба передних
```

---

## Тесты

**Файл:** `workspace/s2_core/tests/test_effect_mutation.cpp`

- `TirePuncture_AppliedOnEntry` — агент въезжает в зону: tire_data.fl_ok = false
- `TirePuncture_PersistsAfterExit` — агент выходит: fl_ok остаётся false
- `TirePuncture_AppliedOnce` — агент проезжает зону несколько раз: fl_ok = false, fr_ok не меняется
- `TirePuncture_NoCapability` — агент без "wheeled": TirePunctureData не создаётся
- `TirePuncture_MultipleTires` — affected_tires: ["fl", "rl"]: оба поля false
- `TirePuncture_DiffDrivePenalty` — агент с 2 проколами и cmd_vel=1.0:
  итоговая скорость ≈ 1.0 × 0.7² = 0.49

---

## Критерии завершения

- [ ] TirePunctureData создаётся в SharedState при первом MUTATION
- [ ] Прокол необратим — не исчезает при выходе из зоны
- [ ] DiffDrivePlugin снижает скорость при наличии проколов
- [ ] tire_puncture_count доступен в AgentSnapshot
- [ ] Все тесты проходят в Docker
