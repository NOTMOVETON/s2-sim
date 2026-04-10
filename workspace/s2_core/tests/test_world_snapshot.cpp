#include <gtest/gtest.h>
#include <s2/world_snapshot.hpp>
#include <nlohmann/json.hpp>

namespace s2 {

namespace {

// Вспомогательная функция: создать минимальный snapshot для теста
WorldSnapshot make_minimal_snapshot() {
    WorldSnapshot snap;
    snap.sim_time = 42.0;
    return snap;
}

} // namespace

// =====_snapshot_to_json=====

TEST(WorldSnapshotTest, MinimalSnapshotSerializesCorrectly) {
    WorldSnapshot snap = make_minimal_snapshot();

    auto json = snapshot_to_json(snap, false);

    EXPECT_DOUBLE_EQ(json["sim_time"].get<double>(), 42.0);
    EXPECT_TRUE(json["agents"].is_array());
    EXPECT_TRUE(json["agents"].empty());
    EXPECT_TRUE(json["props"].is_array());
    EXPECT_TRUE(json["actors"].is_array());
    EXPECT_TRUE(json["zones"].is_array());
    EXPECT_FALSE(json.contains("geometry"));
}

TEST(WorldSnapshotTest, GeometryIncludedWhenFlagIsTrue) {
    WorldSnapshot snap = make_minimal_snapshot();

    GeometrySnapshot geom;
    geom.type = "box";
    geom.x = 0; geom.y = 0; geom.z = 0;
    geom.sx = 10; geom.sy = 0.2; geom.sz = 2;
    geom.color = "#808080";
    snap.geometry.push_back(geom);

    auto json = snapshot_to_json(snap, true);

    EXPECT_TRUE(json.contains("geometry"));
    EXPECT_EQ(json["geometry"].size(), 1);
    EXPECT_EQ(json["geometry"][0]["type"].get<std::string>(), "box");
    EXPECT_DOUBLE_EQ(json["geometry"][0]["x"].get<double>(), 0.0);
}

TEST(WorldSnapshotTest, GeometryExcludedWhenFlagIsFalse) {
    WorldSnapshot snap = make_minimal_snapshot();

    GeometrySnapshot geom;
    geom.type = "box";
    snap.geometry.push_back(geom);

    auto json = snapshot_to_json(snap, false);

    EXPECT_FALSE(json.contains("geometry"));
}

TEST(WorldSnapshotTest, AgentSnapshotSerializesCorrectly) {
    WorldSnapshot snap;
    snap.sim_time = 0.0;

    AgentSnapshot agent;
    agent.id = 1;
    agent.name = "robot_0";
    agent.pose = Pose3D(1.0, 2.0, 0.0, 0.0, 0.0, 0.5);
    agent.velocity = Velocity{};
    agent.visual.type = "box";
    agent.visual.size = Vec3(0.5, 0.4, 0.3);
    agent.visual.color = "#FF6B35";
    agent.battery_level = 75.5;
    agent.effective_speed_scale = 0.85;
    agent.motion_locked = false;

    snap.agents.push_back(agent);

    auto json = snapshot_to_json(snap, false);

    EXPECT_EQ(json["agents"].size(), 1);
    auto& a = json["agents"][0];
    EXPECT_EQ(a["id"].get<uint32_t>(), 1);
    EXPECT_EQ(a["name"].get<std::string>(), "robot_0");
    EXPECT_DOUBLE_EQ(a["pose"]["x"].get<double>(), 1.0);
    EXPECT_DOUBLE_EQ(a["pose"]["y"].get<double>(), 2.0);
    EXPECT_DOUBLE_EQ(a["pose"]["yaw"].get<double>(), 0.5);
    EXPECT_DOUBLE_EQ(a["battery_level"].get<double>(), 75.5);
    EXPECT_DOUBLE_EQ(a["effective_speed_scale"].get<double>(), 0.85);
    EXPECT_EQ(a["motion_locked"].get<bool>(), false);
}

TEST(WorldSnapshotTest, PropSnapshotWithAttachment) {
    WorldSnapshot snap;

    PropSnapshot prop;
    prop.id = 100;
    prop.type = "box";
    prop.pose = Pose3D(3, 4, 0, 0, 0, 0);
    prop.movable = true;
    prop.attached_to_agent = AgentId(1);

    snap.props.push_back(prop);

    auto json = snapshot_to_json(snap, false);

    EXPECT_EQ(json["props"].size(), 1);
    EXPECT_EQ(json["props"][0]["id"].get<uint32_t>(), 100);
    EXPECT_EQ(json["props"][0]["attached_to_agent"].get<uint32_t>(), 1);
}

TEST(WorldSnapshotTest, PropSnapshotWithoutAttachment) {
    WorldSnapshot snap;

    PropSnapshot prop;
    prop.id = 101;
    prop.movable = false;

    snap.props.push_back(prop);

    auto json = snapshot_to_json(snap, false);

    EXPECT_TRUE(json["props"][0]["attached_to_agent"].is_null());
}

TEST(WorldSnapshotTest, ActorSnapshotSerializesCorrectly) {
    WorldSnapshot snap;

    ActorSnapshot actor;
    actor.id = 1;
    actor.name = "door_1";
    actor.state = "open";

    snap.actors.push_back(actor);

    auto json = snapshot_to_json(snap, false);

    EXPECT_EQ(json["actors"].size(), 1);
    EXPECT_EQ(json["actors"][0]["name"].get<std::string>(), "door_1");
    EXPECT_EQ(json["actors"][0]["state"].get<std::string>(), "open");
}

TEST(WorldSnapshotTest, ZoneSnapshotWithAgentsInside) {
    WorldSnapshot snap;

    ZoneSnapshot zone;
    zone.id = "ice_zone_1";
    zone.enabled = true;
    zone.shape.type = ZoneShapeType::SPHERE;
    zone.shape.center = Vec3(5, 5, 0);
    zone.shape.radius = 3.0;
    zone.agents_inside = {1, 2};

    snap.zones.push_back(zone);

    auto json = snapshot_to_json(snap, false);

    EXPECT_EQ(json["zones"].size(), 1);
    EXPECT_EQ(json["zones"][0]["id"].get<std::string>(), "ice_zone_1");
    EXPECT_EQ(json["zones"][0]["enabled"].get<bool>(), true);
    EXPECT_EQ(json["zones"][0]["shape"]["shape_type"].get<std::string>(), "sphere");
    EXPECT_DOUBLE_EQ(json["zones"][0]["shape"]["radius"].get<double>(), 3.0);
    EXPECT_EQ(json["zones"][0]["agents_inside"].size(), 2);
}

TEST(WorldSnapshotTest, ZoneSnapshotWithAABBShape) {
    WorldSnapshot snap;

    ZoneSnapshot zone;
    zone.shape.type = ZoneShapeType::AABB;
    zone.shape.center = Vec3(0, 0, 0);
    zone.shape.half_size = Vec3(5, 5, 3);  // half_size

    snap.zones.push_back(zone);

    auto json = snapshot_to_json(snap, false);

    EXPECT_EQ(json["zones"][0]["shape"]["shape_type"].get<std::string>(), "aabb");
    // JSON должен содержать full size (half_size * 2)
    EXPECT_DOUBLE_EQ(json["zones"][0]["shape"]["size"][0].get<double>(), 10.0);
    EXPECT_DOUBLE_EQ(json["zones"][0]["shape"]["size"][1].get<double>(), 10.0);
    EXPECT_DOUBLE_EQ(json["zones"][0]["shape"]["size"][2].get<double>(), 6.0);
}

TEST(WorldSnapshotTest, MultipleAgentsSerializeInOrder) {
    WorldSnapshot snap;

    for (int i = 0; i < 5; ++i) {
        AgentSnapshot agent;
        agent.id = i;
        agent.name = "robot_" + std::to_string(i);
        snap.agents.push_back(agent);
    }

    auto json = snapshot_to_json(snap, false);

    EXPECT_EQ(json["agents"].size(), 5);
    for (int i = 0; i < 5; ++i) {
        EXPECT_EQ(json["agents"][i]["id"].get<uint32_t>(), i);
    }
}

} // namespace s2