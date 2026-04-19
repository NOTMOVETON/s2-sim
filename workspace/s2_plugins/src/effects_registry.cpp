#include <s2/effects_registry.hpp>
#include <s2/effects/ice_modifier.hpp>
#include <s2/effects/boost_zone.hpp>
#include <s2/effects/motion_lock_zone.hpp>
#include <s2/effects/conveyor_effect.hpp>
#include <s2/effects/wind_effect.hpp>

namespace s2 {

std::unique_ptr<EffectPlugin> create_effect(
    const std::string& type, const YAML::Node& params)
{
    std::unique_ptr<EffectPlugin> plugin;

    if      (type == "ice_modifier") plugin = std::make_unique<effects::IceModifier>();
    else if (type == "boost_zone")   plugin = std::make_unique<effects::BoostZone>();
    else if (type == "motion_lock")  plugin = std::make_unique<effects::MotionLockZone>();
    else if (type == "conveyor")     plugin = std::make_unique<effects::ConveyorEffect>();
    else if (type == "wind")         plugin = std::make_unique<effects::WindEffect>();
    // Новые эффекты добавляются здесь по мере реализации задач 26–29.

    if (plugin) {
        plugin->on_init(params);
    }

    return plugin;
}

} // namespace s2
