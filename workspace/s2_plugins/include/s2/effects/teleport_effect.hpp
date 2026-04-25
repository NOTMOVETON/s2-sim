#pragma once
#include <s2/interfaces/effect_plugin.hpp>
#include <s2/components/pending_teleport.hpp>
#include <unordered_map>
#include <unordered_set>

namespace s2::effects {

/// Телепортирует агентов при выполнении условия триггера.
///
/// trigger_mode:
///   "immediate"     — сразу при входе в зону (MUTATION, однократно)
///   "after_seconds" — после N секунд нахождения внутри (CONTINUOUS, таймер)
///   "on_command"    — по явной команде (CONTINUOUS, внешний триггер — задача 35)
///
/// target_mode:
///   "fixed"   — destination фиксирован в конфиге
///   "runtime" — destination задаётся через SetZoneTeleportTargetCommand (задача 29)
///
/// target_entity: "agent" | "prop" | "all"
///   Фильтрация по типу сущности. "prop" обрабатывается в задаче 35.
class TeleportEffect : public EffectPlugin {
public:
    void on_init(const YAML::Node& params) override {
        trigger_mode_  = params["trigger_mode"].as<std::string>("immediate");
        target_mode_   = params["target_mode"].as<std::string>("fixed");
        target_entity_ = params["target_entity"].as<std::string>("agent");
        trigger_secs_  = params["after_seconds"].as<double>(3.0);

        if (params["destination"]) {
            const auto& d = params["destination"];
            fixed_destination_.x() = d["x"].as<double>(0.0);
            fixed_destination_.y() = d["y"].as<double>(0.0);
            fixed_destination_.z() = d["z"].as<double>(0.0);
            fixed_yaw_   = d["yaw"].as<double>(0.0);
            has_fixed_   = true;
        }
    }

    EffectType effect_type() const override {
        // "immediate" — MUTATION: вызывается однократно при входе в зону.
        // Остальные режимы — CONTINUOUS: вызывается каждый тик.
        return (trigger_mode_ == "immediate") ? EffectType::MUTATION
                                              : EffectType::CONTINUOUS;
    }

    std::vector<std::string> required_capabilities() const override {
        return {};
    }

    /// Вызывается для trigger_mode = "immediate" (MUTATION — только при входе).
    void apply_mutation(SharedState& state, const EffectContext& ctx) override {
        if (!should_teleport_entity()) return;
        do_teleport(state, ctx);
    }

    /// Вызывается каждый тик для trigger_mode = "after_seconds" или "on_command".
    void apply_continuous(SharedState& state, const EffectContext& ctx) override {
        if (trigger_mode_ != "after_seconds") return;

        time_inside_[ctx.agent_id] += ctx.dt;
        if (time_inside_[ctx.agent_id] >= trigger_secs_) {
            time_inside_[ctx.agent_id] = 0.0;
            if (teleported_.count(ctx.agent_id) == 0) {
                teleported_.insert(ctx.agent_id);
                do_teleport(state, ctx);
            }
        }
    }

    /// Вызывается ZoneSystem при выходе агента из зоны.
    /// Сбрасывает таймер и флаг телепортации, чтобы агент мог телепортироваться снова.
    void on_agent_exit(SharedState& /*state*/, const EffectContext& ctx) override {
        time_inside_.erase(ctx.agent_id);
        teleported_.erase(ctx.agent_id);
    }

    std::optional<VisualHint> visual_hint() const override {
        return VisualHint{
            "glow",
            {{"color", "#CC44FF"}, {"pulse_rate", 3.0}, {"intensity", 1.0}}
        };
    }

private:
    /// Проверяет, должна ли данная сущность телепортироваться.
    /// "prop" обрабатывается в задаче 35 — пока только "agent" и "all".
    bool should_teleport_entity() const {
        return target_entity_ == "agent" || target_entity_ == "all";
    }

    /// Устанавливает PendingTeleport в SharedState агента.
    /// SimEngine применит его в фазе 3m текущего или следующего тика.
    void do_teleport(SharedState& state, const EffectContext& /*ctx*/) {
        Vec3 dest = has_fixed_ ? fixed_destination_ : runtime_destination_;
        double yaw = has_fixed_ ? fixed_yaw_ : runtime_yaw_;
        state.emplace<PendingTeleport>(PendingTeleport{dest, yaw, true});
    }

    std::string trigger_mode_{"immediate"};
    std::string target_mode_{"fixed"};
    std::string target_entity_{"agent"};
    double trigger_secs_{3.0};

    Vec3 fixed_destination_{Vec3::Zero()};
    double fixed_yaw_{0.0};
    bool has_fixed_{false};

    // Runtime destination (устанавливается через SetZoneTeleportTargetCommand, задача 29)
    Vec3 runtime_destination_{Vec3::Zero()};
    double runtime_yaw_{0.0};

    // Состояние на агента (per-agent state для CONTINUOUS режима)
    std::unordered_map<AgentId, double> time_inside_;
    std::unordered_set<AgentId>         teleported_;
};

} // namespace s2::effects
