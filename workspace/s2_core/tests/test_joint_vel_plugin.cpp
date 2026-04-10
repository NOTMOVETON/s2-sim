/**
 * @file test_joint_vel_plugin.cpp
 * Тесты плагина управления джоинтами через скорость.
 */

#include <s2/plugins/joint_vel.hpp>
#include <s2/agent.hpp>
#include <s2/kinematic_tree.hpp>
#include <gtest/gtest.h>

namespace
{

// Вспомогательная: создать агента с кинематическим деревом
s2::Agent make_agent_with_tree()
{
    s2::Agent agent;
    agent.id = 0;
    agent.name = "test";

    auto tree = std::make_unique<s2::KinematicTree>();

    // base_link (корень)
    s2::Link base;
    base.name = "base_link";
    base.parent = "";
    base.joint.type = s2::JointType::FIXED;
    tree->add_link(std::move(base));

    // arm — revolute, ось Y, [-1.57, 1.57]
    s2::Link arm;
    arm.name = "arm";
    arm.parent = "base_link";
    arm.joint.type = s2::JointType::REVOLUTE;
    arm.joint.axis = s2::Vec3{0, 1, 0};
    arm.joint.min = -1.57;
    arm.joint.max =  1.57;
    arm.joint.value = 0.0;
    arm.origin.x = 0.5;
    arm.origin.z = 0.2;
    tree->add_link(std::move(arm));

    // bucket — prismatic, [0, 0.5]
    s2::Link bucket;
    bucket.name = "bucket";
    bucket.parent = "arm";
    bucket.joint.type = s2::JointType::PRISMATIC;
    bucket.joint.axis = s2::Vec3{1, 0, 0};
    bucket.joint.min = 0.0;
    bucket.joint.max = 0.5;
    bucket.joint.value = 0.0;
    tree->add_link(std::move(bucket));

    agent.kinematic_tree = std::move(tree);
    return agent;
}

// Вспомогательная: создать YAML-конфиг для JointVelPlugin
YAML::Node make_config(const std::string& topic = "/cmd_vel_mount")
{
    YAML::Node node;
    node["topic"] = topic;
    YAML::Node j1;
    j1["name"] = "arm";
    j1["axis"] = "linear_x";
    j1["max_vel"] = 0.1;
    YAML::Node j2;
    j2["name"] = "bucket";
    j2["axis"] = "angular_z";
    j2["max_vel"] = 0.05;
    node["joints"].push_back(j1);
    node["joints"].push_back(j2);
    return node;
}

} // anonymous namespace

// ─── Базовые свойства ─────────────────────────────────────────────────────

TEST(JointVelPluginTest, TypeIsCorrect)
{
    s2::plugins::JointVelPlugin plugin;
    EXPECT_EQ(plugin.type(), "joint_vel");
}

TEST(JointVelPluginTest, HasInputs)
{
    s2::plugins::JointVelPlugin plugin;
    EXPECT_TRUE(plugin.has_inputs());
}

TEST(JointVelPluginTest, CommandTopicsDefault)
{
    s2::plugins::JointVelPlugin plugin;
    EXPECT_EQ(plugin.command_topics(), std::vector<std::string>{"/cmd_vel_mount"});
}

// ─── from_config ──────────────────────────────────────────────────────────

TEST(JointVelPluginTest, FromConfigSetsCustomTopic)
{
    s2::plugins::JointVelPlugin plugin;
    auto cfg = make_config("/my_topic");
    plugin.from_config(cfg);
    EXPECT_EQ(plugin.command_topics(), std::vector<std::string>{"/my_topic"});
}

// ─── handle_input + update ────────────────────────────────────────────────

TEST(JointVelPluginTest, HandleInputMovesJoint)
{
    s2::plugins::JointVelPlugin plugin;
    plugin.from_config(make_config());

    auto agent = make_agent_with_tree();

    // Команда: linear.x = 0.05 → arm получает target_vel = 0.05
    std::string input = R"({"linear":{"x":0.05,"y":0,"z":0},"angular":{"x":0,"y":0,"z":0}})";
    plugin.handle_input(input);
    plugin.update(1.0, agent);  // dt = 1с

    // arm.value должен стать 0.05
    for (const auto& link : agent.kinematic_tree->links()) {
        if (link.name == "arm") {
            EXPECT_NEAR(link.joint.value, 0.05, 1e-6);
        }
    }
}

TEST(JointVelPluginTest, HandleInputMovesJointByName)
{
    s2::plugins::JointVelPlugin plugin;
    plugin.from_config(make_config());

    auto agent = make_agent_with_tree();

    // Команда: {"arm": 0.05} → arm получает target_vel = 0.05
    std::string input = R"({"arm": 0.05})";
    plugin.handle_input(input);
    plugin.update(1.0, agent);  // dt = 1с

    // arm.value должен стать 0.05
    for (const auto& link : agent.kinematic_tree->links()) {
        if (link.name == "arm") {
            EXPECT_NEAR(link.joint.value, 0.05, 1e-6);
        }
    }
}

TEST(JointVelPluginTest, HandleInputClampsByMaxVel)
{
    s2::plugins::JointVelPlugin plugin;
    plugin.from_config(make_config());

    auto agent = make_agent_with_tree();

    // Команда превышает max_vel=0.1
    std::string input = R"({"linear":{"x":5.0,"y":0,"z":0},"angular":{"x":0,"y":0,"z":0}})";
    plugin.handle_input(input);
    plugin.update(1.0, agent);

    // arm.value не должен превысить max_vel * dt = 0.1
    for (const auto& link : agent.kinematic_tree->links()) {
        if (link.name == "arm") {
            EXPECT_NEAR(link.joint.value, 0.1, 1e-6);
        }
    }
}

TEST(JointVelPluginTest, HandleInputBucketAxis)
{
    s2::plugins::JointVelPlugin plugin;
    plugin.from_config(make_config());

    auto agent = make_agent_with_tree();

    // angular.z = 0.03 → bucket получает target_vel = 0.03
    std::string input = R"({"linear":{"x":0,"y":0,"z":0},"angular":{"x":0,"y":0,"z":0.03}})";
    plugin.handle_input(input);
    plugin.update(1.0, agent);

    for (const auto& link : agent.kinematic_tree->links()) {
        if (link.name == "bucket") {
            EXPECT_NEAR(link.joint.value, 0.03, 1e-6);
        }
    }
}

TEST(JointVelPluginTest, UpdateWithoutKinematicTreeIsNoop)
{
    s2::plugins::JointVelPlugin plugin;
    plugin.from_config(make_config());

    s2::Agent agent;  // без kinematic_tree
    std::string input = R"({"linear":{"x":1.0}})";
    plugin.handle_input(input);
    // Не должно крашиться
    EXPECT_NO_THROW(plugin.update(1.0, agent));
}

TEST(JointVelPluginTest, JointClampedAtMax)
{
    s2::plugins::JointVelPlugin plugin;
    plugin.from_config(make_config());

    auto agent = make_agent_with_tree();

    // Много шагов вперёд — arm не выйдет за 1.57 рад
    std::string input = R"({"linear":{"x":0.1}})";
    plugin.handle_input(input);
    for (int i = 0; i < 100; ++i) {
        plugin.update(1.0, agent);
    }

    for (const auto& link : agent.kinematic_tree->links()) {
        if (link.name == "arm") {
            EXPECT_LE(link.joint.value, 1.57 + 1e-6);
        }
    }
}

TEST(JointVelPluginTest, ToJsonContainsJoints)
{
    s2::plugins::JointVelPlugin plugin;
    plugin.from_config(make_config());
    std::string json = plugin.to_json();
    EXPECT_NE(json.find("joint_vel"), std::string::npos);
    EXPECT_NE(json.find("arm"), std::string::npos);
    EXPECT_NE(json.find("bucket"), std::string::npos);
}

// ─── Регрессия: имена звеньев URDF должны совпадать с именами в конфиге ───

TEST(JointVelPluginTest, JointNamesMatchUrdfLinks)
{
    /**
     * В конфиге joint_vel используются имена звеньев (link name), а не имена джоинтов (joint name).
     * В URDF: joint называется "arm_joint", а дочерний link — "arm".
     * Плагин ищет link с именем, указанным в конфиге, поэтому в конфиге должно быть "arm",
     * а не "arm_joint". Аналогично для "bucket".
     */
    // Конфиг, совпадающий с реальным URDF dozer (link name == arm, bucket)
    YAML::Node node;
    node["topic"] = "/arm_cmd";
    YAML::Node j1;
    j1["name"] = "arm";      // link name, НЕ joint name
    j1["axis"] = "linear_x";
    j1["max_vel"] = 0.01;
    YAML::Node j2;
    j2["name"] = "bucket";   // link name, НЕ joint name
    j2["axis"] = "linear_y";
    j2["max_vel"] = 0.01;
    node["joints"].push_back(j1);
    node["joints"].push_back(j2);

    s2::plugins::JointVelPlugin plugin;
    plugin.from_config(node);

    auto agent = make_agent_with_tree();

    // Команда напрямую — имя поля == имя link
    std::string input = R"({"arm": 0.01, "bucket": 0.01})";
    plugin.handle_input(input);
    plugin.update(1.0, agent);

    // Оба звена должны получить значение
    double arm_val = 0.0, bucket_val = 0.0;
    for (const auto& link : agent.kinematic_tree->links()) {
        if (link.name == "arm")    arm_val = link.joint.value;
        if (link.name == "bucket") bucket_val = link.joint.value;
    }

    EXPECT_NEAR(arm_val, 0.01, 1e-6);
    EXPECT_NEAR(bucket_val, 0.01, 1e-6);
}
