/**
 * @file test_door_behavior.cpp
 * И��теграционные тесты DoorBehavior через SimEngine.
 *
 * Проверяет end-to-end: создание актора с DoorBehavior,
 * cmd::Interact("open") -> phase2_actors -> FSM переходы -> ActorStateChanged события.
 *
 * Phase 2 Plan 05: финальная верификация DoorBehavior wiring.
 */

#include <gtest/gtest.h>

#include <s2/sim_engine.hpp>
#include <s2/world.hpp>
#include <s2/actor.hpp>
#include <s2/behaviors/door_behavior.hpp>
#include <s2/event_bus.hpp>
#include <s2/kernel_command.hpp>

namespace s2
{

// ============================================================================
// Вспомогательные функции
// ============================================================================

/**
 * Создать SimEngine с одним актором-дверью.
 * Возвращает unique_ptr — SimEngine не является movable (mutex, atomic).
 */
static std::unique_ptr<SimEngine> make_engine_with_door(
    ActorId door_id,
    double open_duration   = 0.1,
    double close_duration  = 0.1,
    double auto_close_secs = 0.0)
{
    auto engine = std::make_unique<SimEngine>(SimEngine::Config{.update_rate = 100.0});

    SimWorld world;
    Actor door;
    door.id   = door_id;
    door.name = "test_door";
    door.type = "door";
    door.collision_enabled = true;
    door.current_state = "CLOSED";

    auto behavior = std::make_unique<DoorBehavior>();
    YAML::Node cfg;
    cfg["open_duration"]   = open_duration;
    cfg["close_duration"]  = close_duration;
    cfg["auto_close_secs"] = auto_close_secs;
    behavior->on_init(cfg);
    behavior->on_spawn(door_id);
    door.behavior = std::move(behavior);

    world.add_actor(std::move(door));
    engine->load_world(std::move(world));
    return engine;
}

// ============================================================================
// Тесты
// ============================================================================

/**
 * Тест: дверь открывается че��ез Interact("open").
 * CLOSED -> OPENING -> OPEN.
 * collision_enabled = true при OPENING, false при OPEN.
 */
TEST(DoorBehaviorIntegration, OpenViaInteract)
{
    auto engine = make_engine_with_door(10);
    auto* actor = engine->world().get_actor(10);
    ASSERT_NE(actor, nullptr);

    // Начальное состояние
    EXPECT_EQ(actor->behavior->current_state(), "CLOSED");
    EXPECT_TRUE(actor->collision_enabled);

    // Отправить Interact{action:"open"}
    engine->push_command(cmd::Interact{
        .source_id    = 1,
        .target_id    = 10,
        .action       = "open",
        .params       = nlohmann::json::object(),
        .max_distance = 0.0
    });
    engine->step(1);

    // После одного тика: OPENING
    EXPECT_EQ(actor->behavior->current_state(), "OPENING");

    // Прождать open_duration (0.1s / 0.01s = 10 тиков; возьмём 15 для надёжности)
    engine->step(15);

    // Должна быть OPEN
    EXPECT_EQ(actor->behavior->current_state(), "OPEN");
    // collision_enabled = false при OPEN
    EXPECT_FALSE(actor->collision_enabled);
}

/**
 * Тест: ActorStateChanged публикуется при FSM-переходе.
 * cmd::Interact("open") -> CLOSED->OPENING -> ActorStateChanged событие.
 */
TEST(DoorBehaviorIntegration, PublishesActorStateChanged)
{
    auto engine = make_engine_with_door(20);

    // Первый тик инициализирует bus_ в DoorBehavior (устанавливается при update)
    engine->step(1);

    std::vector<event::ActorStateChanged> events;
    engine->bus().subscribe<event::ActorStateChanged>(
        [&](const event::ActorStateChanged& e) {
            events.push_back(e);
        });

    // Отправить Interact("open") -- bus_ уже установлен после первого update()
    engine->push_command(cmd::Interact{
        .source_id    = 1,
        .target_id    = 20,
        .action       = "open",
        .params       = nlohmann::json::object(),
        .max_distance = 0.0
    });
    engine->step(5);

    // Должно быть ��инимум одно событие: CLOSED -> OPENING
    ASSERT_FALSE(events.empty());
    EXPECT_EQ(events[0].actor, 20u);
    EXPECT_EQ(events[0].old_state, "CLOSED");
    EXPECT_EQ(events[0].new_state, "OPENING");
}

/**
 * Тест: дверь возвращается в CLOSED пр�� close.
 * open -> OPENING -> OPEN, close -> CLOSING -> CLOSED.
 */
TEST(DoorBehaviorIntegration, CloseAfterOpen)
{
    auto engine = make_engine_with_door(30);

    // Открыт��
    engine->push_command(cmd::Interact{
        .source_id = 1, .target_id = 30, .action = "open",
        .params = nlohmann::json::object(), .max_distance = 0.0
    });
    engine->step(15);  // OPENING -> OPEN

    auto* actor = engine->world().get_actor(30);
    ASSERT_NE(actor, nullptr);
    EXPECT_EQ(actor->behavior->current_state(), "OPEN");

    // Закрыть
    engine->push_command(cmd::Interact{
        .source_id = 1, .target_id = 30, .action = "close",
        .params = nlohmann::json::object(), .max_distance = 0.0
    });
    engine->step(1);
    EXPECT_EQ(actor->behavior->current_state(), "CLOSING");

    engine->step(15);  // CLOSING -> CLOSED
    EXPECT_EQ(actor->behavior->current_state(), "CLOSED");
    EXPECT_TRUE(actor->collision_enabled);
}

} // namespace s2
