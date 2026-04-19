#pragma once

#include <s2/interfaces/effect_plugin.hpp>
#include <cmath>
#include <algorithm>

namespace s2::effects {

/// Замедляет агентов с capability "surface_contact".
/// Публикует add_scale(traction_coefficient, "ice_zone_<id>").
/// Поддерживает noise_amplitude для рандомизированного скольжения.
class IceModifier : public EffectPlugin {
public:
    void on_init(const YAML::Node& params) override {
        traction_coeff_  = params["traction_coefficient"].as<double>(0.2);
        noise_amplitude_ = params["noise_amplitude"].as<double>(0.0);
    }

    EffectType effect_type() const override { return EffectType::MODIFIER; }

    std::vector<std::string> required_capabilities() const override {
        return {"surface_contact"};
    }

    void apply_modifier(SharedState& state, const EffectContext& ctx) override {
        double scale = traction_coeff_;

        if (noise_amplitude_ > 0.0) {
            // Детерминированный псевдо-шум через время симуляции и ID агента.
            // sin(time * freq + agent_id * offset) даёт стабильное скольжение без random_device.
            double noise = noise_amplitude_ *
                std::sin(ctx.sim_time * 7.3 + static_cast<double>(ctx.agent_id) * 1.7);
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
