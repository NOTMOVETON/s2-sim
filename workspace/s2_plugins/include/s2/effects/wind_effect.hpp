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
