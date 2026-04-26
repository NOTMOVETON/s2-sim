#pragma once

#include <s2/interfaces/effect_plugin.hpp>
#include <yaml-cpp/yaml.h>

namespace s2 {

/**
 * @brief ЭМ-помехи — добавляет шум GNSS/IMU сенсорам (per D-07, D-08).
 *
 * required_capabilities: [gnss_sensor, imu_sensor]
 * Добавляет noise_addend * zone_strength к noise_std.
 *
 * YAML параметры:
 *   noise_addend: 0.5  # Добавка шума при zone_strength=1.0
 */
class EMIEffect : public EffectPlugin
{
public:
    void on_init(const YAML::Node& params) override
    {
        noise_addend_ = params["noise_addend"].as<double>(0.5);
    }

    EffectType effect_type() const override { return EffectType::SENSOR; }

    std::vector<std::string> required_capabilities() const override
    {
        return {"gnss_sensor", "imu_sensor"};
    }

    std::vector<SensorMod> sensor_mods(const EffectContext& ctx) const override
    {
        double scaled = noise_addend_ * ctx.zone_strength;
        return {{.param = "noise_std", .multiplier = 1.0, .addend = scaled}};
    }

    std::optional<VisualHint> visual_hint() const override
    {
        return VisualHint{
            "glow",
            {{"color", "#FFAA22"}, {"pulse_rate", 2.0}, {"intensity", 0.7}}
        };
    }

private:
    double noise_addend_{0.5};
};

} // namespace s2
