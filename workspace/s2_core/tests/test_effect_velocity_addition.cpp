#include <gtest/gtest.h>

#include <s2/effects/conveyor_effect.hpp>
#include <s2/effects/wind_effect.hpp>
#include <s2/effects_registry.hpp>
#include <s2/zone_system.hpp>
#include <s2/sim_engine.hpp>
#include <s2/agent.hpp>
#include <s2/actor.hpp>
#include <s2/sim_bus.hpp>
#include <s2/world.hpp>

namespace s2 {

// ─── Вспомогательные функции ─────────────────────────────────────────────────

static Agent make_agent(AgentId id, double x = 0.0, double y = 0.0)
{
    Agent a;
    a.id = id;
    a.world_pose.x = x;
    a.world_pose.y = y;
    a.world_pose.z = 0.0;
    return a;
}

static Agent make_agent_with_capability(const std::string& cap, AgentId id = 1)
{
    Agent a = make_agent(id);
    a.capabilities.insert(cap);
    return a;
}

static Zone make_sphere_zone_with_effect(
    const ZoneId& zone_id,
    std::unique_ptr<EffectPlugin> plugin,
    const std::vector<std::string>& required_caps = {})
{
    Zone z;
    z.id           = zone_id;
    z.enabled      = true;
    z.shape.type   = ZoneShapeType::SPHERE;
    z.shape.center = Vec3::Zero();
    z.shape.radius = 100.0;

    Zone::EffectDesc desc;
    desc.type                  = "test_plugin";
    desc.enabled               = true;
    desc.effect_type           = plugin->effect_type();
    desc.required_capabilities = required_caps;
    desc.plugin                = std::move(plugin);

    z.effects.push_back(std::move(desc));
    return z;
}

static void run_tick(ZoneSystem& zs, std::vector<Agent>& agents, double sim_time = 0.0)
{
    SimBus bus;
    std::vector<Actor> actors;
    zs.tick(agents, actors, bus, sim_time, 0.01);
}

// ─── Тест 1: ConveyorEffect — агент с нулевой cmd_vel дрейфует по конвейеру ───

TEST(VelocityAddition, ConveyorEffect_DriftsAgent)
{
    auto plugin = std::make_unique<effects::ConveyorEffect>();
    plugin->on_init(YAML::Load("direction: {x: 1.0, y: 0.0, z: 0.0}\nspeed: 1.5"));

    ZoneSystem zs;
    zs.add_zone(make_sphere_zone_with_effect("conv", std::move(plugin), {"surface_contact"}));

    std::vector<Agent> agents;
    agents.push_back(make_agent_with_capability("surface_contact"));

    constexpr int N = 10;
    constexpr double dt = 0.01;
    // cmd_vel = 0, world_velocity = {0,0,0}

    for (int i = 0; i < N; ++i) {
        run_tick(zs, agents, i * dt);
        agents[0].state.resolve();

        const Vec3& add = agents[0].state.effective().velocity_addition;
        agents[0].world_pose.x += add.x() * dt;
        agents[0].world_pose.y += add.y() * dt;

        agents[0].state.clear_contributions();
    }

    // Ожидаемое смещение: speed * dt * N = 1.5 * 0.01 * 10 = 0.15 м
    constexpr double expected = 1.5 * dt * N;
    EXPECT_NEAR(agents[0].world_pose.x, expected, expected * 0.05);
    EXPECT_NEAR(agents[0].world_pose.y, 0.0, 1e-9);
}

// ─── Тест 2: ConveyorEffect + собственная скорость суммируются ───────────────

TEST(VelocityAddition, ConveyorEffect_AddsToOwnVelocity)
{
    auto plugin = std::make_unique<effects::ConveyorEffect>();
    plugin->on_init(YAML::Load("direction: {x: 1.0, y: 0.0, z: 0.0}\nspeed: 0.5"));

    ZoneSystem zs;
    zs.add_zone(make_sphere_zone_with_effect("conv", std::move(plugin), {"surface_contact"}));

    std::vector<Agent> agents;
    agents.push_back(make_agent_with_capability("surface_contact"));
    // Агент сам едет вперёд
    agents[0].world_velocity.linear.x() = 1.0;

    constexpr double dt = 0.01;

    run_tick(zs, agents, 0.0);
    agents[0].state.resolve();

    const Vec3& add = agents[0].state.effective().velocity_addition;

    // Итоговое смещение за один тик: (1.0 + 0.5) * dt = 0.015
    double displacement = (agents[0].world_velocity.linear.x() + add.x()) * dt;
    EXPECT_NEAR(displacement, 1.5 * dt, 1e-9);
}

// ─── Тест 3: ConveyorEffect — агент без surface_contact не дрейфует ──────────

TEST(VelocityAddition, ConveyorEffect_RequiresSurfaceContact)
{
    auto plugin = std::make_unique<effects::ConveyorEffect>();
    plugin->on_init(YAML::Load("direction: {x: 1.0, y: 0.0, z: 0.0}\nspeed: 2.0"));

    ZoneSystem zs;
    zs.add_zone(make_sphere_zone_with_effect("conv", std::move(plugin), {"surface_contact"}));

    std::vector<Agent> agents;
    agents.push_back(make_agent(1));  // без capability

    run_tick(zs, agents, 0.0);
    agents[0].state.resolve();

    // velocity_addition должен остаться нулевым
    EXPECT_NEAR(agents[0].state.effective().velocity_addition.x(), 0.0, 1e-9);
    EXPECT_NEAR(agents[0].state.effective().velocity_addition.y(), 0.0, 1e-9);
}

// ─── Тест 4: WindEffect — все агенты (без capability) дрейфуют ───────────────

TEST(VelocityAddition, WindEffect_AllAgents)
{
    auto plugin = std::make_unique<effects::WindEffect>();
    plugin->on_init(YAML::Load("wind_vector: {x: 0.3, y: 0.0, z: 0.0}"));

    ZoneSystem zs;
    // Ветер не требует capabilities — передаём пустой список
    zs.add_zone(make_sphere_zone_with_effect("wind", std::move(plugin)));

    std::vector<Agent> agents;
    agents.push_back(make_agent(1));  // обычный агент без capabilities

    run_tick(zs, agents, 0.0);
    agents[0].state.resolve();

    EXPECT_NEAR(agents[0].state.effective().velocity_addition.x(), 0.3, 1e-9);
    EXPECT_NEAR(agents[0].state.effective().velocity_addition.y(), 0.0, 1e-9);
}

// ─── Тест 5: WindEffect — порывы колеблются синусоидально ────────────────────

TEST(VelocityAddition, WindEffect_Gusts)
{
    auto plugin = std::make_unique<effects::WindEffect>();
    plugin->on_init(YAML::Load(
        "wind_vector: {x: 1.0, y: 0.0, z: 0.0}\n"
        "gust_amplitude: 0.5\n"
        "gust_frequency: 1.0"
    ));

    ZoneSystem zs;
    zs.add_zone(make_sphere_zone_with_effect("wind", std::move(plugin)));

    // Проверяем что при разных sim_time получаем разные значения velocity_addition,
    // соответствующие sin-функции
    for (double t : {0.0, 0.25, 0.5, 0.75}) {
        std::vector<Agent> agents;
        agents.push_back(make_agent(1));

        SimBus bus;
        std::vector<Actor> actors;
        zs.tick(agents, actors, bus, t, 0.01);
        agents[0].state.resolve();

        // wind_vector.x = 1.0, normalized = {1,0,0}
        // gust = 0.5 * sin(t * 1.0 * 2π)
        double expected_gust = 0.5 * std::sin(t * 1.0 * 2.0 * M_PI);
        double expected_vx = 1.0 + expected_gust;
        EXPECT_NEAR(agents[0].state.effective().velocity_addition.x(), expected_vx, 1e-9)
            << "Ожидаемый vx при sim_time=" << t;

        agents[0].state.clear_contributions();
    }
}

// ─── Тест 6: Конвейер + ветер — velocity_addition суммируются ────────────────

TEST(VelocityAddition, ConveyorAndWind_Combined)
{
    auto conveyor = std::make_unique<effects::ConveyorEffect>();
    conveyor->on_init(YAML::Load("direction: {x: 1.0, y: 0.0, z: 0.0}\nspeed: 1.0"));

    auto wind = std::make_unique<effects::WindEffect>();
    wind->on_init(YAML::Load("wind_vector: {x: 0.0, y: 0.5, z: 0.0}"));

    ZoneSystem zs;
    zs.add_zone(make_sphere_zone_with_effect("conv", std::move(conveyor), {"surface_contact"}));
    zs.add_zone(make_sphere_zone_with_effect("wind", std::move(wind)));

    std::vector<Agent> agents;
    agents.push_back(make_agent_with_capability("surface_contact"));

    run_tick(zs, agents, 0.0);
    agents[0].state.resolve();

    const Vec3& add = agents[0].state.effective().velocity_addition;
    EXPECT_NEAR(add.x(), 1.0, 1e-9);  // конвейер по X
    EXPECT_NEAR(add.y(), 0.5, 1e-9);  // ветер по Y
}

// ─── Тест 7: velocity_addition применяется после actuation в SimEngine ────────

TEST(VelocityAddition, VelocityAddition_AppliedAfterActuation)
{
    // Настройка SimEngine с фабрикой эффектов
    SimEngine engine{{.update_rate = 100.0, .viz_rate = 30.0}};
    engine.set_effect_factory(s2::create_effect);

    // Агент без плагинов — world_velocity вручную задаётся через capability
    // (нет DiffDrivePlugin, поэтому world_velocity не меняется тиком,
    //  только velocity_addition из конвейера)
    SimWorld world;
    Agent agent = make_agent_with_capability("surface_contact");
    // Задаём начальную скорость тела: +1.0 по X (в локальных координатах тела)
    // yaw=0 → мировые = локальные, так что это +1.0 вдоль мирового X
    agent.world_velocity.linear.x() = 1.0;
    world.add_agent(std::move(agent));

    // Зона конвейера: скорость +0.5 по мировому X
    {
        auto plugin = std::make_unique<effects::ConveyorEffect>();
        plugin->on_init(YAML::Load("direction: {x: 1.0, y: 0.0, z: 0.0}\nspeed: 0.5"));

        Zone z;
        z.id           = "conveyor";
        z.enabled      = true;
        z.shape.type   = ZoneShapeType::SPHERE;
        z.shape.center = Vec3::Zero();
        z.shape.radius = 100.0;

        Zone::EffectDesc desc;
        desc.type                  = "conveyor";
        desc.enabled               = true;
        desc.effect_type           = plugin->effect_type();
        desc.required_capabilities = {"surface_contact"};
        desc.plugin                = std::move(plugin);
        z.effects.push_back(std::move(desc));

        world.add_zone(std::move(z));
    }

    engine.load_world(std::move(world));

    // Один тик: dt = 0.01
    engine.step(1);

    const Agent* a = engine.world().get_agent(1);
    ASSERT_NE(a, nullptr);

    // Ожидаемое смещение = (cmd_vel + additive) * dt = (1.0 + 0.5) * 0.01 = 0.015
    constexpr double expected_x = 1.5 * 0.01;
    EXPECT_NEAR(a->world_pose.x, expected_x, 1e-9);
    EXPECT_NEAR(a->world_pose.y, 0.0, 1e-9);
}

} // namespace s2
