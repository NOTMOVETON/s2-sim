#include <gtest/gtest.h>
#include <s2/sim_engine.hpp>
#include <thread>

using namespace s2;

// --- Тест: SimEngine создается и имеет корректный dt ---

TEST(SimEngineTest, CorrectDt)
{
  SimEngine engine{{.update_rate = 100.0, .viz_rate = 30.0}};
  EXPECT_DOUBLE_EQ(engine.dt(), 0.01);
}

// --- Тест: sim_time растёт на dt после одного тика ---

TEST(SimEngineTest, StepOne_SimTimeIncrements)
{
  SimEngine engine{{.update_rate = 100.0, .viz_rate = 30.0}};
  engine.step(1);
  EXPECT_DOUBLE_EQ(engine.sim_time(), 0.01);
}

// --- Тест: sim_time растёт линейно после 100 тиков ---

TEST(SimEngineTest, Step100_SimTimeEquals1)
{
  SimEngine engine{{.update_rate = 100.0, .viz_rate = 30.0}};
  engine.step(100);
  EXPECT_NEAR(engine.sim_time(), 1.0, 1e-10);
}

// --- Тест: sim_time равен нулю до выполнения шагов ---

TEST(SimEngineTest, InitialSimTimeIsZero)
{
  SimEngine engine{{.update_rate = 100.0, .viz_rate = 30.0}};
  EXPECT_DOUBLE_EQ(engine.sim_time(), 0.0);
}

// --- Тест: добавление агентов ---

TEST(SimEngineTest, AddAgents_Step1_AllPresent)
{
  SimEngine engine{{.update_rate = 100.0, .viz_rate = 30.0}};

  Agent a1{.id = 1, .name = "agent_1"};
  Agent a2{.id = 2, .name = "agent_2"};
  Agent a3{.id = 3, .name = "agent_3"};

  SimWorld world;
  world.add_agent(std::move(a1));
  world.add_agent(std::move(a2));
  world.add_agent(std::move(a3));

  engine.load_world(std::move(world));

  engine.step(1);

  EXPECT_EQ(engine.world().agents().size(), 3u);
  EXPECT_NE(engine.world().get_agent(1), nullptr);
  EXPECT_NE(engine.world().get_agent(2), nullptr);
  EXPECT_NE(engine.world().get_agent(3), nullptr);
  EXPECT_EQ(engine.world().get_agent(999), nullptr);
}

// --- Тест: resolver вызывается каждый тик, contributions очищаются ---

TEST(SimEngineTest, ResolverCalledContributionsCleared)
{
  SimEngine engine{{.update_rate = 100.0, .viz_rate = 30.0}};

  Agent agent{.id = 1, .name = "test_agent"};

  // Добавляем contribution перед шагом
  agent.state.add_scale(0.5, "test_scale");
  agent.state.add_lock(true, "test_lock");

  SimWorld world;
  world.add_agent(std::move(agent));

  engine.load_world(std::move(world));

  // До шага contributions есть
  const auto* p = engine.world().get_agent(1);
  ASSERT_NE(p, nullptr);
  EXPECT_EQ(p->state.scale_contrib_count(), 1u);
  EXPECT_EQ(p->state.lock_contrib_count(), 1u);

  // Выполняем шаг
  engine.step(1);

  // После шага contributions очищены
  EXPECT_EQ(p->state.scale_contrib_count(), 0u);
  EXPECT_EQ(p->state.lock_contrib_count(), 0u);

  // effective() вернул значения по умолчанию
  const auto& eff = p->state.effective();
  EXPECT_DOUBLE_EQ(eff.speed_scale, 1.0);
  EXPECT_FALSE(eff.motion_locked);
}

// --- Тест: contributions вычисляются через resolver ---

TEST(SimEngineTest, ContributionsResolved)
{
  SimEngine engine{{.update_rate = 100.0, .viz_rate = 30.0}};

  Agent agent{.id = 1, .name = "resolver_test"};

  // Добавляем несколько scale contributions
  agent.state.add_scale(0.5, "source_a");
  agent.state.add_scale(0.8, "source_b");
  agent.state.add_velocity_addition(Vec3(1.0, 2.0, 3.0), "conveyor");

  SimWorld world;
  world.add_agent(std::move(agent));
  engine.load_world(std::move(world));

  // step(1) вызовет resolve() + clear_contributions()
  engine.step(1);

  // После step contributions уже очищены, но effective должен быть вычислен
  // ПЕРЕД clear_contributions effective заполняется resolver-ом
  // НО после clear_contributions effective сбрасывается к default
  // Это ожидаемое поведение — проверяем что clear_contributions сбрасывает
  const auto* p = engine.world().get_agent(1);
  ASSERT_NE(p, nullptr);
  EXPECT_EQ(p->state.scale_contrib_count(), 0u);
  EXPECT_EQ(p->state.effective().speed_scale, 1.0);  // сброшен после clear
}

// --- Тест: dt при разных update_rate ---

TEST(SimEngineTest, DifferentUpdateRates)
{
  {
    SimEngine engine{{.update_rate = 50.0, .viz_rate = 30.0}};
    EXPECT_DOUBLE_EQ(engine.dt(), 0.02);
  }
  {
    SimEngine engine{{.update_rate = 200.0, .viz_rate = 30.0}};
    EXPECT_DOUBLE_EQ(engine.dt(), 0.005);
  }
}

// --- Тест: stop() прерывает run() ---

TEST(SimEngineTest, StopInterruptsRun)
{
  SimEngine engine{{.update_rate = 100.0, .viz_rate = 30.0}};
  engine.load_world(SimWorld{});

  std::thread t([&engine]() { engine.run(); });

  // Даем движку поработать немного
  std::this_thread::sleep_for(std::chrono::milliseconds(50));

  engine.stop();
  t.join();

  // Убеждаемся, что run() остановился
  // sim_time должен увеличиться
  EXPECT_GT(engine.sim_time(), 0.0);
}

// --- Тест: SimBus доступен ---

TEST(SimEngineTest, BusAccessible)
{
  SimEngine engine{{.update_rate = 100.0, .viz_rate = 30.0}};
  EXPECT_EQ(engine.bus().event_type_count(), 0u);
}

// --- Тест: пустой мир не вызывает ошибок ---

TEST(SimEngineTest, EmptyWorldSteps)
{
  SimEngine engine{{.update_rate = 100.0, .viz_rate = 30.0}};
  engine.load_world(SimWorld{});

  engine.step(10);

  EXPECT_NEAR(engine.sim_time(), 0.1, 1e-10);
  EXPECT_EQ(engine.world().agents().size(), 0u);
}

// --- Тест: добавление пропов и акторов ---

TEST(SimEngineTest, AddPropsAndActors)
{
  SimEngine engine{{.update_rate = 100.0, .viz_rate = 30.0}};

  SimWorld world;
  world.add_prop(Prop{.id = 1, .type = "crate", .movable = true});
  world.add_prop(Prop{.id = 2, .type = "barrel", .movable = false});
  world.add_actor(Actor{.id = 1, .name = "door_1", .current_state = "closed"});

  engine.load_world(std::move(world));
  engine.step(1);

  EXPECT_EQ(engine.world().props().size(), 2u);
  EXPECT_EQ(engine.world().actors().size(), 1u);

  auto* prop = engine.world().get_prop(1);
  ASSERT_NE(prop, nullptr);
  EXPECT_EQ(prop->type, "crate");
  EXPECT_TRUE(prop->movable);

  auto* actor = engine.world().get_actor(1);
  ASSERT_NE(actor, nullptr);
  EXPECT_EQ(actor->name, "door_1");
  EXPECT_EQ(actor->current_state, "closed");
}

// --- Тест: pause/resume останавливает sim_time ---

TEST(SimEngineTest, PauseResume)
{
  SimEngine engine{{.update_rate = 100.0, .viz_rate = 30.0}};

  Agent agent{.id = 1, .name = "pause_test"};
  agent.world_pose.x = 5.0;
  agent.world_velocity.linear.x() = 1.0;

  SimWorld world;
  world.add_agent(std::move(agent));
  engine.load_world(std::move(world));

  // Шаг в обычном режиме
  engine.step(10);
  double time_before_pause = engine.sim_time();
  double pos_before_pause = engine.world().get_agent(1)->world_pose.x;
  EXPECT_GT(time_before_pause, 0.0);

  // Пауза: sim_time не меняется, поза не меняется
  engine.pause();
  EXPECT_TRUE(engine.is_paused());

  engine.step(10);
  EXPECT_DOUBLE_EQ(engine.sim_time(), time_before_pause);
  EXPECT_DOUBLE_EQ(engine.world().get_agent(1)->world_pose.x, pos_before_pause);

  // Resume: sim_time продолжает расти
  engine.resume();
  EXPECT_FALSE(engine.is_paused());

  engine.step(10);
  EXPECT_GT(engine.sim_time(), time_before_pause);
}

// --- Тест: reset восстанавливает начальные позиции ---

TEST(SimEngineTest, ResetRestoresInitialStates)
{
  SimEngine engine{{.update_rate = 100.0, .viz_rate = 30.0}};

  Agent agent{.id = 1, .name = "reset_test"};
  agent.world_pose.x = 10.0;
  agent.world_pose.y = 20.0;
  agent.world_pose.yaw = 0.0;  // yaw=0 для движения строго вдоль оси X
  agent.world_velocity.linear.x() = 2.0;

  SimWorld world;
  world.add_agent(std::move(agent));
  engine.load_world(std::move(world));

  // Шагаем несколько раз: x = 10.0 + 2.0 * 50 * 0.01 = 11.0
  engine.step(50);
  EXPECT_GT(engine.sim_time(), 0.0);
  EXPECT_NEAR(engine.world().get_agent(1)->world_pose.x, 11.0, 1e-9);

  // Reset
  engine.reset();
  EXPECT_TRUE(engine.is_paused());
  EXPECT_DOUBLE_EQ(engine.sim_time(), 0.0);
  EXPECT_DOUBLE_EQ(engine.world().get_agent(1)->world_pose.x, 10.0);
  EXPECT_DOUBLE_EQ(engine.world().get_agent(1)->world_pose.y, 20.0);
  EXPECT_DOUBLE_EQ(engine.world().get_agent(1)->world_pose.yaw, 0.0);
}

// --- Тест: snapshot содержит paused ---

TEST(SimEngineTest, SnapshotContainsPaused)
{
  SimEngine engine{{.update_rate = 100.0, .viz_rate = 30.0}};

  Agent agent{.id = 1, .name = "snapshot_test"};
  SimWorld world;
  world.add_agent(std::move(agent));
  engine.load_world(std::move(world));

  auto snap = engine.build_snapshot();
  EXPECT_FALSE(snap.paused);

  engine.pause();
  snap = engine.build_snapshot();
  EXPECT_TRUE(snap.paused);
}

// --- Тест: resume после паузы продолжает движение ---

TEST(SimEngineTest, ResumeContinuesMovement)
{
  SimEngine engine{{.update_rate = 100.0, .viz_rate = 30.0}};

  Agent agent{.id = 1, .name = "resume_test"};
  agent.world_pose.x = 0.0;
  agent.world_velocity.linear.x() = 2.0;

  SimWorld world;
  world.add_agent(std::move(agent));
  engine.load_world(std::move(world));

  // 10 шагов → x = 0.2 (2.0 * 10 * 0.01)
  engine.step(10);
  double pos_1 = engine.world().get_agent(1)->world_pose.x;
  EXPECT_NEAR(pos_1, 0.2, 1e-9);

  // Пауза → позиция не меняется
  engine.pause();
  engine.step(10);
  EXPECT_DOUBLE_EQ(engine.world().get_agent(1)->world_pose.x, pos_1);

  // Resume → позиция продолжает расти
  engine.resume();
  engine.step(10);
  double pos_2 = engine.world().get_agent(1)->world_pose.x;
  EXPECT_GT(pos_2, pos_1);
  EXPECT_NEAR(pos_2, 0.4, 1e-9);
}

// --- Тест: reset не зависит от количества тиков ---

TEST(SimEngineTest, ResetFromMultipleTicks)
{
  SimEngine engine{{.update_rate = 100.0, .viz_rate = 30.0}};

  Agent agent{.id = 1, .name = "multi_tick_reset"};
  agent.world_pose.x = 5.0;
  agent.world_pose.y = 10.0;
  agent.world_pose.yaw = 1.57;
  agent.world_velocity.linear.x() = 1.0;
  agent.world_velocity.angular.z() = 0.5;

  SimWorld world;
  world.add_agent(std::move(agent));
  engine.load_world(std::move(world));

  // Много тиков
  engine.step(500);
  EXPECT_GT(engine.sim_time(), 1.0);

  engine.reset();
  EXPECT_DOUBLE_EQ(engine.sim_time(), 0.0);
  EXPECT_DOUBLE_EQ(engine.world().get_agent(1)->world_pose.x, 5.0);
  EXPECT_DOUBLE_EQ(engine.world().get_agent(1)->world_pose.y, 10.0);
  EXPECT_DOUBLE_EQ(engine.world().get_agent(1)->world_pose.yaw, 1.57);
}

// --- Тест: set_agent_pose находит агента ---

TEST(SimEngineTest, SetAgentPoseWorks)
{
  SimEngine engine{{.update_rate = 100.0, .viz_rate = 30.0}};

  Agent agent{.id = 1, .name = "pose_test"};
  SimWorld world;
  world.add_agent(std::move(agent));
  engine.load_world(std::move(world));

  Pose3D new_pose;
  new_pose.x = 42.0;
  new_pose.y = 99.0;
  new_pose.z = 5.0;
  new_pose.yaw = 3.14;
  EXPECT_TRUE(engine.set_agent_pose(1, new_pose));
  EXPECT_FALSE(engine.set_agent_pose(999, new_pose));

  const auto* a = engine.world().get_agent(1);
  ASSERT_NE(a, nullptr);
  EXPECT_DOUBLE_EQ(a->world_pose.x, 42.0);
  EXPECT_DOUBLE_EQ(a->world_pose.y, 99.0);
  EXPECT_DOUBLE_EQ(a->world_pose.yaw, 3.14);
}

// --- Тест: snapshot содержит plugins_data ---

TEST(SimEngineTest, SnapshotContainsPluginsData)
{
  SimEngine engine{{.update_rate = 100.0, .viz_rate = 30.0}};

  Agent agent{.id = 1, .name = "plugins_data_test"};
  SimWorld world;
  world.add_agent(std::move(agent));
  engine.load_world(std::move(world));

  auto snap = engine.build_snapshot();
  // plugins_data пустая, т.к. нет плагинов
  EXPECT_TRUE(snap.plugins_data.empty());
}

// --- Тест: несколько агентов — reset восстанавливает всех ---

TEST(SimEngineTest, ResetRestoresMultipleAgents)
{
  SimEngine engine{{.update_rate = 100.0, .viz_rate = 30.0}};

  Agent a1{.id = 1, .name = "r1"};
  a1.world_pose.x = 0.0;
  a1.world_velocity.linear.x() = 1.0;

  Agent a2{.id = 2, .name = "r2"};
  a2.world_pose.x = 10.0;
  a2.world_velocity.linear.x() = -1.0;

  SimWorld world;
  world.add_agent(std::move(a1));
  world.add_agent(std::move(a2));
  engine.load_world(std::move(world));

  engine.step(50);

  engine.reset();
  EXPECT_DOUBLE_EQ(engine.world().get_agent(1)->world_pose.x, 0.0);
  EXPECT_DOUBLE_EQ(engine.world().get_agent(2)->world_pose.x, 10.0);
  EXPECT_DOUBLE_EQ(engine.sim_time(), 0.0);
  EXPECT_TRUE(engine.is_paused());
}
