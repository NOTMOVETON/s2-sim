#include <gtest/gtest.h>

#include <s2/effects/charging_effect.hpp>
#include <s2/components/battery_component.hpp>
#include <s2/effects_registry.hpp>
#include <s2/zone_system.hpp>
#include <s2/sim_engine.hpp>
#include <s2/agent.hpp>
#include <s2/actor.hpp>
#include <s2/sim_bus.hpp>
#include <s2/world.hpp>

namespace s2 {

// ─── Вспомогательные функции ─────────────────────────────────────────────────

static Agent make_agent_with_battery(AgentId id = 1, double initial_level = 1.0)
{
    Agent a;
    a.id = id;
    a.world_pose.x = 0.0;
    a.world_pose.y = 0.0;
    a.world_pose.z = 0.0;
    a.capabilities.insert("has_battery");
    a.state.emplace<BatteryComponent>(BatteryComponent{initial_level, false});
    return a;
}

static Agent make_agent_without_battery(AgentId id = 1)
{
    Agent a;
    a.id = id;
    a.world_pose.x = 0.0;
    a.world_pose.y = 0.0;
    a.world_pose.z = 0.0;
    // Нет capability "has_battery", нет BatteryComponent
    return a;
}

// Зона с ChargingEffect, агент внутри (радиус 100м)
static Zone make_charging_zone(const ZoneId& zone_id, double charge_rate = 0.1)
{
    Zone z;
    z.id           = zone_id;
    z.enabled      = true;
    z.shape.type   = ZoneShapeType::SPHERE;
    z.shape.center = Vec3::Zero();
    z.shape.radius = 100.0;

    auto plugin = std::make_unique<effects::ChargingEffect>();
    std::string yaml_str = "charge_rate: " + std::to_string(charge_rate);
    plugin->on_init(YAML::Load(yaml_str));

    Zone::EffectDesc desc;
    desc.type                  = "charging";   // важно для on_agent_exit
    desc.enabled               = true;
    desc.effect_type           = plugin->effect_type();
    desc.required_capabilities = {"has_battery"};
    desc.plugin                = std::move(plugin);

    z.effects.push_back(std::move(desc));
    return z;
}

static void run_ticks(ZoneSystem& zs, std::vector<Agent>& agents, int n,
                      double dt = 0.01)
{
    SimBus bus;
    std::vector<Actor> actors;
    for (int i = 0; i < n; ++i) {
        zs.tick(agents, actors, bus, i * dt, dt);
    }
}

// ─── Тест 1: Уровень батареи растёт при нахождении в зоне ──────────────────

TEST(ChargingEffect, IncreasesLevel)
{
    ZoneSystem zs;
    zs.add_zone(make_charging_zone("charger", 0.1));

    std::vector<Agent> agents;
    agents.push_back(make_agent_with_battery(1, 0.5));

    constexpr int N  = 10;
    constexpr double dt = 1.0;  // 1 секунда на тик для простой арифметики
    SimBus bus;
    std::vector<Actor> actors;
    for (int i = 0; i < N; ++i) {
        zs.tick(agents, actors, bus, i * dt, dt);
    }

    const auto* bat = agents[0].state.get<BatteryComponent>();
    ASSERT_NE(bat, nullptr);
    // 0.5 + 0.1 * 1.0 * 10 = 1.0 (с капом)
    EXPECT_NEAR(bat->level, 1.0, 1e-9);
}

// ─── Тест 2: Уровень не превышает 1.0 ───────────────────────────────────────

TEST(ChargingEffect, CapsAt100Percent)
{
    ZoneSystem zs;
    zs.add_zone(make_charging_zone("charger", 0.1));

    std::vector<Agent> agents;
    agents.push_back(make_agent_with_battery(1, 0.95));

    constexpr double dt = 1.0;
    SimBus bus;
    std::vector<Actor> actors;
    // 10 тиков: 0.95 + 0.1 * 10 = 1.95 без капа, с капом = 1.0
    for (int i = 0; i < 10; ++i) {
        zs.tick(agents, actors, bus, i * dt, dt);
    }

    const auto* bat = agents[0].state.get<BatteryComponent>();
    ASSERT_NE(bat, nullptr);
    EXPECT_LE(bat->level, 1.0);
}

// ─── Тест 3: Агент без "has_battery" — уровень не меняется ──────────────────

TEST(ChargingEffect, NoCapability)
{
    ZoneSystem zs;
    zs.add_zone(make_charging_zone("charger", 0.1));

    std::vector<Agent> agents;
    agents.push_back(make_agent_without_battery(1));
    // Добавляем BatteryComponent вручную, но без capability
    agents[0].state.emplace<BatteryComponent>(BatteryComponent{0.5, false});

    constexpr double dt = 1.0;
    SimBus bus;
    std::vector<Actor> actors;
    for (int i = 0; i < 5; ++i) {
        zs.tick(agents, actors, bus, i * dt, dt);
    }

    const auto* bat = agents[0].state.get<BatteryComponent>();
    ASSERT_NE(bat, nullptr);
    // Эффект не применился — уровень остался 0.5
    EXPECT_NEAR(bat->level, 0.5, 1e-9);
}

// ─── Тест 4: Флаг charging = true пока агент в зоне ─────────────────────────

TEST(ChargingEffect, ChargingFlagSet)
{
    ZoneSystem zs;
    zs.add_zone(make_charging_zone("charger", 0.01));

    std::vector<Agent> agents;
    agents.push_back(make_agent_with_battery(1, 0.5));

    SimBus bus;
    std::vector<Actor> actors;
    zs.tick(agents, actors, bus, 0.0, 0.01);

    const auto* bat = agents[0].state.get<BatteryComponent>();
    ASSERT_NE(bat, nullptr);
    EXPECT_TRUE(bat->charging);
}

// ─── Тест 5: Флаг charging сбрасывается при выходе из зоны ──────────────────

TEST(ChargingEffect, ChargingFlagClearedOnExit)
{
    ZoneSystem zs;
    zs.add_zone(make_charging_zone("charger", 0.01));

    std::vector<Agent> agents;
    agents.push_back(make_agent_with_battery(1, 0.5));

    SimBus bus;
    std::vector<Actor> actors;

    // Первый тик — агент внутри
    zs.tick(agents, actors, bus, 0.0, 0.01);
    {
        const auto* bat = agents[0].state.get<BatteryComponent>();
        ASSERT_NE(bat, nullptr);
        EXPECT_TRUE(bat->charging);
    }

    // Перемещаем агента за пределы зоны (радиус 100м)
    agents[0].world_pose.x = 200.0;

    // Второй тик — агент снаружи, on_agent_exit должен сбросить charging
    zs.tick(agents, actors, bus, 0.01, 0.01);
    {
        const auto* bat = agents[0].state.get<BatteryComponent>();
        ASSERT_NE(bat, nullptr);
        EXPECT_FALSE(bat->charging);
    }
}

// ─── Тест 6: Компонент создаётся если отсутствует ───────────────────────────

TEST(ChargingEffect, CreatesComponentIfMissing)
{
    ZoneSystem zs;
    zs.add_zone(make_charging_zone("charger", 0.1));

    std::vector<Agent> agents;
    // Агент с capability, но без явного BatteryComponent
    Agent a;
    a.id = 1;
    a.world_pose.x = 0.0;
    a.capabilities.insert("has_battery");
    agents.push_back(std::move(a));

    SimBus bus;
    std::vector<Actor> actors;
    zs.tick(agents, actors, bus, 0.0, 0.01);

    const auto* bat = agents[0].state.get<BatteryComponent>();
    ASSERT_NE(bat, nullptr) << "BatteryComponent должен быть создан ChargingEffect";
    // Уровень = 1.0 (дефолт) + charge_rate * dt = min(1.0, 1.0 + 0.001) = 1.0
    EXPECT_NEAR(bat->level, 1.0, 1e-9);
}

// ─── Тест 7: BatteryComponent доступен через SharedState после тика ──────────
// battery_level в AgentSnapshot заполняется BatteryPlugin (задача 32).
// Здесь проверяем что ChargingEffect корректно обновляет компонент внутри SimEngine.

TEST(ChargingEffect, ComponentAccessibleViaState)
{
    SimEngine engine{{.update_rate = 100.0, .viz_rate = 30.0}};
    engine.set_effect_factory(s2::create_effect);

    SimWorld world;

    Agent agent;
    agent.id = 1;
    agent.name = "robot_0";
    agent.world_pose.x = 0.0;
    agent.capabilities.insert("has_battery");
    agent.state.emplace<BatteryComponent>(BatteryComponent{0.7, false});
    world.add_agent(std::move(agent));

    // Зона зарядки
    Zone z;
    z.id           = "charger";
    z.enabled      = true;
    z.shape.type   = ZoneShapeType::SPHERE;
    z.shape.center = Vec3::Zero();
    z.shape.radius = 100.0;

    Zone::EffectDesc desc;
    desc.type                  = "charging";
    desc.enabled               = true;
    desc.effect_type           = EffectType::CONTINUOUS;
    desc.required_capabilities = {"has_battery"};
    desc.plugin                = s2::create_effect("charging", YAML::Load("charge_rate: 0.1"));
    z.effects.push_back(std::move(desc));
    world.add_zone(std::move(z));

    engine.load_world(std::move(world));
    engine.step(1);

    const Agent* a = engine.world().get_agent(1);
    ASSERT_NE(a, nullptr);

    const auto* bat = a->state.get<BatteryComponent>();
    ASSERT_NE(bat, nullptr) << "BatteryComponent должен быть в SharedState после тика";
    // level = 0.7 + charge_rate * dt, dt = 1/update_rate = 0.01
    EXPECT_GT(bat->level, 0.7);
    EXPECT_LE(bat->level, 1.0);
    EXPECT_TRUE(bat->charging);
}

} // namespace s2
