/**
 * @file test_lidar_plugin.cpp
 * Тесты для LidarPlugin и RaycastEngine (dynamic_prims).
 */

#include <s2/plugins/lidar.hpp>
#include <s2/raycast_engine.hpp>
#include <s2/sensor_data.hpp>
#include <s2/agent.hpp>
#include <s2/kinematic_tree.hpp>

#include <gtest/gtest.h>
#include <yaml-cpp/yaml.h>
#include <cmath>
#include <string>

using namespace s2;
using namespace s2::plugins;

// ─── Вспомогательные функции ────────────────────────────────────────────────

static WorldPrimitive make_box(double cx, double cy, double cz,
                                double sx, double sy, double sz)
{
    WorldPrimitive p;
    p.type = "box";
    p.pose = Pose3D{cx, cy, cz, 0, 0, 0};
    p.size = Vec3{sx, sy, sz};
    return p;
}

static WorldPrimitive make_sphere_prim(double cx, double cy, double cz, double r)
{
    WorldPrimitive p;
    p.type = "sphere";
    p.pose = Pose3D{cx, cy, cz, 0, 0, 0};
    p.radius = r;
    return p;
}

static Agent make_agent(double x = 0, double y = 0, double z = 0)
{
    Agent agent;
    agent.id = 0;
    agent.name = "robot";
    agent.world_pose = Pose3D{x, y, z, 0, 0, 0};
    agent.has_collision = false;
    return agent;
}

static YAML::Node make_lidar_config(int num_rays = 8,
                                    double min_range = 0.1,
                                    double max_range = 10.0,
                                    double start_angle = -M_PI,
                                    double end_angle = M_PI,
                                    const std::string& mount_link = "")
{
    YAML::Node node;
    node["num_rays"]    = num_rays;
    node["min_range"]   = min_range;
    node["max_range"]   = max_range;
    node["start_angle"] = start_angle;
    node["end_angle"]   = end_angle;
    if (!mount_link.empty()) node["mount_link"] = mount_link;
    return node;
}

// ─── RaycastEngine: dynamic_prims ───────────────────────────────────────────

TEST(RaycastEngineDynamic, DynamicAgentVisible)
{
    // Сфера-агент на расстоянии 5 по X должна быть видна лидаром
    RaycastEngine engine;

    WorldPrimitive agent_prim = make_sphere_prim(5.0, 0.0, 0.5, 0.35);
    engine.set_dynamic_agents({agent_prim});

    Ray ray;
    ray.origin    = Vec3{0.0, 0.0, 0.5};
    ray.direction = Vec3{1.0, 0.0, 0.0};
    ray.max_range = 20.0;

    auto result = engine.cast(ray);
    EXPECT_TRUE(result.hit);
    EXPECT_NEAR(result.distance, 5.0 - 0.35, 0.1);
}

TEST(RaycastEngineDynamic, DynamicPrimsReplaced)
{
    // Второй вызов set_dynamic_agents() заменяет первые примитивы
    RaycastEngine engine;

    WorldPrimitive far_agent   = make_sphere_prim(20.0, 0.0, 0.5, 0.35);
    WorldPrimitive close_agent = make_sphere_prim(5.0,  0.0, 0.5, 0.35);

    engine.set_dynamic_agents({far_agent});
    engine.set_dynamic_agents({close_agent}); // заменяет

    Ray ray;
    ray.origin    = Vec3{0.0, 0.0, 0.5};
    ray.direction = Vec3{1.0, 0.0, 0.0};
    ray.max_range = 20.0;

    auto result = engine.cast(ray);
    EXPECT_TRUE(result.hit);
    // Должен попасть в близкую сферу (~4.65), не в далёкую (~19.65)
    EXPECT_LT(result.distance, 10.0);
}

TEST(RaycastEngineDynamic, NoDynamicAgents_Miss)
{
    // Без статической и динамической геометрии — miss
    RaycastEngine engine;

    Ray ray;
    ray.origin    = Vec3{0.0, 0.0, 0.0};
    ray.direction = Vec3{1.0, 0.0, 0.0};
    ray.max_range = 10.0;

    auto result = engine.cast(ray);
    EXPECT_FALSE(result.hit);
}

// ─── LidarPlugin ─────────────────────────────────────────────────────────────

TEST(LidarPluginTest, TypeAndDefaults)
{
    LidarPlugin plugin;
    EXPECT_EQ(plugin.type(), "lidar");
    EXPECT_EQ(plugin.display_label(), "Lidar");
    EXPECT_NEAR(plugin.default_publish_rate_hz(), 10.0, 1e-6);
    EXPECT_TRUE(plugin.has_inputs());
}

TEST(LidarPluginTest, NoRaycastEngine_NoData)
{
    // Без установленного RaycastEngine данные не появляются
    LidarPlugin plugin;
    plugin.from_config(make_lidar_config());

    Agent agent = make_agent();
    plugin.initialize(agent);

    plugin.update(1.0, agent);

    auto* data = agent.state.get<LidarScanData>();
    EXPECT_EQ(data, nullptr);
}

TEST(LidarPluginTest, HitsStaticBox)
{
    // Луч вдоль +X должен попасть в box на расстоянии 5 м
    RaycastEngine engine;
    WorldPrimitive wall = make_box(5.0, 0.0, 0.5, 0.2, 4.0, 1.0);
    engine.set_static_geometry({wall});

    LidarPlugin plugin;
    // Один луч точно вдоль +X (угол 0), max_range = 10
    YAML::Node cfg = make_lidar_config(1, 0.1, 10.0, 0.0, 0.0);
    plugin.from_config(cfg);
    plugin.set_raycast_engine(&engine);

    Agent agent = make_agent(0.0, 0.0, 0.5);
    plugin.initialize(agent);
    plugin.update(1.0, agent);

    auto* data = agent.state.get<LidarScanData>();
    ASSERT_NE(data, nullptr);
    EXPECT_EQ(data->ranges.size(), 1u);
    // Расстояние должно быть ~4.9 (до ближайшей грани box)
    EXPECT_LT(data->ranges[0], 5.0f);
    EXPECT_GT(data->ranges[0], 0.1f);
}

TEST(LidarPluginTest, MissReturnsMaxRange)
{
    // Луч без препятствий → max_range
    RaycastEngine engine;
    // Нет геометрии

    LidarPlugin plugin;
    YAML::Node cfg = make_lidar_config(4, 0.1, 10.0, 0.0, 3.0 * M_PI / 2.0);
    plugin.from_config(cfg);
    plugin.set_raycast_engine(&engine);

    Agent agent = make_agent();
    plugin.initialize(agent);
    plugin.update(1.0, agent);

    auto* data = agent.state.get<LidarScanData>();
    ASSERT_NE(data, nullptr);
    for (float r : data->ranges) {
        EXPECT_NEAR(r, 10.0f, 1e-3f);
    }
}

TEST(LidarPluginTest, HitsDynamicAgent)
{
    // Динамический агент (другой робот) должен быть виден лидару
    RaycastEngine engine;

    WorldPrimitive other_robot = make_sphere_prim(4.0, 0.0, 0.5, 0.35);
    engine.set_dynamic_agents({other_robot});

    LidarPlugin plugin;
    // Один луч вдоль +X
    YAML::Node cfg = make_lidar_config(1, 0.1, 10.0, 0.0, 0.0);
    plugin.from_config(cfg);
    plugin.set_raycast_engine(&engine);

    Agent agent = make_agent(0.0, 0.0, 0.5);
    plugin.initialize(agent);
    plugin.update(1.0, agent);

    auto* data = agent.state.get<LidarScanData>();
    ASSERT_NE(data, nullptr);
    ASSERT_EQ(data->ranges.size(), 1u);
    EXPECT_LT(data->ranges[0], 10.0f); // попадание
}

TEST(LidarPluginTest, MinRangeFilter)
{
    // Попадание ближе min_range → возвращается max_range
    RaycastEngine engine;
    // Box вплотную к лидару (расстояние ~0.01, min_range = 0.5)
    WorldPrimitive close_wall = make_box(0.05, 0.0, 0.5, 0.1, 4.0, 1.0);
    engine.set_static_geometry({close_wall});

    LidarPlugin plugin;
    YAML::Node cfg = make_lidar_config(1, 0.5, 10.0, 0.0, 0.0);
    plugin.from_config(cfg);
    plugin.set_raycast_engine(&engine);

    Agent agent = make_agent(0.0, 0.0, 0.5);
    plugin.initialize(agent);
    plugin.update(1.0, agent);

    auto* data = agent.state.get<LidarScanData>();
    ASSERT_NE(data, nullptr);
    ASSERT_EQ(data->ranges.size(), 1u);
    // Попадание < min_range → max_range
    EXPECT_NEAR(data->ranges[0], 10.0f, 1e-3f);
}

TEST(LidarPluginTest, PublishRateThrottling)
{
    // При 10 Hz и шаге 0.01 с → публикация происходит каждые 10 вызовов
    RaycastEngine engine;

    LidarPlugin plugin;
    YAML::Node cfg = make_lidar_config(4);
    cfg["publish_rate_hz"] = 10.0;
    plugin.from_config(cfg);
    plugin.set_base_rate(10.0);
    plugin.set_raycast_engine(&engine);

    Agent agent = make_agent();
    plugin.initialize(agent);

    uint64_t last_seq = 0;
    int publish_count = 0;

    // 100 тиков × 0.01 с = 1 с → должно быть ~10 публикаций
    for (int i = 0; i < 100; ++i) {
        plugin.update(0.01, agent);
        auto* data = agent.state.get<LidarScanData>();
        if (data && data->seq > last_seq) {
            last_seq = data->seq;
            ++publish_count;
        }
    }

    EXPECT_GE(publish_count, 9);
    EXPECT_LE(publish_count, 11);
}

TEST(LidarPluginTest, SeqIncrementsOnEachPublish)
{
    // Seq монотонно растёт с каждой новой публикацией
    RaycastEngine engine;

    LidarPlugin plugin;
    plugin.from_config(make_lidar_config(4));
    plugin.set_raycast_engine(&engine);

    Agent agent = make_agent();
    plugin.initialize(agent);

    uint64_t prev_seq = 0;

    for (int i = 0; i < 3; ++i) {
        // Достаточно большой dt чтобы спровоцировать публикацию
        plugin.update(1.0, agent);
        auto* data = agent.state.get<LidarScanData>();
        ASSERT_NE(data, nullptr);
        EXPECT_GT(data->seq, prev_seq);
        prev_seq = data->seq;
    }
}

TEST(LidarPluginTest, ToJsonVisibleFalse)
{
    // visible = false → points пустой
    LidarPlugin plugin;
    const std::string json = plugin.to_json();
    EXPECT_NE(json.find("\"type\":\"lidar_points\""), std::string::npos);
    EXPECT_NE(json.find("\"visible\":false"), std::string::npos);
    EXPECT_NE(json.find("\"points\":[]"), std::string::npos);
}

TEST(LidarPluginTest, ToJsonVisibleTrue_WithHits)
{
    // visible = true после обновления → points непустой при наличии попаданий
    RaycastEngine engine;
    WorldPrimitive wall = make_box(5.0, 0.0, 0.5, 0.2, 4.0, 1.0);
    engine.set_static_geometry({wall});

    LidarPlugin plugin;
    // Один луч вдоль +X
    YAML::Node cfg = make_lidar_config(1, 0.1, 10.0, 0.0, 0.0);
    plugin.from_config(cfg);
    plugin.set_raycast_engine(&engine);

    // Включаем visible
    plugin.handle_input("{\"visible\": true}");

    Agent agent = make_agent(0.0, 0.0, 0.5);
    plugin.initialize(agent);
    plugin.update(1.0, agent);

    const std::string json = plugin.to_json();
    EXPECT_NE(json.find("\"visible\":true"), std::string::npos);
    // Должны быть точки — не пустой массив
    EXPECT_EQ(json.find("\"points\":[]"), std::string::npos);
}

TEST(LidarPluginTest, AngleSector_NoHitsOutside)
{
    // Луч вдоль -X (угол π) — не попадает если лидар смотрит только вперёд [−π/4, π/4]
    RaycastEngine engine;
    // Стена сзади
    WorldPrimitive back_wall = make_box(-5.0, 0.0, 0.5, 0.2, 4.0, 1.0);
    engine.set_static_geometry({back_wall});

    LidarPlugin plugin;
    // 4 луча строго в секторе [-π/4, π/4] (вперёд)
    YAML::Node cfg = make_lidar_config(4, 0.1, 10.0, -M_PI/4.0, M_PI/4.0);
    plugin.from_config(cfg);
    plugin.set_raycast_engine(&engine);

    Agent agent = make_agent(0.0, 0.0, 0.5);
    plugin.initialize(agent);
    plugin.update(1.0, agent);

    auto* data = agent.state.get<LidarScanData>();
    ASSERT_NE(data, nullptr);
    // Все лучи не должны попасть в стену сзади
    for (float r : data->ranges) {
        EXPECT_NEAR(r, 10.0f, 1e-3f);
    }
}

TEST(LidarPluginTest, MountLink_OffsetPose)
{
    // Если задан mount_link — лучи бросаются из позы этого линка
    RaycastEngine engine;
    // Стена на X = 5
    WorldPrimitive wall = make_box(5.0, 0.0, 1.0, 0.2, 4.0, 1.0);
    engine.set_static_geometry({wall});

    // Создаём агента с кинематическим деревом
    Agent agent = make_agent(0.0, 0.0, 0.0);
    auto tree = std::make_unique<KinematicTree>();

    Link base;
    base.name = "base_link";
    base.parent = "";
    base.joint.type = JointType::FIXED;
    tree->add_link(std::move(base));

    // Линк лидара — смещён по Z на 1.0 вверх
    Link lidar_link;
    lidar_link.name   = "lidar_mast";
    lidar_link.parent = "base_link";
    lidar_link.joint.type = JointType::FIXED;
    lidar_link.origin.x = 0.0;
    lidar_link.origin.y = 0.0;
    lidar_link.origin.z = 1.0;
    tree->add_link(std::move(lidar_link));

    agent.kinematic_tree = std::move(tree);

    LidarPlugin plugin;
    YAML::Node cfg = make_lidar_config(1, 0.1, 10.0, 0.0, 0.0, "lidar_mast");
    plugin.from_config(cfg);
    plugin.set_raycast_engine(&engine);
    plugin.initialize(agent);
    plugin.update(1.0, agent);

    auto* data = agent.state.get<LidarScanData>();
    ASSERT_NE(data, nullptr);
    ASSERT_EQ(data->ranges.size(), 1u);
    // Луч из высоты z=1.0 должен попасть в стену
    EXPECT_LT(data->ranges[0], 10.0f);
}

TEST(RaycastEngineDynamic, RotatedBoxOBB)
{
    // Горизонтальный луч должен попасть в повёрнутый box (рампа с pitch=+π/4)
    // Box повёрнут на 45° по pitch: его верхняя грань наклонена, но центр на z=1
    // Луч из (0, 0, 1) по X+ — при AABB не попадёт, при OBB — попадёт
    RaycastEngine engine;

    WorldPrimitive ramp;
    ramp.type = "box";
    ramp.pose = Pose3D{3.0, 0.0, 1.0, 0.0, M_PI / 4.0, 0.0};  // pitch = 45°
    ramp.size = Vec3{2.0, 2.0, 0.2};
    engine.set_static_geometry({ramp});

    Ray ray;
    ray.origin    = Vec3{0.0, 0.0, 1.0};
    ray.direction = Vec3{1.0, 0.0, 0.0};
    ray.max_range = 10.0;

    auto result = engine.cast(ray);
    // OBB: луч на z=1 пересекает повёрнутый box, центр которого тоже на z=1
    EXPECT_TRUE(result.hit);
    EXPECT_LT(result.distance, 10.0);
}

TEST(RaycastEngineDynamic, AxisAlignedBoxStillWorks)
{
    // Проверка: не сломали обычный (невовёрнутый) box после перехода на OBB
    RaycastEngine engine;

    WorldPrimitive wall = make_box(5.0, 0.0, 0.5, 0.2, 4.0, 1.0);
    engine.set_static_geometry({wall});

    Ray ray;
    ray.origin    = Vec3{0.0, 0.0, 0.5};
    ray.direction = Vec3{1.0, 0.0, 0.0};
    ray.max_range = 20.0;

    auto result = engine.cast(ray);
    EXPECT_TRUE(result.hit);
    EXPECT_NEAR(result.distance, 4.9, 0.05);
}

TEST(LidarPluginTest, FieldsFilledCorrectly)
{
    // Проверяем, что LidarScanData заполняется корректно
    RaycastEngine engine;

    LidarPlugin plugin;
    YAML::Node cfg = make_lidar_config(8, 0.2, 8.0, -M_PI, M_PI);
    cfg["publish_rate_hz"] = 5.0;
    plugin.from_config(cfg);
    plugin.set_base_rate(5.0);
    plugin.set_raycast_engine(&engine);

    Agent agent = make_agent();
    plugin.initialize(agent);
    plugin.update(1.0, agent);

    auto* data = agent.state.get<LidarScanData>();
    ASSERT_NE(data, nullptr);

    EXPECT_EQ(data->ranges.size(), 8u);
    EXPECT_NEAR(data->range_min, 0.2f, 1e-3f);
    EXPECT_NEAR(data->range_max, 8.0f, 1e-3f);
    EXPECT_NEAR(data->angle_min, static_cast<float>(-M_PI), 1e-3f);
    EXPECT_NEAR(data->angle_max, static_cast<float>(M_PI), 1e-3f);
    EXPECT_NEAR(data->scan_time, 0.2f, 1e-3f); // 1/5 Hz
}
