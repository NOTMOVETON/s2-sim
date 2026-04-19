#pragma once

#include <s2/interfaces/effect_plugin.hpp>
#include <s2/types.hpp>
#include <yaml-cpp/yaml.h>
#include <memory>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

namespace s2 {

/**
 * @brief Описание зоны с набором эффектов.
 *
 * Зона имеет геометрическую форму (ZoneShape) и список эффектов.
 * Каждый тик ZoneSystem проверяет, какие агенты находятся внутри зоны,
 * и применяет соответствующие эффекты.
 *
 * Zone — move-only: содержит unique_ptr<EffectPlugin> в EffectDesc.
 */
struct Zone
{
    ZoneId id;
    bool enabled{true};

    ZoneShape shape;
    std::string detection_mode{"center"};  ///< "center" | "bounding" (bounding — TODO)

    /// Привязка зоны к актору или агенту (зона следует за объектом).
    std::optional<ActorId>  attached_to_actor;
    std::optional<AgentId>  attached_to_agent;
    Vec3 attachment_offset{Vec3::Zero()};

    /// Визуальные параметры для отображения в визуализаторе.
    std::string color{"#4488FF"};
    double opacity{0.3};
    bool visible{true};
    std::string label;

    // ── Описание одного эффекта в зоне ──────────────────────────────────────

    struct EffectDesc
    {
        std::string type;                           ///< "ice_modifier", "charging", ...
        bool enabled{true};
        EffectType effect_type{EffectType::MODIFIER};
        std::vector<std::string> required_capabilities;
        YAML::Node params;                          ///< Параметры для on_init()

        std::unique_ptr<EffectPlugin> plugin;       ///< Создаётся фабрикой в ZoneSystem

        // EffectDesc — move-only из-за unique_ptr
        EffectDesc() = default;
        EffectDesc(const EffectDesc&) = delete;
        EffectDesc& operator=(const EffectDesc&) = delete;
        EffectDesc(EffectDesc&&) = default;
        EffectDesc& operator=(EffectDesc&&) = default;
    };

    std::vector<EffectDesc> effects;

    /// Агенты, находящиеся внутри в текущий момент.
    std::unordered_set<AgentId> inside_agents;
};

} // namespace s2
