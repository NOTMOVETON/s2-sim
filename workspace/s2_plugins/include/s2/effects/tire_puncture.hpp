#pragma once
#include <s2/interfaces/effect_plugin.hpp>
#include <s2/components/tire_puncture_data.hpp>

namespace s2::effects {

/// Прокалывает шины агента при входе в зону.
/// Требует capability "wheeled".
/// Необратимо — состояние сохраняется после выхода из зоны.
class TirePunctureEffect : public EffectPlugin {
public:
    void on_init(const YAML::Node& /*params*/) override {}

    EffectType effect_type() const override { return EffectType::MUTATION; }

    std::vector<std::string> required_capabilities() const override {
        return {"wheeled"};
    }

    void apply_mutation(SharedState& state, const EffectContext& /*ctx*/) override {
        auto* data = state.get<TirePunctureData>();
        if (!data) {
            data = &state.emplace<TirePunctureData>();
        }
        data->punctured = true;
    }

    std::optional<VisualHint> visual_hint() const override {
        return VisualHint{
            "grid",
            {{"color", "#AA2200"}, {"spacing", 0.3}, {"line_width", 1.0}}
        };
    }
};

} // namespace s2::effects
