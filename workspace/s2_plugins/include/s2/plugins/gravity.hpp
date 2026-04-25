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
    PluginRole  role() const override  { return PluginRole::ACTUATION; }

    void from_config(const YAML::Node& node) override
    {
        gravity_accel_  = node["gravity_accel"].as<double>(9.81);
        max_fall_speed_ = node["max_fall_speed"].as<double>(20.0);
        epsilon_        = node["grounded_epsilon"].as<double>(0.02);
        friction_coef_  = node["friction_coef"].as<double>(0.0);
    }

    void set_collision_system(const CollisionSystem* cs) override
    {
        collision_ = cs;
    }

    void update(double dt, Agent& agent, const PluginContext& /*ctx*/) override
    {
        if (!collision_) return;

        auto support = collision_->find_support_surface(
            agent.world_pose.position(), agent.bounding.radius);

        const double ground_z = support
            ? support->ground_z + agent.bounding.radius
            : -std::numeric_limits<double>::infinity();

        const bool grounded = support &&
            (agent.world_pose.z <= ground_z + epsilon_);

        if (grounded)
        {
            fall_velocity_ = 0.0;
            agent.world_pose.z = ground_z;

            // Скольжение по склону
            const Vec3& n = support->normal;
            Vec3 g_vec{0.0, 0.0, -gravity_accel_};
            double g_dot_n = g_vec.dot(n);
            Vec3 g_tangential = g_vec - g_dot_n * n;
            double g_t_mag = g_tangential.norm();

            if (g_t_mag > 1e-6 && friction_coef_ < 1.0 - 1e-9)
            {
                // (1 - friction) масштабирует гравитационный эффект
                slide_velocity_ += g_tangential * (1.0 - friction_coef_) * dt;

                // Скорость привода (DiffDrive задаёт x,y в body frame до GravityPlugin)
                double drive_speed = std::sqrt(
                    agent.world_velocity.linear.x() * agent.world_velocity.linear.x() +
                    agent.world_velocity.linear.y() * agent.world_velocity.linear.y());

                double slide_speed = slide_velocity_.norm();
                if (drive_speed > 1e-6)
                {
                    // При движении: скольжение ограничено процентом от скорости привода.
                    // Максимум slide = drive * (1-friction) => минимум net = drive * friction.
                    // Робот ВСЕГДА поднимается если friction > 0.
                    double max_slide = drive_speed * (1.0 - friction_coef_);
                    if (slide_speed > max_slide)
                        slide_velocity_ *= max_slide / slide_speed;
                }
                else
                {
                    // Стоя: скольжение до max_fall_speed
                    if (slide_speed > max_fall_speed_)
                        slide_velocity_ *= max_fall_speed_ / slide_speed;
                }
            }
            else
            {
                // Плоский пол или friction=1: сбрасываем скольжение
                slide_velocity_ = Vec3::Zero();
            }

            // Добавить slide к velocity в body frame (поверх DiffDrive)
            if (slide_velocity_.squaredNorm() > 1e-12)
            {
                Eigen::Matrix3d R =
                    CollisionSystem::rotation_from_pose(agent.world_pose);
                Vec3 slide_body = R.transpose() * slide_velocity_;
                agent.world_velocity.linear.x() += slide_body.x();
                agent.world_velocity.linear.y() += slide_body.y();
            }
        }
        else
        {
            fall_velocity_ -= gravity_accel_ * dt;
            fall_velocity_ = std::max(fall_velocity_, -max_fall_speed_);
            agent.world_pose.z += fall_velocity_ * dt;

            slide_velocity_ = Vec3::Zero();

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
        j["grounded"]       = (fall_velocity_ == 0.0);
        j["fall_velocity"]  = fall_velocity_;
        j["slide_speed"]    = slide_velocity_.norm();
        j["friction_coef"]  = friction_coef_;
        return j.dump();
    }

    std::string display_label() const override { return "Gravity"; }

    nlohmann::json config_schema() const override
    {
        return nlohmann::json::array({
            {{"key","gravity_accel"},    {"label","Gravity accel (m/s2)"},  {"type","number"}, {"default",9.81}},
            {{"key","max_fall_speed"},   {"label","Max fall speed (m/s)"},  {"type","number"}, {"default",20.0}},
            {{"key","grounded_epsilon"}, {"label","Grounded epsilon (m)"},  {"type","number"}, {"default",0.02}},
            {{"key","friction_coef"},    {"label","Friction coefficient"},   {"type","number"}, {"default",0.0}}
        });
    }

private:
    double gravity_accel_  = 9.81;
    double max_fall_speed_ = 20.0;
    double epsilon_        = 0.02;
    double friction_coef_  = 0.0;   ///< коэффициент трения (0=лёд, 1=полное сцепление)
    double fall_velocity_  = 0.0;   ///< текущая вертикальная скорость (< 0 = вниз)
    Vec3   slide_velocity_ = Vec3::Zero(); ///< скорость скольжения (мировые координаты)

    const CollisionSystem* collision_ = nullptr;
};

} // namespace s2::plugins
