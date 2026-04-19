#pragma once

#include <s2/effect_context.hpp>
#include <s2/shared_state.hpp>
#include <s2/types.hpp>
#include <yaml-cpp/yaml.h>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <vector>

namespace s2 {

/**
 * @brief Интерфейс плагина эффекта зоны.
 *
 * Конкретные эффекты (IceModifier, ChargingEffect, ConveyorEffect и т.д.)
 * наследуются от этого класса и переопределяют нужные методы.
 *
 * Жизненный цикл плагина:
 *  1. Создаётся фабрикой эффектов (EffectFactory) при загрузке сцены.
 *  2. on_init() вызывается один раз — инициализация из YAML-параметров.
 *  3. Каждый тик, пока агент внутри зоны:
 *     - apply_modifier()   — для MODIFIER-эффектов (лёд, буст)
 *     - apply_continuous() — для CONTINUOUS-эффектов (зарядка)
 *  4. При входе агента в зону:
 *     - apply_mutation()   — для MUTATION-эффектов (прокол колеса)
 *  5. sensor_mods() — запрашивается перед обновлением сенсоров агента (задача 31).
 *  6. visual_hint() — запрашивается визуализатором для анимации зоны (задача 30).
 */
class EffectPlugin
{
public:
    virtual ~EffectPlugin() = default;

    /// Инициализация из YAML-параметров зоны.
    /// Вызывается один раз после создания плагина.
    virtual void on_init(const YAML::Node& params) = 0;

    /// Тип эффекта (MODIFIER / CONTINUOUS / MUTATION / SENSOR).
    virtual EffectType effect_type() const = 0;

    /// Capabilities, которые должны быть у агента для применения эффекта.
    /// Пустой список — применяется ко всем агентам.
    virtual std::vector<std::string> required_capabilities() const { return {}; }

    // ── Методы применения эффектов ──────────────────────────────────────────

    /// MODIFIER: публикует contribution в SharedState (add_scale / add_lock / add_velocity_addition).
    /// Вызывается каждый тик пока агент в зоне.
    virtual void apply_modifier(SharedState& /*state*/, const EffectContext& /*ctx*/) {}

    /// CONTINUOUS: напрямую изменяет single-owner поле в SharedState (например, заряд батареи).
    /// Вызывается каждый тик пока агент в зоне.
    virtual void apply_continuous(SharedState& /*state*/, const EffectContext& /*ctx*/) {}

    /// MUTATION: однократное необратимое воздействие при входе в зону (прокол колеса).
    /// Вызывается один раз при входе. Состояние сохраняется при выходе.
    virtual void apply_mutation(SharedState& /*state*/, const EffectContext& /*ctx*/) {}

    // ── SENSOR-эффекты (задача 31) ──────────────────────────────────────────

    /// Описание модификации параметра сенсора.
    struct SensorMod
    {
        std::string param;       ///< Имя параметра: "max_range", "noise_std"
        double multiplier{1.0};  ///< Множитель (применяется к текущему значению)
        double addend{0.0};      ///< Слагаемое (добавляется после умножения)
    };

    /// SENSOR: список модификаций параметров сенсоров.
    /// Применяется перед вызовом sensor->update() (задача 31).
    virtual std::vector<SensorMod> sensor_mods(const EffectContext& /*ctx*/) const { return {}; }

    // ── Визуальные подсказки (задача 30) ────────────────────────────────────

    /// Подсказка для визуализатора: как анимировать зону.
    struct VisualHint
    {
        std::string type;          ///< "arrows", "particles", "glow", "grid"
        nlohmann::json params;     ///< {"direction":[1,0,0], "speed":2.0, "color":"#F60"}
    };

    /// Опциональная подсказка для визуализатора.
    virtual std::optional<VisualHint> visual_hint() const { return std::nullopt; }
};

} // namespace s2
