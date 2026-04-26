/**
 * @file test_grab_attach.cpp
 * Интеграционные тесты AttachObject/DetachObject через SimEngine.
 *
 * Проверяет end-to-end: push_command(AttachObject) -> prop.attached_to_agent,
 * push_command(DetachObject) -> prop.attached_to_agent = nullopt.
 *
 * Также проверяет phase6_attachments (обновление world_pose привязанных пропов).
 *
 * Phase 2 Plan 05: финальная верификация attachment wiring.
 */

#include <gtest/gtest.h>

#include <s2/sim_engine.hpp>
#include <s2/world.hpp>
#include <s2/prop.hpp>
#include <s2/agent.hpp>
#include <s2/event_bus.hpp>
#include <s2/kernel_command.hpp>

namespace s2
{

// ============================================================================
// Тесты AttachObject
// ============================================================================

/**
 * Тест: AttachObject записывает attached_to_agent, attach_link, attach_offset в Prop.
 */
TEST(GrabAttachIntegration, AttachObjectUpdatesProp)
{
    SimEngine engine{{.update_rate = 100.0}};
    SimWorld world;

    Agent agent;
    agent.id = 1;
    agent.world_pose = Pose3D{1.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    world.add_agent(std::move(agent));

    Prop prop;
    prop.id      = 100;
    prop.movable = true;
    prop.world_pose = Pose3D{1.5, 0.0, 0.0, 0.0, 0.0, 0.0};
    world.add_prop(std::move(prop));

    engine.load_world(std::move(world));

    // Отправить AttachObject
    engine.push_command(cmd::AttachObject{
        .parent_id  = 1,
        .link       = "gripper",
        .child_id   = 100,
        .local_pose = Pose3D{0.5, 0.0, 0.0, 0.0, 0.0, 0.0}
    });
    engine.step(1);

    const auto* p = engine.world().get_prop(100);
    ASSERT_NE(p, nullptr);
    EXPECT_TRUE(p->attached_to_agent.has_value());
    EXPECT_EQ(p->attached_to_agent.value(), 1u);
    EXPECT_EQ(p->attach_link, "gripper");
}

/**
 * Тест: ObjectAttached событие публикуется при AttachObject.
 */
TEST(GrabAttachIntegration, AttachPublishesObjectAttachedEvent)
{
    SimEngine engine{{.update_rate = 100.0}};
    SimWorld world;

    Agent agent;
    agent.id = 1;
    world.add_agent(std::move(agent));

    Prop prop;
    prop.id      = 101;
    prop.movable = true;
    world.add_prop(std::move(prop));

    engine.load_world(std::move(world));

    std::vector<event::ObjectAttached> events;
    engine.bus().subscribe<event::ObjectAttached>(
        [&](const event::ObjectAttached& e) {
            events.push_back(e);
        });

    engine.push_command(cmd::AttachObject{
        .parent_id = 1, .link = "arm", .child_id = 101,
        .local_pose = Pose3D{}
    });
    engine.step(1);

    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0].obj, 101u);
    EXPECT_EQ(events[0].agent, 1u);
    EXPECT_EQ(events[0].link, "arm");
}

// ============================================================================
// Тесты DetachObject
// ============================================================================

/**
 * Тест: DetachObject очищает attachment у Prop.
 */
TEST(GrabAttachIntegration, DetachObjectClearsAttachment)
{
    SimEngine engine{{.update_rate = 100.0}};
    SimWorld world;

    Agent agent;
    agent.id = 1;
    world.add_agent(std::move(agent));

    Prop prop;
    prop.id               = 102;
    prop.movable          = true;
    prop.attached_to_agent = 1;  // уже прикреплён
    prop.attach_link      = "arm";
    world.add_prop(std::move(prop));

    engine.load_world(std::move(world));

    engine.push_command(cmd::DetachObject{
        .child_id  = 102,
        .drop_pose = std::nullopt
    });
    engine.step(1);

    const auto* p = engine.world().get_prop(102);
    ASSERT_NE(p, nullptr);
    EXPECT_FALSE(p->attached_to_agent.has_value());
    EXPECT_TRUE(p->attach_link.empty());
}

/**
 * Тест: ObjectReleased событие публикуется при DetachObject.
 */
TEST(GrabAttachIntegration, DetachPublishesObjectReleasedEvent)
{
    SimEngine engine{{.update_rate = 100.0}};
    SimWorld world;

    Agent agent;
    agent.id = 1;
    world.add_agent(std::move(agent));

    Prop prop;
    prop.id               = 103;
    prop.movable          = true;
    prop.attached_to_agent = 1;
    world.add_prop(std::move(prop));

    engine.load_world(std::move(world));

    std::vector<event::ObjectReleased> events;
    engine.bus().subscribe<event::ObjectReleased>(
        [&](const event::ObjectReleased& e) {
            events.push_back(e);
        });

    engine.push_command(cmd::DetachObject{
        .child_id  = 103,
        .drop_pose = std::nullopt
    });
    engine.step(1);

    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0].obj, 103u);
    EXPECT_EQ(events[0].agent, 1u);
}

/**
 * Тест: DetachObject с drop_pose устанавливает позу пропа.
 */
TEST(GrabAttachIntegration, DetachWithDropPose)
{
    SimEngine engine{{.update_rate = 100.0}};
    SimWorld world;

    Agent agent;
    agent.id = 1;
    world.add_agent(std::move(agent));

    Prop prop;
    prop.id               = 104;
    prop.movable          = true;
    prop.attached_to_agent = 1;
    prop.world_pose       = Pose3D{0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    world.add_prop(std::move(prop));

    engine.load_world(std::move(world));

    Pose3D drop{5.0, 3.0, 1.0, 0.0, 0.0, 0.0};
    engine.push_command(cmd::DetachObject{
        .child_id  = 104,
        .drop_pose = drop
    });
    engine.step(1);

    const auto* p = engine.world().get_prop(104);
    ASSERT_NE(p, nullptr);
    EXPECT_NEAR(p->world_pose.x, 5.0, 1e-9);
    EXPECT_NEAR(p->world_pose.y, 3.0, 1e-9);
    EXPECT_NEAR(p->world_pose.z, 1.0, 1e-9);
}

// ============================================================================
// Тест phase6_attachments: позиция пропа обновляется из позы родителя
// ============================================================================

/**
 * Тест: attached проп следует за агентом.
 * Агент двигается -> phase6 обновляет world_pose пропа.
 */
TEST(GrabAttachIntegration, AttachedPropFollowsAgent)
{
    SimEngine engine{{.update_rate = 100.0}};
    SimWorld world;

    Agent agent;
    agent.id = 1;
    agent.world_pose = Pose3D{0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    world.add_agent(std::move(agent));

    Prop prop;
    prop.id = 105;
    prop.movable = true;
    world.add_prop(std::move(prop));

    engine.load_world(std::move(world));

    // Attach
    engine.push_command(cmd::AttachObject{
        .parent_id  = 1,
        .link       = "",
        .child_id   = 105,
        .local_pose = Pose3D{0.5, 0.0, 0.3, 0.0, 0.0, 0.0}
    });
    engine.step(1);

    // Переместить агента
    engine.push_command(cmd::SetPose{
        .id   = 1,
        .pose = Pose3D{10.0, 5.0, 0.0, 0.0, 0.0, 0.0}
    });
    engine.step(1);

    // Проп должен сместиться за агентом (agent.pose + offset)
    const auto* p = engine.world().get_prop(105);
    ASSERT_NE(p, nullptr);
    EXPECT_NEAR(p->world_pose.x, 10.5, 1e-9);
    EXPECT_NEAR(p->world_pose.y, 5.0, 1e-9);
    EXPECT_NEAR(p->world_pose.z, 0.3, 1e-9);
}

} // namespace s2
