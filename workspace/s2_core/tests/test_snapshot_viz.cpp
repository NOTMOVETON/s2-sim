#include <gtest/gtest.h>
#include <s2/sim_engine.hpp>
#include <s2/world.hpp>
#include <s2/world_snapshot.hpp>
#include <nlohmann/json.hpp>

using namespace s2;

// ─── Тесты build_snapshot ─────────────────────────────────────────────

TEST(SnapshotBuildTest, EmptyWorldSnapshot) {
    SimEngine engine({100.0, 30.0});
    engine.load_world(SimWorld{});
    engine.step(1);

    auto snap = engine.build_snapshot();
    EXPECT_DOUBLE_EQ(snap.sim_time, 0.01);
    EXPECT_TRUE(snap.agents.empty());
    EXPECT_TRUE(snap.props.empty());
    EXPECT_TRUE(snap.actors.empty());
    EXPECT_TRUE(snap.geometry.empty());
}

TEST(SnapshotBuildTest, SnapshotContainsAgents) {
    SimEngine engine({100.0, 30.0});
    SimWorld world;

    Agent agent;
    agent.id = 0;
    agent.name = "test_agent";
    agent.world_pose.x = 1.0;
    agent.world_pose.y = 2.0;
    agent.world_pose.z = 0.5;
    agent.world_pose.yaw = 0.3;
    agent.world_velocity.linear = Vec3{0.0, 0.0, 0.0};
    agent.world_velocity.angular = Vec3{0.0, 0.0, 0.0};
    agent.visual.color = "#FF0000";
    world.add_agent(std::move(agent));

    engine.load_world(std::move(world));
    engine.step(5);

    auto snap = engine.build_snapshot();
    EXPECT_EQ(snap.agents.size(), 1u);
    EXPECT_EQ(snap.agents[0].name, "test_agent");
    EXPECT_DOUBLE_EQ(snap.agents[0].pose.x, 1.0);
    EXPECT_DOUBLE_EQ(snap.agents[0].pose.y, 2.0);
    EXPECT_DOUBLE_EQ(snap.agents[0].pose.z, 0.5);
}

TEST(SnapshotBuildTest, SnapshotContainsProps) {
    SimEngine engine({100.0, 30.0});
    SimWorld world;

    Prop prop;
    prop.id = 0;
    prop.type = "box";
    prop.world_pose.x = 5.0;
    prop.movable = true;
    world.add_prop(std::move(prop));

    engine.load_world(std::move(world));
    engine.step(1);

    auto snap = engine.build_snapshot();
    EXPECT_EQ(snap.props.size(), 1u);
    EXPECT_EQ(snap.props[0].type, "box");
    EXPECT_TRUE(snap.props[0].movable);
}

TEST(SnapshotBuildTest, SnapshotContainsActors) {
    SimEngine engine({100.0, 30.0});
    SimWorld world;

    Actor actor;
    actor.id = 0;
    actor.name = "door";
    actor.current_state = "closed";
    world.add_actor(std::move(actor));

    engine.load_world(std::move(world));
    engine.step(1);

    auto snap = engine.build_snapshot();
    EXPECT_EQ(snap.actors.size(), 1u);
    EXPECT_EQ(snap.actors[0].name, "door");
    EXPECT_EQ(snap.actors[0].state, "closed");
}

TEST(SnapshotBuildTest, SnapshotContainsGeometry) {
    SimEngine engine({100.0, 30.0});
    SimWorld world;

    WorldPrimitive prim;
    prim.type = "box";
    prim.pose.x = 3.0;
    prim.pose.y = 4.0;
    prim.size = Vec3{1.0, 2.0, 0.5};
    prim.color = "#808080";
    world.static_geometry().push_back(std::move(prim));

    engine.load_world(std::move(world));
    engine.step(1);

    auto snap = engine.build_snapshot();
    EXPECT_EQ(snap.geometry.size(), 1u);
    EXPECT_EQ(snap.geometry[0].type, "box");
    EXPECT_DOUBLE_EQ(snap.geometry[0].x, 3.0);
    EXPECT_DOUBLE_EQ(snap.geometry[0].y, 4.0);
    EXPECT_DOUBLE_EQ(snap.geometry[0].sx, 1.0);
    EXPECT_DOUBLE_EQ(snap.geometry[0].sy, 2.0);
    EXPECT_DOUBLE_EQ(snap.geometry[0].sz, 0.5);
}

// ─── Тесты snapshot_to_json ───────────────────────────────────────────

TEST(SnapshotToJsonTest, MinimalSnapshot) {
    WorldSnapshot snap;
    snap.sim_time = 1.5;

    auto json = snapshot_to_json(snap, false);
    EXPECT_DOUBLE_EQ(json["sim_time"].get<double>(), 1.5);
    EXPECT_EQ(json["agents"].size(), 0u);
    EXPECT_EQ(json["props"].size(), 0u);
    EXPECT_EQ(json["actors"].size(), 0u);
    EXPECT_EQ(json["geometry"].size(), 0u);
}

TEST(SnapshotToJsonTest, AgentInJson) {
    WorldSnapshot snap;
    snap.sim_time = 0.5;

    AgentSnapshot as;
    as.id = 42;
    as.name = "robot";
    as.pose.x = 10.0;
    as.pose.y = 20.0;
    as.pose.z = 0.0;
    as.pose.yaw = 1.57;
    as.visual.color = "#00FF00";
    as.battery_level = 85.0;
    snap.agents.push_back(as);

    auto json = snapshot_to_json(snap, false);
    EXPECT_EQ(json["agents"].size(), 1u);
    EXPECT_EQ(json["agents"][0]["id"].get<int>(), 42);
    EXPECT_EQ(json["agents"][0]["name"].get<std::string>(), "robot");
    EXPECT_DOUBLE_EQ(json["agents"][0]["pose"]["x"].get<double>(), 10.0);
    EXPECT_DOUBLE_EQ(json["agents"][0]["pose"]["y"].get<double>(), 20.0);
    EXPECT_DOUBLE_EQ(json["agents"][0]["battery_level"].get<double>(), 85.0);
    EXPECT_EQ(json["agents"][0]["color"].get<std::string>(), "#00FF00");
}

TEST(SnapshotToJsonTest, GeometryInJson) {
    WorldSnapshot snap;
    snap.sim_time = 0.0;

    GeometrySnapshot gs;
    gs.type = "sphere";
    gs.x = 1.0; gs.y = 2.0; gs.z = 0.0;
    gs.radius = 0.5;
    gs.color = "#FF0000";
    snap.geometry.push_back(gs);

    auto json = snapshot_to_json(snap, true);
    EXPECT_EQ(json["geometry"].size(), 1u);
    EXPECT_EQ(json["geometry"][0]["type"].get<std::string>(), "sphere");
    EXPECT_DOUBLE_EQ(json["geometry"][0]["x"].get<double>(), 1.0);
    EXPECT_DOUBLE_EQ(json["geometry"][0]["radius"].get<double>(), 0.5);
}

TEST(SnapshotToJsonTest, GeometryExcludedWhenFlagFalse) {
    WorldSnapshot snap;
    snap.sim_time = 0.0;

    GeometrySnapshot gs;
    gs.type = "box";
    gs.x = 0.0; gs.y = 0.0; gs.z = 0.0;
    gs.sx = 1.0; gs.sy = 1.0; gs.sz = 1.0;
    snap.geometry.push_back(gs);

    auto json = snapshot_to_json(snap, false);
    EXPECT_EQ(json["geometry"].size(), 0u);
}

TEST(SnapshotToJsonTest, PropInJson) {
    WorldSnapshot snap;
    snap.sim_time = 0.0;

    PropSnapshot ps;
    ps.id = 5;
    ps.type = "crate";
    ps.movable = true;
    snap.props.push_back(ps);

    auto json = snapshot_to_json(snap, false);
    EXPECT_EQ(json["props"].size(), 1u);
    EXPECT_EQ(json["props"][0]["id"].get<int>(), 5);
    EXPECT_EQ(json["props"][0]["type"].get<std::string>(), "crate");
    EXPECT_TRUE(json["props"][0]["movable"].get<bool>());
}

TEST(SnapshotToJsonTest, ActorInJson) {
    WorldSnapshot snap;
    snap.sim_time = 0.0;

    ActorSnapshot acs;
    acs.id = 1;
    acs.name = "elevator";
    acs.state = "moving";
    snap.actors.push_back(acs);

    auto json = snapshot_to_json(snap, false);
    EXPECT_EQ(json["actors"].size(), 1u);
    EXPECT_EQ(json["actors"][0]["name"].get<std::string>(), "elevator");
    EXPECT_EQ(json["actors"][0]["state"].get<std::string>(), "moving");
}

// ─── Тесты SimEngine + viz_rate ───────────────────────────────────────

TEST(SimEngineVizTest, VizRateDoesNotAffectSimTime) {
    SimEngine engine_fast({1000.0, 1000.0});
    SimWorld world;
    engine_fast.load_world(std::move(world));
    engine_fast.step(100);
    EXPECT_NEAR(engine_fast.sim_time(), 0.1, 0.001);
}

TEST(SimEngineVizTest, ZeroVizRateNoCrash) {
    SimEngine engine({100.0, 0.0});
    SimWorld world;
    Agent agent;
    agent.id = 0;
    agent.name = "test";
    world.add_agent(std::move(agent));
    engine.load_world(std::move(world));
    engine.step(10);
    auto snap = engine.build_snapshot();
    EXPECT_EQ(snap.agents.size(), 1u);
}