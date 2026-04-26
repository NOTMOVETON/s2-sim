#pragma once

#include <s2/interfaces/effect_plugin.hpp>
#include <yaml-cpp/yaml.h>

namespace s2 {

/**
 * @brief Эффект тумана — ухудшает дальность оптических сенсоров (per D-07, D-08).
 *
 * required_capabilities: [optical_sensor]
 * Масштабирует max_range по zone_strength:
 *   multiplier = range_multiplier + (1 - range_multiplier) * (1 - zone_strength)
 *
 * YAML параметры:
 *   range_multiplier: 0.3  # Минимальный множитель при zone_strength=1.0
 */
class FogEffect : public EffectPlugin
{
public:
    void on_init(const YAML::Node& params) override
    {
        range_multiplier_ = params["range_multiplier"].as<double>(0.3);
    }

    EffectType effect_type() const override { return EffectType::SENSOR; }

    std::vector<std::string> required_capabilities() const override
    {
        return {"optical_sensor"};
    }

    std::vector<SensorMod> sensor_mods(const EffectContext& ctx) const override
    {
        double mult = range_multiplier_ + (1.0 - range_multiplier_) * (1.0 - ctx.zone_strength);
        return {{.param = "max_range", .multiplier = mult, .addend = 0.0}};
    }

    std::optional<VisualHint> visual_hint() const override
    {
        return VisualHint{
            "glow",
            {{"color", "#AADDFF"}, {"pulse_rate", 0.5}, {"intensity", 0.4}}
        };
    }

private:
    double range_multiplier_{0.3};
};

} // namespace s2
