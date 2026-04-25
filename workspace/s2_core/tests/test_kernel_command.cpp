/**
 * @file test_kernel_command.cpp
 * Тесты KernelCommand: создание, хранение в vector, std::visit dispatch.
 */

#include <s2/kernel_command.hpp>
#include <gtest/gtest.h>
#include <string>
#include <variant>
#include <vector>

namespace s2
{

TEST(KernelCommand, SpawnEntityCreation)
{
  KernelCommand cmd = cmd::SpawnEntity{
      .entity_type = "agent",
      .config_yaml = "name: robot_1\n"
  };
  EXPECT_TRUE(std::holds_alternative<cmd::SpawnEntity>(cmd));
  auto& spawn = std::get<cmd::SpawnEntity>(cmd);
  EXPECT_EQ(spawn.entity_type, "agent");
}

TEST(KernelCommand, DespawnEntityCreation)
{
  KernelCommand cmd = cmd::DespawnEntity{.id = 42};
  EXPECT_TRUE(std::holds_alternative<cmd::DespawnEntity>(cmd));
  EXPECT_EQ(std::get<cmd::DespawnEntity>(cmd).id, 42u);
}

TEST(KernelCommand, SetPoseCreation)
{
  Pose3D pose{1.0, 2.0, 0.0, 0.0, 0.0, 1.57};
  KernelCommand cmd = cmd::SetPose{.id = 1, .pose = pose};
  EXPECT_TRUE(std::holds_alternative<cmd::SetPose>(cmd));
  auto& sp = std::get<cmd::SetPose>(cmd);
  EXPECT_EQ(sp.id, 1u);
  EXPECT_NEAR(sp.pose.x, 1.0, 1e-9);
  EXPECT_NEAR(sp.pose.yaw, 1.57, 1e-9);
}

TEST(KernelCommand, SetEnabledCreation)
{
  KernelCommand cmd = cmd::SetEnabled{.id = 5, .enabled = false};
  EXPECT_TRUE(std::holds_alternative<cmd::SetEnabled>(cmd));
  EXPECT_EQ(std::get<cmd::SetEnabled>(cmd).enabled, false);
}

TEST(KernelCommand, InteractCreation)
{
  nlohmann::json params = {{"force", 10.0}};
  KernelCommand cmd = cmd::Interact{
      .source_id = 1, .target_id = 2,
      .action = "push", .params = params,
      .max_distance = 3.0
  };
  EXPECT_TRUE(std::holds_alternative<cmd::Interact>(cmd));
  auto& interact = std::get<cmd::Interact>(cmd);
  EXPECT_EQ(interact.action, "push");
  EXPECT_NEAR(interact.max_distance, 3.0, 1e-9);
}

TEST(KernelCommand, AttachDetachCreation)
{
  KernelCommand attach = cmd::AttachObject{
      .parent_id = 1, .link = "gripper", .child_id = 2,
      .local_pose = Pose3D{0.1, 0.0, 0.0, 0.0, 0.0, 0.0}
  };
  EXPECT_TRUE(std::holds_alternative<cmd::AttachObject>(attach));

  KernelCommand detach = cmd::DetachObject{.child_id = 2};
  EXPECT_TRUE(std::holds_alternative<cmd::DetachObject>(detach));
  EXPECT_FALSE(std::get<cmd::DetachObject>(detach).drop_pose.has_value());
}

TEST(KernelCommand, ZoneCommandsCreation)
{
  KernelCommand spawn = cmd::SpawnZone{
      .shape = ZoneShape{}, .effects = {"ice"}, .visible = true
  };
  EXPECT_TRUE(std::holds_alternative<cmd::SpawnZone>(spawn));

  KernelCommand despawn = cmd::DespawnZone{.id = "zone_1"};
  EXPECT_TRUE(std::holds_alternative<cmd::DespawnZone>(despawn));

  KernelCommand toggle = cmd::ToggleZone{.id = "zone_1", .enabled = false};
  EXPECT_TRUE(std::holds_alternative<cmd::ToggleZone>(toggle));
}

TEST(KernelCommand, SceneCommandsCreation)
{
  KernelCommand load = cmd::LoadScene{.name = "warehouse.yaml"};
  EXPECT_TRUE(std::holds_alternative<cmd::LoadScene>(load));

  KernelCommand save = cmd::SaveScene{.name = "output.yaml"};
  EXPECT_TRUE(std::holds_alternative<cmd::SaveScene>(save));

  KernelCommand new_scene = cmd::NewScene{};
  EXPECT_TRUE(std::holds_alternative<cmd::NewScene>(new_scene));
}

TEST(KernelCommand, QueueCanHoldMultipleTypes)
{
  KernelCommandQueue queue;
  queue.push_back(cmd::SpawnEntity{.entity_type = "agent", .config_yaml = ""});
  queue.push_back(cmd::SetPose{.id = 1, .pose = Pose3D{}});
  queue.push_back(cmd::Interact{.source_id = 1, .target_id = 2, .action = "open"});

  EXPECT_EQ(queue.size(), 3u);
  EXPECT_TRUE(std::holds_alternative<cmd::SpawnEntity>(queue[0]));
  EXPECT_TRUE(std::holds_alternative<cmd::SetPose>(queue[1]));
  EXPECT_TRUE(std::holds_alternative<cmd::Interact>(queue[2]));
}

TEST(KernelCommand, VisitDispatch)
{
  // std::visit корректно диспатчит все варианты
  KernelCommandQueue queue;
  queue.push_back(cmd::SpawnEntity{.entity_type = "prop", .config_yaml = ""});
  queue.push_back(cmd::DespawnEntity{.id = 99});
  queue.push_back(cmd::NewScene{});

  std::vector<std::string> visited_types;

  for (const auto& c : queue)
  {
    std::visit([&visited_types](const auto& item)
    {
      using T = std::decay_t<decltype(item)>;
      if constexpr (std::is_same_v<T, cmd::SpawnEntity>)
        visited_types.push_back("spawn_entity");
      else if constexpr (std::is_same_v<T, cmd::DespawnEntity>)
        visited_types.push_back("despawn_entity");
      else if constexpr (std::is_same_v<T, cmd::NewScene>)
        visited_types.push_back("new_scene");
      else
        visited_types.push_back("other");
    }, c);
  }

  ASSERT_EQ(visited_types.size(), 3u);
  EXPECT_EQ(visited_types[0], "spawn_entity");
  EXPECT_EQ(visited_types[1], "despawn_entity");
  EXPECT_EQ(visited_types[2], "new_scene");
}

}  // namespace s2
