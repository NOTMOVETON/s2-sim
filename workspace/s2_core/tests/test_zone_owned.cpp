#include <gtest/gtest.h>
#include <s2/zone_system.hpp>
#include <s2/agent.hpp>
#include <s2/sim_bus.hpp>
#include <s2/kinematic_tree.hpp>

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

} // namespace

// ── Тесты owned_zones позиционирования (Plan 03, задача 2) ──────────────────

// Зона с attached_to_entity_id следует за агентом
TEST(ZoneOwned, ZoneFollowsAgent)
{
    s2::ZoneSystem zs;
    s2::Zone zone;
    zone.id = "follow_zone";
    zone.shape.type = s2::ZoneShapeType::SPHERE;
    zone.shape.radius = 1.0;
    zone.shape.center = s2::Vec3(0, 0, 0);
    zone.attached_to_entity_id = "1"; // agent id=1
    zone.attachment_offset = s2::Vec3(1, 0, 0);
    zs.add_zone(std::move(zone));

    std::vector<s2::Agent> agents;
    agents.push_back(make_agent(1, 2.0, 0.0, 0.0));

    zs.update_owned_zones_positions(agents);

    const auto& zones = zs.all_zones();
    ASSERT_EQ(zones.size(), 1u);
    // zone.shape.center = (2,0,0) + (1,0,0) = (3,0,0)
    EXPECT_NEAR(zones[0].shape.center.x(), 3.0, 1e-9);
    EXPECT_NEAR(zones[0].shape.center.y(), 0.0, 1e-9);
    EXPECT_NEAR(zones[0].shape.center.z(), 0.0, 1e-9);
}

// Зона без владельца (id не найден) остаётся на месте
TEST(ZoneOwned, NoOwnerNoMove)
{
    s2::ZoneSystem zs;
    s2::Zone zone;
    zone.id = "orphan_zone";
    zone.shape.type = s2::ZoneShapeType::SPHERE;
    zone.shape.center = s2::Vec3(5, 0, 0);
    zone.shape.radius = 1.0;
    zone.attached_to_entity_id = "999"; // не существует
    zs.add_zone(std::move(zone));

    std::vector<s2::Agent> agents; // пустой список

    zs.update_owned_zones_positions(agents);

    const auto& zones = zs.all_zones();
    ASSERT_EQ(zones.size(), 1u);
    // Зона осталась на месте — не crash
    EXPECT_NEAR(zones[0].shape.center.x(), 5.0, 1e-9);
}

// Зона без attached_to_entity_id — не двигается
TEST(ZoneOwned, NotAttachedStaysInPlace)
{
    s2::ZoneSystem zs;
    s2::Zone zone;
    zone.id = "static_zone";
    zone.shape.type = s2::ZoneShapeType::SPHERE;
    zone.shape.center = s2::Vec3(10, 0, 0);
    zone.shape.radius = 2.0;
    // attached_to_entity_id пустой — не перемещается
    zs.add_zone(std::move(zone));

    std::vector<s2::Agent> agents;
    agents.push_back(make_agent(1, 20.0, 0.0, 0.0));

    zs.update_owned_zones_positions(agents);

    EXPECT_NEAR(zs.all_zones()[0].shape.center.x(), 10.0, 1e-9);
}

// Зона с attached_to_link — обновляется по позиции линка
TEST(ZoneOwned, AttachedToLinkFollowsLink)
{
    s2::ZoneSystem zs;
    s2::Zone zone;
    zone.id = "link_zone";
    zone.shape.type = s2::ZoneShapeType::SPHERE;
    zone.shape.center = s2::Vec3(0, 0, 0);
    zone.shape.radius = 0.5;
    zone.attached_to_entity_id = "1";
    zone.attached_to_link = "cargo_link";
    zone.attachment_offset = s2::Vec3(0, 0, 0.5); // offset от позиции линка
    zs.add_zone(std::move(zone));

    // Агент в origin, с kinematic_tree где cargo_link смещён на (3,0,0)
    auto tree = std::make_unique<s2::KinematicTree>();
    s2::Link cargo_link;
    cargo_link.name = "cargo_link";
    cargo_link.parent = "";
    cargo_link.origin.x = 3.0;
    tree->add_link(std::move(cargo_link));

    std::vector<s2::Agent> agents;
    agents.push_back(make_agent(1, 0.0, 0.0, 0.0));
    agents.back().kinematic_tree = std::move(tree);
    zs.update_owned_zones_positions(agents);

    const auto& zones = zs.all_zones();
    ASSERT_EQ(zones.size(), 1u);
    // Позиция линка = agent(0,0,0) + link_origin(3,0,0) = (3,0,0)
    // zone.center = (3,0,0) + offset(0,0,0.5) = (3,0,0.5)
    EXPECT_NEAR(zones[0].shape.center.x(), 3.0, 1e-9);
    EXPECT_NEAR(zones[0].shape.center.y(), 0.0, 1e-9);
    EXPECT_NEAR(zones[0].shape.center.z(), 0.5, 1e-9);
}

// Зона с attached_to_link, но линк не найден — fallback на позицию агента + offset
TEST(ZoneOwned, AttachedToLinkNotFoundFallback)
{
    s2::ZoneSystem zs;
    s2::Zone zone;
    zone.id = "missing_link_zone";
    zone.shape.type = s2::ZoneShapeType::SPHERE;
    zone.shape.center = s2::Vec3(0, 0, 0);
    zone.shape.radius = 0.5;
    zone.attached_to_entity_id = "1";
    zone.attached_to_link = "nonexistent_link";
    zone.attachment_offset = s2::Vec3(1, 0, 0);
    zs.add_zone(std::move(zone));

    // Агент в (5,0,0), с kinematic_tree без "nonexistent_link"
    auto tree = std::make_unique<s2::KinematicTree>();
    s2::Link other_link;
    other_link.name = "other_link";
    other_link.parent = "";
    tree->add_link(std::move(other_link));

    std::vector<s2::Agent> agents;
    agents.push_back(make_agent(1, 5.0, 0.0, 0.0));
    agents.back().kinematic_tree = std::move(tree);
    zs.update_owned_zones_positions(agents);

    const auto& zones = zs.all_zones();
    ASSERT_EQ(zones.size(), 1u);
    // Fallback: agent(5,0,0) + offset(1,0,0) = (6,0,0)
    EXPECT_NEAR(zones[0].shape.center.x(), 6.0, 1e-9);
}
