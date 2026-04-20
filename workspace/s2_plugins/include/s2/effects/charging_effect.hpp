#pragma once
#include <s2/interfaces/effect_plugin.hpp>
#include <s2/components/battery_component.hpp>
#include <algorithm>

namespace s2::effects {

/// Заряжает батарею агента пока он находится в зоне.
/// Требует capability "has_battery" и BatteryComponent в SharedState.
///
/// Алгоритм CONTINUOUS (вызывается каждый тик пока агент внутри):
///   battery.level = min(1.0, battery.level + charge_rate × dt)
///   battery.charging = true
///
/// При выходе из зоны battery.charging сбрасывается в false
/// (обрабатывается в ZoneSystem::on_agent_exit).
class ChargingEffect : public EffectPlugin {
public:
    void on_init(const YAML::Node& params) override {
        charge_rate_ = params["charge_rate"].as<double>(0.1);
    }

    EffectType effect_type() const override { return EffectType::CONTINUOUS; }

    std::vector<std::string> required_capabilities() const override {
        return {"has_battery"};
    }

    void apply_continuous(SharedState& state, const EffectContext& ctx) override {
        auto* bat = state.get<BatteryComponent>();
        if (!bat) {
            bat = &state.emplace<BatteryComponent>();
        }
        bat->level = std::min(1.0, bat->level + charge_rate_ * ctx.dt);
        bat->charging = true;
    }

    void on_agent_exit(SharedState& state, const EffectContext& /*ctx*/) override {
        if (auto* bat = state.get<BatteryComponent>()) {
            bat->charging = false;
        }
    }

    std::optional<VisualHint> visual_hint() const override {
        return VisualHint{
            "glow",
            {{"color", "#FFDD44"}, {"pulse_rate", 2.0}, {"intensity", 0.8}}
        };
    }

private:
    double charge_rate_{0.1};
};

} // namespace s2::effects
