#include <s2/plugins/plugin_base.hpp>
#include <s2/plugins/color.hpp>
#include <s2/plugins/diff_drive.hpp>
#include <s2/plugins/gnss.hpp>
#include <s2/plugins/imu.hpp>
#include <s2/plugins/joint_vel.hpp>
#include <s2/plugins/trajectory_recorder.hpp>
#include <s2/plugins/path_display.hpp>

#include <unordered_map>
#include <functional>

namespace s2
{
namespace plugins
{

using FactoryMap = std::unordered_map<std::string, std::function<std::unique_ptr<IAgentPlugin>()>>;

static FactoryMap& factories()
{
    static FactoryMap map;
    return map;
}

struct PluginRegistrar
{
    PluginRegistrar(const std::string& type, std::function<std::unique_ptr<IAgentPlugin>()> fn)
    {
        factories()[type] = std::move(fn);
    }
};

static const PluginRegistrar register_color("color", []() { return std::make_unique<ColorPlugin>(); });
static const PluginRegistrar register_diff_drive("diff_drive", []() { return std::make_unique<DiffDrivePlugin>(); });
static const PluginRegistrar register_gnss("gnss", []() { return std::make_unique<GnssPlugin>(); });
static const PluginRegistrar register_imu("imu", []() { return std::make_unique<ImuPlugin>(); });
static const PluginRegistrar register_joint_vel("joint_vel", []() { return std::make_unique<JointVelPlugin>(); });
static const PluginRegistrar register_trajectory_recorder("trajectory_recorder", []() { return std::make_unique<TrajectoryRecorderPlugin>(); });
static const PluginRegistrar register_path_display("path_display", []() { return std::make_unique<PathDisplayPlugin>(); });

std::unique_ptr<IAgentPlugin> create_plugin(const std::string& type, const YAML::Node& node)
{
    auto& map = factories();
    auto it = map.find(type);
    if (it == map.end())
    {
        return nullptr;
    }

    auto plugin = it->second();
    plugin->from_config(node);
    return plugin;
}

} // namespace plugins
} // namespace s2