#include <gtest/gtest.h>

#include <s2/effects/tire_puncture.hpp>
#include <s2/components/tire_puncture_data.hpp>
#include <s2/effects_registry.hpp>
#include <s2/zone_system.hpp>
#include <s2/sim_engine.hpp>
#include <s2/agent.hpp>
#include <s2/actor.hpp>
#include <s2/sim_bus.hpp>
#include <s2/world.hpp>
#include <s2/plugins/diff_drive.hpp>

namespace s2 {

// ─── Вспомогательные функции ─────────────────────────────────────────────────

static Agent make_wheeled_agent(AgentId id = 1)
{
    Agent a;
    a.id = id;
    a.world_pose.x = 0.0;
    a.world_pose.y = 0.0;
    a.world_pose.z = 0.0;
    a.capabilities.insert("wheeled");
    return a;
}

static Agent make_agent_no_capability(AgentId id = 1)
{
    Agent a;
    a.id = id;
    a.world_pose.x = 0.0;
    a.world_pose.y = 0.0;
    a.world_pose.z = 0.0;
    return a;
}

static Zone make_puncture_zone(const ZoneId& zone_id)
{
    Zone z;
    z.id           = zone_id;
    z.enabled      = true;
    z.shape.type   = ZoneShapeType::SPHERE;
    z.shape.center = Vec3::Zero();
    z.shape.radius = 100.0;

    auto plugin = std::make_unique<effects::TirePunctureEffect>();
    plugin->on_init(YAML::Node{});

    Zone::EffectDesc desc;
    desc.type                  = "tire_puncture";
    desc.enabled               = true;
    desc.effect_type           = plugin->effect_type();
    desc.required_capabilities = {"wheeled"};
    desc.plugin                = std::move(plugin);

    z.effects.push_back(std::move(desc));
    return z;
}

// ─── Тест 1: Прокол применяется при входе в зону ─────────────────────────────

TEST(TirePuncture, AppliedOnEntry)
{
    ZoneSystem zs;
    zs.add_zone(make_puncture_zone("nails"));

    std::vector<Agent> agents;
    agents.push_back(make_wheeled_agent(1));

    SimBus bus;
    std::vector<Actor> actors;
    zs.tick(agents, actors, bus, 0.0, 0.01);

    const auto* tire = agents[0].state.get<TirePunctureData>();
    ASSERT_NE(tire, nullptr) << "TirePunctureData должен быть создан при входе";
    EXPECT_TRUE(tire->punctured);
}

// ─── Тест 2: Прокол сохраняется после выхода из зоны ─────────────────────────

TEST(TirePuncture, PersistsAfterExit)
{
    ZoneSystem zs;
    zs.add_zone(make_puncture_zone("nails"));

    std::vector<Agent> agents;
    agents.push_back(make_wheeled_agent(1));

    SimBus bus;
    std::vector<Actor> actors;

    zs.tick(agents, actors, bus, 0.0, 0.01);

    // Перемещаем агента за пределы зоны
    agents[0].world_pose.x = 200.0;
    zs.tick(agents, actors, bus, 0.01, 0.01);

    const auto* tire = agents[0].state.get<TirePunctureData>();
    ASSERT_NE(tire, nullptr) << "TirePunctureData должен остаться после выхода";
    EXPECT_TRUE(tire->punctured) << "Прокол необратим — punctured остаётся true";
}

// ─── Тест 3: Агент без "wheeled" — TirePunctureData не создаётся ──────────────

TEST(TirePuncture, NoCapability)
{
    ZoneSystem zs;
    zs.add_zone(make_puncture_zone("nails"));

    std::vector<Agent> agents;
    agents.push_back(make_agent_no_capability(1));

    SimBus bus;
    std::vector<Actor> actors;
    zs.tick(agents, actors, bus, 0.0, 0.01);

    const auto* tire = agents[0].state.get<TirePunctureData>();
    EXPECT_EQ(tire, nullptr) << "TirePunctureData не должен создаваться без capability wheeled";
}

// ─── Тест 4: DiffDrive снижает скорость вдвое при проколе ────────────────────

TEST(TirePuncture, DiffDrivePenalty)
{
    Agent agent;
    agent.id = 1;

    TirePunctureData tire;
    tire.punctured = true;
    agent.state.emplace<TirePunctureData>(tire);

    DiffDriveData dd;
    dd.desired_linear  = 1.0;
    dd.desired_angular = 0.0;
    agent.state.emplace<DiffDriveData>(dd);

    agent.state.resolve();

    plugins::DiffDrivePlugin plugin;
    plugin.from_config(YAML::Load("max_linear_vel: 2.0\nmax_angular_vel: 1.5"));

    plugin.update(0.01, agent);

    // При проколе скорость = 1.0 × 0.5 = 0.5
    EXPECT_NEAR(agent.world_velocity.linear.x(), 0.5, 0.01);
}

// ─── Тест 5: DiffDrive не снижает скорость без прокола ───────────────────────

TEST(TirePuncture, DiffDriveNoPenaltyWhenOk)
{
    Agent agent;
    agent.id = 1;

    DiffDriveData dd;
    dd.desired_linear  = 1.0;
    dd.desired_angular = 0.0;
    agent.state.emplace<DiffDriveData>(dd);

    agent.state.resolve();

    plugins::DiffDrivePlugin plugin;
    plugin.from_config(YAML::Load("max_linear_vel: 2.0\nmax_angular_vel: 1.5"));

    plugin.update(0.01, agent);

    // Без прокола скорость = 1.0 (штрафа нет)
    EXPECT_NEAR(agent.world_velocity.linear.x(), 1.0, 0.01);
}

} // namespace s2
