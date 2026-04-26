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

/// Режим обнаружения агента в зоне (per D-12).
enum class DetectionMode {
    CENTER,    ///< Центр Entity (текущее поведение по умолчанию)
    BOUNDING,  ///< Bounding shape Entity пересекает форму зоны
    PER_LINK   ///< Каждый линк kinematic_tree проверяется отдельно
};

/// Политика самоуничтожения зоны (per D-13).
struct SelfDestructPolicy {
    enum class Type { NONE, ON_ANY_CONTACT, ON_EFFECT_APPLIED };
    Type type{Type::NONE};
};

/// Параметры lifecycle зоны (per D-09, D-10).
struct ZoneLifecycle {
    double initial_strength{1.0};  ///< Начальная сила при спавне (0.0–1.0)
    double growth_rate{0.0};       ///< Прирост strength/сек (0 = нет роста)
    double max_strength{1.0};      ///< Потолок роста
    double decay_delay{0.0};       ///< Секунд до начала затухания
    double decay_rate{0.0};        ///< Убыль strength/сек (0 = нет затухания)
    double remove_threshold{0.05}; ///< Удалить зону если strength < этого значения
};

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
    std::string detection_mode{"center"};  ///< @deprecated Используйте detection_mode_enum

    /// Привязка зоны к актору или агенту (зона следует за объектом).
    std::optional<ActorId>  attached_to_actor;
    std::optional<AgentId>  attached_to_agent;
    Vec3 attachment_offset{Vec3::Zero()};

    /// Визуальные параметры для отображения в визуализаторе.
    std::string color{"#4488FF"};
    double opacity{0.3};
    bool visible{true};
    std::string label;

    // ── Lifecycle (ZONE-04) ──────────────────────────────────────────────────
    double strength{1.0};              ///< Текущая сила зоны (0.0–1.0)
    ZoneLifecycle lifecycle;           ///< Параметры роста/затухания

    // ── Detection mode (ZONE-06) ─────────────────────────────────────────────
    DetectionMode detection_mode_enum{DetectionMode::CENTER};

    // ── Self-destruct (ZONE-07) ──────────────────────────────────────────────
    SelfDestructPolicy self_destruct;

    // ── Owned zones / per-link attachment (ZONE-09) ──────────────────────────
    std::string attached_to_entity_id; ///< Generic EntityId для owned_zones
    std::optional<std::string> attached_to_link; ///< Линк для per-link attachment

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
