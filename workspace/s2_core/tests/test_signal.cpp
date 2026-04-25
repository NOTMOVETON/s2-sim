/**
 * @file test_signal.cpp
 * Тесты для struct Signal и поля Agent::signals.
 *
 * Покрывает:
 *  - Значения по умолчанию Signal
 *  - Все поля Signal из спецификации D-15
 *  - Agent::signals — пустой вектор по умолчанию
 *  - wire-конвенция: range = infinity, requires_los = false
 */

#include <s2/types.hpp>
#include <s2/agent.hpp>

#include <gtest/gtest.h>
#include <limits>
#include <string>

namespace s2
{

// ============================================================================
// Signal — значения по умолчанию
// ============================================================================

TEST(Signal, DefaultValues)
{
  Signal s;
  EXPECT_TRUE(s.signal_type.empty());
  EXPECT_TRUE(s.signal_id.empty());
  EXPECT_DOUBLE_EQ(s.range, 0.0);
  EXPECT_FALSE(s.requires_los);
  EXPECT_TRUE(s.enabled);
}

// ============================================================================
// Signal — все поля из D-15
// ============================================================================

TEST(Signal, AllFieldsPresent)
{
  Signal s;
  s.signal_type = "aruco";
  s.signal_id   = "marker_42";
  s.local_pose  = Pose3D{0.1, 0.0, 0.5, 0.0, 0.0, 0.0};
  s.params      = nlohmann::json{{"size", 0.2}};
  s.range       = 5.0;
  s.requires_los = true;
  s.enabled     = true;

  EXPECT_EQ(s.signal_type, "aruco");
  EXPECT_EQ(s.signal_id, "marker_42");
  EXPECT_DOUBLE_EQ(s.local_pose.x, 0.1);
  EXPECT_DOUBLE_EQ(s.local_pose.z, 0.5);
  EXPECT_NEAR(s.params["size"].get<double>(), 0.2, 1e-9);
  EXPECT_DOUBLE_EQ(s.range, 5.0);
  EXPECT_TRUE(s.requires_los);
  EXPECT_TRUE(s.enabled);
}

// ============================================================================
// Signal — wire-конвенция (D-17)
// ============================================================================

TEST(Signal, WireConvention)
{
  // wire-сигнал: range = infinity, requires_los = false
  Signal wire;
  wire.signal_type  = "wire";
  wire.signal_id    = "wire_01";
  wire.range        = std::numeric_limits<double>::infinity();
  wire.requires_los = false;
  wire.enabled      = true;

  EXPECT_TRUE(std::isinf(wire.range));
  EXPECT_FALSE(wire.requires_los);
  EXPECT_TRUE(wire.enabled);
}

// ============================================================================
// Agent::signals — пустой по умолчанию (D-16)
// ============================================================================

TEST(AgentSignals, EmptyByDefault)
{
  Agent agent;
  EXPECT_TRUE(agent.signals.empty());
}

TEST(AgentSignals, CanAddSignal)
{
  Agent agent;
  Signal s;
  s.signal_type = "rfid";
  s.signal_id   = "tag_001";
  s.range       = 1.5;
  agent.signals.push_back(s);

  ASSERT_EQ(agent.signals.size(), 1u);
  EXPECT_EQ(agent.signals[0].signal_type, "rfid");
  EXPECT_DOUBLE_EQ(agent.signals[0].range, 1.5);
}

TEST(AgentSignals, MultipleSignals)
{
  Agent agent;
  agent.signals.push_back(Signal{"aruco", "m_0", {}, {}, 5.0, true, true});
  agent.signals.push_back(Signal{"wire",  "w_0", {}, {}, std::numeric_limits<double>::infinity(), false, true});

  ASSERT_EQ(agent.signals.size(), 2u);
  EXPECT_EQ(agent.signals[0].signal_type, "aruco");
  EXPECT_TRUE(std::isinf(agent.signals[1].range));
}

}  // namespace s2
