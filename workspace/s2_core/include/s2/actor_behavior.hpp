#pragma once

/**
 * @file actor_behavior.hpp
 * IActorBehavior — интерфейс поведения актора (Phase 5).
 *
 * Конкретные реализации (DoorBehavior, ConveyorBehavior и др.) добавляются в Phase 15.
 * WorldContext сейчас — stub; полный вариант (с WorldQuery) появится в Phase 9.
 */

#include <s2/world_context.hpp>
#include <s2/types.hpp>
#include <nlohmann/json.hpp>
#include <yaml-cpp/yaml.h>

#include <string>

namespace s2 {

struct Actor; // полное определение в entity.hpp

struct SignalEvent {}; // Phase 14 stub

class IActorBehavior {
public:
    virtual ~IActorBehavior() = default;

    // ── Обязательные методы ────────────────────────────────────────────────
    virtual std::string type() const = 0;
    virtual void on_init(const YAML::Node& config) = 0;
    virtual void on_spawn(Actor& actor) = 0;
    virtual void on_reset() = 0;
    virtual void update(double dt, Actor& actor, WorldContext& ctx) = 0;
    virtual std::string current_state() const = 0;
    virtual nlohmann::json to_json() const = 0;

    // ── Опциональные методы (default no-op) ───────────────────────────────
    virtual void on_signal(const SignalEvent& /*event*/) {}
    virtual void on_interact(EntityId /*requester*/,
                             const std::string& /*action*/,
                             const nlohmann::json& /*params*/) {}

    // ── Phase 20: Material system stubs ───────────────────────────────────
    virtual bool can_release_material() const { return false; }
    virtual bool can_accept_material() const { return false; }
    virtual void release_material() {}
    virtual void accept_material() {}
    virtual bool is_deformable() const { return false; }
    virtual void apply_deformation() {}
};

} // namespace s2
