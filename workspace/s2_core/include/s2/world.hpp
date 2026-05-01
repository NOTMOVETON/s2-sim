#pragma once

/**
 * @file world.hpp
 * SimWorld — контейнер для всех сущностей симуляции.
 *
 * Variant D: три типизированных вектора (cache-friendly для тикового цикла)
 * + четыре index-map для O(1) lookup по EntityId.
 * swap-and-pop при despawn — без инвалидации итераторов других типов.
 */

#include <s2/agent.hpp>
#include <s2/actor.hpp>
#include <s2/prop.hpp>
#include <s2/heightmap.hpp>
#include <s2/zone.hpp>

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace s2
{

/**
 * @brief Статический примитив мира (стена, колонна, сфера).
 *
 * Используется для коллизий со статикой и визуализации.
 * Типы: "box", "cylinder", "sphere".
 */
struct WorldPrimitive {
    std::string type;
    Pose3D pose;
    Vec3 size{1, 1, 1};
    double radius{0.5};
    double height{1.0};
    std::string color{ "#808080" };
};

/**
 * @brief Контейнер для всех сущностей симуляции.
 *
 * Хранение: Variant D — три contiguous-вектора + 4 unordered_map для O(1) lookup.
 * Итерация по типу: agents()/actors()/props() — cache-friendly.
 * Поиск по EntityId: get_agent/get_actor/get_prop — O(1).
 */
class SimWorld
{
public:
    // ── Добавление ──────────────────────────────────────────────────────────

    void add_agent(Agent agent)
    {
        EntityId id = agent.id;
        agent.entity_type = EntityType::AGENT;
        entity_type_[id] = EntityType::AGENT;
        agent_idx_[id]   = agents_.size();
        agents_.push_back(std::move(agent));
    }

    void add_actor(Actor actor)
    {
        EntityId id = actor.id;
        actor.entity_type = EntityType::ACTOR;
        entity_type_[id] = EntityType::ACTOR;
        actor_idx_[id]   = actors_.size();
        actors_.push_back(std::move(actor));
    }

    void add_prop(Prop prop)
    {
        EntityId id = prop.id;
        prop.entity_type = EntityType::PROP;
        entity_type_[id] = EntityType::PROP;
        prop_idx_[id]    = props_.size();
        props_.push_back(std::move(prop));
    }

    // ── O(1) lookup ─────────────────────────────────────────────────────────

    Agent* get_agent(EntityId id)
    {
        auto it = agent_idx_.find(id);
        return (it != agent_idx_.end()) ? &agents_[it->second] : nullptr;
    }

    const Agent* get_agent(EntityId id) const
    {
        auto it = agent_idx_.find(id);
        return (it != agent_idx_.end()) ? &agents_[it->second] : nullptr;
    }

    Actor* get_actor(EntityId id)
    {
        auto it = actor_idx_.find(id);
        return (it != actor_idx_.end()) ? &actors_[it->second] : nullptr;
    }

    const Actor* get_actor(EntityId id) const
    {
        auto it = actor_idx_.find(id);
        return (it != actor_idx_.end()) ? &actors_[it->second] : nullptr;
    }

    Prop* get_prop(EntityId id)
    {
        auto it = prop_idx_.find(id);
        return (it != prop_idx_.end()) ? &props_[it->second] : nullptr;
    }

    const Prop* get_prop(EntityId id) const
    {
        auto it = prop_idx_.find(id);
        return (it != prop_idx_.end()) ? &props_[it->second] : nullptr;
    }

    std::optional<EntityType> get_entity_type(EntityId id) const
    {
        auto it = entity_type_.find(id);
        if (it == entity_type_.end()) return std::nullopt;
        return it->second;
    }

    // ── Despawn: swap-and-pop ────────────────────────────────────────────────

    void remove_agent(EntityId id)
    {
        auto it = agent_idx_.find(id);
        if (it == agent_idx_.end()) return;
        size_t idx  = it->second;
        size_t last = agents_.size() - 1;
        if (idx != last) {
            std::swap(agents_[idx], agents_[last]);
            agent_idx_[agents_[idx].id] = idx;
        }
        agents_.pop_back();
        agent_idx_.erase(id);
        entity_type_.erase(id);
    }

    // ── Итерация ────────────────────────────────────────────────────────────

    std::vector<Agent>&        agents()        { return agents_; }
    const std::vector<Agent>&  agents()  const { return agents_; }
    std::vector<Actor>&        actors()        { return actors_; }
    const std::vector<Actor>&  actors()  const { return actors_; }
    std::vector<Prop>&         props()         { return props_; }
    const std::vector<Prop>&   props()   const { return props_; }

    // ── Статика и heightmap ──────────────────────────────────────────────────

    std::vector<WorldPrimitive>& static_geometry()       { return static_geometry_; }
    const std::vector<WorldPrimitive>& static_geometry() const { return static_geometry_; }

    void add_static_primitive(WorldPrimitive prim)
    {
        static_geometry_.push_back(std::move(prim));
    }

    void set_heightmap(Heightmap hm) { heightmap_ = std::move(hm); }
    const Heightmap& heightmap() const { return heightmap_; }

    // ── Зоны ────────────────────────────────────────────────────────────────

    void add_zone(Zone zone)
    {
        zones_.push_back(std::move(zone));
    }

    Zone* get_zone(const ZoneId& id)
    {
        for (auto& zone : zones_)
            if (zone.id == id) return &zone;
        return nullptr;
    }

    std::vector<Zone>& zones()             { return zones_; }
    const std::vector<Zone>& zones() const { return zones_; }

    // ── Коллизия со статикой ─────────────────────────────────────────────────

    bool check_sphere_collision(const Vec3& center, double radius) const;

private:
    // Variant D: per-type contiguous storage
    std::vector<Agent> agents_;
    std::vector<Actor> actors_;
    std::vector<Prop>  props_;

    // Unified O(1) index maps
    std::unordered_map<EntityId, EntityType> entity_type_;
    std::unordered_map<EntityId, size_t>     agent_idx_;
    std::unordered_map<EntityId, size_t>     actor_idx_;
    std::unordered_map<EntityId, size_t>     prop_idx_;

    std::vector<Zone>          zones_;
    std::vector<WorldPrimitive> static_geometry_;
    Heightmap heightmap_ = Heightmap::flat(40.0, 40.0);
};

// ─── Inline: проверка коллизии сферы со статикой ─────────────────────────────

inline bool SimWorld::check_sphere_collision(const Vec3& center, double radius) const
{
    for (const auto& prim : static_geometry_)
    {
        if (prim.type == "box")
        {
            double half_x = prim.size.x() / 2.0;
            double half_y = prim.size.y() / 2.0;
            double half_z = prim.size.z() / 2.0;
            double cx = std::max(prim.pose.x - half_x, std::min(center.x(), prim.pose.x + half_x));
            double cy = std::max(prim.pose.y - half_y, std::min(center.y(), prim.pose.y + half_y));
            double cz = std::max(prim.pose.z - half_z, std::min(center.z(), prim.pose.z + half_z));
            double dx = center.x() - cx;
            double dy = center.y() - cy;
            double dz = center.z() - cz;
            if (dx * dx + dy * dy + dz * dz < radius * radius)
                return true;
        }
        else if (prim.type == "sphere")
        {
            double dx = center.x() - prim.pose.x;
            double dy = center.y() - prim.pose.y;
            double dz = center.z() - prim.pose.z;
            double dist = std::sqrt(dx * dx + dy * dy + dz * dz);
            if (dist < radius + prim.radius)
                return true;
        }
        else if (prim.type == "cylinder")
        {
            double dx = center.x() - prim.pose.x;
            double dy = center.y() - prim.pose.y;
            double dz = center.z() - prim.pose.z;
            double dist = std::sqrt(dx * dx + dy * dy + dz * dz);
            if (dist < radius + prim.radius)
                return true;
        }
    }
    return false;
}

} // namespace s2
