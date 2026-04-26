#include <s2/effects_registry.hpp>
#include <s2/effects/ice_modifier.hpp>
#include <s2/effects/boost_zone.hpp>
#include <s2/effects/motion_lock_zone.hpp>
#include <s2/effects/conveyor_effect.hpp>
#include <s2/effects/wind_effect.hpp>
#include <s2/effects/charging_effect.hpp>
#include <s2/effects/tire_puncture.hpp>
#include <s2/effects/teleport_effect.hpp>
#include <s2/effects/fog_effect.hpp>
#include <s2/effects/emi_effect.hpp>

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
    else if (type == "charging")      plugin = std::make_unique<effects::ChargingEffect>();
    else if (type == "tire_puncture") plugin = std::make_unique<effects::TirePunctureEffect>();
    else if (type == "teleport")      plugin = std::make_unique<effects::TeleportEffect>();
    else if (type == "fog")           plugin = std::make_unique<FogEffect>();
    else if (type == "emi")           plugin = std::make_unique<EMIEffect>();
    // Новые эффекты добавляются здесь по мере реализации задач 29+.

    if (plugin) {
        plugin->on_init(params);
    }

    return plugin;
}

} // namespace s2
