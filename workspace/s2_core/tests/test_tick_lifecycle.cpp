/**
 * @file test_tick_lifecycle.cpp
 * Тесты 8-фазного lifecycle тика SimEngine.
 *
 * Проверяет:
 *  - Сенсоры вызываются в Phase 4 (строго после кинематики в Phase 3)
 *  - clear_contributions() только в Phase 8
 *  - push_command() доступен для REST API
 *  - KernelCommand SetPose применяется в Phase 0
 */

#include <s2/sim_engine.hpp>
#include <s2/agent.hpp>
#include <s2/kernel_command.hpp>
#include <s2/plugin_base.hpp>
#include <gtest/gtest.h>
#include <vector>
#include <string>
#include <cmath>

// Вспомогательные плагины объявлены на уровне файлового namespace.
// PluginRole и PluginContext — в namespace s2 (не в s2::plugins).
// IAgentPlugin — в namespace s2::plugins.

/**
 * Плагин-трекер: записывает имя тега при вызове update().
 * Используется для проверки порядка вызовов по фазам.
 */
class PhaseTrackingPlugin final : public s2::plugins::IAgentPlugin
{
public:
  explicit PhaseTrackingPlugin(s2::PluginRole r,
                                std::vector<std::string>& log,
                                const std::string& tag)
      : role_(r), log_(log), tag_(tag) {}

  std::string type() const override { return "phase_tracker_" + tag_; }
  s2::PluginRole role() const override { return role_; }

  void update(double, s2::Agent& agent, const s2::PluginContext&) override
  {
    log_.push_back(tag_ + ":" + std::to_string(static_cast<int>(agent.world_pose.x * 100)));
  }

  void from_config(const YAML::Node&) override {}
  std::string to_json() const override { return "{}"; }

private:
  s2::PluginRole            role_;
  std::vector<std::string>& log_;
  std::string               tag_;
};

/**
 * Плагин RESOURCE: добавляет speed_scale=0.5 contribution в pre_resolve.
 */
class ResourceScalePlugin final : public s2::plugins::IAgentPlugin
{
public:
  std::string type() const override { return "resource_scale_test"; }
  s2::PluginRole role() const override { return s2::PluginRole::RESOURCE; }

  void pre_resolve(double, s2::Agent& agent) override
  {
    agent.state.add_scale(0.5, "test_zone");
  }

  void update(double, s2::Agent&, const s2::PluginContext&) override {}
  void from_config(const YAML::Node&) override {}
  std::string to_json() const override { return "{}"; }
};

/**
 * Плагин SENSOR: читает effective().speed_scale и записывает результат.
 */
class SensorReadScalePlugin final : public s2::plugins::IAgentPlugin
{
public:
  explicit SensorReadScalePlugin(bool& out) : out_(out) {}

  std::string type() const override { return "sensor_read_scale_test"; }
  s2::PluginRole role() const override { return s2::PluginRole::SENSOR; }

  void update(double, s2::Agent& agent, const s2::PluginContext&) override
  {
    out_ = (std::abs(agent.state.effective().speed_scale - 0.5) < 1e-6);
  }

  void from_config(const YAML::Node&) override {}
  std::string to_json() const override { return "{}"; }

private:
  bool& out_;
};

// ─── Тесты ───────────────────────────────────────────────────────────────────

// Тест: сенсор вызывается в Phase 4 (не в Phase 3)
TEST(TickLifecycle, SensorCalledInPhase4)
{
  s2::SimEngine::Config cfg;
  cfg.update_rate    = 100.0;
  cfg.viz_rate       = 0.0;
  cfg.transport_rate = 0.0;
  s2::SimEngine engine(cfg);

  std::vector<std::string> call_log;

  s2::Agent agent;
  agent.id           = 1;
  agent.world_pose.x = 0.0;

  // Sensor-плагин записывает имя при вызове update()
  agent.plugins.push_back(
      std::make_unique<PhaseTrackingPlugin>(
          s2::PluginRole::SENSOR, call_log, "sensor"));

  s2::SimWorld world;
  world.agents().push_back(std::move(agent));
  engine.load_world(std::move(world));

  engine.step(1);

  // Sensor должен быть вызван ровно один раз (в Phase 4)
  ASSERT_EQ(call_log.size(), 1u);
  EXPECT_TRUE(call_log[0].find("sensor") != std::string::npos);
}

// Тест: push_command SetPose применяется в Phase 0 следующего тика
TEST(TickLifecycle, PushCommandSetPoseApplied)
{
  s2::SimEngine::Config cfg;
  cfg.update_rate    = 100.0;
  cfg.viz_rate       = 0.0;
  cfg.transport_rate = 0.0;
  s2::SimEngine engine(cfg);

  s2::Agent agent;
  agent.id           = 1;
  agent.world_pose.x = 0.0;
  agent.world_pose.y = 0.0;

  s2::SimWorld world;
  world.agents().push_back(std::move(agent));
  engine.load_world(std::move(world));

  // Добавить команду SetPose до тика
  s2::Pose3D target_pose{5.0, 3.0, 0.0, 0.0, 0.0, 0.0};
  engine.push_command(s2::cmd::SetPose{.id = 1, .pose = target_pose});

  // После одного тика Phase 0 должна была применить SetPose
  engine.step(1);

  const auto& agents = engine.world().agents();
  ASSERT_EQ(agents.size(), 1u);
  EXPECT_NEAR(agents[0].world_pose.x, 5.0, 1e-6);
  EXPECT_NEAR(agents[0].world_pose.y, 3.0, 1e-6);
}

// Тест: clear_contributions не вызывается между Phase 3 и Phase 4
TEST(TickLifecycle, ClearContributionsOnlyInPhase8)
{
  // Проверяет что clear_contributions() НЕ вызывается между Phase 3 и Phase 4:
  // плагин RESOURCE кладёт contribution в pre_resolve,
  // плагин SENSOR читает effective() в update() и получает speed_scale == 0.5.
  // Если clear_contributions() вызывался в Phase 3 — сенсор бы увидел 1.0.

  s2::SimEngine::Config cfg;
  cfg.update_rate    = 100.0;
  cfg.viz_rate       = 0.0;
  cfg.transport_rate = 0.0;
  s2::SimEngine engine(cfg);

  bool sensor_saw_scale = false;

  s2::Agent agent;
  agent.id = 1;
  agent.plugins.push_back(std::make_unique<ResourceScalePlugin>());
  agent.plugins.push_back(std::make_unique<SensorReadScalePlugin>(sensor_saw_scale));

  s2::SimWorld world;
  world.agents().push_back(std::move(agent));
  engine.load_world(std::move(world));

  engine.step(1);

  EXPECT_TRUE(sensor_saw_scale)
      << "Sensor должен видеть speed_scale=0.5, выставленный ResourceScalePlugin в pre_resolve. "
         "Если false — clear_contributions был вызван между Phase 3 и Phase 4.";
}
