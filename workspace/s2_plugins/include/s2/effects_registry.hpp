#pragma once

#include <s2/interfaces/effect_plugin.hpp>
#include <yaml-cpp/yaml.h>
#include <memory>
#include <string>

namespace s2 {

/// Фабрика эффектов зон.
/// Создаёт конкретный EffectPlugin по строковому имени типа и параметрам.
/// Вызывается ZoneSystem::add_zone() при загрузке сцены.
///
/// Зарегистрированные типы:
///   "ice_modifier"  — IceModifier (замедление через traction_coefficient)
///   "boost_zone"    — BoostZone (ускорение через speed_multiplier)
///   "motion_lock"   — MotionLockZone (блокировка движения)
std::unique_ptr<EffectPlugin> create_effect(
    const std::string& type, const YAML::Node& params);

} // namespace s2
