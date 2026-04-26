#include <gtest/gtest.h>
#include <s2/zone.hpp>
#include <s2/zone_system.hpp>
#include <s2/sim_bus.hpp>
#include <s2/agent.hpp>
#include <s2/actor.hpp>

// ── Тесты данных (из Plan 01) ────────────────────────────────────────────────

TEST(ZoneDetectionMode, DefaultIsCenter)
{
    s2::Zone z;
    EXPECT_EQ(z.detection_mode_enum, s2::DetectionMode::CENTER);
}

TEST(ZoneDetectionMode, CanSetAllModes)
{
    s2::Zone z;
    z.detection_mode_enum = s2::DetectionMode::BOUNDING;
    EXPECT_EQ(z.detection_mode_enum, s2::DetectionMode::BOUNDING);
    z.detection_mode_enum = s2::DetectionMode::PER_LINK;
    EXPECT_EQ(z.detection_mode_enum, s2::DetectionMode::PER_LINK);
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

// ── Тесты ZoneSystem BOUNDING detection (Plan 03) ───────────────────────────

// BOUNDING: агент на расстоянии 2.5 от центра зоны (radius=2.0),
// но с bounding_radius=0.5 → 2.5 < 2.0 + 0.5 = 2.5 → на границе, считается внутри.
// CENTER: тот же агент снаружи (2.5 > 2.0).
TEST(ZoneSystemDetection, BoundingDetectsOverlap)
{
    // CENTER mode: агент на расстоянии 2.5 от зоны r=2.0 → снаружи
    {
        s2::ZoneSystem zs;
        auto zone = make_sphere_zone("center_zone", s2::Vec3(0, 0, 0), 2.0);
        zone.detection_mode_enum = s2::DetectionMode::CENTER;
        zs.add_zone(std::move(zone));

        std::vector<s2::Agent> agents;
        auto a = make_agent(1, 2.5, 0.0, 0.0);
        a.bounding.radius = 0.5;
        agents.push_back(std::move(a));
        std::vector<s2::Actor> actors;
        s2::SimBus bus;

        zs.tick(agents, actors, bus, 0.0, 0.01);
        EXPECT_EQ(zs.all_zones()[0].inside_agents.count(1u), 0u);
    }

    // BOUNDING mode: тот же агент → внутри (distance < zone.radius + agent.bounding_radius)
    {
        s2::ZoneSystem zs;
        auto zone = make_sphere_zone("bounding_zone", s2::Vec3(0, 0, 0), 2.0);
        zone.detection_mode_enum = s2::DetectionMode::BOUNDING;
        zs.add_zone(std::move(zone));

        std::vector<s2::Agent> agents;
        auto a = make_agent(1, 2.3, 0.0, 0.0); // 2.3 < 2.0 + 0.5 = 2.5 → внутри
        a.bounding.radius = 0.5;
        agents.push_back(std::move(a));
        std::vector<s2::Actor> actors;
        s2::SimBus bus;

        zs.tick(agents, actors, bus, 0.0, 0.01);
        EXPECT_EQ(zs.all_zones()[0].inside_agents.count(1u), 1u);
    }
}

// PER_LINK: без kinematic_tree → fallback на CENTER
TEST(ZoneSystemDetection, PerLinkFallbackToCenter)
{
    s2::ZoneSystem zs;
    auto zone = make_sphere_zone("perlink_zone", s2::Vec3(0, 0, 0), 2.0);
    zone.detection_mode_enum = s2::DetectionMode::PER_LINK;
    zs.add_zone(std::move(zone));

    std::vector<s2::Agent> agents;
    auto a = make_agent(1, 1.0, 0.0, 0.0); // внутри (1.0 < 2.0)
    a.kinematic_tree = nullptr; // нет kinematic_tree → fallback на CENTER
    agents.push_back(std::move(a));
    std::vector<s2::Actor> actors;
    s2::SimBus bus;

    zs.tick(agents, actors, bus, 0.0, 0.01);
    EXPECT_EQ(zs.all_zones()[0].inside_agents.count(1u), 1u);
}

// PER_LINK: с kinematic_tree — проверяем что линк внутри зоны детектируется
TEST(ZoneSystemDetection, PerLinkWithKinematicTree)
{
    s2::ZoneSystem zs;
    // Зона в точке (5,0,0) r=1.0
    auto zone = make_sphere_zone("perlink_zone2", s2::Vec3(5.0, 0.0, 0.0), 1.0);
    zone.detection_mode_enum = s2::DetectionMode::PER_LINK;
    zs.add_zone(std::move(zone));

    std::vector<s2::Agent> agents;
    auto a = make_agent(1, 0.0, 0.0, 0.0); // агент в origin, далеко от зоны
    // Создаём kinematic tree с линком, чей world pose попадает в зону
    auto tree = std::make_unique<s2::KinematicTree>();
    s2::Link arm_link;
    arm_link.name = "arm_link";
    arm_link.parent = "";
    arm_link.origin.x = 5.0; // смещение линка (5,0,0) — попадает в зону
    tree->add_link(std::move(arm_link));
    a.kinematic_tree = std::move(tree);
    agents.push_back(std::move(a));
    std::vector<s2::Actor> actors;
    s2::SimBus bus;

    zs.tick(agents, actors, bus, 0.0, 0.01);
    EXPECT_EQ(zs.all_zones()[0].inside_agents.count(1u), 1u);
}
