#include <gtest/gtest.h>

#include <s2/plugins/battery.hpp>
#include <s2/components/battery_component.hpp>
#include <s2/agent.hpp>
#include <s2/sim_engine.hpp>
#include <s2/world.hpp>
#include <s2/zone_system.hpp>
#include <s2/effects/charging_effect.hpp>
#include <s2/effects_registry.hpp>
#include <s2/world_query.hpp>
#include <s2/event_bus.hpp>
#include <s2/plugin_base.hpp>
#include <nlohmann/json.hpp>

// Null-контекст для тестов плагинов
static s2::WorldQuery         g_null_world;
static s2::EventBus           g_null_bus;
static s2::KernelCommandQueue g_null_cmds;
static s2::PluginContext      g_ctx{g_null_world, g_null_bus, g_null_cmds};

namespace s2 {
namespace plugins {

// ─── Вспомогательная функция ─────────────────────────────────────────────────

static Agent make_agent(AgentId id = 1)
{
    Agent a;
    a.id   = id;
    a.name = "robot_" + std::to_string(id);
    a.world_pose.x = 0.0;
    a.world_pose.y = 0.0;
    a.world_pose.z = 0.0;
    return a;
}

// ─── Тест 1: initialize() создаёт BatteryComponent с initial_level ───────────

TEST(BatteryPlugin, InitializeCreatesComponent)
{
    BatteryPlugin plugin;
    YAML::Node cfg;
    cfg["initial_level"] = 0.6;
    plugin.from_config(cfg);

    Agent agent = make_agent();
    plugin.initialize(agent);

    const auto* bat = agent.state.get<BatteryComponent>();
    ASSERT_NE(bat, nullptr);
    EXPECT_NEAR(bat->level, 0.6, 1e-9);
    EXPECT_FALSE(bat->charging);
}

// ─── Тест 2: initialize() не перезаписывает уже существующий компонент ────────

TEST(BatteryPlugin, InitializePreservesExistingComponent)
{
    BatteryPlugin plugin;
    YAML::Node cfg;
    cfg["initial_level"] = 0.9;
    plugin.from_config(cfg);

    Agent agent = make_agent();
    // Кто-то уже установил компонент (например ChargingEffect)
    agent.state.emplace<BatteryComponent>(BatteryComponent{0.3, true});

    plugin.initialize(agent);

    const auto* bat = agent.state.get<BatteryComponent>();
    ASSERT_NE(bat, nullptr);
    // Существующий уровень (0.3) сохранён, initial_level (0.9) проигнорирован
    EXPECT_NEAR(bat->level, 0.3, 1e-9);
}

// ─── Тест 3: update() пишет BatteryData в SharedState ────────────────────────

TEST(BatteryPlugin, UpdateWritesBatteryData)
{
    BatteryPlugin plugin;
    YAML::Node cfg;
    cfg["initial_level"]   = 0.75;
    cfg["nominal_voltage"] = 24.0;
    cfg["capacity_ah"]     = 10.0;
    cfg["publish_rate_hz"] = 0.0;  // публиковать каждый тик
    plugin.from_config(cfg);

    Agent agent = make_agent();
    plugin.initialize(agent);
    plugin.update(0.01, agent, g_ctx);

    const auto* data = agent.state.get<BatteryData>();
    ASSERT_NE(data, nullptr);
    EXPECT_NEAR(data->level, 0.75, 1e-9);
    EXPECT_FALSE(data->charging);
    EXPECT_NEAR(data->nominal_voltage, 24.0, 1e-9);
    EXPECT_NEAR(data->capacity_ah, 10.0, 1e-9);
    EXPECT_GT(data->seq, 0u);
}

// ─── Тест 4: update() уважает publish_rate_hz (не пишет каждый тик) ─────────

TEST(BatteryPlugin, PublishRateThrottles)
{
    BatteryPlugin plugin;
    YAML::Node cfg;
    cfg["initial_level"]   = 1.0;
    cfg["publish_rate_hz"] = 1.0;  // раз в секунду
    plugin.from_config(cfg);

    Agent agent = make_agent();
    plugin.initialize(agent);

    // dt = 0.01 с, 50 тиков = 0.5 с — ещё не должно быть публикации
    for (int i = 0; i < 50; ++i)
        plugin.update(0.01, agent, g_ctx);

    const auto* data = agent.state.get<BatteryData>();
    EXPECT_EQ(data, nullptr);  // данных ещё нет

    // Ещё 60 тиков — итого 1.1 с — должна быть публикация
    for (int i = 0; i < 60; ++i)
        plugin.update(0.01, agent, g_ctx);

    data = agent.state.get<BatteryData>();
    ASSERT_NE(data, nullptr);
    EXPECT_GT(data->seq, 0u);
}

// ─── Тест 5: contribute_snapshot() добавляет battery_level и battery_charging ─

TEST(BatteryPlugin, ContributeSnapshot)
{
    BatteryPlugin plugin;
    YAML::Node cfg;
    cfg["initial_level"] = 0.8;
    plugin.from_config(cfg);

    Agent agent = make_agent();
    plugin.initialize(agent);

    nlohmann::json extra = nlohmann::json::object();
    plugin.contribute_snapshot(extra, agent);

    EXPECT_TRUE(extra.contains("battery_level"));
    EXPECT_TRUE(extra.contains("battery_charging"));
    EXPECT_NEAR(extra["battery_level"].get<double>(), 0.8, 1e-9);
    EXPECT_FALSE(extra["battery_charging"].get<bool>());
}

// ─── Тест 6: contribute_snapshot() без компонента → -1.0 ─────────────────────

TEST(BatteryPlugin, ContributeSnapshotNoComponent)
{
    BatteryPlugin plugin;
    Agent agent = make_agent();
    // Не инициализируем — компонента нет

    nlohmann::json extra = nlohmann::json::object();
    plugin.contribute_snapshot(extra, agent);

    EXPECT_NEAR(extra["battery_level"].get<double>(), -1.0, 1e-9);
    EXPECT_FALSE(extra["battery_charging"].get<bool>());
}

// ─── Тест 7: интеграция — BatteryPlugin + ChargingEffect через SimEngine ──────

TEST(BatteryPlugin, IntegrationWithChargingEffect)
{
    SimEngine engine{{.update_rate = 100.0, .viz_rate = 30.0}};
    engine.set_effect_factory(s2::create_effect);

    SimWorld world;

    Agent agent;
    agent.id   = 1;
    agent.name = "robot_0";
    agent.capabilities.insert("has_battery");

    // BatteryPlugin — инициализирует компонент с начальным уровнем 0.5
    auto bplugin = std::make_unique<BatteryPlugin>();
    YAML::Node bcfg;
    bcfg["initial_level"]   = 0.5;
    bcfg["publish_rate_hz"] = 0.0;
    bplugin->from_config(bcfg);
    agent.plugins.push_back(std::move(bplugin));

    world.add_agent(std::move(agent));

    // Зона зарядки
    Zone z;
    z.id = "charger";
    z.enabled = true;
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

    // 10 тиков по 0.01 с = 0.1 с → уровень должен вырасти на 0.1 * 0.1 = 0.01
    engine.step(10);

    const Agent* a = engine.world().get_agent(1);
    ASSERT_NE(a, nullptr);

    // Проверяем BatteryComponent (читает ChargingEffect + BatteryPlugin инициализировал)
    const auto* bat = a->state.get<BatteryComponent>();
    ASSERT_NE(bat, nullptr);
    EXPECT_GT(bat->level, 0.5);
    EXPECT_TRUE(bat->charging);

    // Проверяем AgentSnapshot.extra через build_snapshot
    auto snap = engine.build_snapshot();
    ASSERT_EQ(snap.agents.size(), 1u);
    const auto& extra = snap.agents[0].extra;
    EXPECT_TRUE(extra.contains("battery_level"));
    EXPECT_TRUE(extra.contains("battery_charging"));
    EXPECT_GT(extra["battery_level"].get<double>(), 0.5);
    EXPECT_TRUE(extra["battery_charging"].get<bool>());
}

// ─── Тест 8: from_config() парсит technology строкой ─────────────────────────

TEST(BatteryPlugin, FromConfigTechnology)
{
    BatteryPlugin plugin;
    YAML::Node cfg;
    cfg["technology"] = "lipo";
    plugin.from_config(cfg);

    Agent agent = make_agent();
    plugin.initialize(agent);
    plugin.update(0.01, agent, g_ctx);  // publish_rate_hz по умолчанию = 1.0, но проверим to_json

    // Проверяем через BatteryData в SharedState после достаточного времени
    YAML::Node cfg2;
    cfg2["technology"]     = "lipo";
    cfg2["publish_rate_hz"] = 0.0;
    BatteryPlugin plugin2;
    plugin2.from_config(cfg2);
    plugin2.initialize(agent);
    plugin2.update(0.01, agent, g_ctx);

    const auto* data = agent.state.get<BatteryData>();
    ASSERT_NE(data, nullptr);
    EXPECT_EQ(data->technology, 3u);  // POWER_SUPPLY_TECHNOLOGY_LIPO = 3
}

// ─── Тест 9: pre_resolve() разряжает батарею на 1%/с ────────────────────────

TEST(BatteryPlugin, DrainReducesLevel)
{
    BatteryPlugin plugin;
    YAML::Node cfg;
    cfg["initial_level"]   = 1.0;
    cfg["drain_rate"]      = 0.01;  // 1%/с
    cfg["publish_rate_hz"] = 0.0;
    plugin.from_config(cfg);

    Agent agent = make_agent();
    plugin.initialize(agent);

    // 100 тиков по 0.01 с = 1.0 с → уровень должен упасть на 0.01
    for (int i = 0; i < 100; ++i) {
        plugin.pre_resolve(0.01, agent);
        agent.state.resolve();
        plugin.update(0.01, agent, g_ctx);
        agent.state.clear_contributions();
    }

    const auto* bat = agent.state.get<BatteryComponent>();
    ASSERT_NE(bat, nullptr);
    EXPECT_NEAR(bat->level, 0.99, 1e-6);
}

// ─── Тест 10: pre_resolve() не разряжает при charging = true ─────────────────

TEST(BatteryPlugin, NoDrainWhenCharging)
{
    BatteryPlugin plugin;
    YAML::Node cfg;
    cfg["initial_level"]   = 0.5;
    cfg["drain_rate"]      = 0.01;
    cfg["publish_rate_hz"] = 0.0;
    plugin.from_config(cfg);

    Agent agent = make_agent();
    plugin.initialize(agent);

    // Выставим charging = true
    auto* bat = agent.state.get<BatteryComponent>();
    bat->charging = true;

    for (int i = 0; i < 100; ++i) {
        plugin.pre_resolve(0.01, agent);
        agent.state.resolve();
        plugin.update(0.01, agent, g_ctx);
        agent.state.clear_contributions();
    }

    bat = agent.state.get<BatteryComponent>();
    ASSERT_NE(bat, nullptr);
    EXPECT_NEAR(bat->level, 0.5, 1e-9);  // уровень не изменился
}

// ─── Тест 11: при уровне < 20% добавляется scale contribution ────────────────

TEST(BatteryPlugin, LowBatteryAddsScaleContribution)
{
    BatteryPlugin plugin;
    YAML::Node cfg;
    cfg["initial_level"]   = 0.10;  // 10% — в диапазоне замедления
    cfg["publish_rate_hz"] = 0.0;
    plugin.from_config(cfg);

    Agent agent = make_agent();
    plugin.initialize(agent);

    plugin.pre_resolve(0.001, agent);  // маленький dt — заряд почти не меняется

    // Проверяем что вклад есть (resolve ещё не вызван, но contributions уже добавлены)
    EXPECT_EQ(agent.state.scale_contrib_count(), 1u);
    EXPECT_EQ(agent.state.lock_contrib_count(),  0u);

    agent.state.resolve();
    const auto& eff = agent.state.effective();
    // scale = (0.10 - 0.05) / 0.15 ≈ 0.333
    EXPECT_NEAR(eff.speed_scale, (0.10 - 0.05) / 0.15, 0.01);
    EXPECT_FALSE(eff.motion_locked);
}

// ─── Тест 12: при уровне <= 5% движение заблокировано ────────────────────────

TEST(BatteryPlugin, CriticalBatteryLocksMotion)
{
    BatteryPlugin plugin;
    YAML::Node cfg;
    cfg["initial_level"]   = 0.03;  // 3% — критический уровень
    cfg["publish_rate_hz"] = 0.0;
    plugin.from_config(cfg);

    Agent agent = make_agent();
    plugin.initialize(agent);

    plugin.pre_resolve(0.001, agent);
    agent.state.resolve();

    const auto& eff = agent.state.effective();
    EXPECT_TRUE(eff.motion_locked);
    EXPECT_EQ(agent.state.lock_contrib_count(), 1u);
}

// ─── Тест 13: при полном заряде нет contributions ────────────────────────────

TEST(BatteryPlugin, FullBatteryNoContribution)
{
    BatteryPlugin plugin;
    YAML::Node cfg;
    cfg["initial_level"]   = 1.0;
    cfg["publish_rate_hz"] = 0.0;
    plugin.from_config(cfg);

    Agent agent = make_agent();
    plugin.initialize(agent);

    plugin.pre_resolve(0.001, agent);

    EXPECT_EQ(agent.state.scale_contrib_count(), 0u);
    EXPECT_EQ(agent.state.lock_contrib_count(),  0u);

    agent.state.resolve();
    EXPECT_NEAR(agent.state.effective().speed_scale, 1.0, 1e-9);
    EXPECT_FALSE(agent.state.effective().motion_locked);
}

// ─── Тест 14: граница 20% — нет contribution (первый тик без разряда) ─────────

TEST(BatteryPlugin, BoundaryAt20PercentNoContribution)
{
    BatteryPlugin plugin;
    YAML::Node cfg;
    cfg["initial_level"]   = 0.20;
    cfg["drain_rate"]      = 0.0;  // нет разряда — проверяем именно границу
    cfg["publish_rate_hz"] = 0.0;
    plugin.from_config(cfg);

    Agent agent = make_agent();
    plugin.initialize(agent);

    plugin.pre_resolve(0.01, agent);

    EXPECT_EQ(agent.state.scale_contrib_count(), 0u);
    EXPECT_EQ(agent.state.lock_contrib_count(),  0u);
}

// ─── Тест 15: граница 5% — lock contribution ─────────────────────────────────

TEST(BatteryPlugin, BoundaryAt5PercentLocks)
{
    BatteryPlugin plugin;
    YAML::Node cfg;
    cfg["initial_level"]   = 0.05;
    cfg["drain_rate"]      = 0.0;
    cfg["publish_rate_hz"] = 0.0;
    plugin.from_config(cfg);

    Agent agent = make_agent();
    plugin.initialize(agent);

    plugin.pre_resolve(0.01, agent);
    agent.state.resolve();

    EXPECT_TRUE(agent.state.effective().motion_locked);
}

// ─── Тест 16: заряд снимает блокировку при выходе из critical ────────────────

TEST(BatteryPlugin, ChargingAboveCriticalUnlocks)
{
    BatteryPlugin plugin;
    YAML::Node cfg;
    cfg["initial_level"]   = 0.03;  // ниже critical — заблокировано
    cfg["drain_rate"]      = 0.0;
    cfg["publish_rate_hz"] = 0.0;
    plugin.from_config(cfg);

    Agent agent = make_agent();
    plugin.initialize(agent);

    // Первый тик — критический уровень, motion locked
    plugin.pre_resolve(0.01, agent);
    agent.state.resolve();
    EXPECT_TRUE(agent.state.effective().motion_locked);
    agent.state.clear_contributions();

    // Зарядка подняла уровень выше critical
    auto* bat = agent.state.get<BatteryComponent>();
    bat->level    = 0.10;
    bat->charging = true;

    plugin.pre_resolve(0.01, agent);
    agent.state.resolve();
    // Теперь 10% — замедление, но не lock
    EXPECT_FALSE(agent.state.effective().motion_locked);
    EXPECT_LT(agent.state.effective().speed_scale, 1.0);
}

} // namespace plugins
} // namespace s2
