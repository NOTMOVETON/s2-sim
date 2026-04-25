/**
 * @file test_plugin_lifecycle.cpp
 * Тесты lifecycle-методов IAgentPlugin: on_spawn, on_despawn, on_reset, on_scene_load.
 */

#include <s2/plugin_base.hpp>
#include <s2/agent.hpp>
#include <s2/world_query.hpp>
#include <s2/event_bus.hpp>
#include <gtest/gtest.h>
#include <string>
#include <vector>

namespace s2
{
namespace plugins
{

// Тестовый плагин, отслеживающий вызовы lifecycle-методов
class TestLifecyclePlugin : public IAgentPlugin
{
public:
  std::string type() const override { return "test_lifecycle"; }
  PluginRole  role() const override { return PluginRole::UTILITY; }

  void update(double, Agent&, const PluginContext&) override {}
  void from_config(const YAML::Node&) override {}
  std::string to_json() const override { return "{}"; }

  void on_spawn(Agent&)               override { calls_.push_back("spawn"); }
  void on_despawn(Agent&)             override { calls_.push_back("despawn"); }
  void on_scene_load(const SimWorld&) override { calls_.push_back("scene_load"); }
  void on_reset(Agent&)               override { calls_.push_back("reset"); }

  const std::vector<std::string>& calls() const { return calls_; }

private:
  std::vector<std::string> calls_;
};

TEST(PluginLifecycle, OnSpawnCalled)
{
  TestLifecyclePlugin plugin;
  Agent agent;
  plugin.on_spawn(agent);
  ASSERT_EQ(plugin.calls().size(), 1u);
  EXPECT_EQ(plugin.calls()[0], "spawn");
}

TEST(PluginLifecycle, OnDespawnCalled)
{
  TestLifecyclePlugin plugin;
  Agent agent;
  plugin.on_despawn(agent);
  ASSERT_EQ(plugin.calls().size(), 1u);
  EXPECT_EQ(plugin.calls()[0], "despawn");
}

TEST(PluginLifecycle, OnResetCalled)
{
  TestLifecyclePlugin plugin;
  Agent agent;
  plugin.on_reset(agent);
  ASSERT_EQ(plugin.calls().size(), 1u);
  EXPECT_EQ(plugin.calls()[0], "reset");
}

TEST(PluginLifecycle, DefaultImplNoOp)
{
  // Базовая реализация не падает и ничего не делает
  // (используем реальный плагин, который НЕ переопределяет lifecycle)
  class MinimalPlugin : public IAgentPlugin
  {
  public:
    std::string type() const override { return "minimal"; }
    PluginRole  role() const override { return PluginRole::UTILITY; }
    void update(double, Agent&, const PluginContext&) override {}
    void from_config(const YAML::Node&) override {}
    std::string to_json() const override { return "{}"; }
    // on_spawn и т.п. — используют default-реализацию из base
  };

  MinimalPlugin plugin;
  Agent agent;
  EXPECT_NO_THROW(plugin.on_spawn(agent));
  EXPECT_NO_THROW(plugin.on_despawn(agent));
  EXPECT_NO_THROW(plugin.on_reset(agent));
}

TEST(PluginLifecycle, ProvidedCapabilitiesDefaultEmpty)
{
  TestLifecyclePlugin plugin;
  EXPECT_TRUE(plugin.provided_capabilities().empty());
}

TEST(PluginLifecycle, PluginContextHoldsReferences)
{
  WorldQuery world;
  EventBus bus;
  KernelCommandQueue cmds;
  PluginContext ctx{world, bus, cmds};

  // PluginContext корректно создаётся и хранит ссылки
  EXPECT_EQ(&ctx.world, &world);
  EXPECT_EQ(&ctx.bus, &bus);
  EXPECT_EQ(&ctx.commands, &cmds);
}

}  // namespace plugins
}  // namespace s2
