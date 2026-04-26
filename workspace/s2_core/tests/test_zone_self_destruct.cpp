#include <gtest/gtest.h>
#include <s2/zone.hpp>
#include <s2/zone_system.hpp>
#include <s2/sim_bus.hpp>
#include <s2/agent.hpp>
#include <s2/actor.hpp>

// ── Тесты данных (из Plan 01) ────────────────────────────────────────────────

TEST(ZoneSelfDestruct, DefaultIsNone)
{
    s2::Zone z;
    EXPECT_EQ(z.self_destruct.type, s2::SelfDestructPolicy::Type::NONE);
}

TEST(ZoneSelfDestruct, CanSetOnAnyContact)
{
    s2::Zone z;
    z.self_destruct.type = s2::SelfDestructPolicy::Type::ON_ANY_CONTACT;
    EXPECT_EQ(z.self_destruct.type, s2::SelfDestructPolicy::Type::ON_ANY_CONTACT);
}

TEST(ZoneSelfDestruct, CanSetOnEffectApplied)
{
    s2::Zone z;
    z.self_destruct.type = s2::SelfDestructPolicy::Type::ON_EFFECT_APPLIED;
    EXPECT_EQ(z.self_destruct.type, s2::SelfDestructPolicy::Type::ON_EFFECT_APPLIED);
}

// ── Вспомогательные функции ──────────────────────────────────────────────────

namespace {

s2::Agent make_agent(s2::AgentId id, double x, double y, double z = 0.0)
{
    s2::Agent a;
    a.id = id;
    a.world_pose.x = x;
    a.world_pose.y = y;
    a.world_pose.z = z;
    return a;
}

s2::Zone make_sphere_zone(const s2::ZoneId& id, s2::Vec3 center, double radius)
{
    s2::Zone z;
    z.id           = id;
    z.enabled      = true;
    z.shape.type   = s2::ZoneShapeType::SPHERE;
    z.shape.center = center;
    z.shape.radius = radius;
    return z;
}

} // namespace

// Стаб-эффект для отслеживания вызовов apply_modifier
class StubEffectPlugin : public s2::EffectPlugin
{
public:
    mutable int apply_count = 0;
    void on_init(const YAML::Node&) override {}
    s2::EffectType effect_type() const override { return s2::EffectType::MODIFIER; }
    void apply_modifier(s2::SharedState&, const s2::EffectContext&) override
    {
        ++apply_count;
    }
};

// ── Тесты ZoneSystem self_destruct (Plan 03) ────────────────────────────────

// ON_ANY_CONTACT: агент заходит → зона удаляется в конце тика
TEST(ZoneSelfDestructSystem, OnAnyContactDestroysZone)
{
    s2::ZoneSystem zs;
    auto zone = make_sphere_zone("bomb_zone", s2::Vec3(0, 0, 0), 5.0);
    zone.self_destruct.type = s2::SelfDestructPolicy::Type::ON_ANY_CONTACT;
    zs.add_zone(std::move(zone));

    std::vector<s2::Agent> agents;
    agents.push_back(make_agent(1, 0.0, 0.0, 0.0)); // внутри зоны
    std::vector<s2::Actor> actors;
    s2::SimBus bus;

    ASSERT_EQ(zs.all_zones().size(), 1u);

    zs.tick(agents, actors, bus, 0.0, 0.01);

    // Зона должна быть удалена после тика
    EXPECT_EQ(zs.all_zones().size(), 0u);
}

// ON_EFFECT_APPLIED: зона жива если агент не имеет нужных capabilities
TEST(ZoneSelfDestructSystem, OnEffectAppliedSurvivesWithoutCapability)
{
    s2::ZoneSystem zs;
    auto zone = make_sphere_zone("cap_zone", s2::Vec3(0, 0, 0), 5.0);
    zone.self_destruct.type = s2::SelfDestructPolicy::Type::ON_EFFECT_APPLIED;

    s2::Zone::EffectDesc desc;
    desc.type = "stub";
    desc.enabled = true;
    desc.effect_type = s2::EffectType::MODIFIER;
    desc.required_capabilities = {"special_cap"};
    auto stub = std::make_unique<StubEffectPlugin>();
    desc.plugin = std::move(stub);
    zone.effects.push_back(std::move(desc));

    zs.add_zone(std::move(zone));

    // Агент БЕЗ capabilities → эффект не применяется → зона жива
    std::vector<s2::Agent> agents;
    agents.push_back(make_agent(1, 0.0, 0.0, 0.0));
    std::vector<s2::Actor> actors;
    s2::SimBus bus;

    zs.tick(agents, actors, bus, 0.0, 0.01);
    EXPECT_EQ(zs.all_zones().size(), 1u); // зона осталась
}

// ON_EFFECT_APPLIED: зона удаляется если эффект применился (агент имеет capabilities)
TEST(ZoneSelfDestructSystem, OnEffectAppliedDestroysWithCapability)
{
    s2::ZoneSystem zs;
    auto zone = make_sphere_zone("cap_zone2", s2::Vec3(0, 0, 0), 5.0);
    zone.self_destruct.type = s2::SelfDestructPolicy::Type::ON_EFFECT_APPLIED;

    s2::Zone::EffectDesc desc;
    desc.type = "stub";
    desc.enabled = true;
    desc.effect_type = s2::EffectType::MODIFIER;
    desc.required_capabilities = {"special_cap"};
    auto stub = std::make_unique<StubEffectPlugin>();
    desc.plugin = std::move(stub);
    zone.effects.push_back(std::move(desc));

    zs.add_zone(std::move(zone));

    // Агент С capabilities → эффект применяется → зона удаляется
    std::vector<s2::Agent> agents;
    auto a = make_agent(1, 0.0, 0.0, 0.0);
    a.capabilities.insert("special_cap");
    agents.push_back(std::move(a));
    std::vector<s2::Actor> actors;
    s2::SimBus bus;

    zs.tick(agents, actors, bus, 0.0, 0.01);
    EXPECT_EQ(zs.all_zones().size(), 0u); // зона удалена
}
