/**
 * @file test_actor_behavior.cpp
 * Тесты системы поведения акторов (Phase 5):
 * ActorFSM, BehaviorRegistry, IActorBehavior, SceneLoader с behavior.
 */

#include <gtest/gtest.h>

#include <s2/actor_behavior.hpp>
#include <s2/actor_fsm.hpp>
#include <s2/behavior_registry.hpp>
#include <s2/entity.hpp>
#include <s2/scene_loader.hpp>

#include <fstream>
#include <filesystem>

using namespace s2;

// ─── TestBehavior — минимальная реализация для тестов ──────────────────────

namespace {

class TestBehavior final : public IActorBehavior {
public:
    std::string type() const override { return "TestBehavior"; }
    void on_init(const YAML::Node&) override {}
    void on_spawn(Actor&) override {}
    void on_reset() override {}
    void update(double, Actor&, WorldContext&) override {}
    std::string current_state() const override { return "idle"; }
    nlohmann::json to_json() const override { return {{"type", "TestBehavior"}}; }
};

std::string make_temp_yaml(const std::string& content) {
    std::string path = std::filesystem::temp_directory_path() / "s2_test_actor_behavior.yaml";
    std::ofstream f(path);
    f << content;
    return path;
}

} // namespace

// ─── ActorFSM: базовые переходы ────────────────────────────────────────────

TEST(ActorFSM, BasicTransitions)
{
    ActorFSM fsm;
    fsm.add_state("closed");
    fsm.add_state("open");
    fsm.add_transition("closed", "open",  "open");
    fsm.add_transition("open",   "close", "closed");
    fsm.set_initial("closed");

    EXPECT_EQ(fsm.current_state(), "closed");

    EXPECT_TRUE(fsm.fire("open"));
    EXPECT_EQ(fsm.current_state(), "open");

    // Нет перехода "open" из состояния "open"
    EXPECT_FALSE(fsm.fire("open"));
    EXPECT_EQ(fsm.current_state(), "open");

    EXPECT_TRUE(fsm.fire("close"));
    EXPECT_EQ(fsm.current_state(), "closed");
}

// ─── ActorFSM: переход с condition ─────────────────────────────────────────

TEST(ActorFSM, ConditionalTransition)
{
    ActorFSM fsm;
    fsm.add_state("idle");
    fsm.add_state("active");
    fsm.set_initial("idle");

    bool flag = false;
    fsm.add_transition("idle", "trigger", "active", [&]() { return flag; });

    // Условие false — переход не выполняется
    EXPECT_FALSE(fsm.fire("trigger"));
    EXPECT_EQ(fsm.current_state(), "idle");

    // Условие true — переход выполняется
    flag = true;
    EXPECT_TRUE(fsm.fire("trigger"));
    EXPECT_EQ(fsm.current_state(), "active");
}

// ─── BehaviorRegistry: create по имени ─────────────────────────────────────

TEST(BehaviorRegistry, CreateByName)
{
    BehaviorRegistry registry;
    registry.register_type("TestBehavior",
        [](const YAML::Node&) { return std::make_unique<TestBehavior>(); });

    auto b = registry.create("TestBehavior", YAML::Node{});
    EXPECT_NE(b, nullptr);
    EXPECT_EQ(b->type(), "TestBehavior");
}

// ─── BehaviorRegistry: неизвестный тип → runtime_error ────────────────────

TEST(BehaviorRegistry, UnknownTypeThrows)
{
    BehaviorRegistry registry;
    EXPECT_THROW(registry.create("Unknown", YAML::Node{}), std::runtime_error);
}

// ─── BehaviorRegistry: has_type ────────────────────────────────────────────

TEST(BehaviorRegistry, HasType)
{
    BehaviorRegistry registry;
    EXPECT_FALSE(registry.has_type("TestBehavior"));
    registry.register_type("TestBehavior",
        [](const YAML::Node&) { return std::make_unique<TestBehavior>(); });
    EXPECT_TRUE(registry.has_type("TestBehavior"));
}

// ─── SceneLoader: загрузка behavior из YAML ────────────────────────────────

TEST(SceneLoader, LoadBehaviorFromYAML)
{
    const std::string yaml = R"(
s2:
  update_rate: 100
  visualizer:
    enabled: false
  actors:
    - name: door_1
      behavior:
        type: TestBehavior
)";
    auto path = make_temp_yaml(yaml);

    BehaviorRegistry registry;
    registry.register_type("TestBehavior",
        [](const YAML::Node&) { return std::make_unique<TestBehavior>(); });

    auto scene = SceneLoader::load(path, {}, &registry);

    ASSERT_EQ(scene.actors.size(), 1u);
    ASSERT_NE(scene.actors[0].behavior, nullptr);
    EXPECT_EQ(scene.actors[0].behavior->type(), "TestBehavior");
}

// ─── SceneLoader: без registry — behavior остаётся nullptr ─────────────────

TEST(SceneLoader, NoBehaviorRegistryKeepsNullptr)
{
    const std::string yaml = R"(
s2:
  update_rate: 100
  visualizer:
    enabled: false
  actors:
    - name: door_1
      behavior:
        type: TestBehavior
)";
    auto path = make_temp_yaml(yaml);

    // Передаём nullptr — actor.behavior должен остаться nullptr без краша
    auto scene = SceneLoader::load(path, {}, nullptr);

    ASSERT_EQ(scene.actors.size(), 1u);
    EXPECT_EQ(scene.actors[0].behavior, nullptr);
}
