#include <gtest/gtest.h>
#include <s2/sim_engine.hpp>
#include <s2/kernel_command.hpp>
#include <s2/zone_system.hpp>
#include <s2/sim_bus.hpp>
#include <s2/agent.hpp>

/**
 * @file test_zone_commands.cpp
 * Тесты обработчиков KernelCommands для зон: SpawnZone, DespawnZone, ToggleZone.
 *
 * Стратегия: используем ZoneSystem напрямую + apply_kernel_command через SimEngine,
 * т.к. SimEngine::step() дренирует command_queue_ и вызывает apply_kernel_command().
 */

namespace s2 {

// ── Вспомогательные функции ──────────────────────────────────────────────────

static Agent make_test_agent(AgentId id, double x, double y, double z = 0.0)
{
    Agent a;
    a.id = id;
    a.world_pose.x = x;
    a.world_pose.y = y;
    a.world_pose.z = z;
    return a;
}

static SimEngine make_test_engine()
{
    SimEngine::Config cfg;
    cfg.update_rate = 100.0;
    cfg.viz_rate = 0.0;       // Не пытаемся отправлять визуализацию
    cfg.transport_rate = 0.0;
    return SimEngine(cfg);
}

// ── Тест 1: SpawnZone создаёт зону с заданным id_hint ───────────────────────

TEST(ZoneCommands, SpawnZoneCreatesZone)
{
    auto engine = make_test_engine();
    SimWorld world;
    engine.load_world(std::move(world));

    cmd::SpawnZone spawn;
    spawn.shape.type   = ZoneShapeType::SPHERE;
    spawn.shape.center = Vec3{1.0, 2.0, 0.0};
    spawn.shape.radius = 3.0;
    spawn.id_hint      = "my_zone";
    spawn.visible      = true;
    spawn.color        = "#FF0000";
    spawn.opacity      = 0.5;
    spawn.label        = "Test Zone";
    spawn.effects      = {"ice", "fog"};

    engine.push_command(KernelCommand{spawn});
    engine.step(1);

    const auto& zones = engine.zone_system().all_zones();
    ASSERT_EQ(zones.size(), 1u);
    EXPECT_EQ(zones[0].id, "my_zone");
    EXPECT_EQ(zones[0].shape.type, ZoneShapeType::SPHERE);
    EXPECT_DOUBLE_EQ(zones[0].shape.radius, 3.0);
    EXPECT_EQ(zones[0].color, "#FF0000");
    EXPECT_DOUBLE_EQ(zones[0].opacity, 0.5);
    EXPECT_EQ(zones[0].label, "Test Zone");
    EXPECT_EQ(zones[0].visible, true);
    // Два EffectDesc с правильными типами (плагины без фабрики = nullptr)
    ASSERT_EQ(zones[0].effects.size(), 2u);
    EXPECT_EQ(zones[0].effects[0].type, "ice");
    EXPECT_EQ(zones[0].effects[1].type, "fog");
}

// ── Тест 2: SpawnZone с пустым id_hint → auto-generated id ──────────────────

TEST(ZoneCommands, SpawnZoneAutoId)
{
    auto engine = make_test_engine();
    SimWorld world;
    engine.load_world(std::move(world));

    cmd::SpawnZone spawn;
    spawn.shape.type   = ZoneShapeType::SPHERE;
    spawn.shape.center = Vec3{0.0, 0.0, 0.0};
    spawn.shape.radius = 1.0;
    // id_hint не задан (пустая строка)

    engine.push_command(KernelCommand{spawn});
    engine.step(1);

    const auto& zones = engine.zone_system().all_zones();
    ASSERT_EQ(zones.size(), 1u);
    // Автосгенерированный id начинается с "zone_"
    EXPECT_TRUE(zones[0].id.find("zone_") == 0)
        << "Auto-generated id должен начинаться с 'zone_', получен: " << zones[0].id;
}

// ── Тест 3: SpawnZone с attached_to заполняет attached_to_entity_id ─────────

TEST(ZoneCommands, SpawnZoneAttachedTo)
{
    auto engine = make_test_engine();
    SimWorld world;
    engine.load_world(std::move(world));

    cmd::SpawnZone spawn;
    spawn.shape.type   = ZoneShapeType::SPHERE;
    spawn.shape.center = Vec3{0.0, 0.0, 0.0};
    spawn.shape.radius = 1.0;
    spawn.id_hint      = "attached_zone";
    spawn.attached_to  = EntityId{42};

    engine.push_command(KernelCommand{spawn});
    engine.step(1);

    const auto& zones = engine.zone_system().all_zones();
    ASSERT_EQ(zones.size(), 1u);
    // attached_to_entity_id должен быть заполнен строковым представлением EntityId
    EXPECT_EQ(zones[0].attached_to_entity_id, "42");
}

// ── Тест 4: DespawnZone удаляет зону ─────────────────────────────────────────

TEST(ZoneCommands, DespawnZoneRemovesZone)
{
    auto engine = make_test_engine();
    SimWorld world;
    engine.load_world(std::move(world));

    // Сначала создаём зону
    cmd::SpawnZone spawn;
    spawn.shape.type   = ZoneShapeType::SPHERE;
    spawn.shape.center = Vec3{0.0, 0.0, 0.0};
    spawn.shape.radius = 5.0;
    spawn.id_hint      = "to_remove";
    engine.push_command(KernelCommand{spawn});
    engine.step(1);
    ASSERT_EQ(engine.zone_system().all_zones().size(), 1u);

    // Удаляем зону
    cmd::DespawnZone despawn;
    despawn.id = "to_remove";
    engine.push_command(KernelCommand{despawn});
    engine.step(1);

    EXPECT_EQ(engine.zone_system().all_zones().size(), 0u);
}

// ── Тест 5: ToggleZone false → зона выключается ─────────────────────────────

TEST(ZoneCommands, ToggleZoneDisables)
{
    auto engine = make_test_engine();
    SimWorld world;
    engine.load_world(std::move(world));

    // Создаём зону (enabled по умолчанию)
    cmd::SpawnZone spawn;
    spawn.shape.type   = ZoneShapeType::SPHERE;
    spawn.shape.center = Vec3{0.0, 0.0, 0.0};
    spawn.shape.radius = 5.0;
    spawn.id_hint      = "toggle_zone";
    engine.push_command(KernelCommand{spawn});
    engine.step(1);
    ASSERT_TRUE(engine.zone_system().all_zones()[0].enabled);

    // Выключаем зону
    cmd::ToggleZone toggle;
    toggle.id = "toggle_zone";
    toggle.enabled = false;
    engine.push_command(KernelCommand{toggle});
    engine.step(1);

    EXPECT_FALSE(engine.zone_system().all_zones()[0].enabled);
}

// ── Тест 6: ToggleZone false отправляет ZoneExited для агентов внутри ────────

TEST(ZoneCommands, ToggleZoneSendsExitEvents)
{
    auto engine = make_test_engine();
    SimWorld world;
    // Агент в центре будущей зоны
    world.agents().push_back(make_test_agent(1, 0.0, 0.0, 0.0));
    engine.load_world(std::move(world));

    // Создаём большую зону с агентом внутри
    cmd::SpawnZone spawn;
    spawn.shape.type   = ZoneShapeType::SPHERE;
    spawn.shape.center = Vec3{0.0, 0.0, 0.0};
    spawn.shape.radius = 10.0;
    spawn.id_hint      = "exit_zone";
    engine.push_command(KernelCommand{spawn});
    engine.step(1);  // Зона создана и тик включил агента в inside_agents

    // Подписываемся на ZoneExited
    ZoneId exited_zone;
    EntityId exited_entity = 0;
    engine.bus().subscribe<event::ZoneExited>([&](const event::ZoneExited& e) {
        exited_zone   = e.zone_id;
        exited_entity = e.entity_id;
    });

    // Выключаем зону — ожидаем ZoneExited
    cmd::ToggleZone toggle;
    toggle.id = "exit_zone";
    toggle.enabled = false;
    engine.push_command(KernelCommand{toggle});
    engine.step(1);

    EXPECT_EQ(exited_zone, "exit_zone");
    EXPECT_EQ(exited_entity, 1u);
}

// ── Тест 7: SpawnZone с неизвестным эффектом не крэшится ─────────────────────

TEST(ZoneCommands, SpawnZoneUnknownEffectNoCrash)
{
    auto engine = make_test_engine();
    SimWorld world;
    engine.load_world(std::move(world));

    cmd::SpawnZone spawn;
    spawn.shape.type   = ZoneShapeType::SPHERE;
    spawn.shape.center = Vec3{0.0, 0.0, 0.0};
    spawn.shape.radius = 1.0;
    spawn.id_hint      = "unknown_eff";
    spawn.effects      = {"nonexistent_effect_xyz"};

    // Не должно крэшиться — EffectDesc создаётся с type, plugin остаётся nullptr
    EXPECT_NO_THROW({
        engine.push_command(KernelCommand{spawn});
        engine.step(1);
    });

    const auto& zones = engine.zone_system().all_zones();
    ASSERT_EQ(zones.size(), 1u);
    ASSERT_EQ(zones[0].effects.size(), 1u);
    EXPECT_EQ(zones[0].effects[0].type, "nonexistent_effect_xyz");
    EXPECT_EQ(zones[0].effects[0].plugin, nullptr);
}

} // namespace s2
