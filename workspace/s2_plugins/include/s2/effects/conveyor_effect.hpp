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
