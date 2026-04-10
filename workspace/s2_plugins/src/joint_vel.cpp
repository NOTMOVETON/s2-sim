/**
 * @file joint_vel.cpp
 * Реализация плагина управления джоинтами через скорость.
 */

#include <s2/plugins/joint_vel.hpp>
#include <s2/agent.hpp>
#include <s2/kinematic_tree.hpp>

#include <nlohmann/json.hpp>
#include <cmath>
#include <stdexcept>

namespace s2
{
namespace plugins
{

// ─── Вспомогательная: извлечь значение поля Twist из JSON ──────────────────

static double extract_twist_field(const nlohmann::json& j, const std::string& axis)
{
    // Поддерживаем два формата:
    //   {"linear":{"x":..., "y":..., "z":...}, "angular":{"x":..., "y":..., "z":...}}
    //   {"linear_x":..., "angular_z":...}  (плоский, используется в SimEngine)
    if (axis == "linear_x")  {
        if (j.contains("linear") && j["linear"].contains("x"))  return j["linear"]["x"].get<double>();
        if (j.contains("linear_x")) return j["linear_x"].get<double>();
    } else if (axis == "linear_y")  {
        if (j.contains("linear") && j["linear"].contains("y"))  return j["linear"]["y"].get<double>();
        if (j.contains("linear_y")) return j["linear_y"].get<double>();
    } else if (axis == "linear_z")  {
        if (j.contains("linear") && j["linear"].contains("z"))  return j["linear"]["z"].get<double>();
        if (j.contains("linear_z")) return j["linear_z"].get<double>();
    } else if (axis == "angular_x") {
        if (j.contains("angular") && j["angular"].contains("x")) return j["angular"]["x"].get<double>();
        if (j.contains("angular_x")) return j["angular_x"].get<double>();
    } else if (axis == "angular_y") {
        if (j.contains("angular") && j["angular"].contains("y")) return j["angular"]["y"].get<double>();
        if (j.contains("angular_y")) return j["angular_y"].get<double>();
    } else if (axis == "angular_z") {
        if (j.contains("angular") && j["angular"].contains("z")) return j["angular"]["z"].get<double>();
        if (j.contains("angular_z")) return j["angular_z"].get<double>();
    }
    return 0.0;
}

// ─── IAgentPlugin impl ─────────────────────────────────────────────────────

std::string JointVelPlugin::inputs_schema() const
{
    nlohmann::json j;
    for (const auto& m : joints_)
    {
        j[m.joint_name] = {
            {"type", "number"},
            {"default", 0.0},
            {"min", -m.max_vel},
            {"max", m.max_vel},
            {"unit", "rad/s"}
        };
    }
    return j.dump();
}

void JointVelPlugin::from_config(const YAML::Node& node)
{
    if (node["topic"])
        topic_ = node["topic"].as<std::string>();

    if (!node["joints"]) return;

    for (const auto& jn : node["joints"])
    {
        JointMapping m;
        m.joint_name = jn["name"].as<std::string>("");
        m.twist_axis = jn["axis"].as<std::string>("linear_x");
        m.max_vel    = jn["max_vel"].as<double>(0.1);
        if (!m.joint_name.empty())
            joints_.push_back(m);
    }
}

void JointVelPlugin::handle_input(const std::string& json_input)
{
    auto j = nlohmann::json::parse(json_input, nullptr, false);
    if (j.is_discarded()) return;

    for (auto& mapping : joints_)
    {
        double raw_vel = 0.0;
        
        // 1. Проверяем, есть ли значение по имени джоинта
        if (j.contains(mapping.joint_name)) {
            raw_vel = j[mapping.joint_name].get<double>();
        } else {
            // 2. Если нет, пробуем старый формат Twist
            raw_vel = extract_twist_field(j, mapping.twist_axis);
        }

        // Clamping по max_vel
        if (raw_vel > mapping.max_vel)       raw_vel = mapping.max_vel;
        else if (raw_vel < -mapping.max_vel) raw_vel = -mapping.max_vel;
        mapping.target_vel = raw_vel;
    }
}

void JointVelPlugin::update(double dt, Agent& agent)
{
    if (!agent.kinematic_tree) return;

    for (auto& mapping : joints_)
    {
        if (mapping.target_vel == 0.0) continue;

        // Находим текущее значение джоинта
        double current_val = 0.0;
        for (const auto& link : agent.kinematic_tree->links())
        {
            if (link.name == mapping.joint_name)
            {
                current_val = link.joint.value;
                break;
            }
        }

        double new_val = current_val + mapping.target_vel * dt;
        // Clamping происходит внутри KinematicTree::set_joint_value
        agent.kinematic_tree->set_joint_value(mapping.joint_name, new_val);
    }
}

std::string JointVelPlugin::to_json() const
{
    nlohmann::json j;
    j["plugin"] = "joint_vel";
    nlohmann::json joints_arr = nlohmann::json::array();
    for (const auto& m : joints_)
    {
        joints_arr.push_back({{"name", m.joint_name}, {"target_vel", m.target_vel}});
    }
    j["joints"] = joints_arr;
    return j.dump();
}

} // namespace plugins
} // namespace s2
