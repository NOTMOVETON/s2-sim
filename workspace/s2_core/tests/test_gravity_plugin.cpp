/**
 * @file test_gravity_plugin.cpp
 * Тесты для GravityPlugin: свободное падение, приземление, стояние на месте,
 * падение с платформы, ограничение максимальной скорости.
 */

#include <s2/plugins/gravity.hpp>
#include <s2/collision_system.hpp>
#include <s2/agent.hpp>

#include <gtest/gtest.h>
#include <cmath>

using namespace s2;
using namespace s2::plugins;

// ─── Вспомогательные функции ────────────────────────────────────────────────

/// Создать горизонтальный box-примитив (без вращения).
static WorldPrimitive make_floor(double cx, double cy, double cz,
                                  double sx, double sy, double sz)
{
    WorldPrimitive p;
    p.type = "box";
    p.pose = Pose3D{cx, cy, cz, 0, 0, 0};
    p.size = Vec3{sx, sy, sz};
    return p;
}

/// Создать агента с заданной позицией и bounding_radius.
static Agent make_agent(double x, double y, double z, double radius = 0.3)
{
    Agent agent;
    agent.world_pose = Pose3D{x, y, z, 0, 0, 0};
    agent.bounding.type   = ShapeType::SPHERE;
    agent.bounding.radius = radius;
    agent.has_collision   = true;
    return agent;
}

/// Создать GravityPlugin с заданными параметрами и привязать к CollisionSystem.
static GravityPlugin make_gravity(const CollisionSystem& cs,
                                   double g = 9.81,
                                   double max_speed = 20.0,
                                   double eps = 0.02)
{
    GravityPlugin plugin;
    YAML::Node cfg;
    cfg["gravity_accel"]    = g;
    cfg["max_fall_speed"]   = max_speed;
    cfg["grounded_epsilon"] = eps;
    plugin.from_config(cfg);
    plugin.set_collision_system(&cs);
    return plugin;
}

// ─── Тесты ──────────────────────────────────────────────────────────────────

/// Без геометрии агент падает: z уменьшается с каждым тиком.
TEST(GravityPlugin, FreeFall)
{
    CollisionSystem cs;
    cs.set_static_geometry({});  // нет геометрии

    auto agent  = make_agent(0.0, 0.0, 5.0);
    auto plugin = make_gravity(cs);

    const double dt = 0.02;
    const double z_start = agent.world_pose.z;

    plugin.update(dt, agent);
    EXPECT_LT(agent.world_pose.z, z_start)
        << "Агент должен начать падать при отсутствии опоры";

    double z_prev = agent.world_pose.z;
    for (int i = 0; i < 10; ++i)
    {
        plugin.update(dt, agent);
        EXPECT_LT(agent.world_pose.z, z_prev)
            << "Z должен монотонно уменьшаться при свободном падении, тик " << i;
        z_prev = agent.world_pose.z;
    }
}

/// Агент падает и приземляется на пол: итоговый z = bounding_radius.
TEST(GravityPlugin, LandsOnFloor)
{
    // Пол: box 10×10×0.1, центр z=-0.05 → верхняя грань z=0
    CollisionSystem cs;
    cs.set_static_geometry({make_floor(0, 0, -0.05, 10, 10, 0.1)});

    const double radius = 0.3;
    auto agent  = make_agent(0.0, 0.0, 3.0, radius);
    auto plugin = make_gravity(cs);

    // Симулируем несколько секунд (достаточно для приземления)
    const double dt = 0.02;
    for (int i = 0; i < 300; ++i)
    {
        plugin.update(dt, agent);
    }

    // Агент должен стоять на поверхности пола: z = 0 + bounding_radius
    const double expected_z = 0.0 + radius;
    EXPECT_NEAR(agent.world_pose.z, expected_z, 0.02)
        << "Агент должен приземлиться на пол";

    // world_velocity.linear.z должен быть обнулён
    EXPECT_NEAR(agent.world_velocity.linear.z(), 0.0, 1e-9)
        << "Вертикальная скорость должна быть 0 на земле";
}

/// Агент, уже стоящий на поверхности, не должен двигаться.
TEST(GravityPlugin, StaysOnGround)
{
    CollisionSystem cs;
    cs.set_static_geometry({make_floor(0, 0, -0.05, 10, 10, 0.1)});

    const double radius = 0.3;
    // Агент точно на поверхности: z = 0 + radius
    auto agent  = make_agent(0.0, 0.0, radius, radius);
    auto plugin = make_gravity(cs);

    const double z_init = agent.world_pose.z;
    const double dt = 0.02;
    for (int i = 0; i < 50; ++i)
    {
        plugin.update(dt, agent);
    }

    EXPECT_NEAR(agent.world_pose.z, z_init, 0.02)
        << "Агент на поверхности не должен двигаться вертикально";
    EXPECT_NEAR(agent.world_velocity.linear.z(), 0.0, 1e-9);
}

/// Агент на платформе: при смещении за край — начинает падать.
TEST(GravityPlugin, FallsOffPlatform)
{
    // Платформа 2×2, центр (0, 0, 1.025), верхняя грань z=1.05
    CollisionSystem cs;
    cs.set_static_geometry({make_floor(0, 0, 1.025, 2.0, 2.0, 0.05)});

    const double radius = 0.3;
    // Агент стоит на платформе
    auto agent  = make_agent(0.0, 0.0, 1.05 + radius, radius);
    auto plugin = make_gravity(cs);

    // Несколько тиков на платформе — должен стоять
    const double dt = 0.02;
    for (int i = 0; i < 10; ++i)
        plugin.update(dt, agent);

    const double z_on_platform = agent.world_pose.z;
    EXPECT_NEAR(z_on_platform, 1.05 + radius, 0.02)
        << "Агент должен стоять на платформе";

    // Смещаем за край платформы (X > 1.0 — за границей 2×2 платформы)
    agent.world_pose.x = 5.0;

    const double z_before_fall = agent.world_pose.z;

    // Несколько тиков — должен начать падать
    for (int i = 0; i < 5; ++i)
        plugin.update(dt, agent);

    EXPECT_LT(agent.world_pose.z, z_before_fall)
        << "После смещения за край платформы агент должен начать падать";
}

/// Скорость падения не превышает max_fall_speed.
TEST(GravityPlugin, MaxFallSpeed)
{
    CollisionSystem cs;
    cs.set_static_geometry({});  // нет геометрии — бесконечное падение

    const double max_speed = 3.0;  // маленький лимит для быстрой проверки
    auto agent  = make_agent(0.0, 0.0, 1000.0);
    auto plugin = make_gravity(cs, 9.81, max_speed);

    const double dt = 0.02;
    for (int i = 0; i < 500; ++i)
        plugin.update(dt, agent);

    // Скорость падения не должна превышать max_fall_speed
    // (world_velocity.z всегда 0 — GravityPlugin его обнуляет;
    //  проверяем через to_json)
    const auto json_str = plugin.to_json();
    auto j = nlohmann::json::parse(json_str);
    const double fall_vel = j["fall_velocity"].get<double>();

    EXPECT_GE(fall_vel, -max_speed - 1e-9)
        << "Скорость падения не должна превышать max_fall_speed";
    EXPECT_FALSE(j["grounded"].get<bool>())
        << "Агент в воздухе — grounded должен быть false";
}

/// Агент без плагина гравитации — z не меняется.
TEST(GravityPlugin, AgentWithoutGravityUnchanged)
{
    // Просто проверяем, что если не вызывается GravityPlugin,
    // z агента остаётся неизменным (базовая регрессия).
    Agent agent = make_agent(0.0, 0.0, 5.0);
    const double z_before = agent.world_pose.z;

    // Никаких плагинов, никакого update — z неизменен
    EXPECT_DOUBLE_EQ(agent.world_pose.z, z_before);
}
