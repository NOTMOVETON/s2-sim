#include <s2/zone_system.hpp>
#include <algorithm>

namespace s2 {

void ZoneSystem::set_effect_factory(EffectFactory factory)
{
    effect_factory_ = std::move(factory);
}

void ZoneSystem::add_zone(Zone zone)
{
    // Если фабрика задана — создать плагины для эффектов, у которых plugin == nullptr
    if (effect_factory_) {
        for (auto& desc : zone.effects) {
            if (!desc.plugin && !desc.type.empty()) {
                desc.plugin = effect_factory_(desc.type, desc.params);
                if (desc.plugin) {
                    desc.plugin->on_init(desc.params);
                    // effect_type всегда берётся из плагина
                    desc.effect_type = desc.plugin->effect_type();
                    // required_capabilities: YAML имеет приоритет над дефолтом плагина.
                    // Если YAML не указал capabilities (пустой список) — берём из плагина.
                    if (desc.required_capabilities.empty()) {
                        desc.required_capabilities = desc.plugin->required_capabilities();
                    }
                }
            }
        }
    }
    zones_.push_back(std::move(zone));
}

void ZoneSystem::tick(SimWorld& world, SimBus& bus, double sim_time, double dt)
{
    tick(world.agents(), world.actors(), bus, sim_time, dt);
}

void ZoneSystem::tick(
    std::vector<Agent>& agents,
    const std::vector<Actor>& actors,
    SimBus& bus,
    double sim_time,
    double dt)
{

    // === Шаг 1: Обновить позиции attached-зон ===
    for (auto& zone : zones_) {
        if (zone.attached_to_actor.has_value()) {
            ActorId actor_id = zone.attached_to_actor.value();
            for (const auto& actor : actors) {
                if (actor.id == actor_id) {
                    zone.shape.center = actor.world_pose.position() + zone.attachment_offset;
                    break;
                }
            }
        } else if (zone.attached_to_agent.has_value()) {
            AgentId agent_id = zone.attached_to_agent.value();
            for (const auto& agent : agents) {
                if (agent.id == agent_id) {
                    zone.shape.center = agent.world_pose.position() + zone.attachment_offset;
                    break;
                }
            }
        }
    }

    // === Шаг 2: Проверить enter/exit для каждой зоны ===
    for (auto& zone : zones_) {
        // Если зона выключена — выгнать всех агентов из inside_agents
        if (!zone.enabled) {
            std::vector<AgentId> to_exit(zone.inside_agents.begin(), zone.inside_agents.end());
            for (AgentId aid : to_exit) {
                for (auto& agent : agents) {
                    if (agent.id == aid) {
                        zone.inside_agents.erase(aid);
                        on_agent_exit(agent, zone, bus);
                        break;
                    }
                }
            }
            continue;
        }

        for (auto& agent : agents) {
            Vec3 point = detection_point(agent, zone.detection_mode);
            bool is_inside  = zone.shape.contains(point);
            bool was_inside = zone.inside_agents.count(agent.id) > 0;

            if (is_inside && !was_inside) {
                zone.inside_agents.insert(agent.id);
                on_agent_enter(agent, zone, bus, sim_time, dt);
            } else if (!is_inside && was_inside) {
                zone.inside_agents.erase(agent.id);
                on_agent_exit(agent, zone, bus);
            }
        }
    }

    // === Шаг 3: Применить активные эффекты ===
    for (auto& zone : zones_) {
        if (!zone.enabled) continue;
        for (AgentId aid : zone.inside_agents) {
            for (auto& agent : agents) {
                if (agent.id == aid) {
                    apply_active_effects(agent, zone, sim_time, dt);
                    break;
                }
            }
        }
    }
}

bool ZoneSystem::resize_zone(const ZoneId& id, const ZoneShape& new_shape)
{
    for (auto& zone : zones_) {
        if (zone.id == id) {
            zone.shape = new_shape;
            return true;
        }
    }
    return false;
}

bool ZoneSystem::move_zone(const ZoneId& id, const Vec3& new_center)
{
    for (auto& zone : zones_) {
        if (zone.id == id) {
            // Только для не-attached зон
            if (!zone.attached_to_actor.has_value() && !zone.attached_to_agent.has_value()) {
                zone.shape.center = new_center;
            }
            return true;
        }
    }
    return false;
}

bool ZoneSystem::attach_zone_to_actor(const ZoneId& id, ActorId actor_id, const Vec3& offset)
{
    for (auto& zone : zones_) {
        if (zone.id == id) {
            zone.attached_to_actor  = actor_id;
            zone.attached_to_agent  = std::nullopt;
            zone.attachment_offset  = offset;
            return true;
        }
    }
    return false;
}

bool ZoneSystem::toggle_zone(const ZoneId& id, bool enabled)
{
    for (auto& zone : zones_) {
        if (zone.id == id) {
            zone.enabled = enabled;
            return true;
        }
    }
    return false;
}

bool ZoneSystem::toggle_effect(const ZoneId& id, size_t effect_idx, bool enabled)
{
    for (auto& zone : zones_) {
        if (zone.id == id) {
            if (effect_idx < zone.effects.size()) {
                zone.effects[effect_idx].enabled = enabled;
                return true;
            }
            return false;
        }
    }
    return false;
}

std::vector<ZoneId> ZoneSystem::zones_containing(const Vec3& point) const
{
    std::vector<ZoneId> result;
    for (const auto& zone : zones_) {
        if (zone.enabled && zone.shape.contains(point)) {
            result.push_back(zone.id);
        }
    }
    return result;
}

Vec3 ZoneSystem::detection_point(const Agent& agent, const std::string& mode)
{
    // "bounding" — TODO: bounding overlap в будущей задаче. Fallback на center.
    (void)mode;
    return agent.world_pose.position();
}

bool ZoneSystem::capabilities_match(const Agent& agent,
                                    const std::vector<std::string>& required)
{
    if (required.empty()) return true;
    for (const auto& cap : required) {
        if (agent.capabilities.count(cap) == 0) return false;
    }
    return true;
}

void ZoneSystem::on_agent_enter(Agent& agent, Zone& zone, SimBus& bus,
                                double sim_time, double dt)
{
    bus.publish(event::AgentEnteredZone{.agent = agent.id, .zone = zone.id});

    // MUTATION-эффекты применяются однократно при входе
    for (auto& desc : zone.effects) {
        if (!desc.enabled || !desc.plugin) continue;
        if (desc.effect_type != EffectType::MUTATION) continue;
        if (!capabilities_match(agent, desc.required_capabilities)) continue;

        EffectContext ctx;
        ctx.sim_time       = sim_time;
        ctx.dt             = dt;
        ctx.zone_id        = zone.id;
        ctx.zone_center    = zone.shape.center;
        ctx.zone_half_size = zone.shape.half_size;
        ctx.entity_id      = agent.id;
        ctx.agent_id       = agent.id;
        ctx.agent_position = agent.world_pose.position();

        desc.plugin->apply_mutation(agent.state, ctx);
    }
}

void ZoneSystem::on_agent_exit(Agent& agent, Zone& zone, SimBus& bus)
{
    bus.publish(event::AgentExitedZone{.agent = agent.id, .zone = zone.id});

    // Уведомить плагины эффектов о выходе агента.
    // Каждый плагин сам решает что сбросить в SharedState (например charging-флаг).
    for (auto& desc : zone.effects) {
        if (!desc.enabled || !desc.plugin) continue;
        if (!capabilities_match(agent, desc.required_capabilities)) continue;

        EffectContext ctx;
        ctx.zone_id        = zone.id;
        ctx.zone_center    = zone.shape.center;
        ctx.zone_half_size = zone.shape.half_size;
        ctx.entity_id      = agent.id;
        ctx.agent_id       = agent.id;
        ctx.agent_position = agent.world_pose.position();

        desc.plugin->on_agent_exit(agent.state, ctx);
    }

    // MUTATION не отменяется
    // MODIFIER/CONTINUOUS прекращают действовать автоматически (агент не в inside_agents)
}

void ZoneSystem::apply_active_effects(Agent& agent, Zone& zone,
                                      double sim_time, double dt)
{
    for (auto& desc : zone.effects) {
        if (!desc.enabled || !desc.plugin) continue;
        if (!capabilities_match(agent, desc.required_capabilities)) continue;

        EffectContext ctx;
        ctx.sim_time       = sim_time;
        ctx.dt             = dt;
        ctx.zone_id        = zone.id;
        ctx.zone_center    = zone.shape.center;
        ctx.zone_half_size = zone.shape.half_size;
        ctx.entity_id      = agent.id;
        ctx.agent_id       = agent.id;
        ctx.agent_position = agent.world_pose.position();

        switch (desc.effect_type) {
            case EffectType::MODIFIER:
                desc.plugin->apply_modifier(agent.state, ctx);
                break;
            case EffectType::CONTINUOUS:
                desc.plugin->apply_continuous(agent.state, ctx);
                break;
            case EffectType::MUTATION:
                // MUTATION не применяется повторно — только при входе
                break;
            case EffectType::SENSOR:
                // SENSOR-модификации запрашиваются вне этого метода (задача 31)
                break;
        }
    }
}

} // namespace s2
