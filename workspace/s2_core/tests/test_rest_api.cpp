#include <gtest/gtest.h>
#include <s2/sim_engine.hpp>
#include <s2/kernel_command.hpp>

TEST(RestApiLogicTest, PauseSim_ViaEnqueue_AppliedInNextTick) {
    s2::SimEngine engine{{.update_rate = 100.0, .viz_rate = 0.0}};
    s2::SimWorld world;
    engine.load_world(std::move(world));
    engine.resume();
    EXPECT_FALSE(engine.is_paused());
    engine.enqueue(s2::cmd::PauseSim{});
    engine.step(1);
    EXPECT_TRUE(engine.is_paused());
}

TEST(RestApiLogicTest, SetSpeed_ViaEnqueue_Stored) {
    s2::SimEngine engine{{.update_rate = 100.0, .viz_rate = 0.0}};
    s2::SimWorld world;
    engine.load_world(std::move(world));
    engine.enqueue(s2::cmd::SetSpeed{2.0});
    engine.step(1);
    SUCCEED();
}
