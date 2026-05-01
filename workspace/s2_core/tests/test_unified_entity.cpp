/**
 * @file test_unified_entity.cpp
 * Тесты unified entity model (Phase 2): Agent/Actor/Prop flat structs,
 * SimWorld O(1) lookup, role() enforcement.
 */

#include <gtest/gtest.h>

#include <s2/entity.hpp>
#include <s2/world.hpp>
#include <s2/sim_engine.hpp>
#include <s2/plugins/diff_drive.hpp>

#include <yaml-cpp/yaml.h>

using namespace s2;

// ─── AgentData имеет поля Phase 2 ─────────────────────────────────────────

TEST(UnifiedEntity, AgentDataHasPhase2Fields)
{
    AgentData agent;
    agent.tags["role"] = "scout";
    agent.immune_to_effects.insert("ice");
    agent.enabled = false;
    agent.entity_type = EntityType::AGENT;

    EXPECT_EQ(agent.tags.at("role"), "scout");
    EXPECT_TRUE(agent.immune_to_effects.count("ice"));
    EXPECT_FALSE(agent.enabled);
    EXPECT_EQ(agent.entity_type, EntityType::AGENT);

    // AgentData — alias для Agent
    Agent* as_agent = &agent;
    EXPECT_NE(as_agent, nullptr);
}

// ─── PropData не имеет SharedState — проверяем поля что есть ──────────────

TEST(UnifiedEntity, PropDataHasExpectedFields)
{
    PropData prop;
    prop.movable = true;
    prop.properties["weight"] = "50kg";
    prop.tags["zone"] = "storage";
    prop.immune_to_effects.insert("heat");
    prop.entity_type = EntityType::PROP;

    EXPECT_TRUE(prop.movable);
    EXPECT_EQ(prop.properties.at("weight"), "50kg");
    EXPECT_EQ(prop.tags.at("zone"), "storage");
    EXPECT_TRUE(prop.immune_to_effects.count("heat"));
    EXPECT_EQ(prop.entity_type, EntityType::PROP);
}

// ─── SimWorld: get_agent O(1) ──────────────────────────────────────────────

TEST(UnifiedEntity, SimWorldGetAgentO1)
{
    SimWorld world;

    Agent a1; a1.id = 1; a1.name = "alpha";
    Agent a2; a2.id = 2; a2.name = "beta";
    Agent a3; a3.id = 3; a3.name = "gamma";

    world.add_agent(std::move(a1));
    world.add_agent(std::move(a2));
    world.add_agent(std::move(a3));

    ASSERT_NE(world.get_agent(2), nullptr);
    EXPECT_EQ(world.get_agent(2)->name, "beta");

    auto etype = world.get_entity_type(3);
    ASSERT_TRUE(etype.has_value());
    EXPECT_EQ(etype.value(), EntityType::AGENT);

    EXPECT_EQ(world.get_agent(999), nullptr);
}

// ─── SimWorld: add/remove_agent обновляет индексы ─────────────────────────

TEST(UnifiedEntity, SimWorldRemoveAgentUpdatesIndex)
{
    SimWorld world;

    Agent a1; a1.id = 1; a1.name = "one";
    Agent a2; a2.id = 2; a2.name = "two";
    Agent a3; a3.id = 3; a3.name = "three";

    world.add_agent(std::move(a1));
    world.add_agent(std::move(a2));
    world.add_agent(std::move(a3));

    world.remove_agent(2);

    EXPECT_EQ(world.agents().size(), 2u);
    EXPECT_NE(world.get_agent(1), nullptr);
    EXPECT_NE(world.get_agent(3), nullptr);
    EXPECT_EQ(world.get_agent(2), nullptr);
}

// ─── role() enforcement: два actuation-плагина → исключение ───────────────

TEST(UnifiedEntity, LoadWorldThrowsOnMultipleActuationPlugins)
{
    SimEngine engine{SimEngine::Config{.update_rate = 100.0}};

    SimWorld world;
    Agent agent;
    agent.id = 1;
    agent.name = "dual_drive";

    auto dd1 = std::make_unique<plugins::DiffDrivePlugin>();
    auto dd2 = std::make_unique<plugins::DiffDrivePlugin>();

    YAML::Node cfg;
    dd1->from_config(cfg);
    dd2->from_config(cfg);

    agent.plugins.push_back(std::move(dd1));
    agent.plugins.push_back(std::move(dd2));
    world.add_agent(std::move(agent));

    EXPECT_THROW(engine.load_world(std::move(world)), std::runtime_error);
}

// ─── AgentData.tags — map<string,string> ──────────────────────────────────

TEST(UnifiedEntity, AgentDataTagsMapWorks)
{
    AgentData agent;
    agent.tags["transport_type"]      = "ros2";
    agent.tags["transport_domain_id"] = "50";
    agent.tags["team"]                = "red";

    EXPECT_EQ(agent.tags.count("transport_type"), 1u);
    EXPECT_EQ(agent.tags.at("transport_domain_id"), "50");
    EXPECT_EQ(agent.tags.at("team"), "red");
}
