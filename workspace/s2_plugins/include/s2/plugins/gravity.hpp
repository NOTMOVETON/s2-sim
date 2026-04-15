#pragma once

#include <s2/plugin_base.hpp>
#include <s2/collision_system.hpp>
#include <nlohmann/json.hpp>

#include <cmath>
#include <limits>
#include <memory>

namespace s2::plugins
{

/**
 * @brief Плагин гравитации. Тип: Resource.
 *
 * Симулирует вертикальное падение агента при отсутствии опорной поверхности.
 * Каждый тик запрашивает CollisionSystem::find_support_surface() и управляет
 * Z-координатой агента напрямую.
 *
 * Фаза: 3e (до кинематики 3f). GravityPlugin меняет pose.z и обнуляет
 * world_velocity.linear.z(), чтобы 3f не применял z-скорость повторно.
 *
 * Конфиг YAML:
 *   type: gravity
 *   gravity_accel: 9.81    # ускорение, м/с² (по умолчанию 9.81)
 *   max_fall_speed: 20.0   # максимальная скорость падения, м/с
 *   grounded_epsilon: 0.02 # допуск "на поверхности", м
 */
class GravityPlugin : public IAgentPlugin
{
public:
    std::string type() const override { return "gravity"; }

    void from_config(const YAML::Node& node) override
    {
        gravity_accel_  = node["gravity_accel"].as<double>(9.81);
        max_fall_speed_ = node["max_fall_speed"].as<double>(20.0);
        epsilon_        = node["grounded_epsilon"].as<double>(0.02);
    }

    void set_collision_system(const CollisionSystem* cs) override
    {
        collision_ = cs;
    }

    void update(double dt, Agent& agent) override
    {
        if (!collision_) return;

        auto support = collision_->find_support_surface(
            agent.world_pose.position(), agent.bounding.radius);

        const double ground_z = support
            ? *support + agent.bounding.radius
            : -std::numeric_limits<double>::infinity();

        const bool grounded = support &&
            (agent.world_pose.z <= ground_z + epsilon_);

        if (grounded)
        {
            fall_velocity_ = 0.0;
            agent.world_pose.z = ground_z;
        }
        else
        {
            fall_velocity_ -= gravity_accel_ * dt;
            fall_velocity_ = std::max(fall_velocity_, -max_fall_speed_);
            agent.world_pose.z += fall_velocity_ * dt;

            // Защита от проваливания сквозь поверхность при большом dt
            if (support && agent.world_pose.z < ground_z)
            {
                agent.world_pose.z = ground_z;
                fall_velocity_ = 0.0;
            }
        }

        // Запрещаем фазе 3f дополнительно изменять Z
        agent.world_velocity.linear.z() = 0.0;
    }

    std::string to_json() const override
    {
        nlohmann::json j;
        j["grounded"]      = (fall_velocity_ == 0.0);
        j["fall_velocity"] = fall_velocity_;
        return j.dump();
    }

    std::string display_label() const override { return "Gravity"; }

    std::string config_schema() const override
    {
        return R"JSON([
            {"key":"gravity_accel",    "label":"Gravity accel, m/s2", "type":"number", "default":9.81},
            {"key":"max_fall_speed",   "label":"Max fall speed, m/s", "type":"number", "default":20.0},
            {"key":"grounded_epsilon", "label":"Grounded epsilon, m", "type":"number", "default":0.02}
        ])JSON";
    }

private:
    double gravity_accel_  = 9.81;
    double max_fall_speed_ = 20.0;
    double epsilon_        = 0.02;
    double fall_velocity_  = 0.0;  ///< текущая вертикальная скорость (< 0 = вниз)

    const CollisionSystem* collision_ = nullptr;
};

} // namespace s2::plugins
