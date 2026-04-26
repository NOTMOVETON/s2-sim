#include <gtest/gtest.h>

#include <s2/behaviors/door_behavior.hpp>
#include <s2/actor.hpp>
#include <s2/event_bus.hpp>
#include <s2/kernel_command.hpp>
#include <s2/world_query.hpp>

// ============================================================================
// Тестовый fixture для DoorBehavior
// ============================================================================

class DoorBehaviorTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // Создать behavior с дефолтной конфигурацией
        door_ = std::make_unique<s2::DoorBehavior>();
        YAML::Node cfg;
        cfg["open_duration"]   = 0.5;
        cfg["close_duration"]  = 0.5;
        cfg["auto_close_secs"] = 0.0; // не закрывается автоматически
        door_->on_init(cfg);
        door_->on_spawn(1);

        // Подготовить актора
        actor_.id = 1;
        actor_.name = "test_door";
        actor_.type = "door";
        actor_.collision_enabled = true;
        actor_.current_state = "CLOSED";
    }

    // Хелпер: вызвать update на N секунд (шагами по dt)
    void advance(double seconds, double dt = 0.01)
    {
        s2::WorldContext ctx{world_, bus_, commands_, sim_time_};
        int steps = static_cast<int>(seconds / dt);
        for (int i = 0; i < steps; ++i) {
            door_->update(dt, actor_, ctx);
            sim_time_ += dt;
        }
    }

    std::unique_ptr<s2::DoorBehavior> door_;
    s2::Actor actor_;
    s2::WorldQuery world_;
    s2::EventBus bus_;
    s2::KernelCommandQueue commands_;
    double sim_time_{0.0};
};

// ============================================================================
// FSM начальное состояние
// ============================================================================

TEST_F(DoorBehaviorTest, InitialStateClosed)
{
    EXPECT_EQ(door_->current_state(), "CLOSED");
}

TEST_F(DoorBehaviorTest, TypeIsDoor)
{
    EXPECT_EQ(door_->type(), "door");
}

// ============================================================================
// on_interact -> FSM переходы
// ============================================================================

TEST_F(DoorBehaviorTest, InteractOpenTransitionsToOpening)
{
    door_->on_interact(42, "open", nlohmann::json::object());
    EXPECT_EQ(door_->current_state(), "OPENING");
}

TEST_F(DoorBehaviorTest, InteractCloseFromOpenTransitionsToClosing)
{
    // Открыть полностью
    door_->on_interact(42, "open", nlohmann::json::object());
    advance(0.6); // open_duration = 0.5, ждём дольше
    EXPECT_EQ(door_->current_state(), "OPEN");

    // Закрыть
    door_->on_interact(42, "close", nlohmann::json::object());
    EXPECT_EQ(door_->current_state(), "CLOSING");
}

// ============================================================================
// Таймерные автопереходы
// ============================================================================

TEST_F(DoorBehaviorTest, OpeningToOpenAfterDuration)
{
    door_->on_interact(42, "open", nlohmann::json::object());
    EXPECT_EQ(door_->current_state(), "OPENING");

    // Через 0.5 секунд должен перейти в OPEN
    advance(0.6);
    EXPECT_EQ(door_->current_state(), "OPEN");
}

TEST_F(DoorBehaviorTest, ClosingToClosedAfterDuration)
{
    // Открыть
    door_->on_interact(42, "open", nlohmann::json::object());
    advance(0.6);
    EXPECT_EQ(door_->current_state(), "OPEN");

    // Закрыть
    door_->on_interact(42, "close", nlohmann::json::object());
    EXPECT_EQ(door_->current_state(), "CLOSING");

    advance(0.6);
    EXPECT_EQ(door_->current_state(), "CLOSED");
}

// ============================================================================
// collision_enabled: false при OPEN, true при CLOSED
// ============================================================================

TEST_F(DoorBehaviorTest, CollisionDisabledWhenOpen)
{
    door_->on_interact(42, "open", nlohmann::json::object());
    advance(0.6); // OPENING -> OPEN
    EXPECT_EQ(door_->current_state(), "OPEN");
    EXPECT_FALSE(actor_.collision_enabled);
}

TEST_F(DoorBehaviorTest, CollisionEnabledWhenClosed)
{
    // Открыть
    door_->on_interact(42, "open", nlohmann::json::object());
    advance(0.6);

    // Закрыть
    door_->on_interact(42, "close", nlohmann::json::object());
    advance(0.6);
    EXPECT_EQ(door_->current_state(), "CLOSED");
    EXPECT_TRUE(actor_.collision_enabled);
}

// ============================================================================
// ActorStateChanged при каждом FSM-переходе
// ============================================================================

TEST_F(DoorBehaviorTest, PublishesActorStateChangedOnTransitions)
{
    int event_count = 0;
    s2::ActorState last_old, last_new;
    bus_.subscribe<s2::event::ActorStateChanged>(
        [&](const s2::event::ActorStateChanged& e) {
            event_count++;
            last_old = e.old_state;
            last_new = e.new_state;
        });

    // Первый update устанавливает bus_ внутри behavior
    {
        s2::WorldContext ctx{world_, bus_, commands_, sim_time_};
        door_->update(0.01, actor_, ctx);
        sim_time_ += 0.01;
    }

    // CLOSED -> OPENING (bus_ уже установлен — событие публикуется)
    door_->on_interact(42, "open", nlohmann::json::object());
    EXPECT_GE(event_count, 1);
    EXPECT_EQ(last_old, "CLOSED");
    EXPECT_EQ(last_new, "OPENING");

    // OPENING -> OPEN (после таймера)
    advance(0.6);
    EXPECT_GE(event_count, 2);
    EXPECT_EQ(last_old, "OPENING");
    EXPECT_EQ(last_new, "OPEN");
}

// ============================================================================
// wire-триггер: on_signal
// ============================================================================

TEST_F(DoorBehaviorTest, WireOpenSignalOpens)
{
    s2::SignalEvent sig;
    sig.signal_id = "wire_open";
    sig.active = true;
    door_->on_signal(sig);
    EXPECT_EQ(door_->current_state(), "OPENING");
}

TEST_F(DoorBehaviorTest, WireCloseSignalCloses)
{
    // Открыть
    door_->on_interact(42, "open", nlohmann::json::object());
    advance(0.6);
    EXPECT_EQ(door_->current_state(), "OPEN");

    // wire_close
    s2::SignalEvent sig;
    sig.signal_id = "wire_close";
    sig.active = true;
    door_->on_signal(sig);
    EXPECT_EQ(door_->current_state(), "CLOSING");
}

TEST_F(DoorBehaviorTest, InactiveSignalIgnored)
{
    s2::SignalEvent sig;
    sig.signal_id = "wire_open";
    sig.active = false;
    door_->on_signal(sig);
    EXPECT_EQ(door_->current_state(), "CLOSED");
}

TEST_F(DoorBehaviorTest, UnknownSignalIgnored)
{
    s2::SignalEvent sig;
    sig.signal_id = "wire_unknown";
    sig.active = true;
    door_->on_signal(sig);
    EXPECT_EQ(door_->current_state(), "CLOSED");
}

// ============================================================================
// current_state() и to_json()
// ============================================================================

TEST_F(DoorBehaviorTest, CurrentStateReturnsCorrectString)
{
    EXPECT_EQ(door_->current_state(), "CLOSED");

    door_->on_interact(42, "open", nlohmann::json::object());
    EXPECT_EQ(door_->current_state(), "OPENING");
}

TEST_F(DoorBehaviorTest, ToJsonContainsState)
{
    std::string json_str = door_->to_json();
    auto json = nlohmann::json::parse(json_str);
    EXPECT_EQ(json["type"], "door");
    EXPECT_EQ(json["state"], "CLOSED");
}

// ============================================================================
// auto_close_secs
// ============================================================================

TEST_F(DoorBehaviorTest, AutoCloseAfterTimeout)
{
    // Создать door с auto_close
    auto auto_door = std::make_unique<s2::DoorBehavior>();
    YAML::Node cfg;
    cfg["open_duration"]   = 0.2;
    cfg["close_duration"]  = 0.5;  // достаточно долго чтобы не завершиться
    cfg["auto_close_secs"] = 0.3;
    auto_door->on_init(cfg);
    auto_door->on_spawn(2);

    s2::Actor actor2;
    actor2.id = 2;
    actor2.collision_enabled = true;

    auto_door->on_interact(42, "open", nlohmann::json::object());

    // Пройти open_duration (0.2с) + немного = 0.3с
    double t = 0.0;
    for (int i = 0; i < 30; ++i) { // 0.3 секунды
        s2::WorldContext ctx{world_, bus_, commands_, t};
        auto_door->update(0.01, actor2, ctx);
        t += 0.01;
    }
    EXPECT_EQ(auto_door->current_state(), "OPEN");

    // Пройти auto_close_secs (0.3с). В OPEN state_timer_ уже ~0.1 после перехода.
    // Нужно ещё ~0.2с чтобы достичь 0.3.
    // Проходим 0.25с — auto_close сработает, но close_duration (0.5) не пройдёт
    for (int i = 0; i < 25; ++i) {
        s2::WorldContext ctx{world_, bus_, commands_, t};
        auto_door->update(0.01, actor2, ctx);
        t += 0.01;
    }
    EXPECT_EQ(auto_door->current_state(), "CLOSING");
}

// ============================================================================
// on_reset
// ============================================================================

TEST_F(DoorBehaviorTest, ResetReturnsToClosed)
{
    door_->on_interact(42, "open", nlohmann::json::object());
    advance(0.6);
    EXPECT_EQ(door_->current_state(), "OPEN");

    door_->on_reset();
    EXPECT_EQ(door_->current_state(), "CLOSED");
}

// ============================================================================
// Синхронизация actor.current_state
// ============================================================================

TEST_F(DoorBehaviorTest, ActorCurrentStateSynced)
{
    door_->on_interact(42, "open", nlohmann::json::object());
    s2::WorldContext ctx{world_, bus_, commands_, sim_time_};
    door_->update(0.01, actor_, ctx);
    EXPECT_EQ(actor_.current_state, door_->current_state());
}

// ============================================================================
// Идемпотентность: повторный open при OPENING не ломается
// ============================================================================

TEST_F(DoorBehaviorTest, DoubleOpenIsIdempotent)
{
    door_->on_interact(42, "open", nlohmann::json::object());
    EXPECT_EQ(door_->current_state(), "OPENING");

    // Повторный open при OPENING — FSM не должен крашнуться
    door_->on_interact(42, "open", nlohmann::json::object());
    EXPECT_EQ(door_->current_state(), "OPENING");
}
