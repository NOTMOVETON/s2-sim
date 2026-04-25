/**
 * @file test_plugin_roles.cpp
 * Тесты системы ролей плагинов: PluginRole enum, матрица доступа.
 */

#include <s2/plugin_base.hpp>
#include <s2/agent.hpp>
#include <gtest/gtest.h>
#include <memory>
#include <vector>

namespace s2
{
namespace plugins
{

// Плагин с ролью ACTUATION
class FakeActuationPlugin : public IAgentPlugin
{
public:
  std::string type() const override { return "fake_actuation"; }
  PluginRole  role() const override { return PluginRole::ACTUATION; }
  void update(double, Agent&, const PluginContext&) override {}
  void from_config(const YAML::Node&) override {}
  std::string to_json() const override { return "{}"; }
};

// Плагин с ролью SENSOR
class FakeSensorPlugin : public IAgentPlugin
{
public:
  std::string type() const override { return "fake_sensor"; }
  PluginRole  role() const override { return PluginRole::SENSOR; }
  std::vector<std::string> provided_capabilities() const override { return {"optical_sensor"}; }
  void update(double, Agent&, const PluginContext&) override {}
  void from_config(const YAML::Node&) override {}
  std::string to_json() const override { return "{}"; }
};

TEST(PluginRole, ActuationRoleReturned)
{
  FakeActuationPlugin p;
  EXPECT_EQ(p.role(), PluginRole::ACTUATION);
}

TEST(PluginRole, SensorRoleReturned)
{
  FakeSensorPlugin p;
  EXPECT_EQ(p.role(), PluginRole::SENSOR);
}

TEST(PluginRole, AllRolesDistinct)
{
  // enum class PluginRole содержит 5 различных значений
  EXPECT_NE(PluginRole::ACTUATION,   PluginRole::SENSOR);
  EXPECT_NE(PluginRole::ACTUATION,   PluginRole::INTERACTION);
  EXPECT_NE(PluginRole::ACTUATION,   PluginRole::RESOURCE);
  EXPECT_NE(PluginRole::ACTUATION,   PluginRole::UTILITY);
  EXPECT_NE(PluginRole::SENSOR,      PluginRole::INTERACTION);
  EXPECT_NE(PluginRole::SENSOR,      PluginRole::RESOURCE);
  EXPECT_NE(PluginRole::SENSOR,      PluginRole::UTILITY);
  EXPECT_NE(PluginRole::INTERACTION, PluginRole::RESOURCE);
  EXPECT_NE(PluginRole::INTERACTION, PluginRole::UTILITY);
  EXPECT_NE(PluginRole::RESOURCE,    PluginRole::UTILITY);
}

TEST(PluginRole, ProvidedCapabilitiesFromSensor)
{
  FakeSensorPlugin p;
  auto caps = p.provided_capabilities();
  ASSERT_EQ(caps.size(), 1u);
  EXPECT_EQ(caps[0], "optical_sensor");
}

TEST(PluginRole, ValidationActuationCountHelper)
{
  // Вспомогательная функция: count actuation plugins
  // (Реальная валидация будет в SceneLoader — Plan 06)
  std::vector<std::unique_ptr<IAgentPlugin>> plugins;
  plugins.push_back(std::make_unique<FakeActuationPlugin>());
  plugins.push_back(std::make_unique<FakeSensorPlugin>());

  int actuation_count = 0;
  for (const auto& p : plugins)
    if (p->role() == PluginRole::ACTUATION)
      actuation_count++;

  EXPECT_EQ(actuation_count, 1);
}

}  // namespace plugins
}  // namespace s2
