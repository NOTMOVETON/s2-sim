#pragma once

#include <s2/interfaces/effect_plugin.hpp>

namespace s2::effects {

/// Блокирует движение любого агента в зоне (add_lock).
/// Применяется ко всем агентам без ограничения по capabilities.
/// Используется для запретных зон и опасных областей.
class MotionLockZone : public EffectPlugin {
public:
    void on_init(const YAML::Node& params) override {
        source_label_ = params["source_label"].as<std::string>("forbidden_zone");
    }

    EffectType effect_type() const override { return EffectType::MODIFIER; }

    std::vector<std::string> required_capabilities() const override {
        return {};  // применяется ко всем агентам
    }

    void apply_modifier(SharedState& state, const EffectContext& ctx) override {
        state.add_lock(true, source_label_ + "_" + ctx.zone_id);
    }

    std::optional<VisualHint> visual_hint() const override {
        return VisualHint{
            "grid",
            {{"color", "#FF2222"}, {"line_width", 2.0}, {"spacing", 0.5}}
        };
    }

private:
    std::string source_label_{"forbidden_zone"};
};

} // namespace s2::effects
