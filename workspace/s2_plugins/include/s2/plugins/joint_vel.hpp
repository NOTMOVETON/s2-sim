#pragma once

/**
 * @file joint_vel.hpp
 * Плагин управления джоинтами через скорость (velocity control).
 *
 * Принимает geometry_msgs/Twist из ROS2 топика.
 * Каждый джоинт маппируется на одно из 6 полей Twist:
 *   linear_x, linear_y, linear_z, angular_x, angular_y, angular_z
 *
 * Пример YAML:
 *   - type: joint_vel
 *     topic: /cmd_vel_mount
 *     joints:
 *       - name: arm
 *         axis: linear_x
 *         max_vel: 0.01
 *       - name: bucket
 *         axis: angular_z
 *         max_vel: 0.01
 */

#include <s2/plugins/plugin_base.hpp>
#include <string>
#include <vector>

namespace s2
{
namespace plugins
{

class JointVelPlugin : public IAgentPlugin
{
public:
    std::string type() const override { return "joint_vel"; }
    bool has_inputs() const override { return true; }
    std::string inputs_schema() const override;

    /**
     * @brief Список входных топиков команд.
     * По умолчанию /cmd_vel_mount, переопределяется через поле topic: в YAML.
     */
    std::vector<std::string> command_topics() const override { return {topic_}; }

    /**
     * @brief Читает конфигурацию плагина из YAML.
     * Поля: topic (строка), joints (список {name, axis, max_vel}).
     */
    void from_config(const YAML::Node& node) override;

    /**
     * @brief Обработать входящую команду Twist (JSON).
     * JSON-формат: {"linear":{"x":0,"y":0,"z":0},"angular":{"x":0,"y":0,"z":0}}
     * или плоский: {"linear_x":0,"angular_z":0}
     */
    void handle_input(const std::string& json_input) override;

    /**
     * @brief Обновить значения джоинтов на основе накопленной скорости.
     * new_val = old_val + target_vel * dt, с clamping по [min, max].
     */
    void update(double dt, Agent& agent) override;

    /**
     * @brief Состояние плагина в JSON.
     * Формат: {"plugin":"joint_vel","joints":[{"name":"arm","value":0.3},...]}
     */
    std::string to_json() const override;

private:
    struct JointMapping
    {
        std::string joint_name;   ///< Имя звена в KinematicTree
        std::string twist_axis;   ///< "linear_x" / "linear_y" / ... / "angular_z"
        double max_vel{0.1};      ///< Максимальная скорость (рад/с или м/с)
        double target_vel{0.0};   ///< Текущая целевая скорость (из последней команды)
    };

    std::string topic_{"/cmd_vel_mount"};
    std::vector<JointMapping> joints_;
};

} // namespace plugins
} // namespace s2
