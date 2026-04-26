#include <s2/zone_system.hpp>
#include <algorithm>
#include <unordered_set>

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

// ── Lifecycle: рост и затухание strength ──────────────────────────────────────

void ZoneSystem::update_lifecycle(double sim_time, double dt)
{
    for (auto& zone : zones_) {
        // Рост
        if (zone.lifecycle.growth_rate > 0.0 &&
            zone.strength < zone.lifecycle.max_strength) {
            zone.strength = std::min(
                zone.strength + zone.lifecycle.growth_rate * dt,
                zone.lifecycle.max_strength);
        }

        // Затухание — только после decay_delay от момента спавна
        if (zone.lifecycle.decay_rate > 0.0) {
            double decay_start = zone.spawn_time + zone.lifecycle.decay_delay;
            if (sim_time >= decay_start) {
                zone.strength = std::max(
                    0.0, zone.strength - zone.lifecycle.decay_rate * dt);
            }
        }
    }
}

// ── Detection: проверка вхождения агента в зону по detection_mode_enum ───────

bool ZoneSystem::agent_in_zone(const Agent& agent, const Zone& zone)
{
    switch (zone.detection_mode_enum) {
    case DetectionMode::CENTER: {
        return zone.shape.contains(agent.world_pose.position());
    }
    case DetectionMode::BOUNDING: {
        Vec3 pos = agent.world_pose.position();
        double agent_r = agent.bounding.radius;
        if (zone.shape.type == ZoneShapeType::SPHERE) {
            double dist = (pos - zone.shape.center).norm();
            return dist < (zone.shape.radius + agent_r);
        }
        // Для AABB: расширяем полуразмеры на agent_r
        if (zone.shape.type == ZoneShapeType::AABB) {
            Vec3 diff = (pos - zone.shape.center).cwiseAbs();
            return diff.x() <= (zone.shape.half_size.x() + agent_r) &&
                   diff.y() <= (zone.shape.half_size.y() + agent_r) &&
                   diff.z() <= (zone.shape.half_size.z() + agent_r);
        }
        // Для CYLINDER: расширяем radius на agent_r, half_height на agent_r
        if (zone.shape.type == ZoneShapeType::CYLINDER) {
            double dz = std::abs(pos.z() - zone.shape.center.z());
            if (dz > zone.shape.half_height + agent_r) return false;
            double dx = pos.x() - zone.shape.center.x();
            double dy = pos.y() - zone.shape.center.y();
            return dx * dx + dy * dy <=
                   (zone.shape.radius + agent_r) * (zone.shape.radius + agent_r);
        }
        // INFINITE
        if (zone.shape.type == ZoneShapeType::INFINITE) return true;
        // Fallback
        return zone.shape.contains(pos);
    }
    case DetectionMode::PER_LINK: {
        if (!agent.kinematic_tree) {
            // Fallback на CENTER если нет kinematic_tree (T-01-06)
            return zone.shape.contains(agent.world_pose.position());
        }
        // Итерируем линки kinematic_tree
        for (const auto& link : agent.kinematic_tree->links()) {
            Pose3D link_pose = agent.kinematic_tree->compute_world_pose(
                link.name, agent.world_pose);
            if (zone.shape.contains(link_pose.position())) {
                return true;
            }
        }
        return false;
    }
    }
    return false;
}

// ── Основной тик ─────────────────────────────────────────────────────────────

void ZoneSystem::tick(
    std::vector<Agent>& agents,
    const std::vector<Actor>& actors,
    SimBus& bus,
    double sim_time,
    double dt)
{
    // === Шаг 0: Lifecycle update (рост/затухание strength) per D-10 ===
    update_lifecycle(sim_time, dt);

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

    // === Шаг 1.5: Сбросить SENSOR-моды прошлого тика ===
    for (auto& agent : agents) {
        agent.active_sensor_mods.clear();
    }

    // === Шаг 2: Проверить enter/exit для каждой зоны ===
    // Накапливаем зоны для self-destruct удаления (T-01-05: не удалять во время итерации)
    std::unordered_set<ZoneId> zones_to_destroy;

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

        bool effect_applied_this_zone = false;

        for (auto& agent : agents) {
            bool is_inside  = agent_in_zone(agent, zone);
            bool was_inside = zone.inside_agents.count(agent.id) > 0;

            if (is_inside && !was_inside) {
                zone.inside_agents.insert(agent.id);
                on_agent_enter(agent, zone, bus, sim_time, dt);

                // Self-destruct ON_ANY_CONTACT: любой enter → уничтожить
                if (zone.self_destruct.type == SelfDestructPolicy::Type::ON_ANY_CONTACT) {
                    zones_to_destroy.insert(zone.id);
                }

                // Self-destruct ON_EFFECT_APPLIED: проверяем применились ли эффекты
                if (zone.self_destruct.type == SelfDestructPolicy::Type::ON_EFFECT_APPLIED) {
                    // Проверяем: хотя бы один эффект capabilities-matched
                    for (const auto& desc : zone.effects) {
                        if (!desc.enabled || !desc.plugin) continue;
                        if (capabilities_match(agent, desc.required_capabilities)) {
                            effect_applied_this_zone = true;
                            break;
                        }
                    }
                }
            } else if (!is_inside && was_inside) {
                zone.inside_agents.erase(agent.id);
                on_agent_exit(agent, zone, bus);
            }
        }

        if (effect_applied_this_zone &&
            zone.self_destruct.type == SelfDestructPolicy::Type::ON_EFFECT_APPLIED) {
            zones_to_destroy.insert(zone.id);
        }
    }

    // === Шаг 3: Применить активные эффекты ===
    for (auto& zone : zones_) {
        if (!zone.enabled) continue;
        if (zones_to_destroy.count(zone.id)) continue; // Зона будет удалена — не применяем эффекты
        for (AgentId aid : zone.inside_agents) {
            for (auto& agent : agents) {
                if (agent.id == aid) {
                    apply_active_effects(agent, zone, sim_time, dt);
                    break;
                }
            }
        }
    }

    // === Шаг 4: Auto-remove зон по lifecycle threshold ===
    for (const auto& zone : zones_) {
        if (zone.lifecycle.remove_threshold > 0.0 &&
            zone.strength < zone.lifecycle.remove_threshold) {
            zones_to_destroy.insert(zone.id);
        }
    }

    // === Шаг 5: Удалить зоны из zones_to_destroy (после итерации) ===
    if (!zones_to_destroy.empty()) {
        for (const auto& zid : zones_to_destroy) {
            remove_zone(zid, agents, bus);
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

bool ZoneSystem::toggle_zone_with_events(const ZoneId& id,
                                          bool enabled,
                                          std::vector<Agent>& agents,
                                          SimBus& bus)
{
    for (auto& zone : zones_) {
        if (zone.id != id) continue;

        if (!enabled && zone.enabled) {
            // Выключаем: отправляем ZoneExited всем inside_agents
            std::vector<AgentId> to_exit(zone.inside_agents.begin(), zone.inside_agents.end());
            for (AgentId aid : to_exit) {
                for (auto& agent : agents) {
                    if (agent.id == aid) {
                        on_agent_exit(agent, zone, bus);
                        break;
                    }
                }
            }
            zone.inside_agents.clear();
        } else if (enabled && !zone.enabled) {
            // Включаем: для всех агентов внутри геометрии отправляем ZoneEntered
            for (auto& agent : agents) {
                if (agent_in_zone(agent, zone)) {
                    zone.inside_agents.insert(agent.id);
                    on_agent_enter(agent, zone, bus, 0.0, 0.0);
                }
            }
        }

        zone.enabled = enabled;
        return true;
    }
    return false;
}

void ZoneSystem::remove_zone(const ZoneId& id,
                              std::vector<Agent>& agents,
                              SimBus& bus)
{
    auto it = std::find_if(zones_.begin(), zones_.end(),
        [&id](const Zone& z) { return z.id == id; });
    if (it == zones_.end()) return;

    // Отправить ZoneExited всем inside_agents
    for (AgentId aid : it->inside_agents) {
        for (auto& agent : agents) {
            if (agent.id == aid) {
                on_agent_exit(agent, *it, bus);
                break;
            }
        }
    }

    zones_.erase(it);
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

bool ZoneSystem::update_zone_visual(const ZoneId& id, const std::string& color, double opacity)
{
    for (auto& zone : zones_) {
        if (zone.id == id) {
            zone.color = color;
            zone.opacity = opacity;
            return true;
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
    // Legacy string-based mode. Используется только при detection_mode_enum == CENTER.
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
    bus.publish(event::ZoneEntered{.zone_id = zone.id, .entity_id = agent.id});  // Новый event — entity-уровень (для Phase 1+ подписчиков)

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
        ctx.zone_strength  = zone.strength;
        ctx.agent_id       = agent.id;
        ctx.agent_position = agent.world_pose.position();

        desc.plugin->apply_mutation(agent.state, ctx);
    }
}

void ZoneSystem::on_agent_exit(Agent& agent, Zone& zone, SimBus& bus)
{
    bus.publish(event::AgentExitedZone{.agent = agent.id, .zone = zone.id});
    bus.publish(event::ZoneExited{.zone_id = zone.id, .entity_id = agent.id});  // Новый event — entity-уровень (для Phase 1+ подписчиков)

    // Уведомить плагины эффектов о выходе агента.
    // Каждый плагин сам решает что сбросить в SharedState (например charging-флаг).
    for (auto& desc : zone.effects) {
        if (!desc.enabled || !desc.plugin) continue;
        if (!capabilities_match(agent, desc.required_capabilities)) continue;

        EffectContext ctx;
        ctx.zone_id        = zone.id;
        ctx.zone_center    = zone.shape.center;
        ctx.zone_half_size = zone.shape.half_size;
        ctx.zone_strength  = zone.strength;
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
        ctx.zone_strength  = zone.strength;
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
            case EffectType::SENSOR: {
                // Собрать SENSOR-моды в agent.active_sensor_mods — применяются в phase4.
                auto mods = desc.plugin->sensor_mods(ctx);
                for (auto& m : mods)
                    agent.active_sensor_mods.push_back(m);
                break;
            }
        }
    }
}

// ── Owned zones: обновление позиций зон привязанных к entity ─────────────────

void ZoneSystem::update_owned_zones_positions(const std::vector<Agent>& agents)
{
    for (auto& zone : zones_) {
        if (zone.attached_to_entity_id.empty()) continue;

        // Найти агента по id
        const Agent* owner = nullptr;
        for (const auto& agent : agents) {
            if (std::to_string(agent.id) == zone.attached_to_entity_id) {
                owner = &agent;
                break;
            }
        }
        if (!owner) continue;

        Vec3 base_pos = owner->world_pose.position();

        // Если задан attached_to_link — найти линк в kinematic_tree
        if (zone.attached_to_link.has_value() && owner->kinematic_tree) {
            const std::string& link_name = zone.attached_to_link.value();
            bool link_found = false;
            for (const auto& link : owner->kinematic_tree->links()) {
                if (link.name == link_name) {
                    Pose3D link_pose = owner->kinematic_tree->compute_world_pose(
                        link_name, owner->world_pose);
                    base_pos = link_pose.position();
                    link_found = true;
                    break;
                }
            }
            // Если линк не найден — fallback на позицию агента + offset
            (void)link_found;
        }

        zone.shape.center = base_pos + zone.attachment_offset;
    }
}

} // namespace s2
