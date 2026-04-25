#include <gtest/gtest.h>

#include <s2/effects/teleport_effect.hpp>
#include <s2/components/pending_teleport.hpp>
#include <s2/zone_system.hpp>
#include <s2/agent.hpp>
#include <s2/actor.hpp>
#include <s2/sim_bus.hpp>

namespace s2 {

// ─── Вспомогательные функции ─────────────────────────────────────────────────

static Agent make_agent(AgentId id = 1)
{
    Agent a;
    a.id = id;
    a.world_pose.x = 0.0;
    a.world_pose.y = 0.0;
    a.world_pose.z = 0.0;
    return a;
}

static Zone make_immediate_zone(const ZoneId& zone_id, Vec3 dest, double yaw = 0.0)
{
    Zone z;
    z.id           = zone_id;
    z.enabled      = true;
    z.shape.type   = ZoneShapeType::SPHERE;
    z.shape.center = Vec3::Zero();
    z.shape.radius = 100.0;

    auto plugin = std::make_unique<effects::TeleportEffect>();
    YAML::Node params;
    params["trigger_mode"] = "immediate";
    params["target_mode"]  = "fixed";
    params["target_entity"] = "agent";
    params["destination"]["x"]   = dest.x();
    params["destination"]["y"]   = dest.y();
    params["destination"]["z"]   = dest.z();
    params["destination"]["yaw"] = yaw;
    plugin->on_init(params);

    Zone::EffectDesc desc;
    desc.type        = "teleport";
    desc.enabled     = true;
    desc.effect_type = plugin->effect_type();
    desc.plugin      = std::move(plugin);
    z.effects.push_back(std::move(desc));
    return z;
}

static Zone make_after_seconds_zone(const ZoneId& zone_id, Vec3 dest, double trigger_secs)
{
    Zone z;
    z.id           = zone_id;
    z.enabled      = true;
    z.shape.type   = ZoneShapeType::SPHERE;
    z.shape.center = Vec3::Zero();
    z.shape.radius = 100.0;

    auto plugin = std::make_unique<effects::TeleportEffect>();
    YAML::Node params;
    params["trigger_mode"]  = "after_seconds";
    params["after_seconds"] = trigger_secs;
    params["target_mode"]   = "fixed";
    params["target_entity"] = "agent";
    params["destination"]["x"] = dest.x();
    params["destination"]["y"] = dest.y();
    params["destination"]["z"] = dest.z();
    plugin->on_init(params);

    Zone::EffectDesc desc;
    desc.type        = "teleport";
    desc.enabled     = true;
    desc.effect_type = plugin->effect_type();
    desc.plugin      = std::move(plugin);
    z.effects.push_back(std::move(desc));
    return z;
}

/// Применяет PendingTeleport к агенту — воспроизводит логику фазы 3m SimEngine.
static void apply_pending_teleport(Agent& agent)
{
    auto* pt = agent.state.get<PendingTeleport>();
    if (pt && pt->pending) {
        agent.world_pose.x   = pt->destination.x();
        agent.world_pose.y   = pt->destination.y();
        agent.world_pose.z   = pt->destination.z();
        agent.world_pose.yaw = pt->yaw;
        agent.world_velocity.linear  = Vec3::Zero();
        agent.world_velocity.angular = Vec3::Zero();
        pt->pending = false;
    }
}

// ─── Тест 1: Немедленный телепорт — PendingTeleport создаётся при входе ──────

TEST(TeleportImmediate, OnEntry)
{
    ZoneSystem zs;
    const Vec3 dest{5.0, 3.0, 0.0};
    zs.add_zone(make_immediate_zone("portal", dest, 1.57));

    std::vector<Agent> agents;
    agents.push_back(make_agent(1));

    SimBus bus;
    std::vector<Actor> actors;
    zs.tick(agents, actors, bus, 0.0, 0.01);

    const auto* pt = agents[0].state.get<PendingTeleport>();
    ASSERT_NE(pt, nullptr) << "PendingTeleport должен быть создан при входе в зону";
    EXPECT_TRUE(pt->pending) << "pending должен быть true";
    EXPECT_NEAR(pt->destination.x(), 5.0, 1e-6);
    EXPECT_NEAR(pt->destination.y(), 3.0, 1e-6);
    EXPECT_NEAR(pt->destination.z(), 0.0, 1e-6);
    EXPECT_NEAR(pt->yaw, 1.57, 1e-6);
}

// ─── Тест 2: После телепорта скорость = 0, позиция = destination ─────────────

TEST(TeleportImmediate, VelocityClearedAfterTeleport)
{
    Agent agent = make_agent(1);
    agent.world_velocity.linear  = Vec3{1.0, 0.5, 0.0};
    agent.world_velocity.angular = Vec3{0.0, 0.0, 0.3};

    PendingTeleport pt;
    pt.destination = Vec3{5.0, 3.0, 1.0};
    pt.yaw         = 1.57;
    pt.pending     = true;
    agent.state.emplace<PendingTeleport>(pt);

    apply_pending_teleport(agent);

    EXPECT_NEAR(agent.world_pose.x, 5.0, 1e-6) << "x должен быть равен destination.x";
    EXPECT_NEAR(agent.world_pose.y, 3.0, 1e-6) << "y должен быть равен destination.y";
    EXPECT_NEAR(agent.world_pose.z, 1.0, 1e-6) << "z должен быть равен destination.z";
    EXPECT_NEAR(agent.world_pose.yaw, 1.57, 1e-6) << "yaw должен совпасть с pt.yaw";
    EXPECT_NEAR(agent.world_velocity.linear.norm(), 0.0, 1e-6) << "линейная скорость должна быть 0";
    EXPECT_NEAR(agent.world_velocity.angular.norm(), 0.0, 1e-6) << "угловая скорость должна быть 0";

    const auto* result = agent.state.get<PendingTeleport>();
    ASSERT_NE(result, nullptr);
    EXPECT_FALSE(result->pending) << "pending должен быть сброшен после применения";
}

// ─── Тест 3: MUTATION применяется только один раз при входе ──────────────────

TEST(TeleportImmediate, OncePerEntry)
{
    ZoneSystem zs;
    zs.add_zone(make_immediate_zone("portal", Vec3{5.0, 0.0, 0.0}));

    std::vector<Agent> agents;
    agents.push_back(make_agent(1));

    SimBus bus;
    std::vector<Actor> actors;

    // Тик 1: агент входит → PendingTeleport{pending=true}
    zs.tick(agents, actors, bus, 0.0, 0.01);
    auto* pt = agents[0].state.get<PendingTeleport>();
    ASSERT_NE(pt, nullptr);
    EXPECT_TRUE(pt->pending);

    // Симулируем обработку SimEngine
    pt->pending = false;

    // Тик 2: агент всё ещё в зоне — MUTATION не повторяется
    zs.tick(agents, actors, bus, 0.01, 0.01);
    pt = agents[0].state.get<PendingTeleport>();
    ASSERT_NE(pt, nullptr);
    EXPECT_FALSE(pt->pending) << "MUTATION не должна повторяться при повторном тике внутри зоны";
}

// ─── Тест 4: Таймер накапливается, телепорт срабатывает после N секунд ───────

TEST(TeleportAfterSeconds, TimerAccumulates)
{
    ZoneSystem zs;
    zs.add_zone(make_after_seconds_zone("slow_portal", Vec3{10.0, 0.0, 0.0}, 3.0));

    std::vector<Agent> agents;
    agents.push_back(make_agent(1));

    SimBus bus;
    std::vector<Actor> actors;

    // 2 тика по 1.5с = 3.0с — ровно на пороге (не срабатывает, нужно превысить)
    // Чтобы точно не сработало: 2 тика по 1.4с = 2.8s < 3.0s
    zs.tick(agents, actors, bus, 0.0, 1.4);
    zs.tick(agents, actors, bus, 1.4, 1.4);

    auto* pt = agents[0].state.get<PendingTeleport>();
    EXPECT_TRUE(pt == nullptr || !pt->pending)
        << "После 2.8s телепорт ещё не должен сработать";

    // Ещё 2 тика по 1.4с: итого 5.6s > 3.0s — телепорт срабатывает
    zs.tick(agents, actors, bus, 2.8, 1.4);

    pt = agents[0].state.get<PendingTeleport>();
    ASSERT_NE(pt, nullptr) << "PendingTeleport должен появиться после 4.2s (> 3.0s)";
    EXPECT_TRUE(pt->pending) << "pending должен быть true";
    EXPECT_NEAR(pt->destination.x(), 10.0, 1e-6);
}

// ─── Тест 5: Выход из зоны сбрасывает таймер ─────────────────────────────────

TEST(TeleportAfterSeconds, ExitResetsTimer)
{
    ZoneSystem zs;
    zs.add_zone(make_after_seconds_zone("slow_portal", Vec3{10.0, 0.0, 0.0}, 3.0));

    std::vector<Agent> agents;
    agents.push_back(make_agent(1));

    SimBus bus;
    std::vector<Actor> actors;

    // Тик 1: 2 секунды в зоне
    zs.tick(agents, actors, bus, 0.0, 2.0);

    // Агент выходит из зоны
    agents[0].world_pose.x = 500.0;
    zs.tick(agents, actors, bus, 2.0, 0.01);

    // Агент возвращается в зону — таймер сброшен
    agents[0].world_pose.x = 0.0;
    zs.tick(agents, actors, bus, 2.01, 2.0);

    // 2 секунды < 3 секунд — телепорт не срабатывает
    const auto* pt = agents[0].state.get<PendingTeleport>();
    EXPECT_TRUE(pt == nullptr || !pt->pending)
        << "После выхода таймер должен сброситься, 2.0s < 3.0s — не должно телепортировать";
}

// ─── Тест 6: Телепортирует только один раз, даже при долгом нахождении ───────

TEST(TeleportAfterSeconds, TeleportedOnlyOnce)
{
    ZoneSystem zs;
    zs.add_zone(make_after_seconds_zone("slow_portal", Vec3{10.0, 0.0, 0.0}, 3.0));

    std::vector<Agent> agents;
    agents.push_back(make_agent(1));

    SimBus bus;
    std::vector<Actor> actors;

    int teleport_count = 0;
    double sim_time    = 0.0;
    const double dt    = 1.0;

    // 10 тиков по 1 секунде = 10 секунд в зоне
    for (int i = 0; i < 10; ++i) {
        // Сбрасываем pending перед каждым тиком (имитируем SimEngine)
        if (auto* pt = agents[0].state.get<PendingTeleport>()) {
            if (pt->pending) {
                ++teleport_count;
                pt->pending = false;
            }
        }
        zs.tick(agents, actors, bus, sim_time, dt);
        sim_time += dt;
    }
    // Проверяем последний тик
    if (auto* pt = agents[0].state.get<PendingTeleport>()) {
        if (pt->pending) ++teleport_count;
    }

    EXPECT_EQ(teleport_count, 1) << "Телепорт должен сработать ровно один раз за сессию в зоне";
}

// ─── Тест 7: Фильтр target_entity — "prop" не телепортирует агента ───────────

TEST(TeleportEntityFilter, AgentAndPropModes)
{
    // Зона с target_entity="agent" — агент получает PendingTeleport
    {
        Zone z;
        z.id           = "portal_agent";
        z.enabled      = true;
        z.shape.type   = ZoneShapeType::SPHERE;
        z.shape.center = Vec3::Zero();
        z.shape.radius = 100.0;

        auto plugin = std::make_unique<effects::TeleportEffect>();
        YAML::Node params;
        params["trigger_mode"]     = "immediate";
        params["target_entity"]    = "agent";
        params["destination"]["x"] = 5.0;
        params["destination"]["y"] = 0.0;
        params["destination"]["z"] = 0.0;
        plugin->on_init(params);

        Zone::EffectDesc desc;
        desc.type        = "teleport";
        desc.enabled     = true;
        desc.effect_type = plugin->effect_type();
        desc.plugin      = std::move(plugin);
        z.effects.push_back(std::move(desc));

        ZoneSystem zs;
        zs.add_zone(std::move(z));

        std::vector<Agent> agents;
        agents.push_back(make_agent(1));
        SimBus bus;
        std::vector<Actor> actors;
        zs.tick(agents, actors, bus, 0.0, 0.01);

        const auto* pt = agents[0].state.get<PendingTeleport>();
        ASSERT_NE(pt, nullptr) << "target_entity=agent — агент должен получить PendingTeleport";
        EXPECT_TRUE(pt->pending);
    }

    // Зона с target_entity="prop" — агент НЕ получает PendingTeleport
    {
        Zone z;
        z.id           = "portal_prop";
        z.enabled      = true;
        z.shape.type   = ZoneShapeType::SPHERE;
        z.shape.center = Vec3::Zero();
        z.shape.radius = 100.0;

        auto plugin = std::make_unique<effects::TeleportEffect>();
        YAML::Node params;
        params["trigger_mode"]     = "immediate";
        params["target_entity"]    = "prop";
        params["destination"]["x"] = 5.0;
        params["destination"]["y"] = 0.0;
        params["destination"]["z"] = 0.0;
        plugin->on_init(params);

        Zone::EffectDesc desc;
        desc.type        = "teleport";
        desc.enabled     = true;
        desc.effect_type = plugin->effect_type();
        desc.plugin      = std::move(plugin);
        z.effects.push_back(std::move(desc));

        ZoneSystem zs;
        zs.add_zone(std::move(z));

        std::vector<Agent> agents;
        agents.push_back(make_agent(1));
        SimBus bus;
        std::vector<Actor> actors;
        zs.tick(agents, actors, bus, 0.0, 0.01);

        const auto* pt = agents[0].state.get<PendingTeleport>();
        EXPECT_TRUE(pt == nullptr || !pt->pending)
            << "target_entity=prop — агент НЕ должен получить PendingTeleport";
    }
}

} // namespace s2
