/**
 * @file test_event_bus.cpp
 * Тесты для EventBus: новые event-типы (EntitySpawned, SignalActivated и т.п.).
 *
 * Тесты для старых event-типов (AgentEnteredZone и т.п.) остаются в test_sim_bus.cpp.
 */

#include <s2/event_bus.hpp>
#include <s2/sim_bus.hpp>  // для теста backward-compat алиаса SimBus
#include <gtest/gtest.h>
#include <string>
#include <vector>

namespace s2
{

TEST(EventBus, EntitySpawnedDelivered)
{
  EventBus bus;
  EntityId received_id = 0;
  std::string received_type;

  bus.subscribe<event::EntitySpawned>(
      [&](const event::EntitySpawned& e) {
        received_id   = e.id;
        received_type = e.entity_type;
      });

  bus.publish(event::EntitySpawned{.id = 42, .entity_type = "agent"});
  EXPECT_EQ(received_id, 42u);
  EXPECT_EQ(received_type, "agent");
}

TEST(EventBus, EntityDespawnedDelivered)
{
  EventBus bus;
  EntityId received = 0;
  bus.subscribe<event::EntityDespawned>(
      [&](const event::EntityDespawned& e) { received = e.id; });
  bus.publish(event::EntityDespawned{.id = 7});
  EXPECT_EQ(received, 7u);
}

TEST(EventBus, SignalActivatedDelivered)
{
  EventBus bus;
  std::string received_id;
  EntityId received_source = 0;

  bus.subscribe<event::SignalActivated>(
      [&](const event::SignalActivated& e) {
        received_id     = e.signal_id;
        received_source = e.source_entity;
      });

  bus.publish(event::SignalActivated{.signal_id = "wire_01", .source_entity = 3});
  EXPECT_EQ(received_id, "wire_01");
  EXPECT_EQ(received_source, 3u);
}

TEST(EventBus, SignalDeactivatedDelivered)
{
  EventBus bus;
  bool called = false;
  bus.subscribe<event::SignalDeactivated>(
      [&](const event::SignalDeactivated&) { called = true; });
  bus.publish(event::SignalDeactivated{.signal_id = "wire_01", .source_entity = 1});
  EXPECT_TRUE(called);
}

TEST(EventBus, ZoneEnteredDelivered)
{
  EventBus bus;
  ZoneId received_zone;
  EntityId received_entity = 0;

  bus.subscribe<event::ZoneEntered>(
      [&](const event::ZoneEntered& e) {
        received_zone   = e.zone_id;
        received_entity = e.entity_id;
      });

  bus.publish(event::ZoneEntered{.zone_id = "ice_zone", .entity_id = 5});
  EXPECT_EQ(received_zone, "ice_zone");
  EXPECT_EQ(received_entity, 5u);
}

TEST(EventBus, ZoneExitedDelivered)
{
  EventBus bus;
  bool called = false;
  bus.subscribe<event::ZoneExited>(
      [&](const event::ZoneExited&) { called = true; });
  bus.publish(event::ZoneExited{.zone_id = "ice_zone", .entity_id = 5});
  EXPECT_TRUE(called);
}

TEST(EventBus, GrabAttemptSucceededFailed)
{
  EventBus bus;
  std::vector<std::string> log;

  bus.subscribe<event::GrabAttempt>(
      [&](const event::GrabAttempt&)   { log.push_back("attempt"); });
  bus.subscribe<event::GrabSucceeded>(
      [&](const event::GrabSucceeded&) { log.push_back("succeeded"); });
  bus.subscribe<event::GrabFailed>(
      [&](const event::GrabFailed& e)  { log.push_back("failed:" + e.reason); });

  bus.publish(event::GrabAttempt{.agent = 1, .target = 2});
  bus.publish(event::GrabSucceeded{.agent = 1, .target = 2});
  bus.publish(event::GrabFailed{.agent = 1, .target = 3, .reason = "too_far"});

  ASSERT_EQ(log.size(), 3u);
  EXPECT_EQ(log[0], "attempt");
  EXPECT_EQ(log[1], "succeeded");
  EXPECT_EQ(log[2], "failed:too_far");
}

TEST(EventBus, DamageDealtDelivered)
{
  EventBus bus;
  double received_amount = 0.0;
  std::string received_type;

  bus.subscribe<event::DamageDealt>(
      [&](const event::DamageDealt& e) {
        received_amount = e.amount;
        received_type   = e.damage_type;
      });

  bus.publish(event::DamageDealt{.source = 1, .target = 2, .amount = 42.5, .damage_type = "fire"});
  EXPECT_NEAR(received_amount, 42.5, 1e-9);
  EXPECT_EQ(received_type, "fire");
}

TEST(EventBus, SimBusAliasWorksAsEventBus)
{
  // Тест backward compat: SimBus через using-алиас
  SimBus bus;  // SimBus = EventBus
  bool called = false;

  bus.subscribe<event::EntitySpawned>(
      [&](const event::EntitySpawned&) { called = true; });
  bus.publish(event::EntitySpawned{.id = 1, .entity_type = "agent"});
  EXPECT_TRUE(called);
}

}  // namespace s2
