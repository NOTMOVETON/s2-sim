#include <s2/plugins/grabber_plugin.hpp>

#include <nlohmann/json.hpp>

namespace s2::plugins
{

// Заглушки для TDD RED -- тесты должны упасть

void GrabberPlugin::from_config(const YAML::Node& /*node*/)
{
    // TODO: реализация в GREEN фазе
}

void GrabberPlugin::handle_input(const std::string& /*json_input*/)
{
    // TODO: реализация в GREEN фазе
}

void GrabberPlugin::update(double /*dt*/, Agent& /*agent*/, const PluginContext& /*ctx*/)
{
    // TODO: реализация в GREEN фазе
}

void GrabberPlugin::on_reset(Agent& /*agent*/)
{
    // TODO: реализация в GREEN фазе
}

std::string GrabberPlugin::inputs_schema() const
{
    return "{}";
}

std::string GrabberPlugin::to_json() const
{
    return "{}";
}

} // namespace s2::plugins
