/**
 * @file test_sim_bus.cpp
 * Тесты для SimBus: подписка, публикация, типобезопасность.
 *
 * Покрывает:
 *  - Базовая подписка и публикация
 *  - Несколько подписчиков на один тип
 *  - Публикация без подписчиков (не падает)
 *  - Разные типы событий не пересекаются
 *  - Пустое событие (default конструктор)
 *  - Лямбды с capture по ссылке и по значению
 *  - subscriber_count() и event_type_count()
 *  - Интеграция: событие → handler модифицирует SharedState
 */

#include <s2/sim_bus.hpp>
#include <s2/shared_state.hpp>

#include <gtest/gtest.h>
#include <string>
#include <vector>

namespace s2
{

// ============================================================================
// Базовая подписка и публикация
// ============================================================================

TEST(SimBus, SubscribeAndPublish)
{
  SimBus bus;
  bool handler_called = false;

  bus.subscribe<event::AgentEnteredZone>(
      [&handler_called](const event::AgentEnteredZone&) {
        handler_called = true;
      });

  bus.publish(event::AgentEnteredZone{.agent = 1, .zone = "test_zone"});
  EXPECT_TRUE(handler_called);
}

TEST(SimBus, PublishWithoutSubscribers)
{
  // Публикация без подписчиков не должна падать
  SimBus bus;
  EXPECT_NO_THROW(
      bus.publish(event::AgentEnteredZone{.agent = 1, .zone = "test_zone"}));
}

// ============================================================================
// Несколько подписчиков
// ============================================================================

TEST(SimBus, MultipleSubscribersSameType)
{
  SimBus bus;
  int call_count = 0;

  bus.subscribe<event::AgentEnteredZone>(
      [&call_count](const event::AgentEnteredZone&) { call_count++; });
  bus.subscribe<event::AgentEnteredZone>(
      [&call_count](const event::AgentEnteredZone&) { call_count++; });
  bus.subscribe<event::AgentEnteredZone>(
      [&call_count](const event::AgentEnteredZone&) { call_count++; });

  bus.publish(event::AgentEnteredZone{.agent = 1, .zone = "test_zone"});
  EXPECT_EQ(call_count, 3);
}

TEST(SimBus, SubscribersReceiveEventData)
{
  SimBus bus;
  AgentId received_agent = 0;
  ZoneId received_zone;

  bus.subscribe<event::AgentEnteredZone>(
      [&received_agent, &received_zone](const event::AgentEnteredZone& e) {
        received_agent = e.agent;
        received_zone = e.zone;
      });

  bus.publish(event::AgentEnteredZone{.agent = 42, .zone = "ice_zone"});
  EXPECT_EQ(received_agent, 42u);
  EXPECT_EQ(received_zone, "ice_zone");
}

// ============================================================================
// Разные типы событий не пересекаются
// ============================================================================

TEST(SimBus, DifferentTypesDoNotInterfere)
{
  SimBus bus;
  int entered_count = 0;
  int exited_count = 0;

  bus.subscribe<event::AgentEnteredZone>(
      [&entered_count](const event::AgentEnteredZone&) { entered_count++; });
  bus.subscribe<event::AgentExitedZone>(
      [&exited_count](const event::AgentExitedZone&) { exited_count++; });

  // Публикуем только AgentEnteredZone
  bus.publish(event::AgentEnteredZone{.agent = 1, .zone = "zone_a"});

  EXPECT_EQ(entered_count, 1);
  EXPECT_EQ(exited_count, 0);
}

TEST(SimBus, AllEventTypesDelivered)
{
  SimBus bus;
  std::vector<std::string> events;

  bus.subscribe<event::AgentEnteredZone>(
      [&events](const event::AgentEnteredZone&) {
        events.push_back("entered");
      });
  bus.subscribe<event::AgentExitedZone>(
      [&events](const event::AgentExitedZone&) { events.push_back("exited"); });
  bus.subscribe<event::ActorStateChanged>(
      [&events](const event::ActorStateChanged&) {
        events.push_back("actor_changed");
      });
  bus.subscribe<event::ObjectAttached>(
      [&events](const event::ObjectAttached&) { events.push_back("attached"); });
  bus.subscribe<event::ObjectReleased>(
      [&events](const event::ObjectReleased&) { events.push_back("released"); });
  bus.subscribe<event::AgentCollision>(
      [&events](const event::AgentCollision&) { events.push_back("collision"); });

  bus.publish(event::AgentEnteredZone{.agent = 1, .zone = "a"});
  bus.publish(event::AgentExitedZone{.agent = 1, .zone = "a"});
  bus.publish(event::ActorStateChanged{
      .actor = 1, .old_state = "closed", .new_state = "open"});
  bus.publish(event::ObjectAttached{.obj = 1, .agent = 1, .link = "gripper"});
  bus.publish(event::ObjectReleased{.obj = 1, .agent = 1});
  bus.publish(event::AgentCollision{.agent = 1, .point = Vec3(0, 0, 0)});

  ASSERT_EQ(events.size(), 6u);
  EXPECT_EQ(events[0], "entered");
  EXPECT_EQ(events[1], "exited");
  EXPECT_EQ(events[2], "actor_changed");
  EXPECT_EQ(events[3], "attached");
  EXPECT_EQ(events[4], "released");
  EXPECT_EQ(events[5], "collision");
}

// ============================================================================
// Краевые случаи
// ============================================================================

TEST(SimBus, EmptyEventDefaultConstructor)
{
  // Событие с default-конструктором должно обрабатываться корректно
  SimBus bus;
  bool called = false;

  bus.subscribe<event::AgentEnteredZone>(
      [&called](const event::AgentEnteredZone&) { called = true; });

  event::AgentEnteredZone empty_event;
  bus.publish(empty_event);
  EXPECT_TRUE(called);
}

TEST(SimBus, LambdaCaptureByValue)
{
  SimBus bus;
  int multiplier = 10;
  int result = 0;

  // Capture по значению — multiplier копируется
  bus.subscribe<event::AgentEnteredZone>(
      [multiplier, &result](const event::AgentEnteredZone& e) {
        result = static_cast<int>(e.agent) * multiplier;
      });

  bus.publish(event::AgentEnteredZone{.agent = 5, .zone = "zone"});
  EXPECT_EQ(result, 50);
}

TEST(SimBus, LambdaCaptureByReference)
{
  SimBus bus;
  int counter = 0;

  // Capture по ссылке — counter изменяется
  bus.subscribe<event::AgentEnteredZone>(
      [&counter](const event::AgentEnteredZone&) { counter++; });

  bus.publish(event::AgentEnteredZone{.agent = 1, .zone = "zone"});
  bus.publish(event::AgentEnteredZone{.agent = 2, .zone = "zone"});
  bus.publish(event::AgentEnteredZone{.agent = 3, .zone = "zone"});

  EXPECT_EQ(counter, 3);
}

TEST(SimBus, SameSubscriberCalledMultipleTimes)
{
  SimBus bus;
  std::vector<AgentId> agents;

  bus.subscribe<event::AgentEnteredZone>(
      [&agents](const event::AgentEnteredZone& e) { agents.push_back(e.agent); });

  bus.publish(event::AgentEnteredZone{.agent = 1, .zone = "a"});
  bus.publish(event::AgentEnteredZone{.agent = 2, .zone = "b"});
  bus.publish(event::AgentEnteredZone{.agent = 3, .zone = "c"});

  ASSERT_EQ(agents.size(), 3u);
  EXPECT_EQ(agents[0], 1u);
  EXPECT_EQ(agents[1], 2u);
  EXPECT_EQ(agents[2], 3u);
}

// ============================================================================
// Счётчики
// ============================================================================

TEST(SimBus, SubscriberCount)
{
  SimBus bus;
  EXPECT_EQ(bus.subscriber_count<event::AgentEnteredZone>(), 0u);

  bus.subscribe<event::AgentEnteredZone>([](const event::AgentEnteredZone&) {});
  EXPECT_EQ(bus.subscriber_count<event::AgentEnteredZone>(), 1u);

  bus.subscribe<event::AgentEnteredZone>([](const event::AgentEnteredZone&) {});
  EXPECT_EQ(bus.subscriber_count<event::AgentEnteredZone>(), 2u);
}

TEST(SimBus, SubscriberCountForUnsubscribedType)
{
  SimBus bus;
  bus.subscribe<event::AgentEnteredZone>([](const event::AgentEnteredZone&) {});

  // На AgentExitedZone никто не подписан
  EXPECT_EQ(bus.subscriber_count<event::AgentExitedZone>(), 0u);
}

TEST(SimBus, EventTypeCount)
{
  SimBus bus;
  EXPECT_EQ(bus.event_type_count(), 0u);

  bus.subscribe<event::AgentEnteredZone>([](const event::AgentEnteredZone&) {});
  EXPECT_EQ(bus.event_type_count(), 1u);

  bus.subscribe<event::AgentExitedZone>([](const event::AgentExitedZone&) {});
  EXPECT_EQ(bus.event_type_count(), 2u);
}

// ============================================================================
// Интеграция: SimBus + SharedState
// ============================================================================

TEST(SimBus, IntegrationWithSharedState)
{
  // Сценарий: при входе в зону SharedState получает contribution
  SimBus bus;
  SharedState state;

  bus.subscribe<event::AgentEnteredZone>(
      [&state](const event::AgentEnteredZone& e) {
        if (e.zone == "ice_zone")
        {
          state.add_scale(0.3, "ice_zone");
        }
        else if (e.zone == "boost_zone")
        {
          state.add_scale(1.5, "boost_zone");
        }
      });

  // Агент входит в ледяную зону
  bus.publish(event::AgentEnteredZone{.agent = 1, .zone = "ice_zone"});

  // Проверяем, что contribution добавлен
  EXPECT_EQ(state.scale_contrib_count(), 1u);

  // Разрешаем и проверяем effective
  state.resolve();
  EXPECT_NEAR(state.effective().speed_scale, 0.3, 1e-10);
}

TEST(SimBus, IntegrationMultipleEventsSharedState)
{
  SimBus bus;
  SharedState state;

  bus.subscribe<event::AgentEnteredZone>(
      [&state](const event::AgentEnteredZone& e) {
        if (e.zone == "ice_zone")
          state.add_scale(0.3, "ice_zone");
      });

  bus.subscribe<event::AgentExitedZone>(
      [&state](const event::AgentExitedZone&) {
        // При выходе — ничего не делаем (в реальной системе сняли бы contribution)
      });

  bus.publish(event::AgentEnteredZone{.agent = 1, .zone = "ice_zone"});
  bus.publish(event::AgentExitedZone{.agent = 1, .zone = "ice_zone"});

  // Только один contribution от AgentEnteredZone
  EXPECT_EQ(state.scale_contrib_count(), 1u);
}

}  // namespace s2