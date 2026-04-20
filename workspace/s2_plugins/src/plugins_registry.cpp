#include <s2/plugins/plugin_base.hpp>
#include <s2/plugins/battery.hpp>
#include <s2/plugins/color.hpp>
#include <s2/plugins/lidar.hpp>
#include <s2/plugins/diff_drive.hpp>
#include <s2/plugins/gnss.hpp>
#include <s2/plugins/gravity.hpp>
#include <s2/plugins/imu.hpp>
#include <s2/plugins/joint_vel.hpp>
#include <s2/plugins/trajectory_recorder.hpp>
#include <s2/plugins/path_display.hpp>
#include <s2/plugins/topic_display.hpp>

#include <nlohmann/json.hpp>
#include <unordered_map>
#include <functional>
#include <vector>
#include <string>

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

static const PluginRegistrar register_battery("battery", []() { return std::make_unique<BatteryPlugin>(); });
static const PluginRegistrar register_color("color", []() { return std::make_unique<ColorPlugin>(); });
static const PluginRegistrar register_diff_drive("diff_drive", []() { return std::make_unique<DiffDrivePlugin>(); });
static const PluginRegistrar register_gravity("gravity", []() { return std::make_unique<GravityPlugin>(); });
static const PluginRegistrar register_gnss("gnss", []() { return std::make_unique<GnssPlugin>(); });
static const PluginRegistrar register_imu("imu", []() { return std::make_unique<ImuPlugin>(); });
static const PluginRegistrar register_joint_vel("joint_vel", []() { return std::make_unique<JointVelPlugin>(); });
static const PluginRegistrar register_trajectory_recorder("trajectory_recorder", []() { return std::make_unique<TrajectoryRecorderPlugin>(); });
static const PluginRegistrar register_path_display("path_display", []() { return std::make_unique<PathDisplayPlugin>(); });
static const PluginRegistrar register_topic_display("topic_display", []() { return std::make_unique<TopicDisplayPlugin>(); });
static const PluginRegistrar register_lidar("lidar", []() { return std::make_unique<LidarPlugin>(); });

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

std::string list_plugin_schemas()
{
    // Обходим реестр и собираем схему каждого плагина.
    // Создаём временный экземпляр, чтобы получить display_label() и config_schema().
    nlohmann::json result = nlohmann::json::array();

    // Фиксированный порядок для предсказуемого UI
    const std::vector<std::string> order = {
        "diff_drive", "gnss", "imu", "lidar", "battery",
        "trajectory_recorder", "path_display", "topic_display",
        "joint_vel", "color"
    };

    auto& map = factories();
    for (const auto& type_name : order)
    {
        auto it = map.find(type_name);
        if (it == map.end()) continue;

        auto plugin = it->second();

        nlohmann::json entry;
        entry["type"]   = type_name;
        entry["label"]  = plugin->display_label();

        const std::string schema_str = plugin->config_schema();
        auto params = nlohmann::json::parse(schema_str, nullptr, /*exceptions=*/false);
        entry["params"] = params.is_discarded() ? nlohmann::json::array() : params;

        result.push_back(std::move(entry));
    }

    // Добавляем плагины, которых нет в order (для расширяемости)
    for (const auto& [type_name, factory] : map)
    {
        bool already_added = false;
        for (const auto& o : order)
            if (o == type_name) { already_added = true; break; }
        if (already_added) continue;

        auto plugin = factory();
        nlohmann::json entry;
        entry["type"]   = type_name;
        entry["label"]  = plugin->display_label();

        const std::string schema_str = plugin->config_schema();
        auto params = nlohmann::json::parse(schema_str, nullptr, false);
        entry["params"] = params.is_discarded() ? nlohmann::json::array() : params;

        result.push_back(std::move(entry));
    }

    return result.dump();
}

} // namespace plugins
} // namespace s2