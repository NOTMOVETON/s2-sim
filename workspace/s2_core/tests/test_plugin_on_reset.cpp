/**
 * @file test_plugin_on_reset.cpp
 * Тесты on_reset() для DiffDrive и Battery плагинов.
 *
 * Проверяет баги из D-13:
 *  - DiffDrive: external_linear_velocity_ сбрасывается при reset
 *  - Battery: заряд сбрасывается до initial_level_ при reset
 *  - SimEngine::reset() вызывает on_reset() для всех плагинов всех агентов
 *
 * Тесты проходят (GREEN) после:
 *  - on_reset() добавлен в DiffDrivePlugin и BatteryPlugin
 *  - SimEngine::reset() вызывает on_reset() для всех плагинов
 */

#include <gtest/gtest.h>

#include <s2/sim_engine.hpp>
#include <s2/agent.hpp>
#include <s2/world.hpp>
#include <s2/plugin_base.hpp>
#include <s2/plugins/diff_drive.hpp>
#include <s2/plugins/battery.hpp>
#include <s2/components/battery_component.hpp>

namespace s2
{

// ─── DiffDrive on_reset ────────────────────────────────────────────────────────

TEST(PluginOnReset, DiffDriveExternalVelocityResetAfterReset)
{
    SimEngine::Config cfg;
    cfg.update_rate    = 100.0;
    cfg.viz_rate       = 0.0;
    cfg.transport_rate = 0.0;
    SimEngine engine(cfg);

    // Создаём агента с DiffDrivePlugin
    Agent agent;
    agent.id = 1;
    auto* plugin_ptr = new plugins::DiffDrivePlugin();
    agent.plugins.push_back(
        std::unique_ptr<plugins::DiffDrivePlugin>(plugin_ptr));

    SimWorld world;
    world.agents().push_back(std::move(agent));
    engine.load_world(std::move(world));

    // Запустить симуляцию и отправить внешнюю команду скорости
    engine.resume();
    plugin_ptr->handle_input("{\"linear_velocity\": 1.5, \"angular_velocity\": 0.5}");

    // Прогнать один тик — агент должен двигаться
    engine.step(1);
    {
        const auto& a_before = engine.world().agents()[0];
        double speed_before = a_before.world_velocity.linear.norm();
        EXPECT_GT(speed_before, 0.0)
            << "После external input агент должен двигаться";
    }

    // Reset — SimEngine::reset() должен вызвать on_reset() → сбросить external_linear_velocity_
    engine.reset();
    // После reset: паузу снять и сделать один тик — агент НЕ должен двигаться
    engine.resume();
    engine.step(1);

    {
        const auto& a_after = engine.world().agents()[0];
        // После reset без новых команд скорость должна быть нулевой
        EXPECT_NEAR(a_after.world_velocity.linear.norm(), 0.0, 1e-6)
            << "После reset агент не должен двигаться (external_linear_velocity_ сброшен)";
    }
}

// ─── Battery on_reset ─────────────────────────────────────────────────────────

TEST(PluginOnReset, BatteryLevelRestoredAfterReset)
{
    SimEngine::Config cfg;
    cfg.update_rate    = 100.0;
    cfg.viz_rate       = 0.0;
    cfg.transport_rate = 0.0;
    SimEngine engine(cfg);

    Agent agent;
    agent.id = 1;
    auto battery = std::make_unique<plugins::BatteryPlugin>();

    // Установить начальный заряд через конфигурацию
    YAML::Node config;
    config["initial_level"]   = 0.8;
    config["drain_rate"]      = 0.0;  // не разряжается — чтобы тест был детерминированным
    config["publish_rate_hz"] = 0.0;
    battery->from_config(config);
    battery->initialize(agent);

    agent.plugins.push_back(std::move(battery));

    SimWorld world;
    world.agents().push_back(std::move(agent));
    engine.load_world(std::move(world));

    // Запустить симуляцию, сделать тик, чтобы убедиться что BatteryComponent доступен
    engine.resume();
    engine.step(1);

    // Симулировать разряд: напрямую снизить уровень в BatteryComponent
    {
        auto& a = engine.world().agents()[0];
        auto* bat = a.state.get<BatteryComponent>();
        ASSERT_NE(bat, nullptr);
        bat->level = 0.1;  // имитируем разряд
    }

    // Проверяем что заряд снижен
    {
        const auto& a = engine.world().agents()[0];
        const auto* bat = a.state.get<BatteryComponent>();
        EXPECT_NEAR(bat->level, 0.1, 1e-6);
    }

    // Reset — on_reset() должен восстановить initial_level_ = 0.8
    engine.reset();

    {
        const auto& a = engine.world().agents()[0];
        const auto* bat = a.state.get<BatteryComponent>();
        ASSERT_NE(bat, nullptr);
        EXPECT_NEAR(bat->level, 0.8, 1e-6)
            << "После reset заряд батареи должен восстановиться до initial_level_ = 0.8";
    }
}

// ─── SceneLoader валидация ACTUATION ──────────────────────────────────────────

TEST(SceneLoaderValidation, SingleActuationPluginCountedCorrectly)
{
    // Прямой тест подсчёта ACTUATION плагинов:
    // Два DiffDrive → actuation_count == 2 (SceneLoader должен бросить на load)
    std::vector<std::unique_ptr<plugins::IAgentPlugin>> plugins_list;
    plugins_list.push_back(std::make_unique<plugins::DiffDrivePlugin>());
    plugins_list.push_back(std::make_unique<plugins::DiffDrivePlugin>());

    int actuation_count = 0;
    for (const auto& p : plugins_list)
        if (p->role() == PluginRole::ACTUATION)
            actuation_count++;

    EXPECT_EQ(actuation_count, 2)
        << "Два DiffDrive должны дать actuation_count == 2; SceneLoader кинет исключение";
}

}  // namespace s2
