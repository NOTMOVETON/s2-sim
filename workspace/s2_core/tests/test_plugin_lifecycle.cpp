/**
 * @file test_plugin_lifecycle.cpp
 * Тесты lifecycle плагинов: on_reset(), role(), provided_capabilities().
 */

#include <gtest/gtest.h>

#include <s2/plugins/diff_drive.hpp>
#include <s2/plugins/battery.hpp>
#include <s2/components/battery_component.hpp>
#include <s2/plugin_base.hpp>
#include <s2/agent.hpp>

#include <yaml-cpp/yaml.h>

namespace s2 {
namespace plugins {

// ─── Вспомогательная ──────────────────────────────────────────────────────────

static Agent make_agent(AgentId id = 1)
{
    Agent a;
    a.id   = id;
    a.name = "test_agent";
    return a;
}

// ─── DiffDrive: on_reset() сбрасывает runtime-состояние ──────────────────────

TEST(PluginLifecycle, DiffDriveOnResetClearsVelocity)
{
    DiffDrivePlugin plugin;
    YAML::Node cfg;
    plugin.from_config(cfg);

    // Подаём команду — появляется external input
    plugin.handle_input(R"({"linear": 1.5, "angular": 0.3})");

    // После сброса команда должна исчезнуть
    plugin.on_reset();

    // Проверяем через to_json(): after reset, desired должен быть 0
    // (update нужен для публикации, но on_reset сбрасывает current_data)
    Agent agent = make_agent();
    plugin.initialize(agent);

    // Tick с dt=0 после сброса — агент не движется
    plugin.update(0.01, agent);
    const auto* dd = agent.state.get<DiffDriveData>();
    ASSERT_NE(dd, nullptr);
    EXPECT_NEAR(dd->desired_linear,  0.0, 1e-9);
    EXPECT_NEAR(dd->desired_angular, 0.0, 1e-9);
}

// ─── DiffDrive: role() и provided_capabilities() ─────────────────────────────

TEST(PluginLifecycle, DiffDriveRoleAndCaps)
{
    DiffDrivePlugin plugin;
    EXPECT_EQ(plugin.role(), PluginRole::actuation);

    auto caps = plugin.provided_capabilities();
    ASSERT_EQ(caps.size(), 1u);
    EXPECT_EQ(caps[0], "diff_drive");
}

// ─── Battery: on_reset() восстанавливает initial_level ───────────────────────

TEST(PluginLifecycle, BatteryOnResetRestoresLevel)
{
    BatteryPlugin plugin;
    YAML::Node cfg;
    cfg["initial_level"] = 0.8;
    cfg["drain_rate"]    = 1.0;  // быстрый разряд для теста
    plugin.from_config(cfg);

    Agent agent = make_agent();
    plugin.initialize(agent);

    // Разряжаем через pre_resolve
    for (int i = 0; i < 50; ++i)
        plugin.pre_resolve(0.01, agent);

    auto* bat = agent.state.get<BatteryComponent>();
    ASSERT_NE(bat, nullptr);
    EXPECT_LT(bat->level, 0.8);  // уровень снизился

    // Сброс — уровень должен вернуться к 0.8
    plugin.on_reset();

    bat = agent.state.get<BatteryComponent>();
    ASSERT_NE(bat, nullptr);
    EXPECT_NEAR(bat->level, 0.8, 1e-9);
    EXPECT_FALSE(bat->charging);
}

// ─── Battery: role() ─────────────────────────────────────────────────────────

TEST(PluginLifecycle, BatteryRole)
{
    BatteryPlugin plugin;
    EXPECT_EQ(plugin.role(), PluginRole::resource);
}

// ─── provided_capabilities() попадают в agent.capabilities ──────────────────
// (интеграция без SimTransportBridge — ручная проверка паттерна)

TEST(PluginLifecycle, ProvidedCapabilitiesRegistration)
{
    DiffDrivePlugin plugin;
    YAML::Node cfg;
    plugin.from_config(cfg);

    Agent agent = make_agent();
    plugin.initialize(agent);

    for (const auto& cap : plugin.provided_capabilities())
        agent.capabilities.insert(cap);

    EXPECT_TRUE(agent.capabilities.count("diff_drive") > 0);
}

} // namespace plugins
} // namespace s2
