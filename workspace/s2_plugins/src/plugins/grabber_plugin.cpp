#include <s2/plugins/grabber_plugin.hpp>

#include <s2/agent.hpp>
#include <s2/event_bus.hpp>
#include <s2/kernel_command.hpp>
#include <s2/shared_state.hpp>

#include <nlohmann/json.hpp>

namespace s2::plugins
{

void GrabberPlugin::from_config(const YAML::Node& node)
{
    interaction_distance_ = node["interaction_distance"].as<double>(1.5);
    grab_link_            = node["grab_link"].as<std::string>("");
}

void GrabberPlugin::handle_input(const std::string& json_input)
{
    // T-02-14: невалидный JSON игнорируется (is_discarded check)
    auto j = nlohmann::json::parse(json_input, nullptr, false);
    if (j.is_discarded()) return;

    std::string action = j.value("action", "");
    if (action == "grab")    grab_requested_    = true;
    if (action == "release") release_requested_ = true;
}

void GrabberPlugin::update(double /*dt*/, Agent& agent, const PluginContext& ctx)
{
    // --- Grab ---
    if (grab_requested_ && held_prop_id_ == 0)
    {
        grab_requested_ = false;

        // Публикуем GrabAttempt перед попыткой
        ctx.bus.publish(event::GrabAttempt{
            .agent  = agent.id,
            .target = 0
        });

        auto prop_id = ctx.world.find_nearest_movable_prop(
            agent.world_pose.position(), interaction_distance_);

        if (prop_id.has_value())
        {
            held_prop_id_ = prop_id.value();
            ctx.commands.push_back(cmd::AttachObject{
                .parent_id  = agent.id,
                .link       = grab_link_,
                .child_id   = static_cast<EntityId>(held_prop_id_),
                .local_pose = Pose3D{}  // смещение = 0 (ядро может уточнить)
            });
            ctx.bus.publish(event::GrabSucceeded{
                .agent  = agent.id,
                .target = static_cast<EntityId>(held_prop_id_)
            });
        }
        else
        {
            ctx.bus.publish(event::GrabFailed{
                .agent  = agent.id,
                .target = 0,
                .reason = "no_prop_in_range"
            });
        }
    }
    else if (grab_requested_)
    {
        // Повторный grab пока держим -- игнорируем
        grab_requested_ = false;
    }

    // --- Release ---
    if (release_requested_ && held_prop_id_ != 0)
    {
        release_requested_ = false;
        ctx.commands.push_back(cmd::DetachObject{
            .child_id  = static_cast<EntityId>(held_prop_id_),
            .drop_pose = std::nullopt
        });
        held_prop_id_ = 0;
    }
    else if (release_requested_)
    {
        // Release без захваченного пропа -- игнорируем
        release_requested_ = false;
    }

    // --- Contribution: manipulation_locked ---
    if (held_prop_id_ != 0)
    {
        agent.state.add_lock(true, "grabber");
    }
}

void GrabberPlugin::on_reset(Agent& /*agent*/)
{
    held_prop_id_      = 0;
    grab_requested_    = false;
    release_requested_ = false;
}

std::string GrabberPlugin::inputs_schema() const
{
    return R"({"type":"object","properties":{"action":{"type":"string","enum":["grab","release"]}}})";
}

std::string GrabberPlugin::to_json() const
{
    return nlohmann::json{
        {"type", "grabber"},
        {"held_prop_id", held_prop_id_},
        {"interaction_distance", interaction_distance_}
    }.dump();
}

} // namespace s2::plugins
