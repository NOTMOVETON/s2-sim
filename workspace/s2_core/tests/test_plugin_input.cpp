#include <gtest/gtest.h>
#include <s2/sim_engine.hpp>
#include <s2/agent.hpp>
#include <s2/plugins/diff_drive.hpp>
#include <cmath>

using namespace s2;
using namespace s2::plugins;

// --- Тест: handle_plugin_input находит правильный плагин у правильного агента ---

TEST(PluginInputTest, PluginInputTargetsCorrectAgent)
{
    SimEngine engine{{.update_rate = 100.0, .viz_rate = 30.0}};

    // Создаём двух агентов с плагинами diff_drive
    Agent agent1{.id = 1, .name = "robot_0"};
    agent1.plugins.push_back(std::make_unique<DiffDrivePlugin>());

    Agent agent2{.id = 2, .name = "robot_1"};
    agent2.plugins.push_back(std::make_unique<DiffDrivePlugin>());

    SimWorld world;
    world.add_agent(std::move(agent1));
    world.add_agent(std::move(agent2));
    engine.load_world(std::move(world));

    // Отправляем команду только robot_1
    bool result = engine.handle_plugin_input(2, "diff_drive",
        R"({"linear_velocity": 1.0, "angular_velocity": 0.0})");

    EXPECT_TRUE(result);

    // Проверяем: только robot_1 должен получить скорость
    const auto* a1 = engine.world().get_agent(1);
    const auto* a2 = engine.world().get_agent(2);
    ASSERT_NE(a1, nullptr);
    ASSERT_NE(a2, nullptr);

    // До tick() скорость ещё не применена плагинами
    // Вызываем tick чтобы плагины обновились
    engine.step(1);

    // robot_0 должен стоять (скорость 0)
    EXPECT_NEAR(a1->world_velocity.linear.x(), 0.0, 1e-10);
    // robot_1 должен двигаться
    EXPECT_NEAR(a2->world_velocity.linear.x(), 1.0, 1e-10);
}

// --- Тест: External input сохраняется (latch) до явной отмены ---

TEST(PluginInputTest, ExternalInputLatches)
{
    SimEngine engine{{.update_rate = 100.0, .viz_rate = 30.0}};

    Agent agent{.id = 1, .name = "test_robot"};
    agent.plugins.push_back(std::make_unique<DiffDrivePlugin>());

    SimWorld world;
    world.add_agent(std::move(agent));
    engine.load_world(std::move(world));

    // Отправляем команду скорости один раз
    engine.handle_plugin_input(1, "diff_drive",
        R"({"linear_velocity": 1.0, "angular_velocity": 0.0})");

    // Первый тик: команда применяется
    engine.step(1);

    const auto* a = engine.world().get_agent(1);
    ASSERT_NE(a, nullptr);
    EXPECT_NEAR(a->world_velocity.linear.x(), 1.0, 1e-10);

    // Второй тик: скорость сохраняется (latch-поведение)
    engine.step(1);
    EXPECT_NEAR(a->world_velocity.linear.x(), 1.0, 1e-10);

    // Третий тик: по-прежнему 1.0
    engine.step(1);
    EXPECT_NEAR(a->world_velocity.linear.x(), 1.0, 1e-10);

    // Явная остановка через команду с нулевой скоростью
    engine.handle_plugin_input(1, "diff_drive",
        R"({"linear_velocity": 0.0, "angular_velocity": 0.0})");
    engine.step(1);
    EXPECT_NEAR(a->world_velocity.linear.x(), 0.0, 1e-10);
}

// --- Тест: позиция продолжает двигаться после одной команды (latch) ---

TEST(PluginInputTest, ExternalInputCausesContinuousMotion)
{
    SimEngine engine{{.update_rate = 100.0, .viz_rate = 30.0}};

    Agent agent{.id = 1, .name = "continuous_robot"};
    agent.world_pose.yaw = 0.0;  // смотрит по оси X
    agent.plugins.push_back(std::make_unique<DiffDrivePlugin>());

    SimWorld world;
    world.add_agent(std::move(agent));
    engine.load_world(std::move(world));

    // Отправляем команду один раз
    engine.handle_plugin_input(1, "diff_drive",
        R"({"linear_velocity": 2.0, "angular_velocity": 0.0})");

    const auto* a = engine.world().get_agent(1);
    double start_x = a->world_pose.x;

    // Первый тик: позиция изменяется (dt=0.01, v=2.0 → dx=0.02)
    engine.step(1);
    double tick1_x = a->world_pose.x;
    EXPECT_NEAR(tick1_x - start_x, 0.02, 1e-10);

    // Второй тик: скорость сохранена (latch), позиция продолжает меняться
    engine.step(1);
    EXPECT_NEAR(a->world_pose.x - tick1_x, 0.02, 1e-10);
}

// --- Тест: несколько команд подряд (для постоянного движения) ---

TEST(PluginInputTest, MultipleInputsForContinuousMotion)
{
    SimEngine engine{{.update_rate = 100.0, .viz_rate = 30.0}};

    Agent agent{.id = 1, .name = "continuous_robot"};
    agent.world_pose.yaw = 0.0;
    agent.plugins.push_back(std::make_unique<DiffDrivePlugin>());

    SimWorld world;
    world.add_agent(std::move(agent));
    engine.load_world(std::move(world));

    // Каждый тик отправляем команду — движение должно быть постоянным
    for (int i = 0; i < 10; ++i) {
        engine.handle_plugin_input(1, "diff_drive",
            R"({"linear_velocity": 1.0, "angular_velocity": 0.0})");
        engine.step(1);
    }

    const auto* a = engine.world().get_agent(1);
    ASSERT_NE(a, nullptr);
    // После 10 тиков: 10 * 0.01 * 1.0 = 0.1
    EXPECT_NEAR(a->world_pose.x, 0.1, 1e-9);
}

// --- Тест: handle_plugin_input возвращает false для несуществующего агента ---

TEST(PluginInputTest, ReturnsFalseForNonexistentAgent)
{
    SimEngine engine{{.update_rate = 100.0, .viz_rate = 30.0}};
    engine.load_world(SimWorld{});

    bool result = engine.handle_plugin_input(999, "diff_drive",
        R"({"linear_velocity": 1.0})");
    EXPECT_FALSE(result);
}

// --- Тест: handle_plugin_input возвращает false для неизвестного плагина ---

TEST(PluginInputTest, ReturnsFalseForUnknownPlugin)
{
    SimEngine engine{{.update_rate = 100.0, .viz_rate = 30.0}};

    Agent agent{.id = 1, .name = "test"};
    SimWorld world;
    world.add_agent(std::move(agent));
    engine.load_world(std::move(world));

    bool result = engine.handle_plugin_input(1, "diff_drive",
        R"({"linear_velocity": 1.0})");
    EXPECT_FALSE(result);  // Агент есть, но без diff_drive плагина
}

// --- Тест: два агента с ID 0 и 1 (как в реальной сцене test_two_robots.yaml) ---

TEST(PluginInputTest, TwoAgentsWithZeroAndOneIds)
{
    SimEngine engine{{.update_rate = 100.0, .viz_rate = 30.0}};

    // robot_0 с ID 0
    Agent robot0{.id = 0, .name = "robot_0"};
    robot0.world_pose.x = -3.0;
    robot0.world_pose.yaw = 0.0;
    robot0.plugins.push_back(std::make_unique<DiffDrivePlugin>());

    // robot_1 с ID 1
    Agent robot1{.id = 1, .name = "robot_1"};
    robot1.world_pose.x = 3.0;
    robot1.world_pose.yaw = 0.0;
    robot1.plugins.push_back(std::make_unique<DiffDrivePlugin>());

    SimWorld world;
    world.add_agent(std::move(robot0));
    world.add_agent(std::move(robot1));
    engine.load_world(std::move(world));

    // Отправляем команду ТОЛЬКО robot_1 (ID=1)
    bool result = engine.handle_plugin_input(1, "diff_drive",
        R"({"linear_velocity": 1.5, "angular_velocity": 0.0})");
    EXPECT_TRUE(result);

    // Один тик
    engine.step(1);

    const auto* r0 = engine.world().get_agent(0);
    const auto* r1 = engine.world().get_agent(1);
    ASSERT_NE(r0, nullptr);
    ASSERT_NE(r1, nullptr);

    // robot_0 должен стоять
    EXPECT_NEAR(r0->world_velocity.linear.x(), 0.0, 1e-10)
        << "robot_0 (ID=0) should NOT move when input is sent to robot_1";

    // robot_1 должен двигаться
    EXPECT_NEAR(r1->world_velocity.linear.x(), 1.5, 1e-10)
        << "robot_1 (ID=1) should move with velocity 1.5";

    // robot_0 позиция не должна измениться
    EXPECT_NEAR(r0->world_pose.x, -3.0, 1e-10)
        << "robot_0 position should remain at -3.0";
}

// --- Тест: отправка команды robot_0 (ID=0) ---

TEST(PluginInputTest, InputToAgentZero)
{
    SimEngine engine{{.update_rate = 100.0, .viz_rate = 30.0}};

    Agent robot0{.id = 0, .name = "robot_0"};
    robot0.world_pose.x = -3.0;
    robot0.world_pose.yaw = 0.0;
    robot0.plugins.push_back(std::make_unique<DiffDrivePlugin>());

    Agent robot1{.id = 1, .name = "robot_1"};
    robot1.world_pose.x = 3.0;
    robot1.world_pose.yaw = 0.0;
    robot1.plugins.push_back(std::make_unique<DiffDrivePlugin>());

    SimWorld world;
    world.add_agent(std::move(robot0));
    world.add_agent(std::move(robot1));
    engine.load_world(std::move(world));

    // Отправляем команду robot_0 (ID=0)
    bool result = engine.handle_plugin_input(0, "diff_drive",
        R"({"linear_velocity": 2.0, "angular_velocity": 0.0})");
    EXPECT_TRUE(result);

    engine.step(1);

    const auto* r0 = engine.world().get_agent(0);
    const auto* r1 = engine.world().get_agent(1);
    ASSERT_NE(r0, nullptr);
    ASSERT_NE(r1, nullptr);

    // robot_0 должен двигаться
    EXPECT_NEAR(r0->world_velocity.linear.x(), 2.0, 1e-10)
        << "robot_0 (ID=0) should move with velocity 2.0";

    // robot_1 должен стоять
    EXPECT_NEAR(r1->world_velocity.linear.x(), 0.0, 1e-10)
        << "robot_1 (ID=1) should NOT move when input is sent to robot_0";
}
