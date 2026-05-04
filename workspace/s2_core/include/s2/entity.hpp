#pragma once

/**
 * @file entity.hpp
 * Unified Entity model — Phase 2.
 *
 * Agent/Actor/Prop — flat structs с расширенным набором полей.
 * Наследование от EntityBase намеренно не используется: C++20 designated
 * initializers требуют, чтобы поля были прямыми членами структуры.
 * Полиморфизм через EntityBase вводится в Phase 9 (WorldQuery).
 *
 * Все три типа содержат одинаковый набор "базовых" полей:
 *   id, entity_type, name, world_pose, collision, visual,
 *   tags, immune_to_effects, signals, enabled, owned_zones.
 * Agent дополнительно хранит: плагины, SharedState, кинематику.
 * Prop структурно НЕ имеет SharedState.
 */

#include <s2/types.hpp>
#include <s2/shared_state.hpp>
#include <s2/kinematic_tree.hpp>
#include <s2/plugin_base.hpp>
#include <s2/actor_behavior.hpp>

#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace s2
{

/**
 * @brief Управляемый агент (робот).
 *
 * Поля сохранены в исходном порядке для совместимости с designated initializers.
 * Новые поля Phase 2 добавлены в конец.
 */
struct Agent
{
    // ── Оригинальные поля (порядок сохранён для designated init) ───────────
    AgentId     id{0};
    std::string name;
    int         domain_id{0};          // backward-compat; Phase 8 → TransportLink
    Pose3D      world_pose;
    Velocity    world_velocity;
    SharedState state;
    std::unordered_set<std::string> capabilities;
    std::vector<std::unique_ptr<plugins::IAgentPlugin>> plugins;
    bool        has_collision{false};
    double      max_slope_rad{0.0};
    double      max_step_height{0.0};
    CollisionShape bounding;           // физическая коллизия (CollisionSystem)
    VisualDesc  visual;
    std::unique_ptr<KinematicTree> kinematic_tree;

    // ── Phase 2 новые поля ──────────────────────────────────────────────────
    EntityType  entity_type{EntityType::AGENT};
    CollisionShape collision;          // форма для zone-detection; bounding — для физики
    std::map<std::string, std::string> tags;
    std::unordered_set<std::string>   immune_to_effects;
    std::vector<ZoneId> signals;       // заглушка Phase 14
    bool         enabled{true};
    std::vector<ZoneId> owned_zones;   // заглушка Phase 12
};

/// Alias для именования в духе CONTEXT.md.
using AgentData = Agent;

/**
 * @brief Активный неагентный объект (дверь, конвейер, лифт).
 *
 * Поля сохранены в исходном порядке для совместимости с designated initializers.
 * BehaviorSlot добавляется в Phase 5.
 */
struct Actor
{
    // ── Оригинальные поля ──────────────────────────────────────────────────
    ActorId     id{0};
    std::string name;
    Pose3D      world_pose;
    ActorState  current_state;
    CollisionShape collision;
    VisualDesc  visual;

    // ── Phase 2 новые поля ──────────────────────────────────────────────────
    EntityType  entity_type{EntityType::ACTOR};
    std::unordered_set<std::string> capabilities;
    std::map<std::string, std::string> tags;
    std::unordered_set<std::string>   immune_to_effects;
    std::vector<ZoneId> signals;       // заглушка Phase 14
    bool         enabled{true};
    std::vector<ZoneId> owned_zones;   // заглушка Phase 12

    // Phase 5: BehaviorSlot
    std::unique_ptr<IActorBehavior> behavior;
};

using ActorData = Actor;

/**
 * @brief Пассивный объект (ящик, бочка, паллета).
 *
 * Структурно НЕ имеет SharedState — это enforcement через отсутствие поля.
 * Поля сохранены в исходном порядке для совместимости с designated initializers.
 */
struct Prop
{
    // ── Оригинальные поля ──────────────────────────────────────────────────
    ObjectId    id{0};
    std::string type;          // вид объекта: "barrel", "crate" (≠ entity_type enum)
    Pose3D      world_pose;
    bool        movable{false};
    CollisionShape collision;
    VisualDesc  visual;
    std::unordered_map<std::string, std::string> properties;

    // ── Phase 2 новые поля ──────────────────────────────────────────────────
    std::string name;          // человекочитаемое имя (у Prop не было)
    EntityType  entity_type{EntityType::PROP};
    std::unordered_set<std::string> capabilities;
    std::map<std::string, std::string> tags;
    std::unordered_set<std::string>   immune_to_effects;
    std::vector<ZoneId> signals;       // заглушка Phase 14
    bool         enabled{true};
    std::vector<ZoneId> owned_zones;   // заглушка Phase 12
    // NO SharedState — структурное ограничение: поля нет
};

using PropData = Prop;

} // namespace s2
