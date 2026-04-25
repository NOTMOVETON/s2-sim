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
#include <s2/components/tire_puncture_data.hpp>
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
    PluginRole  role() const override  { return PluginRole::ACTUATION; }

    void update(double dt, Agent& agent, const PluginContext& /*ctx*/) override
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

        // Читаем effective constraints из SharedState.
        // resolve() вызывается до update() — effective() уже содержит зонные эффекты.
        const auto& eff = agent.state.effective();

        // Если движение заблокировано (motion_lock, e-stop) — немедленно останавливаем
        if (eff.motion_locked) {
            agent.world_velocity.linear = Vec3::Zero();
            agent.world_velocity.angular = Vec3::Zero();
            return;
        }

        // Учёт проколотых шин: снижение скорости и детерминированный drift
        const auto* tire_data = agent.state.get<TirePunctureData>();
        if (tire_data && tire_data->punctured) {
            clamped_linear *= 0.5;
            time_acc_ += dt;
            clamped_angular += 0.05 * std::sin(time_acc_ * 15.0);
        }

        // Применить speed_scale (лёд замедляет, boost ускоряет).
        // Масштабируем обе компоненты: трение влияет и на линейное, и на угловое движение.
        // После умножения повторно ограничиваем аппаратным лимитом.
        clamped_linear  *= eff.speed_scale;
        clamped_angular *= eff.speed_scale;
        clamped_linear  = std::clamp(clamped_linear,  -max_linear_,  max_linear_);
        clamped_angular = std::clamp(clamped_angular, -max_angular_, max_angular_);

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

    std::string display_label() const override { return "DiffDrive (привод)"; }

    nlohmann::json config_schema() const override
    {
        return nlohmann::json::array({
            {{"key","wheel_base"},      {"label","Wheel base (m)"},       {"type","number"},{"default",0.4}},
            {{"key","max_linear_vel"},  {"label","Max linear vel (m/s)"},  {"type","number"},{"default",1.5}},
            {{"key","max_angular_vel"}, {"label","Max angular vel (rad/s)"},{"type","number"},{"default",2.0}}
        });
    }

    void from_config(const YAML::Node& node) override
    {
        // Принимаем оба варианта имён для совместимости с разными версиями конфигов
        if      (node["max_linear_vel"]) max_linear_  = node["max_linear_vel"].as<double>();
        else if (node["max_linear"])     max_linear_  = node["max_linear"].as<double>();

        if      (node["max_angular_vel"]) max_angular_ = node["max_angular_vel"].as<double>();
        else if (node["max_angular"])     max_angular_ = node["max_angular"].as<double>();
    }

    std::string to_json() const override
    {
        return "{\"plugin\":\"diff_drive\","
               "\"max_linear\":" + std::to_string(max_linear_) + ","
               "\"max_angular\":" + std::to_string(max_angular_) + "}";
    }

    // ─── Методы входа (input handling) ───

    void on_reset(Agent& agent) override
    {
      (void)agent;
      // Сброс external команд — агент не должен продолжать движение после reset (D-13)
      external_linear_velocity_  = 0.0;
      external_angular_velocity_ = 0.0;
      has_external_input_        = false;
      time_acc_                  = 0.0;
    }

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

    // Накопленное время для детерминированного drift при проколах шин
    double time_acc_{0.0};
};

} // namespace plugins
} // namespace s2