# Задача 31 — SENSOR эффекты: FogEffect + EMInterference

## Цель

Зоны могут влиять на сенсоры агентов: туман снижает дальность лидара и
увеличивает его шум, EM-помехи увеличивают шум GPS.

Механизм `sensor_mods`: перед вызовом `plugin.update()` ZoneSystem собирает
все SensorMod из активных эффектов и применяет их к конфигу сенсора.

После задачи:
- Агент в туманной зоне видит лидаром вдвое меньше.
- Агент в зоне EM-помех получает зашумлённую позицию от GPS.

## Зависимости

- Задача 23 (инфраструктура зон, EffectPlugin интерфейс)
- Задача 22 (LidarPlugin, уже реализован)
- Задача по GPS/GNSS-плагину (если реализован) или заглушка для теста

---

## Что сделать

### 1. SensorMod в EffectPlugin

**Файл:** `workspace/s2_core/include/s2/interfaces/effect_plugin.hpp`

```cpp
/// Модификация параметра сенсора.
/// param: строка-ключ параметра (например "range", "noise_stddev")
/// multiplier: умножается на текущее значение
/// addend: прибавляется после умножения
///
/// Итог: new_value = current_value * multiplier + addend
///
/// Правило для noise:
///   - Если current_value == 0.0: new_value = addend (базовое зашумление)
///   - Если current_value > 0.0 и multiplier > 0: применить multiplier
struct SensorMod {
    std::string param;
    double multiplier{1.0};
    double addend{0.0};
};

class EffectPlugin {
public:
    // ... существующие методы ...

    /// Модификации параметров сенсоров для агента в этой зоне.
    /// Вызывается ZoneSystem до sensor plugin.update().
    virtual std::vector<SensorMod> sensor_mods(const EffectContext& ctx) const {
        return {};
    }
};
```

### 2. SensorMod resolver — применение модов

**Файл:** `workspace/s2_core/include/s2/sensor_mod_resolver.hpp` (новый)

```cpp
#pragma once
#include <s2/interfaces/effect_plugin.hpp>
#include <unordered_map>
#include <string>
#include <vector>

namespace s2 {

/// Собирает все SensorMod для одного агента и применяет их к параметрам сенсора.
class SensorModResolver {
public:
    void add(const std::vector<EffectPlugin::SensorMod>& mods) {
        for (const auto& m : mods) {
            accumulated_[m.param].push_back(m);
        }
    }

    /// Применить накопленные модификации к значению параметра.
    /// Multipliers перемножаются; addend'ы суммируются.
    double apply(const std::string& param, double current_value) const {
        auto it = accumulated_.find(param);
        if (it == accumulated_.end()) return current_value;

        double total_mult  = 1.0;
        double total_addend = 0.0;
        for (const auto& m : it->second) {
            total_mult   *= m.multiplier;
            total_addend += m.addend;
        }

        // Правило noise: если текущее значение 0 — применить только addend
        if (current_value == 0.0 && total_addend > 0.0) {
            return total_addend;
        }
        return current_value * total_mult + total_addend;
    }

    void clear() { accumulated_.clear(); }

private:
    std::unordered_map<std::string, std::vector<EffectPlugin::SensorMod>> accumulated_;
};

} // namespace s2
```

### 3. ZoneSystem: сбор SensorMod для агента

**Файл:** `workspace/s2_core/include/s2/zone_system.hpp`

Добавить метод `collect_sensor_mods()`:

```cpp
/// Собрать все SensorMod для агента из активных эффектов зон.
/// Вызывается SimEngine перед обновлением сенсоров.
SensorModResolver collect_sensor_mods(AgentId agent_id, double sim_time, double dt) const {
    SensorModResolver resolver;

    for (const auto& zr : zones_) {
        const Zone& zone = zr.zone;
        if (!zone.enabled) continue;
        if (!zone.inside_agents.count(agent_id)) continue;

        EffectContext ctx{sim_time, dt, zone.id, zone.shape.center,
                          zone.shape.half_size, agent_id, {}};

        for (size_t i = 0; i < zone.effects.size(); ++i) {
            if (!zone.effects[i].enabled) continue;
            if (!zr.plugins[i]) continue;
            resolver.add(zr.plugins[i]->sensor_mods(ctx));
        }
    }

    return resolver;
}
```

### 4. SimEngine: применить SensorMod перед обновлением сенсоров

**Файл:** `workspace/s2_core/include/s2/sim_engine.hpp`

В фазе 3, перед вызовом sensor plugin-ов, добавить:

```cpp
// === 3g. Сенсоры (обновление) ===
// Собрать модификации сенсоров из зон
auto sensor_resolver = zone_system_.collect_sensor_mods(
    agent.id, sim_time_, dt_);

// Передать resolver в агента для использования плагинами
agent.state.set_sensor_resolver(std::move(sensor_resolver));

// Запустить сенсорные плагины
for (auto& plugin : agent.sensor_plugins) {
    plugin->update(dt_, agent);
}
```

### 5. SharedState: хранение SensorModResolver

**Файл:** `workspace/s2_core/include/s2/shared_state.hpp`

```cpp
/// Временный resolver — устанавливается SimEngine перед сенсорной фазой,
/// читается сенсорными плагинами, сбрасывается после фазы.
void set_sensor_resolver(SensorModResolver resolver) {
    sensor_resolver_ = std::move(resolver);
}

/// Применить накопленные моды к параметру сенсора.
double apply_sensor_mod(const std::string& param, double current_value) const {
    return sensor_resolver_.apply(param, current_value);
}

private:
    SensorModResolver sensor_resolver_;
```

### 6. LidarPlugin: читать sensor_mods

**Файл:** `workspace/s2_plugins/include/s2/plugins/lidar_plugin.hpp`

В `update()` применить моды к параметрам:

```cpp
void update(double dt, Agent& agent) override {
    // Читать эффективные параметры
    double effective_range = agent.state.apply_sensor_mod("range", range_);
    double effective_noise = agent.state.apply_sensor_mod("noise_stddev", noise_stddev_);

    // ... остальная логика лидара с effective_range и effective_noise ...
}
```

### 7. FogEffect

**Файл:** `workspace/s2_plugins/effects/fog_effect.hpp` (новый)

```cpp
#pragma once
#include <s2/interfaces/effect_plugin.hpp>

namespace s2::effects {

/// Снижает дальность и увеличивает шум лидара.
/// Требует capability "has_lidar".
///
/// Логика noise:
///   - Если у лидара noise_stddev == 0.0 → ставим base_noise
///   - Если noise_stddev > 0 → умножаем на noise_multiplier
class FogEffect : public EffectPlugin {
public:
    void on_init(const YAML::Node& params) override {
        range_multiplier_  = params["range_multiplier"].as<double>(0.5);
        noise_multiplier_  = params["noise_multiplier"].as<double>(1.5);
        base_noise_        = params["base_noise"].as<double>(0.05);
    }

    EffectType effect_type() const override { return EffectType::SENSOR; }

    std::vector<std::string> required_capabilities() const override {
        return {"has_lidar"};
    }

    std::vector<SensorMod> sensor_mods(const EffectContext& ctx) const override {
        return {
            // Уменьшить дальность: new_range = range * range_multiplier
            SensorMod{"range", range_multiplier_, 0.0},

            // Шум: 0 → base_noise; > 0 → multiply
            // Оба мода применяются через resolver:
            //   если noise == 0: 0 * noise_multiplier + base_noise = base_noise
            //   если noise > 0:  noise * noise_multiplier + 0 = noise * mult
            // (multiplier не применяется к нулю из-за правила resolver'а)
            SensorMod{"noise_stddev", noise_multiplier_, base_noise_},
        };
    }

    std::optional<VisualHint> visual_hint() const override {
        return VisualHint{
            "particles",
            {{"color", "#CCCCCC"}, {"density", 40}, {"speed", 0.1},
             {"direction", {0, 0, 0}}}
        };
    }

private:
    double range_multiplier_{0.5};
    double noise_multiplier_{1.5};
    double base_noise_{0.05};
};

} // namespace s2::effects
```

**Примечание по логике noise resolver'а:**

В `SensorModResolver::apply()` применяем такую логику:

```cpp
if (current_value == 0.0) {
    // Если baseline = 0: применить только addend (base_noise)
    return total_addend;
}
return current_value * total_mult + total_addend;
```

При `current_value = 0.0`, `total_mult = 1.5`, `total_addend = 0.05`:
→ `new_value = 0.05` (базовый шум)

При `current_value = 0.02`, `total_mult = 1.5`, `total_addend = 0.05`:
→ `new_value = 0.02 * 1.5 + 0.05 = 0.08`

### 8. EMInterference

**Файл:** `workspace/s2_plugins/effects/em_interference.hpp` (новый)

```cpp
#pragma once
#include <s2/interfaces/effect_plugin.hpp>

namespace s2::effects {

/// Увеличивает шум позиции от GPS/GNSS.
/// Требует capability "has_gnss".
///
/// Логика noise аналогична FogEffect: 0 → base_noise; > 0 → умножить.
class EMInterference : public EffectPlugin {
public:
    void on_init(const YAML::Node& params) override {
        noise_multiplier_ = params["noise_multiplier"].as<double>(2.0);
        base_noise_       = params["base_noise"].as<double>(0.5);  // метры
    }

    EffectType effect_type() const override { return EffectType::SENSOR; }

    std::vector<std::string> required_capabilities() const override {
        return {"has_gnss"};
    }

    std::vector<SensorMod> sensor_mods(const EffectContext& ctx) const override {
        return {
            SensorMod{"position_noise", noise_multiplier_, base_noise_},
        };
    }

    std::optional<VisualHint> visual_hint() const override {
        return VisualHint{
            "glow",
            {{"color", "#FF6600"}, {"pulse_rate", 5.0}, {"intensity", 0.5}}
        };
    }

private:
    double noise_multiplier_{2.0};
    double base_noise_{0.5};
};

} // namespace s2::effects
```

### 9. Регистрация в фабрике

**Файл:** `workspace/s2_plugins/src/effects_registry.cpp`

```cpp
#include <s2/effects/fog_effect.hpp>
#include <s2/effects/em_interference.hpp>

// В create_effect():
else if (type == "fog")              plugin = std::make_unique<effects::FogEffect>();
else if (type == "em_interference")  plugin = std::make_unique<effects::EMInterference>();
```

### 10. Пример YAML

```yaml
zones:
  # Туманная зона — лидар видит вдвое меньше, шум вырастает
  - id: "fog_zone"
    shape:
      type: aabb
      center: {x: 0.0, y: 10.0, z: 1.0}
      half_size: {x: 5.0, y: 5.0, z: 2.0}
    color: "#CCCCCC"
    opacity: 0.2
    label: "Туман"
    effects:
      - type: fog
        params:
          range_multiplier: 0.4     # 40% от нормальной дальности
          noise_multiplier: 1.5     # шум × 1.5 (или base_noise если был 0)
          base_noise: 0.05          # 5 см базовый шум при noise=0

  # Зона EM-помех — GPS зашумляется
  - id: "em_zone"
    shape:
      type: sphere
      center: {x: -5.0, y: 0.0, z: 0.0}
      radius: 3.0
    color: "#FF6600"
    opacity: 0.15
    label: "EM-помехи"
    effects:
      - type: em_interference
        params:
          noise_multiplier: 3.0
          base_noise: 1.0    # 1 метр базовый шум при GPS noise=0
```

---

## Тесты

**Файл:** `workspace/s2_core/tests/test_sensor_effects.cpp`

- `FogEffect_ReducesRange` — агент с "has_lidar" и range=8.0 в туманной зоне:
  effective_range = 8.0 * range_multiplier
- `FogEffect_BaseNoiseWhenZero` — агент с noise_stddev=0 в тумане:
  effective_noise = base_noise (не 0)
- `FogEffect_MultipliesNonzeroNoise` — агент с noise_stddev=0.02:
  effective_noise = 0.02 * noise_multiplier + base_noise
- `FogEffect_NoCapability` — агент без "has_lidar": range не изменился
- `EMInterference_IncreasesGPSNoise` — агент с "has_gnss": effective_noise увеличился
- `EMInterference_BaseNoiseWhenZero` — position_noise = 0 → base_noise применён
- `SensorModResolver_MultipleZones` — агент в двух туманных зонах:
  multipliers перемножаются
- `SensorModResolver_EmptyMods` — нет зон → apply() возвращает original value

---

## Критерии завершения

- [ ] SensorMod собирается ZoneSystem для агентов в зоне
- [ ] SensorModResolver применяет multiplier и addend правильно
- [ ] Правило noise (0 → addend, > 0 → multiply) реализовано
- [ ] LidarPlugin читает эффективные параметры через apply_sensor_mod()
- [ ] FogEffect снижает дальность и увеличивает шум лидара
- [ ] EMInterference увеличивает шум GPS
- [ ] Эффекты не применяются к агентам без нужного capability
- [ ] Все тесты проходят в Docker
