#include <gtest/gtest.h>
#include <s2/zone_spawn_system.hpp>
#include <s2/sim_bus.hpp>
#include <s2/kernel_command.hpp>

/**
 * @file test_zone_spawn_triggers.cpp
 * Тесты ZoneSpawnSystem — декларативный спавн зон по триггерам.
 *
 * Тестируем 3 типа триггеров: timer, event, state_change.
 * Тип command — уже покрыт test_zone_commands.cpp (SpawnZone KernelCommand).
 */

namespace s2 {

// ── Тест 1: timer trigger срабатывает через N секунд ─────────────────────────

TEST(ZoneSpawnTriggers, TimerFires)
{
    KernelCommandQueue queue;
    SimBus bus;
    ZoneSpawnSystem zss;
    zss.init(bus, queue, 0.0);

    ZoneSpawnSystem::ZoneTemplate tmpl;
    tmpl.name = "test_timer";
    tmpl.trigger_type = ZoneSpawnSystem::ZoneTemplate::TriggerType::TIMER;
    tmpl.timer.delay_seconds = 5.0;
    tmpl.spawn_cmd.id_hint = "timer_zone";
    tmpl.spawn_cmd.shape.type = ZoneShapeType::SPHERE;
    tmpl.spawn_cmd.shape.radius = 1.0;
    zss.add_template(std::move(tmpl), 0.0);

    // Ещё рано — не сработал
    zss.tick(3.0);
    EXPECT_EQ(queue.size(), 0u);

    // Время подошло — сработал
    zss.tick(5.5);
    EXPECT_EQ(queue.size(), 1u);

    // Повторно не срабатывает (fired=true)
    zss.tick(6.0);
    EXPECT_EQ(queue.size(), 1u);
}

// ── Тест 2: timer trigger с ненулевым start time ─────────────────────────────

TEST(ZoneSpawnTriggers, TimerFiresWithOffset)
{
    KernelCommandQueue queue;
    SimBus bus;
    ZoneSpawnSystem zss;
    zss.init(bus, queue, 10.0);  // Начинаем с sim_time=10

    ZoneSpawnSystem::ZoneTemplate tmpl;
    tmpl.name = "offset_timer";
    tmpl.trigger_type = ZoneSpawnSystem::ZoneTemplate::TriggerType::TIMER;
    tmpl.timer.delay_seconds = 3.0;
    tmpl.spawn_cmd.id_hint = "offset_zone";
    tmpl.spawn_cmd.shape.type = ZoneShapeType::SPHERE;
    tmpl.spawn_cmd.shape.radius = 1.0;
    zss.add_template(std::move(tmpl), 10.0);

    // sim_time=12 — ещё рано (10 + 3 = 13)
    zss.tick(12.0);
    EXPECT_EQ(queue.size(), 0u);

    // sim_time=13.5 — сработал
    zss.tick(13.5);
    EXPECT_EQ(queue.size(), 1u);
}

// ── Тест 3: event trigger срабатывает при ZoneEntered ────────────────────────

TEST(ZoneSpawnTriggers, EventFiresOnZoneEntered)
{
    KernelCommandQueue queue;
    SimBus bus;
    ZoneSpawnSystem zss;
    zss.init(bus, queue, 0.0);

    ZoneSpawnSystem::ZoneTemplate tmpl;
    tmpl.name = "test_event";
    tmpl.trigger_type = ZoneSpawnSystem::ZoneTemplate::TriggerType::EVENT;
    tmpl.event.event_type = "ZoneEntered";
    tmpl.event.source_entity = "";  // любой
    tmpl.spawn_cmd.id_hint = "event_zone";
    tmpl.spawn_cmd.shape.type = ZoneShapeType::SPHERE;
    tmpl.spawn_cmd.shape.radius = 2.0;
    zss.add_template(std::move(tmpl), 0.0);

    // Публикуем ZoneEntered событие — триггер должен сработать
    bus.publish(event::ZoneEntered{.zone_id = "z1", .entity_id = 1});

    EXPECT_EQ(queue.size(), 1u);
}

// ── Тест 4: event trigger с фильтром по source_entity ────────────────────────

TEST(ZoneSpawnTriggers, EventFiltersBySourceEntity)
{
    KernelCommandQueue queue;
    SimBus bus;
    ZoneSpawnSystem zss;
    zss.init(bus, queue, 0.0);

    ZoneSpawnSystem::ZoneTemplate tmpl;
    tmpl.name = "filtered_event";
    tmpl.trigger_type = ZoneSpawnSystem::ZoneTemplate::TriggerType::EVENT;
    tmpl.event.event_type = "ZoneEntered";
    tmpl.event.source_entity = "42";  // только entity 42
    tmpl.spawn_cmd.id_hint = "filtered_zone";
    tmpl.spawn_cmd.shape.type = ZoneShapeType::SPHERE;
    tmpl.spawn_cmd.shape.radius = 1.0;
    zss.add_template(std::move(tmpl), 0.0);

    // Другой entity — не триггерит
    bus.publish(event::ZoneEntered{.zone_id = "z1", .entity_id = 99});
    EXPECT_EQ(queue.size(), 0u);

    // Правильный entity — триггерит
    bus.publish(event::ZoneEntered{.zone_id = "z1", .entity_id = 42});
    EXPECT_EQ(queue.size(), 1u);
}

// ── Тест 5: event trigger на ZoneExited ──────────────────────────────────────

TEST(ZoneSpawnTriggers, EventFiresOnZoneExited)
{
    KernelCommandQueue queue;
    SimBus bus;
    ZoneSpawnSystem zss;
    zss.init(bus, queue, 0.0);

    ZoneSpawnSystem::ZoneTemplate tmpl;
    tmpl.name = "exit_event";
    tmpl.trigger_type = ZoneSpawnSystem::ZoneTemplate::TriggerType::EVENT;
    tmpl.event.event_type = "ZoneExited";
    tmpl.event.source_entity = "";
    tmpl.spawn_cmd.id_hint = "exit_zone";
    tmpl.spawn_cmd.shape.type = ZoneShapeType::SPHERE;
    tmpl.spawn_cmd.shape.radius = 1.0;
    zss.add_template(std::move(tmpl), 0.0);

    bus.publish(event::ZoneExited{.zone_id = "z1", .entity_id = 5});
    EXPECT_EQ(queue.size(), 1u);
}

// ── Тест 6: state_change trigger срабатывает при ActorStateChanged ───────────
// Pitfall 6 (RESEARCH.md): тестируем через ручной bus.publish()

TEST(ZoneSpawnTriggers, StateChangeFires)
{
    KernelCommandQueue queue;
    SimBus bus;
    ZoneSpawnSystem zss;
    zss.init(bus, queue, 0.0);

    ZoneSpawnSystem::ZoneTemplate tmpl;
    tmpl.name = "test_state";
    tmpl.trigger_type = ZoneSpawnSystem::ZoneTemplate::TriggerType::STATE_CHANGE;
    tmpl.state_change.actor_id = 100;
    tmpl.state_change.target_state = "OPEN";
    tmpl.spawn_cmd.id_hint = "state_zone";
    tmpl.spawn_cmd.shape.type = ZoneShapeType::SPHERE;
    tmpl.spawn_cmd.shape.radius = 1.0;
    zss.add_template(std::move(tmpl), 0.0);

    // Другой актор — не триггерит
    bus.publish(event::ActorStateChanged{.actor = 200, .old_state = "CLOSED", .new_state = "OPEN"});
    EXPECT_EQ(queue.size(), 0u);

    // Правильный актор но не то состояние — не триггерит
    bus.publish(event::ActorStateChanged{.actor = 100, .old_state = "OPEN", .new_state = "CLOSED"});
    EXPECT_EQ(queue.size(), 0u);

    // Правильный актор + правильное новое состояние — триггерит
    bus.publish(event::ActorStateChanged{.actor = 100, .old_state = "CLOSED", .new_state = "OPEN"});
    EXPECT_EQ(queue.size(), 1u);
}

// ── Тест 7: clear() очищает все шаблоны ──────────────────────────────────────

TEST(ZoneSpawnTriggers, ClearTemplates)
{
    KernelCommandQueue queue;
    SimBus bus;
    ZoneSpawnSystem zss;
    zss.init(bus, queue, 0.0);

    ZoneSpawnSystem::ZoneTemplate tmpl;
    tmpl.name = "timer_to_clear";
    tmpl.trigger_type = ZoneSpawnSystem::ZoneTemplate::TriggerType::TIMER;
    tmpl.timer.delay_seconds = 1.0;
    tmpl.spawn_cmd.id_hint = "cleared_zone";
    tmpl.spawn_cmd.shape.type = ZoneShapeType::SPHERE;
    tmpl.spawn_cmd.shape.radius = 1.0;
    zss.add_template(std::move(tmpl), 0.0);

    zss.clear();

    // Timer не должен сработать — шаблон очищен
    zss.tick(2.0);
    EXPECT_EQ(queue.size(), 0u);
}

// ── Тест 8: event trigger может сработать несколько раз ──────────────────────

TEST(ZoneSpawnTriggers, EventTriggersMultipleTimes)
{
    KernelCommandQueue queue;
    SimBus bus;
    ZoneSpawnSystem zss;
    zss.init(bus, queue, 0.0);

    ZoneSpawnSystem::ZoneTemplate tmpl;
    tmpl.name = "multi_event";
    tmpl.trigger_type = ZoneSpawnSystem::ZoneTemplate::TriggerType::EVENT;
    tmpl.event.event_type = "ZoneEntered";
    tmpl.event.source_entity = "";
    tmpl.spawn_cmd.id_hint = "multi_zone";
    tmpl.spawn_cmd.shape.type = ZoneShapeType::SPHERE;
    tmpl.spawn_cmd.shape.radius = 1.0;
    zss.add_template(std::move(tmpl), 0.0);

    bus.publish(event::ZoneEntered{.zone_id = "z1", .entity_id = 1});
    bus.publish(event::ZoneEntered{.zone_id = "z2", .entity_id = 2});

    // Два события — два SpawnZone в очереди
    EXPECT_EQ(queue.size(), 2u);
}

} // namespace s2
