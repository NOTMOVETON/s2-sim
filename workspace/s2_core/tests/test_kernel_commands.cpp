#include <gtest/gtest.h>
#include <s2/command_queue.hpp>
#include <s2/kernel_command.hpp>
#include <s2/sim_engine.hpp>
#include <atomic>
#include <thread>

using namespace s2;

// --- Тест 1: SetPose применяется в начале следующего тика ---

TEST(KernelCommandsTest, SetPose_AppliedInNextTick)
{
    SimEngine engine{{.update_rate = 100.0, .viz_rate = 0.0}};

    Agent agent;
    agent.id = 1;
    agent.name = "robot";
    agent.world_pose = Pose3D{0, 0, 0, 0, 0, 0};

    SimWorld world;
    world.add_agent(std::move(agent));
    engine.load_world(std::move(world));
    engine.pause();

    engine.enqueue(cmd::SetPose{1, Pose3D{1.0, 2.0, 0, 0, 0, 0}});

    // До step() поза не изменилась
    const auto* a_before = engine.world().get_agent(1);
    ASSERT_NE(a_before, nullptr);
    EXPECT_DOUBLE_EQ(a_before->world_pose.x, 0.0);

    engine.step(1);

    // После step() поза обновлена
    const auto* a_after = engine.world().get_agent(1);
    ASSERT_NE(a_after, nullptr);
    EXPECT_DOUBLE_EQ(a_after->world_pose.x, 1.0);
    EXPECT_DOUBLE_EQ(a_after->world_pose.y, 2.0);
}

// --- Тест 2: PauseSim / ResumeSim через очередь ---

TEST(KernelCommandsTest, PauseResume_ViaQueue)
{
    SimEngine engine{{.update_rate = 100.0, .viz_rate = 0.0}};

    SimWorld world;
    Agent agent;
    agent.id = 1;
    agent.name = "robot";
    world.add_agent(std::move(agent));
    engine.load_world(std::move(world));
    engine.resume();

    EXPECT_FALSE(engine.is_paused());

    engine.enqueue(cmd::PauseSim{});
    engine.step(1);
    EXPECT_TRUE(engine.is_paused());

    engine.enqueue(cmd::ResumeSim{});
    engine.step(1);
    EXPECT_FALSE(engine.is_paused());
}

// --- Тест 3: Несколько команд в одном тике — все применяются ---

TEST(KernelCommandsTest, MultipleCommands_AllApplied)
{
    SimEngine engine{{.update_rate = 100.0, .viz_rate = 0.0}};

    Agent agent;
    agent.id = 1;
    agent.name = "robot";
    agent.world_pose = Pose3D{0, 0, 0, 0, 0, 0};

    SimWorld world;
    world.add_agent(std::move(agent));
    engine.load_world(std::move(world));
    engine.pause();

    engine.enqueue(cmd::SetPose{1, Pose3D{5.0, 0, 0, 0, 0, 0}});
    engine.enqueue(cmd::SetEnabled{1, false});
    engine.step(1);

    const auto* a = engine.world().get_agent(1);
    ASSERT_NE(a, nullptr);
    EXPECT_DOUBLE_EQ(a->world_pose.x, 5.0);
    EXPECT_FALSE(a->enabled);
}

// --- Тест 4: CommandQueue drain потокобезопасен ---

TEST(KernelCommandsTest, CommandQueue_DrainIsThreadSafe)
{
    CommandQueue queue;
    std::atomic<int> total{0};

    std::thread producer([&queue]() {
        for (int i = 0; i < 1000; ++i)
            queue.enqueue(cmd::SetPose{static_cast<EntityId>(i), Pose3D{}});
    });

    std::thread consumer([&queue, &total]() {
        for (int i = 0; i < 20; ++i) {
            auto batch = queue.drain();
            total += static_cast<int>(batch.size());
        }
    });

    producer.join();
    consumer.join();

    auto remaining = queue.drain();
    total += static_cast<int>(remaining.size());

    EXPECT_EQ(total.load(), 1000);
}
