#include <gtest/gtest.h>

#include <s2/scene_loader.hpp>
#include <s2/actor.hpp>
#include <s2/prop.hpp>

#include <filesystem>
#include <fstream>
#include <string>

namespace s2 {

namespace {

// Вспомогательная функция: создать временный YAML-файл сцены
std::string make_temp_yaml(const std::string& content) {
    std::string path = (std::filesystem::temp_directory_path() / "s2_test_actor_prop.yaml").string();
    std::ofstream f(path);
    f << content;
    return path;
}

} // anonymous namespace

// ── Тесты парсинга акторов ──────────────────────────────────────────────

TEST(ActorPropLoading, LoadsActorFromYaml) {
    const std::string yaml = R"(
s2:
  update_rate: 100
  actors:
    - name: door_01
      type: door
      pose: {x: 1.0, y: 2.0, z: 0.0, yaw: 0.5}
      collision_enabled: false
    - name: conveyor_01
      type: conveyor
      pose: {x: 5.0, y: 0.0, z: 0.0}
)";
    auto path = make_temp_yaml(yaml);
    auto scene = SceneLoader::load(path);

    ASSERT_EQ(scene.actors.size(), 2u);

    // Первый актор
    EXPECT_EQ(scene.actors[0].name, "door_01");
    EXPECT_EQ(scene.actors[0].type, "door");
    EXPECT_NEAR(scene.actors[0].world_pose.x, 1.0, 1e-9);
    EXPECT_NEAR(scene.actors[0].world_pose.y, 2.0, 1e-9);
    EXPECT_NEAR(scene.actors[0].world_pose.yaw, 0.5, 1e-9);
    EXPECT_FALSE(scene.actors[0].collision_enabled);

    // Второй актор
    EXPECT_EQ(scene.actors[1].name, "conveyor_01");
    EXPECT_EQ(scene.actors[1].type, "conveyor");
    EXPECT_NEAR(scene.actors[1].world_pose.x, 5.0, 1e-9);
    // collision_enabled по умолчанию true
    EXPECT_TRUE(scene.actors[1].collision_enabled);
}

TEST(ActorPropLoading, ActorBehaviorFactory) {
    const std::string yaml = R"(
s2:
  update_rate: 100
  actors:
    - name: door_01
      type: door
      behavior: door
      pose: {x: 0, y: 0, z: 0}
    - name: door_02
      type: door
      behavior: unknown_type
      pose: {x: 1, y: 0, z: 0}
)";
    auto path = make_temp_yaml(yaml);

    // Фабрика поведений: "door" -> создаёт, "unknown_type" -> nullptr (T-02-06)
    int factory_calls = 0;
    SceneLoader::BehaviorFactory factory = [&](const std::string& type,
                                               const YAML::Node& /*node*/)
        -> std::unique_ptr<IActorBehavior> {
        factory_calls++;
        if (type == "door") {
            // Возвращаем nullptr — настоящий DoorBehavior будет в Plan 02-03
            // Тест проверяет что фабрика ВЫЗЫВАЕТСЯ, не что behavior создаётся
            return nullptr;
        }
        return nullptr;
    };

    auto scene = SceneLoader::load(path, SceneLoader::PluginFactory{}, factory);

    ASSERT_EQ(scene.actors.size(), 2u);
    // Фабрика должна быть вызвана 2 раза (для door и unknown_type)
    EXPECT_EQ(factory_calls, 2);
    // Оба behavior nullptr (фабрика возвращает nullptr)
    EXPECT_EQ(scene.actors[0].behavior, nullptr);
    EXPECT_EQ(scene.actors[1].behavior, nullptr);
}

// ── Тесты парсинга пропов ──────────────────────────────────────────────

TEST(ActorPropLoading, LoadsPropFromYaml) {
    const std::string yaml = R"(
s2:
  update_rate: 100
  props:
    - type: crate
      movable: true
      has_collision: true
      pose: {x: 3.0, y: 0.0, z: 0.0}
      capabilities: [fragile, flammable]
      tags:
        material: wood
        weight: heavy
    - type: barrel
      movable: false
      has_collision: false
      pose: {x: 0.0, y: 5.0, z: 0.0}
)";
    auto path = make_temp_yaml(yaml);
    auto scene = SceneLoader::load(path);

    ASSERT_EQ(scene.props.size(), 2u);

    // Первый проп
    EXPECT_EQ(scene.props[0].type, "crate");
    EXPECT_TRUE(scene.props[0].movable);
    EXPECT_TRUE(scene.props[0].has_collision);
    EXPECT_NEAR(scene.props[0].world_pose.x, 3.0, 1e-9);

    // Capabilities
    EXPECT_EQ(scene.props[0].capabilities.size(), 2u);
    EXPECT_TRUE(scene.props[0].capabilities.count("fragile"));
    EXPECT_TRUE(scene.props[0].capabilities.count("flammable"));

    // Tags
    EXPECT_EQ(scene.props[0].tags.size(), 2u);
    EXPECT_EQ(scene.props[0].tags["material"], "wood");
    EXPECT_EQ(scene.props[0].tags["weight"], "heavy");

    // Второй проп
    EXPECT_EQ(scene.props[1].type, "barrel");
    EXPECT_FALSE(scene.props[1].movable);
    EXPECT_FALSE(scene.props[1].has_collision);
}

TEST(ActorPropLoading, LoadsPropWithSignals) {
    const std::string yaml = R"(
s2:
  update_rate: 100
  props:
    - type: button
      pose: {x: 0, y: 0, z: 0}
      signals:
        - signal_id: btn_01
          signal_type: wire
          range: 5.0
          requires_los: true
          enabled: true
        - signal_id: aruco_42
          signal_type: aruco
          range: 10.0
)";
    auto path = make_temp_yaml(yaml);
    auto scene = SceneLoader::load(path);

    ASSERT_EQ(scene.props.size(), 1u);
    ASSERT_EQ(scene.props[0].signals.size(), 2u);

    // Первый сигнал
    EXPECT_EQ(scene.props[0].signals[0].signal_id, "btn_01");
    EXPECT_EQ(scene.props[0].signals[0].signal_type, "wire");
    EXPECT_NEAR(scene.props[0].signals[0].range, 5.0, 1e-9);
    EXPECT_TRUE(scene.props[0].signals[0].requires_los);
    EXPECT_TRUE(scene.props[0].signals[0].enabled);

    // Второй сигнал
    EXPECT_EQ(scene.props[0].signals[1].signal_id, "aruco_42");
    EXPECT_EQ(scene.props[0].signals[1].signal_type, "aruco");
    EXPECT_NEAR(scene.props[0].signals[1].range, 10.0, 1e-9);
    // Defaults
    EXPECT_FALSE(scene.props[0].signals[1].requires_los);
    EXPECT_TRUE(scene.props[0].signals[1].enabled);
}

TEST(ActorPropLoading, PropDefaultValues) {
    const std::string yaml = R"(
s2:
  update_rate: 100
  props:
    - type: decoration
      pose: {x: 0, y: 0, z: 0}
)";
    auto path = make_temp_yaml(yaml);
    auto scene = SceneLoader::load(path);

    ASSERT_EQ(scene.props.size(), 1u);
    // Значения по умолчанию
    EXPECT_FALSE(scene.props[0].movable);
    EXPECT_TRUE(scene.props[0].has_collision);
    EXPECT_TRUE(scene.props[0].capabilities.empty());
    EXPECT_TRUE(scene.props[0].tags.empty());
    EXPECT_TRUE(scene.props[0].signals.empty());
    EXPECT_FALSE(scene.props[0].attached_to_agent.has_value());
}

} // namespace s2
