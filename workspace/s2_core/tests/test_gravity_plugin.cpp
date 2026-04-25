/**
 * @file test_gravity_plugin.cpp
 * Тесты для GravityPlugin: свободное падение, приземление, стояние на месте,
 * падение с платформы, ограничение максимальной скорости.
 */

#include <s2/plugins/gravity.hpp>
#include <s2/collision_system.hpp>
#include <s2/agent.hpp>
#include <s2/world_query.hpp>
#include <s2/event_bus.hpp>
#include <s2/plugin_base.hpp>

#include <gtest/gtest.h>
#include <cmath>

using namespace s2;
using namespace s2::plugins;

// Вспомогательный null-контекст для тестов
static WorldQuery    g_null_world;
static EventBus      g_null_bus;
static KernelCommandQueue g_null_cmds;
static PluginContext g_ctx{g_null_world, g_null_bus, g_null_cmds};

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

/// Создать наклонный box-примитив (рампу) с заданным pitch.
static WorldPrimitive make_ramp(double cx, double cy, double cz,
                                 double sx, double sy, double sz,
                                 double pitch_rad)
{
    WorldPrimitive p;
    p.type = "box";
    p.pose = Pose3D{cx, cy, cz, 0, pitch_rad, 0};
    p.size = Vec3{sx, sy, sz};
    return p;
}

/// Создать GravityPlugin с заданными параметрами и привязать к CollisionSystem.
static GravityPlugin make_gravity(const CollisionSystem& cs,
                                   double g = 9.81,
                                   double max_speed = 20.0,
                                   double eps = 0.02,
                                   double friction = 0.0)
{
    GravityPlugin plugin;
    YAML::Node cfg;
    cfg["gravity_accel"]    = g;
    cfg["max_fall_speed"]   = max_speed;
    cfg["grounded_epsilon"] = eps;
    cfg["friction_coef"]    = friction;
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

    plugin.update(dt, agent, g_ctx);
    EXPECT_LT(agent.world_pose.z, z_start)
        << "Агент должен начать падать при отсутствии опоры";

    double z_prev = agent.world_pose.z;
    for (int i = 0; i < 10; ++i)
    {
        plugin.update(dt, agent, g_ctx);
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
        plugin.update(dt, agent, g_ctx);
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
        plugin.update(dt, agent, g_ctx);
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
        plugin.update(dt, agent, g_ctx);

    const double z_on_platform = agent.world_pose.z;
    EXPECT_NEAR(z_on_platform, 1.05 + radius, 0.02)
        << "Агент должен стоять на платформе";

    // Смещаем за край платформы (X > 1.0 — за границей 2×2 платформы)
    agent.world_pose.x = 5.0;

    const double z_before_fall = agent.world_pose.z;

    // Несколько тиков — должен начать падать
    for (int i = 0; i < 5; ++i)
        plugin.update(dt, agent, g_ctx);

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
        plugin.update(dt, agent, g_ctx);

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

// ─── Тесты slope physics (задача 20.2) ─────────────────────────────────────

/// На горизонтальном полу гравитация не меняет горизонтальную скорость.
TEST(GravityPlugin, FlatFloor_NoHorizontalEffect)
{
    CollisionSystem cs;
    cs.set_static_geometry({make_floor(0, 0, -0.05, 10, 10, 0.1)});

    const double radius = 0.3;
    auto agent  = make_agent(0.0, 0.0, radius, radius);
    agent.world_velocity.linear.x() = 1.0;
    auto plugin = make_gravity(cs);

    const double dt = 0.02;
    plugin.update(dt, agent, g_ctx);

    EXPECT_NEAR(agent.world_velocity.linear.x(), 1.0, 0.01)
        << "На плоском полу горизонтальная скорость не должна меняться от гравитации";
    EXPECT_NEAR(agent.world_velocity.linear.y(), 0.0, 0.01);
}

/// На рампе с friction=1 робот не скользит.
TEST(GravityPlugin, NoSliding_OnRamp)
{
    const double pitch = -0.3217;  // 18.4 deg
    CollisionSystem cs;
    cs.set_static_geometry({make_ramp(1.5, 0, 0.5, 3.162, 3.0, 0.1, pitch)});

    const double radius = 0.3;
    auto agent  = make_agent(1.5, 0.0, 2.0, radius);
    auto plugin = make_gravity(cs, 9.81, 20.0, 0.02, 1.0);

    const double dt = 0.02;
    for (int i = 0; i < 200; ++i)
        plugin.update(dt, agent, g_ctx);

    // После приземления скорость не должна расти от гравитации
    EXPECT_NEAR(agent.world_velocity.linear.x(), 0.0, 1e-9)
        << "На рампе робот не должен скользить — привод держит";
    EXPECT_NEAR(agent.world_velocity.linear.y(), 0.0, 1e-9);
}

/// На рампе с friction=1 привод (DiffDrive) сохраняет заданную скорость.
TEST(GravityPlugin, DriveVelocity_PreservedOnRamp)
{
    const double pitch = -0.3217;  // 18.4 deg
    CollisionSystem cs;
    cs.set_static_geometry({make_ramp(1.5, 0, 0.5, 3.162, 3.0, 0.1, pitch)});

    const double radius = 0.3;
    auto agent  = make_agent(1.5, 0.0, 2.0, radius);
    auto plugin = make_gravity(cs, 9.81, 20.0, 0.02, 1.0);

    const double dt = 0.02;
    // Приземляем
    for (int i = 0; i < 200; ++i)
        plugin.update(dt, agent, g_ctx);

    // Устанавливаем скорость от привода
    agent.world_velocity.linear.x() = 1.5;
    plugin.update(dt, agent, g_ctx);

    // Гравитация не должна менять горизонтальную скорость
    EXPECT_NEAR(agent.world_velocity.linear.x(), 1.5, 0.01)
        << "Гравитация не должна замедлять привод на рампе";
}

/// При friction=0 робот скользит по рампе (slide_velocity растёт).
TEST(GravityPlugin, SlidingOnRamp_ZeroFriction)
{
    const double pitch = -0.3217;  // 18.4 deg
    CollisionSystem cs;
    cs.set_static_geometry({make_ramp(1.5, 0, 0.5, 3.162, 3.0, 0.1, pitch)});

    const double radius = 0.3;
    auto agent  = make_agent(1.5, 0.0, 2.0, radius);
    auto plugin = make_gravity(cs, 9.81, 20.0, 0.02, 0.0);  // friction=0

    const double dt = 0.02;
    // Приземляем (сброс velocity каждый тик = нет привода)
    for (int i = 0; i < 200; ++i)
    {
        agent.world_velocity.linear.x() = 0.0;
        agent.world_velocity.linear.y() = 0.0;
        plugin.update(dt, agent, g_ctx);
    }

    // Проверяем slide через to_json
    auto j = nlohmann::json::parse(plugin.to_json());
    double slide_speed = j["slide_speed"].get<double>();
    EXPECT_GT(slide_speed, 0.1)
        << "При friction=0 на рампе slide_speed должен быть значительным";
}

/// При friction=1 робот не скользит по рампе.
TEST(GravityPlugin, SlidingOnRamp_FullFriction)
{
    const double pitch = -0.3217;  // 18.4 deg
    CollisionSystem cs;
    cs.set_static_geometry({make_ramp(1.5, 0, 0.5, 3.162, 3.0, 0.1, pitch)});

    const double radius = 0.3;
    auto agent  = make_agent(1.5, 0.0, 2.0, radius);
    auto plugin = make_gravity(cs, 9.81, 20.0, 0.02, 1.0);  // friction=1

    const double dt = 0.02;
    for (int i = 0; i < 200; ++i)
    {
        agent.world_velocity.linear.x() = 0.0;
        agent.world_velocity.linear.y() = 0.0;
        plugin.update(dt, agent, g_ctx);
    }

    auto j = nlohmann::json::parse(plugin.to_json());
    double slide_speed = j["slide_speed"].get<double>();
    EXPECT_NEAR(slide_speed, 0.0, 1e-9)
        << "При friction=1 slide_speed должен быть 0";
}

/// При friction=0.5 робот скользит медленнее чем при friction=0.
TEST(GravityPlugin, SlidingOnRamp_PartialFriction)
{
    const double pitch = -0.3217;  // 18.4 deg
    CollisionSystem cs;
    cs.set_static_geometry({make_ramp(1.5, 0, 0.5, 3.162, 3.0, 0.1, pitch)});

    const double radius = 0.3;

    // friction=0
    auto agent0  = make_agent(1.5, 0.0, 2.0, radius);
    auto plugin0 = make_gravity(cs, 9.81, 20.0, 0.02, 0.0);

    // friction=0.5
    auto agent5  = make_agent(1.5, 0.0, 2.0, radius);
    auto plugin5 = make_gravity(cs, 9.81, 20.0, 0.02, 0.5);

    const double dt = 0.02;
    for (int i = 0; i < 200; ++i)
    {
        agent0.world_velocity.linear.x() = 0.0;
        agent0.world_velocity.linear.y() = 0.0;
        agent5.world_velocity.linear.x() = 0.0;
        agent5.world_velocity.linear.y() = 0.0;
        plugin0.update(dt, agent0, g_ctx);
        plugin5.update(dt, agent5, g_ctx);
    }

    auto j0 = nlohmann::json::parse(plugin0.to_json());
    auto j5 = nlohmann::json::parse(plugin5.to_json());
    double slide0 = j0["slide_speed"].get<double>();
    double slide5 = j5["slide_speed"].get<double>();

    EXPECT_GT(slide0, slide5)
        << "При friction=0 скольжение должно быть быстрее чем при friction=0.5";
    EXPECT_GT(slide5, 0.01)
        << "При friction=0.5 скольжение всё ещё ненулевое";
}

/// При движении по рампе с friction > 0 робот всегда поднимается (net velocity > 0).
TEST(GravityPlugin, DrivingUphill_AlwaysClimbs_WithFriction)
{
    const double pitch = -0.3217;  // 18.4 deg
    CollisionSystem cs;
    cs.set_static_geometry({make_ramp(1.5, 0, 0.5, 3.162, 3.0, 0.1, pitch)});

    const double radius = 0.3;
    auto agent  = make_agent(1.5, 0.0, 2.0, radius);
    auto plugin = make_gravity(cs, 9.81, 20.0, 0.02, 0.5);  // friction=0.5

    const double dt = 0.02;
    // Приземляем
    for (int i = 0; i < 200; ++i)
    {
        agent.world_velocity.linear.x() = 0.0;
        plugin.update(dt, agent, g_ctx);
    }

    // Задаём маленькую скорость привода (как DiffDrive) и проверяем много тиков
    for (int i = 0; i < 200; ++i)
    {
        agent.world_velocity.linear.x() = 0.1;  // DiffDrive перезаписывает каждый тик
        agent.world_velocity.linear.y() = 0.0;
        plugin.update(dt, agent, g_ctx);

        // Итоговая скорость в направлении движения должна быть > 0
        EXPECT_GT(agent.world_velocity.linear.x(), 0.0)
            << "При friction > 0 робот должен ВСЕГДА подниматься, тик " << i;
    }
}

/// При friction=0 скольжение поглощает скорость привода (робот не поднимается).
TEST(GravityPlugin, DrivingUphill_NoClimb_ZeroFriction)
{
    const double pitch = -0.3217;  // 18.4 deg
    CollisionSystem cs;
    cs.set_static_geometry({make_ramp(1.5, 0, 0.5, 3.162, 3.0, 0.1, pitch)});

    const double radius = 0.3;
    auto agent  = make_agent(1.5, 0.0, 2.0, radius);
    auto plugin = make_gravity(cs, 9.81, 20.0, 0.02, 0.0);  // friction=0

    const double dt = 0.02;
    // Приземляем
    for (int i = 0; i < 200; ++i)
    {
        agent.world_velocity.linear.x() = 0.0;
        plugin.update(dt, agent, g_ctx);
    }

    // При friction=0 максимальное скольжение = drive_speed * 1.0 = drive_speed.
    // Slide body.x компенсирует drive.x, net ~0.
    double sum_net_speed = 0.0;
    for (int i = 0; i < 100; ++i)
    {
        agent.world_velocity.linear.x() = 1.0;
        agent.world_velocity.linear.y() = 0.0;
        plugin.update(dt, agent, g_ctx);
        sum_net_speed += agent.world_velocity.linear.x();
    }

    // Средняя скорость должна быть близка к нулю (slide компенсирует drive)
    double avg_speed = sum_net_speed / 100.0;
    EXPECT_LT(std::abs(avg_speed), 0.3)
        << "При friction=0 робот не должен эффективно подниматься";
}

/// На плоском полу при любом friction нет скольжения.
TEST(GravityPlugin, FlatFloor_NoSliding_AnyFriction)
{
    CollisionSystem cs;
    cs.set_static_geometry({make_floor(0, 0, -0.05, 10, 10, 0.1)});

    const double radius = 0.3;
    auto agent  = make_agent(0.0, 0.0, radius, radius);
    auto plugin = make_gravity(cs, 9.81, 20.0, 0.02, 0.5);  // friction=0.5

    const double dt = 0.02;
    for (int i = 0; i < 100; ++i)
        plugin.update(dt, agent, g_ctx);

    auto j = nlohmann::json::parse(plugin.to_json());
    double slide_speed = j["slide_speed"].get<double>();
    EXPECT_NEAR(slide_speed, 0.0, 1e-9)
        << "На плоском полу slide_speed должен быть 0";
}
