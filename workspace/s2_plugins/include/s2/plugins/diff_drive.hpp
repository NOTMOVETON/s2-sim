#pragma once

/**
 * @file diff_drive.hpp
 * Плагин кинематики дифференциального привода.
 *
 * Поддерживает внешние команды через handle_input():
 *   {"linear_velocity": 0.5, "angular_velocity": 0.3}
 *
 * External input сохраняется (latch): последняя команда действует до получения новой.
 * Стандартное поведение для ROS2 cmd_vel. Для остановки отправить {linear: 0, angular: 0}.
 */

#include <s2/agent.hpp>
#include <s2/sensor_data.hpp>
#include <yaml-cpp/yaml.h>

#include <cmath>
#include <algorithm>

namespace s2
{
namespace plugins
{

/**
 * @brief Плагин дифференциального привода.
 */
class DiffDrivePlugin : public IAgentPlugin
{
public:
    DiffDrivePlugin() = default;

    std::string type() const override { return "diff_drive"; }

    void update(double /*dt*/, Agent& agent) override
    {
        // Читаем desired из SharedState
        auto* dd = agent.state.get<DiffDriveData>();
        double desired_linear = dd ? dd->desired_linear : 0.0;
        double desired_angular = dd ? dd->desired_angular : 0.0;

        // Запоминаем состояние external ДО сброса
        bool is_external = has_external_input_;
        double clamped_linear;
        double clamped_angular;

        if (is_external) {
            clamped_linear = external_linear_velocity_;
            clamped_angular = external_angular_velocity_;
            // Не сбрасываем has_external_input_ — команда сохраняется до следующей
            // (latch-поведение, стандарт для ROS2 cmd_vel)
        } else {
            clamped_linear = std::clamp(desired_linear, -max_linear_, max_linear_);
            clamped_angular = std::clamp(desired_angular, -max_angular_, max_angular_);
        }

        agent.world_velocity.linear = Vec3{clamped_linear, 0.0, 0.0};
        agent.world_velocity.angular = Vec3{0.0, 0.0, clamped_angular};

        // Обновляем данные для визуализатора.
        // ВАЖНО: при external input НЕ записываем external velocity в desired_linear,
        // иначе на следующий тик desired_linear из SharedState будет равен external
        // и робот будет двигаться бесконечно.
        if (!is_external) {
            current_data.desired_linear = desired_linear;
            current_data.desired_angular = desired_angular;
        }
        current_data.max_linear = max_linear_;
        current_data.max_angular = max_angular_;
        current_data.seq = ++seq_;
        agent.state.emplace<DiffDriveData>(current_data);
    }

    void from_config(const YAML::Node& node) override
    {
        if (node["max_linear"]) max_linear_ = node["max_linear"].as<double>();
        if (node["max_angular"]) max_angular_ = node["max_angular"].as<double>();
    }

    std::string to_json() const override
    {
        return "{\"plugin\":\"diff_drive\","
               "\"max_linear\":" + std::to_string(max_linear_) + ","
               "\"max_angular\":" + std::to_string(max_angular_) + "}";
    }

    // ─── Методы входа (input handling) ───

    bool has_inputs() const override { return true; }

    // ─── Транспортные топики ───

    /**
     * @brief DiffDrive принимает команды через /cmd_vel (Twist).
     */
    std::vector<std::string> command_topics() const override
    {
        return {"/cmd_vel"};
    }

    std::string inputs_schema() const override
    {
        return R"({
            "linear_velocity": {"type": "number", "default": 0.0, "min": -2.0, "max": 2.0, "unit": "m/s"},
            "angular_velocity": {"type": "number", "default": 0.0, "min": -1.5, "max": 1.5, "unit": "rad/s"}
        })";
    }

    void handle_input(const std::string& json_input) override
    {
        try {
            YAML::Node data = YAML::Load(json_input);
            if (data["linear_velocity"]) {
                external_linear_velocity_ = data["linear_velocity"].as<double>();
                external_linear_velocity_ = std::clamp(external_linear_velocity_, -max_linear_, max_linear_);
                has_external_input_ = true;
            }
            if (data["angular_velocity"]) {
                external_angular_velocity_ = data["angular_velocity"].as<double>();
                external_angular_velocity_ = std::clamp(external_angular_velocity_, -max_angular_, max_angular_);
                has_external_input_ = true;
            }
        } catch (const std::exception&) {
            // Игнорируем некорректный ввод
        }
    }

private:
    double max_linear_{2.0};
    double max_angular_{1.5};
    DiffDriveData current_data;

    uint64_t seq_{0};

    // Внешние команды (устанавливаются через handle_input)
    bool has_external_input_{false};
    double external_linear_velocity_{0.0};
    double external_angular_velocity_{0.0};
};

} // namespace plugins
} // namespace s2