#include <gtest/gtest.h>

#include <s2/effects/ice_modifier.hpp>
#include <s2/effects/boost_zone.hpp>
#include <s2/effects/motion_lock_zone.hpp>
#include <s2/plugins/diff_drive.hpp>
#include <s2/zone_system.hpp>
#include <s2/agent.hpp>
#include <s2/actor.hpp>
#include <s2/sim_bus.hpp>
#include <s2/sensor_data.hpp>
#include <s2/world_query.hpp>
#include <s2/event_bus.hpp>
#include <s2/plugin_base.hpp>

// Null-контекст для тестов плагинов
static s2::WorldQuery         g_null_world;
static s2::EventBus           g_null_bus;
static s2::KernelCommandQueue g_null_cmds;
static s2::PluginContext      g_ctx{g_null_world, g_null_bus, g_null_cmds};

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

// Создать зону-сферу с уже готовым плагином эффекта
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
    z.shape.radius = 10.0;  // большой радиус — агент всегда внутри

    Zone::EffectDesc desc;
    desc.type                  = "test_plugin";
    desc.enabled               = true;
    desc.effect_type           = plugin->effect_type();
    desc.required_capabilities = required_caps;
    desc.plugin                = std::move(plugin);

    z.effects.push_back(std::move(desc));
    return z;
}

// Выполнить один тик ZoneSystem и вернуть ссылку на агентов
static void run_tick(ZoneSystem& zs, std::vector<Agent>& agents, double sim_time = 0.0)
{
    SimBus bus;
    std::vector<Actor> actors;
    zs.tick(agents, actors, bus, sim_time, 0.01);
}

// ─── Тест 1: IceModifier замедляет агента с нужным capability ─────────────────

TEST(EffectModifier, IceModifier_SlowsAgent)
{
    auto plugin = std::make_unique<effects::IceModifier>();
    plugin->on_init(YAML::Load("traction_coefficient: 0.2"));

    Zone zone = make_sphere_zone_with_effect("ice_zone", std::move(plugin), {"surface_contact"});

    ZoneSystem zs;
    zs.add_zone(std::move(zone));

    std::vector<Agent> agents;
    agents.push_back(make_agent_with_capability("surface_contact"));

    run_tick(zs, agents);
    agents[0].state.resolve();

    EXPECT_NEAR(agents[0].state.effective().speed_scale, 0.2, 1e-9);
    EXPECT_FALSE(agents[0].state.effective().motion_locked);
}

// ─── Тест 2: IceModifier — агент без capability не замедляется ──────────────

TEST(EffectModifier, IceModifier_NoCapability)
{
    auto plugin = std::make_unique<effects::IceModifier>();
    plugin->on_init(YAML::Load("traction_coefficient: 0.2"));

    Zone zone = make_sphere_zone_with_effect("ice_zone", std::move(plugin), {"surface_contact"});

    ZoneSystem zs;
    zs.add_zone(std::move(zone));

    std::vector<Agent> agents;
    agents.push_back(make_agent(1));  // без capability

    run_tick(zs, agents);
    agents[0].state.resolve();

    // scale не применялся → должен остаться 1.0
    EXPECT_NEAR(agents[0].state.effective().speed_scale, 1.0, 1e-9);
}

// ─── Тест 3: IceModifier с noise_amplitude — scale в [0.01, 1.0] ─────────────

TEST(EffectModifier, IceModifier_NoiseAmplitude)
{
    auto plugin = std::make_unique<effects::IceModifier>();
    plugin->on_init(YAML::Load("traction_coefficient: 0.5\nnoise_amplitude: 0.4"));

    Zone zone = make_sphere_zone_with_effect("ice_zone", std::move(plugin), {"surface_contact"});

    ZoneSystem zs;
    zs.add_zone(std::move(zone));

    std::vector<Agent> agents;
    agents.push_back(make_agent_with_capability("surface_contact"));

    // Прогоняем несколько тиков с разным sim_time
    for (int i = 0; i < 20; ++i) {
        agents[0].state.clear_contributions();
        run_tick(zs, agents, i * 0.1);
        agents[0].state.resolve();
        double s = agents[0].state.effective().speed_scale;
        EXPECT_GE(s, 0.01) << "tick " << i;
        EXPECT_LE(s, 1.0)  << "tick " << i;
    }
}

// ─── Тест 4: BoostZone ускоряет агента с capability ──────────────────────────

TEST(EffectModifier, BoostZone_SpeedsUpAgent)
{
    auto plugin = std::make_unique<effects::BoostZone>();
    plugin->on_init(YAML::Load("speed_multiplier: 1.5"));

    Zone zone = make_sphere_zone_with_effect("boost_zone", std::move(plugin), {"surface_contact"});

    ZoneSystem zs;
    zs.add_zone(std::move(zone));

    std::vector<Agent> agents;
    agents.push_back(make_agent_with_capability("surface_contact"));

    run_tick(zs, agents);
    agents[0].state.resolve();

    EXPECT_NEAR(agents[0].state.effective().speed_scale, 1.5, 1e-9);
    EXPECT_FALSE(agents[0].state.effective().motion_locked);
}

// ─── Тест 5: MotionLockZone блокирует движение ────────────────────────────────

TEST(EffectModifier, MotionLock_BlocksMovement)
{
    auto plugin = std::make_unique<effects::MotionLockZone>();
    plugin->on_init(YAML::Load("source_label: danger"));

    Zone zone = make_sphere_zone_with_effect("lock_zone", std::move(plugin));

    ZoneSystem zs;
    zs.add_zone(std::move(zone));

    std::vector<Agent> agents;
    agents.push_back(make_agent(1));

    run_tick(zs, agents);
    agents[0].state.resolve();

    EXPECT_TRUE(agents[0].state.effective().motion_locked);
}

// ─── Тест 6: Два MODIFIER — произведение scale (ice 0.2 × boost 1.5 ≈ 0.3) ──

TEST(EffectModifier, TwoModifiers_Combined)
{
    // Зона льда
    auto ice = std::make_unique<effects::IceModifier>();
    ice->on_init(YAML::Load("traction_coefficient: 0.2"));

    // Зона буста
    auto boost = std::make_unique<effects::BoostZone>();
    boost->on_init(YAML::Load("speed_multiplier: 1.5"));

    ZoneSystem zs;

    Zone ice_zone = make_sphere_zone_with_effect("ice", std::move(ice), {"surface_contact"});
    Zone boost_zone = make_sphere_zone_with_effect("boost", std::move(boost), {"surface_contact"});
    zs.add_zone(std::move(ice_zone));
    zs.add_zone(std::move(boost_zone));

    std::vector<Agent> agents;
    agents.push_back(make_agent_with_capability("surface_contact"));

    run_tick(zs, agents);
    agents[0].state.resolve();

    // resolve() перемножает scale contributions: 0.2 * 1.5 = 0.3
    EXPECT_NEAR(agents[0].state.effective().speed_scale, 0.3, 1e-9);
    EXPECT_FALSE(agents[0].state.effective().motion_locked);
}

// ─── Тест 7: MotionLock + BoostZone — агент заблокирован (lock приоритетнее) ─

TEST(EffectModifier, MotionLock_StopsRegardlessOfBoost)
{
    auto lock = std::make_unique<effects::MotionLockZone>();
    lock->on_init(YAML::Load("source_label: danger"));

    auto boost = std::make_unique<effects::BoostZone>();
    boost->on_init(YAML::Load("speed_multiplier: 2.0"));

    ZoneSystem zs;
    zs.add_zone(make_sphere_zone_with_effect("lock", std::move(lock)));
    zs.add_zone(make_sphere_zone_with_effect("boost", std::move(boost), {"surface_contact"}));

    std::vector<Agent> agents;
    agents.push_back(make_agent_with_capability("surface_contact"));

    run_tick(zs, agents);
    agents[0].state.resolve();

    // motion_locked = true независимо от speed_scale
    EXPECT_TRUE(agents[0].state.effective().motion_locked);
}

// ─── Тест 8: DiffDrivePlugin читает speed_scale из effective() ────────────────

TEST(EffectModifier, DiffDrive_ReadsEffectiveScale)
{
    // Устанавливаем scale 0.5 напрямую через add_scale
    Agent agent = make_agent(1);
    // Задаём желаемую скорость через DiffDriveData в SharedState
    DiffDriveData dd;
    dd.desired_linear  = 1.0;
    dd.desired_angular = 0.0;
    agent.state.emplace<DiffDriveData>(dd);

    // Публикуем scale 0.5
    agent.state.add_scale(0.5, "test_ice");
    agent.state.resolve();

    // Запускаем DiffDrivePlugin
    plugins::DiffDrivePlugin drive;
    drive.from_config(YAML::Load("max_linear_vel: 2.0\nmax_angular_vel: 1.5"));
    drive.update(0.01, agent, g_ctx);

    // Ожидаем: 1.0 * 0.5 = 0.5 м/с
    EXPECT_NEAR(agent.world_velocity.linear.x(), 0.5, 1e-9);
}

// ─── Тест 9: DiffDrivePlugin — motion_locked = true → velocity = 0 ───────────

TEST(EffectModifier, DiffDrive_MotionLocked_StopsAgent)
{
    Agent agent = make_agent(1);
    DiffDriveData dd;
    dd.desired_linear  = 1.0;
    dd.desired_angular = 0.5;
    agent.state.emplace<DiffDriveData>(dd);

    agent.state.add_lock(true, "forbidden_zone");
    agent.state.resolve();

    plugins::DiffDrivePlugin drive;
    drive.from_config(YAML::Load("max_linear_vel: 2.0\nmax_angular_vel: 1.5"));
    drive.update(0.01, agent, g_ctx);

    EXPECT_NEAR(agent.world_velocity.linear.x(), 0.0, 1e-9);
    EXPECT_NEAR(agent.world_velocity.angular.z(), 0.0, 1e-9);
}

} // namespace s2
