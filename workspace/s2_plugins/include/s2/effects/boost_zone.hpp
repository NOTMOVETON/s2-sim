#pragma once

#include <s2/interfaces/effect_plugin.hpp>

namespace s2::effects {

/// Ускоряет агентов с capability "surface_contact".
/// Публикует add_scale(speed_multiplier, "boost_zone_<id>").
/// speed_multiplier > 1.0 даёт ускорение; финальная скорость ограничена
/// аппаратным лимитом DiffDrivePlugin.
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
