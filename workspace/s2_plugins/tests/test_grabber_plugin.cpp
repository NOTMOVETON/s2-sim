#include <gtest/gtest.h>

#include <s2/plugins/grabber_plugin.hpp>
#include <s2/agent.hpp>
#include <s2/event_bus.hpp>
#include <s2/kernel_command.hpp>
#include <s2/world_query.hpp>

// ============================================================================
// Тестовый WorldQuery с настраиваемым find_nearest_movable_prop
// ============================================================================

class TestWorldQuery : public s2::WorldQuery
{
public:
    std::optional<s2::ObjectId> find_nearest_movable_prop(s2::Vec3 /*pos*/,
                                                          double /*radius*/) const override
    {
        return movable_prop_id_;
    }

    // Настраивается тестами
    std::optional<s2::ObjectId> movable_prop_id_;
};

// ============================================================================
// Fixture для GrabberPlugin
// ============================================================================

class GrabberPluginTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        grabber_ = std::make_unique<s2::plugins::GrabberPlugin>();
        YAML::Node cfg;
        cfg["interaction_distance"] = 2.0;
        cfg["grab_link"] = "gripper";
        grabber_->from_config(cfg);

        agent_.id = 1;
        agent_.world_pose = s2::Pose3D{1.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    }

    void do_update()
    {
        s2::PluginContext ctx{world_, bus_, commands_};
        grabber_->update(0.01, agent_, ctx);
    }

    std::unique_ptr<s2::plugins::GrabberPlugin> grabber_;
    s2::Agent agent_;
    TestWorldQuery world_;
    s2::EventBus bus_;
    s2::KernelCommandQueue commands_;
};

// ============================================================================
// Тесты
// ============================================================================

// --- Базовые свойства ---

TEST_F(GrabberPluginTest, TypeAndRole)
{
    EXPECT_EQ(grabber_->type(), "grabber");
    EXPECT_EQ(grabber_->role(), s2::PluginRole::INTERACTION);
    EXPECT_TRUE(grabber_->has_inputs());
}

// --- Grab: успешный захват ---

TEST_F(GrabberPluginTest, GrabSucceeded_PublishesEventsAndAttachCommand)
{
    // Настроить WorldQuery -- есть movable prop с id=100 рядом
    world_.movable_prop_id_ = 100;

    // Подписка на события
    std::vector<s2::event::GrabAttempt> attempts;
    std::vector<s2::event::GrabSucceeded> successes;
    bus_.subscribe<s2::event::GrabAttempt>([&](const s2::event::GrabAttempt& e) {
        attempts.push_back(e);
    });
    bus_.subscribe<s2::event::GrabSucceeded>([&](const s2::event::GrabSucceeded& e) {
        successes.push_back(e);
    });

    // Отправить grab команду
    grabber_->handle_input(R"({"action":"grab"})");
    do_update();

    // GrabAttempt должен быть опубликован
    ASSERT_EQ(attempts.size(), 1u);
    EXPECT_EQ(attempts[0].agent, 1u);

    // GrabSucceeded должен быть опубликован
    ASSERT_EQ(successes.size(), 1u);
    EXPECT_EQ(successes[0].agent, 1u);
    EXPECT_EQ(successes[0].target, 100u);

    // AttachObject команда должна быть в очереди
    ASSERT_EQ(commands_.size(), 1u);
    auto* attach = std::get_if<s2::cmd::AttachObject>(&commands_[0]);
    ASSERT_NE(attach, nullptr);
    EXPECT_EQ(attach->parent_id, 1u);
    EXPECT_EQ(attach->link, "gripper");
    EXPECT_EQ(attach->child_id, 100u);
}

// --- Grab: нет пропа в радиусе ---

TEST_F(GrabberPluginTest, GrabFailed_NoPropInRange)
{
    // WorldQuery не находит пропа
    world_.movable_prop_id_ = std::nullopt;

    std::vector<s2::event::GrabFailed> failures;
    bus_.subscribe<s2::event::GrabFailed>([&](const s2::event::GrabFailed& e) {
        failures.push_back(e);
    });

    grabber_->handle_input(R"({"action":"grab"})");
    do_update();

    ASSERT_EQ(failures.size(), 1u);
    EXPECT_EQ(failures[0].agent, 1u);
    EXPECT_EQ(failures[0].reason, "no_prop_in_range");
    // Не должно быть AttachObject команд
    EXPECT_TRUE(commands_.empty());
}

// --- Release: успешный отпуск ---

TEST_F(GrabberPluginTest, Release_SendsDetachCommand)
{
    // Сначала захватить
    world_.movable_prop_id_ = 200;
    grabber_->handle_input(R"({"action":"grab"})");
    do_update();
    commands_.clear();

    // Теперь отпустить
    grabber_->handle_input(R"({"action":"release"})");
    do_update();

    // DetachObject команда
    ASSERT_EQ(commands_.size(), 1u);
    auto* detach = std::get_if<s2::cmd::DetachObject>(&commands_[0]);
    ASSERT_NE(detach, nullptr);
    EXPECT_EQ(detach->child_id, 200u);
}

// --- Release без захваченного пропа -- ничего не происходит ---

TEST_F(GrabberPluginTest, Release_WithoutHeldProp_NoCommand)
{
    grabber_->handle_input(R"({"action":"release"})");
    do_update();

    EXPECT_TRUE(commands_.empty());
}

// --- Двойной grab: повторный grab пока держим -- игнорируется ---

TEST_F(GrabberPluginTest, DoubleGrab_Ignored)
{
    world_.movable_prop_id_ = 300;
    grabber_->handle_input(R"({"action":"grab"})");
    do_update();
    commands_.clear();

    // Повторный grab
    grabber_->handle_input(R"({"action":"grab"})");
    do_update();

    // Не должно быть новых AttachObject
    EXPECT_TRUE(commands_.empty());
}

// --- Manipulation locked: contribution пока держим проп ---

TEST_F(GrabberPluginTest, ManipulationLocked_WhileHolding)
{
    world_.movable_prop_id_ = 400;
    grabber_->handle_input(R"({"action":"grab"})");
    do_update();
    commands_.clear();

    // Следующий update -- должен добавить lock contribution
    do_update();

    // Проверить что agent.state имеет lock contribution
    agent_.state.resolve();
    EXPECT_TRUE(agent_.state.effective().motion_locked);
}

// --- Manipulation unlocked: после release нет contribution ---

TEST_F(GrabberPluginTest, ManipulationUnlocked_AfterRelease)
{
    world_.movable_prop_id_ = 500;
    grabber_->handle_input(R"({"action":"grab"})");
    do_update();
    agent_.state.clear_contributions();
    commands_.clear();

    // Release
    grabber_->handle_input(R"({"action":"release"})");
    do_update();
    agent_.state.clear_contributions();

    // Следующий update -- не должно быть lock
    do_update();
    agent_.state.resolve();
    EXPECT_FALSE(agent_.state.effective().motion_locked);
}

// --- on_reset: сбрасывает состояние ---

TEST_F(GrabberPluginTest, OnReset_ClearsState)
{
    world_.movable_prop_id_ = 600;
    grabber_->handle_input(R"({"action":"grab"})");
    do_update();
    commands_.clear();

    grabber_->on_reset(agent_);

    // После reset: release не должен выдать DetachObject (нет held prop)
    grabber_->handle_input(R"({"action":"release"})");
    do_update();
    EXPECT_TRUE(commands_.empty());
}

// --- to_json: содержит held_prop_id и interaction_distance ---

TEST_F(GrabberPluginTest, ToJson_ContainsFields)
{
    auto json_str = grabber_->to_json();
    auto j = nlohmann::json::parse(json_str);
    EXPECT_TRUE(j.contains("type"));
    EXPECT_EQ(j["type"], "grabber");
    EXPECT_TRUE(j.contains("held_prop_id"));
    EXPECT_TRUE(j.contains("interaction_distance"));
}

// --- inputs_schema: содержит action enum ---

TEST_F(GrabberPluginTest, InputsSchema_ContainsAction)
{
    auto schema_str = grabber_->inputs_schema();
    auto j = nlohmann::json::parse(schema_str);
    EXPECT_TRUE(j.contains("type"));
    EXPECT_TRUE(j.contains("properties"));
}

// --- handle_input: невалидный JSON игнорируется (T-02-14) ---

TEST_F(GrabberPluginTest, HandleInput_InvalidJson_Ignored)
{
    grabber_->handle_input("not_valid_json{{{");
    do_update();
    // Не должно крашнуться, без команд
    EXPECT_TRUE(commands_.empty());
}

// --- from_config: устанавливает interaction_distance ---

TEST_F(GrabberPluginTest, FromConfig_SetsInteractionDistance)
{
    auto json_str = grabber_->to_json();
    auto j = nlohmann::json::parse(json_str);
    EXPECT_DOUBLE_EQ(j["interaction_distance"].get<double>(), 2.0);
}
