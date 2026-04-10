/**
 * @file test_color_plugin.cpp
 * Тесты плагина ColorPlugin.
 */

#include <s2/plugins/color.hpp>
#include <s2/agent.hpp>
#include <s2/kinematic_tree.hpp>
#include <gtest/gtest.h>

namespace
{

s2::Agent make_simple_agent(const std::string& color = "#FF6B35")
{
    s2::Agent agent;
    agent.id = 0;
    agent.name = "test_robot";
    agent.visual.color = color;
    return agent;
}

s2::Agent make_urdf_agent()
{
    s2::Agent agent;
    agent.id = 1;
    agent.name = "urdf_robot";

    auto tree = std::make_unique<s2::KinematicTree>();

    s2::Link base;
    base.name = "base_link";
    base.parent = "";
    base.joint.type = s2::JointType::FIXED;
    base.visual.type = "box";
    base.visual.color = "#FF6B35";
    tree->add_link(std::move(base));

    s2::Link arm;
    arm.name = "arm";
    arm.parent = "base_link";
    arm.joint.type = s2::JointType::REVOLUTE;
    arm.joint.axis = s2::Vec3{0, 1, 0};
    arm.joint.min = -1.57;
    arm.joint.max =  1.57;
    arm.visual.type = "cylinder";
    arm.visual.color = "#4ECDC4";
    tree->add_link(std::move(arm));

    // Звено без визуала — плагин не трогает
    s2::Link no_vis;
    no_vis.name = "sensor_link";
    no_vis.parent = "arm";
    no_vis.joint.type = s2::JointType::FIXED;
    no_vis.visual.type = "";
    tree->add_link(std::move(no_vis));

    agent.kinematic_tree = std::move(tree);
    return agent;
}

YAML::Node make_config(const std::string& color = "#FF0000", double duration = 3.0)
{
    YAML::Node node;
    node["service"]  = "/set_color";
    node["color"]    = color;
    node["duration"] = duration;
    return node;
}

} // anonymous namespace

// ─── Базовые свойства ─────────────────────────────────────────────────────────

TEST(ColorPluginTest, TypeIsCorrect)
{
    s2::plugins::ColorPlugin plugin;
    EXPECT_EQ(plugin.type(), "color");
}

TEST(ColorPluginTest, DefaultServiceName)
{
    s2::plugins::ColorPlugin plugin;
    EXPECT_EQ(plugin.service_names(), std::vector<std::string>{"/set_color"});
}

TEST(ColorPluginTest, FromConfigSetsFields)
{
    s2::plugins::ColorPlugin plugin;
    plugin.from_config(make_config("#AABBCC", 7.5));
    // Проверяем косвенно через to_json после триггера
    auto agent = make_simple_agent();
    plugin.initialize(agent);
    plugin.handle_service("/set_color", "");
    plugin.update(0.1, agent);
    EXPECT_EQ(agent.visual.color, "#AABBCC");
}

// ─── initialize() ─────────────────────────────────────────────────────────────

TEST(ColorPluginTest, InitializeStoresOriginalColorSimple)
{
    s2::plugins::ColorPlugin plugin;
    plugin.from_config(make_config());
    auto agent = make_simple_agent("#AABBCC");
    plugin.initialize(agent);

    // Без триггера update должен восстанавливать исходный цвет
    plugin.update(0.1, agent);
    EXPECT_EQ(agent.visual.color, "#AABBCC");
}

TEST(ColorPluginTest, InitializeStoresLinkColorsForUrdf)
{
    s2::plugins::ColorPlugin plugin;
    plugin.from_config(make_config());
    auto agent = make_urdf_agent();
    plugin.initialize(agent);

    // Без триггера — исходные цвета звеньев сохраняются
    plugin.update(0.1, agent);
    for (const auto& link : agent.kinematic_tree->links())
    {
        if (link.name == "base_link") EXPECT_EQ(link.visual.color, "#FF6B35");
        if (link.name == "arm")       EXPECT_EQ(link.visual.color, "#4ECDC4");
    }
}

// ─── handle_service() — триггер ───────────────────────────────────────────────

TEST(ColorPluginTest, ServiceCallChangesColorSimple)
{
    s2::plugins::ColorPlugin plugin;
    plugin.from_config(make_config("#FF0000", 5.0));
    auto agent = make_simple_agent("#FF6B35");
    plugin.initialize(agent);

    std::string resp = plugin.handle_service("/set_color", "");
    EXPECT_NE(resp.find("true"), std::string::npos);

    plugin.update(0.1, agent);
    EXPECT_EQ(agent.visual.color, "#FF0000");
}

TEST(ColorPluginTest, ServiceCallChangesAllLinkColorsForUrdf)
{
    s2::plugins::ColorPlugin plugin;
    plugin.from_config(make_config("red", 3.0));
    auto agent = make_urdf_agent();
    plugin.initialize(agent);

    plugin.handle_service("/set_color", "");
    plugin.update(0.1, agent);

    for (const auto& link : agent.kinematic_tree->links())
    {
        if (!link.visual.type.empty())
            EXPECT_EQ(link.visual.color, "red") << "link: " << link.name;
    }
}

// ─── Таймер ───────────────────────────────────────────────────────────────────

TEST(ColorPluginTest, TimerExpiryRestoresDefaultColorSimple)
{
    s2::plugins::ColorPlugin plugin;
    plugin.from_config(make_config("#FF0000", 0.5));
    auto agent = make_simple_agent("#FF6B35");
    plugin.initialize(agent);

    plugin.handle_service("/set_color", "");

    plugin.update(0.2, agent);
    EXPECT_EQ(agent.visual.color, "#FF0000");

    plugin.update(0.2, agent);
    EXPECT_EQ(agent.visual.color, "#FF0000");

    // Таймер истёк
    plugin.update(0.2, agent);
    EXPECT_EQ(agent.visual.color, "#FF6B35");
}

TEST(ColorPluginTest, TimerExpiryRestoresLinkColorsForUrdf)
{
    s2::plugins::ColorPlugin plugin;
    plugin.from_config(make_config("blue", 0.3));
    auto agent = make_urdf_agent();
    plugin.initialize(agent);

    plugin.handle_service("/set_color", "");

    plugin.update(0.2, agent);
    for (const auto& link : agent.kinematic_tree->links())
        if (!link.visual.type.empty())
            EXPECT_EQ(link.visual.color, "blue");

    // Таймер истёк
    plugin.update(0.2, agent);
    for (const auto& link : agent.kinematic_tree->links())
    {
        if (link.name == "base_link") EXPECT_EQ(link.visual.color, "#FF6B35");
        if (link.name == "arm")       EXPECT_EQ(link.visual.color, "#4ECDC4");
    }
}

TEST(ColorPluginTest, ZeroDurationImmediateReset)
{
    s2::plugins::ColorPlugin plugin;
    plugin.from_config(make_config("#FF0000", 0.0));
    auto agent = make_simple_agent("#FF6B35");
    plugin.initialize(agent);

    plugin.handle_service("/set_color", "");
    // timer_ == 0 → первый update сразу восстанавливает
    plugin.update(0.1, agent);
    EXPECT_EQ(agent.visual.color, "#FF6B35");
}

// ─── to_json() ────────────────────────────────────────────────────────────────

TEST(ColorPluginTest, ToJsonContainsColorAndRemaining)
{
    s2::plugins::ColorPlugin plugin;
    plugin.from_config(make_config("#FF0000", 3.0));
    auto agent = make_simple_agent();
    plugin.initialize(agent);

    plugin.handle_service("/set_color", "");
    plugin.update(0.5, agent);

    std::string json = plugin.to_json();
    EXPECT_NE(json.find("active_color"), std::string::npos);
    EXPECT_NE(json.find("#FF0000"), std::string::npos);
    EXPECT_NE(json.find("remaining"), std::string::npos);
}

TEST(ColorPluginTest, ToJsonRemainingZeroWhenIdle)
{
    s2::plugins::ColorPlugin plugin;
    plugin.from_config(make_config());
    auto agent = make_simple_agent();
    plugin.initialize(agent);

    std::string json = plugin.to_json();
    EXPECT_NE(json.find("\"remaining\":0"), std::string::npos);
}
