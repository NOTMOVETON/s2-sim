#include <gtest/gtest.h>
#include <s2/zone.hpp>
#include <s2/zone_system.hpp>
#include <s2/effect_context.hpp>
#include <s2/sim_bus.hpp>
#include <s2/agent.hpp>
#include <s2/actor.hpp>

// ── Тесты данных (из Plan 01) ────────────────────────────────────────────────

// Тест: поля lifecycle доступны и имеют правильные дефолты
TEST(ZoneLifecycle, DefaultValues)
{
    s2::Zone z;
    EXPECT_DOUBLE_EQ(z.strength, 1.0);
    EXPECT_DOUBLE_EQ(z.lifecycle.initial_strength, 1.0);
    EXPECT_DOUBLE_EQ(z.lifecycle.growth_rate, 0.0);
    EXPECT_DOUBLE_EQ(z.lifecycle.decay_rate, 0.0);
    EXPECT_DOUBLE_EQ(z.lifecycle.decay_delay, 0.0);
    EXPECT_DOUBLE_EQ(z.lifecycle.max_strength, 1.0);
    EXPECT_DOUBLE_EQ(z.lifecycle.remove_threshold, 0.05);
}

TEST(ZoneLifecycle, CanSetStrength)
{
    s2::Zone z;
    z.strength = 0.5;
    EXPECT_DOUBLE_EQ(z.strength, 0.5);
}

TEST(ZoneLifecycle, EffectContextStrength)
{
    s2::EffectContext ctx;
    EXPECT_DOUBLE_EQ(ctx.zone_strength, 1.0);
    EXPECT_EQ(ctx.contact_link, "");
    ctx.zone_strength = 0.3;
    ctx.contact_link = "left_wheel";
    EXPECT_DOUBLE_EQ(ctx.zone_strength, 0.3);
    EXPECT_EQ(ctx.contact_link, "left_wheel");
}

// ── Вспомогательные функции для ZoneSystem-тестов ────────────────────────────

namespace {

s2::Agent make_agent(s2::AgentId id, double x, double y, double z = 0.0)
{
    s2::Agent a;
    a.id = id;
    a.world_pose.x = x;
    a.world_pose.y = y;
    a.world_pose.z = z;
    return a;
}

s2::Zone make_sphere_zone(const s2::ZoneId& id, s2::Vec3 center, double radius)
{
    s2::Zone z;
    z.id           = id;
    z.enabled      = true;
    z.shape.type   = s2::ZoneShapeType::SPHERE;
    z.shape.center = center;
    z.shape.radius = radius;
    return z;
}

} // namespace

// ── Тесты ZoneSystem lifecycle (Plan 03) ─────────────────────────────────────

// Рост strength
TEST(ZoneSystemLifecycle, StrengthGrowth)
{
    s2::ZoneSystem zs;
    auto zone = make_sphere_zone("grow_zone", s2::Vec3(0, 0, 0), 5.0);
    zone.strength = 0.5;
    zone.lifecycle.growth_rate = 0.5; // +0.5/сек
    zone.lifecycle.max_strength = 1.0;
    zs.add_zone(std::move(zone));

    std::vector<s2::Agent> agents;
    std::vector<s2::Actor> actors;
    s2::SimBus bus;

    // dt = 1.0 сек → strength должен вырасти на 0.5 → стать 1.0
    zs.tick(agents, actors, bus, 1.0, 1.0);
    EXPECT_NEAR(zs.all_zones()[0].strength, 1.0, 1e-9);
}

// Рост ограничен max_strength
TEST(ZoneSystemLifecycle, StrengthGrowthCapped)
{
    s2::ZoneSystem zs;
    auto zone = make_sphere_zone("grow_zone", s2::Vec3(0, 0, 0), 5.0);
    zone.strength = 0.8;
    zone.lifecycle.growth_rate = 0.5;
    zone.lifecycle.max_strength = 1.0;
    zs.add_zone(std::move(zone));

    std::vector<s2::Agent> agents;
    std::vector<s2::Actor> actors;
    s2::SimBus bus;

    // dt = 1.0 → 0.8 + 0.5 = 1.3, но ограничено max_strength = 1.0
    zs.tick(agents, actors, bus, 1.0, 1.0);
    EXPECT_NEAR(zs.all_zones()[0].strength, 1.0, 1e-9);
}

// Затухание strength
TEST(ZoneSystemLifecycle, StrengthDecay)
{
    s2::ZoneSystem zs;
    auto zone = make_sphere_zone("decay_zone", s2::Vec3(0, 0, 0), 5.0);
    zone.strength = 1.0;
    zone.lifecycle.decay_rate = 0.5; // -0.5/сек
    zone.lifecycle.decay_delay = 0.0; // без задержки
    zone.spawn_time = 0.0;
    zs.add_zone(std::move(zone));

    std::vector<s2::Agent> agents;
    std::vector<s2::Actor> actors;
    s2::SimBus bus;

    // dt = 1.0 → strength = 1.0 - 0.5 = 0.5
    zs.tick(agents, actors, bus, 1.0, 1.0);
    EXPECT_NEAR(zs.all_zones()[0].strength, 0.5, 1e-9);
}

// Затухание с decay_delay: не затухает пока sim_time < spawn_time + decay_delay
TEST(ZoneSystemLifecycle, DecayDelayRespected)
{
    s2::ZoneSystem zs;
    auto zone = make_sphere_zone("decay_zone", s2::Vec3(0, 0, 0), 5.0);
    zone.strength = 1.0;
    zone.lifecycle.decay_rate = 0.5;
    zone.lifecycle.decay_delay = 5.0; // 5 секунд задержки
    zone.spawn_time = 0.0;
    zs.add_zone(std::move(zone));

    std::vector<s2::Agent> agents;
    std::vector<s2::Actor> actors;
    s2::SimBus bus;

    // sim_time=2.0, dt=1.0 → ещё в пределах delay → strength не меняется
    zs.tick(agents, actors, bus, 2.0, 1.0);
    EXPECT_NEAR(zs.all_zones()[0].strength, 1.0, 1e-9);

    // sim_time=6.0, dt=1.0 → delay прошёл → decay
    zs.tick(agents, actors, bus, 6.0, 1.0);
    EXPECT_NEAR(zs.all_zones()[0].strength, 0.5, 1e-9);
}

// Auto-remove: зона удаляется когда strength < remove_threshold
TEST(ZoneSystemLifecycle, AutoRemoveWhenBelowThreshold)
{
    s2::ZoneSystem zs;
    auto zone = make_sphere_zone("dying_zone", s2::Vec3(0, 0, 0), 5.0);
    zone.strength = 0.1;
    zone.lifecycle.decay_rate = 0.2;
    zone.lifecycle.decay_delay = 0.0;
    zone.lifecycle.remove_threshold = 0.05;
    zone.spawn_time = 0.0;
    zs.add_zone(std::move(zone));

    std::vector<s2::Agent> agents;
    std::vector<s2::Actor> actors;
    s2::SimBus bus;

    ASSERT_EQ(zs.all_zones().size(), 1u);

    // dt = 1.0 → strength = 0.1 - 0.2 = max(0, -0.1) = 0.0 < 0.05 → auto-remove
    zs.tick(agents, actors, bus, 1.0, 1.0);
    EXPECT_EQ(zs.all_zones().size(), 0u);
}

// ctx.zone_strength заполняется из zone.strength через мок-эффект
class StrengthRecorderEffect : public s2::EffectPlugin
{
public:
    mutable double recorded_strength = -1.0;

    void on_init(const YAML::Node&) override {}
    s2::EffectType effect_type() const override { return s2::EffectType::MODIFIER; }
    void apply_modifier(s2::SharedState&, const s2::EffectContext& ctx) override
    {
        recorded_strength = ctx.zone_strength;
    }
};

TEST(ZoneSystemLifecycle, EffectContextStrengthFilled)
{
    s2::ZoneSystem zs;
    auto zone = make_sphere_zone("str_zone", s2::Vec3(0, 0, 0), 5.0);
    zone.strength = 0.42;

    s2::Zone::EffectDesc desc;
    desc.type = "recorder";
    desc.enabled = true;
    desc.effect_type = s2::EffectType::MODIFIER;

    auto recorder = std::make_unique<StrengthRecorderEffect>();
    auto* raw = recorder.get();
    desc.plugin = std::move(recorder);
    zone.effects.push_back(std::move(desc));
    zs.add_zone(std::move(zone));

    std::vector<s2::Agent> agents;
    agents.push_back(make_agent(1, 0.0, 0.0, 0.0)); // внутри зоны
    std::vector<s2::Actor> actors;
    s2::SimBus bus;

    zs.tick(agents, actors, bus, 0.0, 0.01);
    EXPECT_NEAR(raw->recorded_strength, 0.42, 1e-9);
}

// remove_zone: удаляет зону и отправляет ZoneExited всем агентам внутри
TEST(ZoneSystemLifecycle, RemoveZoneSendsExitEvents)
{
    s2::ZoneSystem zs;
    zs.add_zone(make_sphere_zone("rm_zone", s2::Vec3(0, 0, 0), 5.0));

    std::vector<s2::Agent> agents;
    agents.push_back(make_agent(1, 0.0, 0.0, 0.0));
    std::vector<s2::Actor> actors;
    s2::SimBus bus;

    // Агент входит в зону
    zs.tick(agents, actors, bus, 0.0, 0.01);
    ASSERT_EQ(zs.all_zones().size(), 1u);
    ASSERT_EQ(zs.all_zones()[0].inside_agents.count(1u), 1u);

    // Подписываемся на exit-событие
    int exit_count = 0;
    bus.subscribe<s2::event::ZoneExited>([&](const s2::event::ZoneExited& e) {
        if (e.zone_id == "rm_zone") ++exit_count;
    });

    // Удаляем зону
    zs.remove_zone("rm_zone", agents, bus);

    EXPECT_EQ(zs.all_zones().size(), 0u);
    EXPECT_GE(exit_count, 1);
}

// toggle_zone_with_events: disable → ZoneExited, enable → ZoneEntered
TEST(ZoneSystemLifecycle, ToggleWithEvents)
{
    s2::ZoneSystem zs;
    zs.add_zone(make_sphere_zone("toggle_zone", s2::Vec3(0, 0, 0), 5.0));

    std::vector<s2::Agent> agents;
    agents.push_back(make_agent(1, 0.0, 0.0, 0.0));
    std::vector<s2::Actor> actors;
    s2::SimBus bus;

    // Агент входит в зону
    zs.tick(agents, actors, bus, 0.0, 0.01);
    ASSERT_EQ(zs.all_zones()[0].inside_agents.count(1u), 1u);

    // Выключаем зону с событиями
    int exit_count = 0;
    bus.subscribe<s2::event::ZoneExited>([&](const s2::event::ZoneExited& e) {
        if (e.zone_id == "toggle_zone") ++exit_count;
    });

    zs.toggle_zone_with_events("toggle_zone", false, agents, bus);
    EXPECT_GE(exit_count, 1);

    // Включаем зону с событиями — агент ещё внутри геометрии
    int enter_count = 0;
    bus.subscribe<s2::event::ZoneEntered>([&](const s2::event::ZoneEntered& e) {
        if (e.zone_id == "toggle_zone") ++enter_count;
    });

    zs.toggle_zone_with_events("toggle_zone", true, agents, bus);
    EXPECT_GE(enter_count, 1);
}
