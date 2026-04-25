#pragma once

/**
 * @file battery.hpp
 * Плагин батареи агента.
 *
 * Отвечает за:
 *  - Инициализацию BatteryComponent при загрузке сцены (initialize())
 *  - Публикацию состояния батареи в SharedState (BatteryData) для транспортного слоя
 *  - Заполнение AgentSnapshot.extra (battery_level, battery_charging) для визуализатора
 *  - Публикацию sensor_msgs/BatteryState в ROS2
 *
 * Зарядка управляется ChargingEffect (зональный эффект): он изменяет BatteryComponent.
 * BatteryPlugin только читает BatteryComponent и транслирует его в нужные форматы.
 *
 * Конфигурация YAML (все поля опциональны):
 *   - type: battery
 *     initial_level: 1.0          # Начальный заряд [0.0, 1.0]
 *     nominal_voltage: 24.0       # Номинальное напряжение [В]
 *     capacity_ah: 10.0           # Ёмкость (последняя полная) [Ач]
 *     design_capacity_ah: 10.0    # Расчётная ёмкость [Ач]
 *     technology: lion            # nimh | lion | lipo | life | nicd | limn
 *     location: ""                # Слот/разъём
 *     serial_number: ""           # Серийный номер
 *     publish_rate_hz: 1.0        # Частота публикации ROS2
 *     topic: ""                   # Переопределить имя топика (по умолчанию /battery_state)
 */

#include <s2/agent.hpp>
#include <s2/sensor_data.hpp>
#include <s2/components/battery_component.hpp>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <string>

namespace s2
{
namespace plugins
{

class BatteryPlugin : public IAgentPlugin
{
public:
    BatteryPlugin() = default;

    std::string type() const override { return "battery"; }
    PluginRole  role() const override  { return PluginRole::RESOURCE; }
    double default_publish_rate_hz() const override { return publish_rate_hz_; }
    std::string display_label() const override { return "Battery"; }

    // ─── Инициализация ───────────────────────────────────────────────────────

    /**
     * @brief Инициализировать BatteryComponent при загрузке агента.
     *
     * Если компонент уже существует (например, загружен из сохранения) —
     * сохраняем существующий уровень, только обновляем параметры.
     * Если компонента нет — создаём с initial_level_.
     */
    void initialize(Agent& agent) override
    {
        auto* existing = agent.state.get<BatteryComponent>();
        if (existing) {
            // Компонент уже есть — сохраняем уровень, параметры обновятся из конфига
        } else {
            agent.state.emplace<BatteryComponent>(
                BatteryComponent{initial_level_, false});
        }
    }

    // ─── Lifecycle ───────────────────────────────────────────────────────────

    /**
     * @brief Сбросить заряд батареи до начального значения (D-13).
     *
     * Вызывается SimEngine::reset() — агент должен начать следующую симуляцию
     * с тем же уровнем заряда, с которым был загружен из сцены.
     */
    void on_reset(Agent& agent) override
    {
        // Восстановить заряд батареи до начального значения
        auto* bat = agent.state.get<BatteryComponent>();
        if (bat) {
            bat->level    = initial_level_;
            bat->charging = false;
        }
    }

    // ─── Ранняя фаза тика (до resolve) ───────────────────────────────────────

    /**
     * @brief Разряд батареи + публикация contributions в SharedState.
     *
     * Вызывается SimEngine в фазе 3a — до resolve() — чтобы DiffDrivePlugin
     * уже в этом тике получил корректный speed_scale и motion_locked.
     *
     * Поведение:
     *  - Разряд: level -= drain_rate_ * dt, когда не заряжается.
     *    Зарядка управляется ChargingEffect (зональный эффект),
     *    который изменяет BatteryComponent.charging и level.
     *  - level ∈ (0.05, 0.20): линейное замедление.
     *    scale = (level - 0.05) / 0.15, от 0 до 1.
     *    При level = 0.20: scale = 1.0 (без ограничений).
     *    При level = 0.05 + ε: scale ≈ 0.
     *  - level ≤ 0.05: motion_lock (движение заблокировано).
     */
    void pre_resolve(double dt, Agent& agent) override
    {
        auto* bat = agent.state.get<BatteryComponent>();
        if (!bat) return;

        // Разряд — только когда не заряжается
        if (!bat->charging) {
            bat->level = std::max(0.0, bat->level - drain_rate_ * dt);
        }

        // Contributions в SharedState на основе текущего уровня
        if (bat->level <= critical_level_) {
            // Критический уровень — полная блокировка движения
            agent.state.add_lock(true, "battery_critical");
        } else if (bat->level < low_level_) {
            // Низкий уровень — линейное замедление [0, 1]
            double scale = (bat->level - critical_level_) /
                           (low_level_ - critical_level_);
            agent.state.add_scale(scale, "battery_low");
        }
        // level >= low_level_: нет ограничений, contribution не добавляем
    }

    // ─── Основной тик ────────────────────────────────────────────────────────

    void update(double dt, Agent& agent, const PluginContext& /*ctx*/) override
    {
        const auto* bat = agent.state.get<BatteryComponent>();
        if (!bat) return;

        // Кешируем для to_json() и contribute_snapshot()
        cached_level_    = bat->level;
        cached_charging_ = bat->charging;

        // Управляем таймером публикации
        publish_timer_ += dt;
        double interval = (publish_rate_hz_ > 0.0) ? (1.0 / publish_rate_hz_) : 0.0;
        if (interval > 0.0 && publish_timer_ < interval - 1e-9)
            return;
        publish_timer_ -= interval;

        // Пишем BatteryData в SharedState — транспортный мост заберёт это в on_post_tick()
        BatteryData data;
        data.seq               = ++seq_;
        data.level             = bat->level;
        data.charging          = bat->charging;
        data.nominal_voltage   = nominal_voltage_;
        data.capacity_ah       = capacity_ah_;
        data.design_capacity_ah = design_capacity_ah_;
        data.technology        = technology_;
        data.location          = location_;
        data.serial_number     = serial_number_;

        agent.state.emplace<BatteryData>(data);
    }

    // ─── Снапшот ─────────────────────────────────────────────────────────────

    /**
     * @brief Добавить battery_level и battery_charging в AgentSnapshot.extra.
     *
     * Вызывается SimEngine::build_snapshot() каждый тик.
     */
    void contribute_snapshot(nlohmann::json& out, const Agent& agent) const override
    {
        const auto* bat = agent.state.get<BatteryComponent>();
        if (bat) {
            out["battery_level"]    = bat->level;
            out["battery_charging"] = bat->charging;
        } else {
            out["battery_level"]    = -1.0;  // нет компонента
            out["battery_charging"] = false;
        }
    }

    // ─── Сериализация ────────────────────────────────────────────────────────

    std::string to_json() const override
    {
        nlohmann::json j;
        j["plugin"]          = "battery";
        j["level"]           = cached_level_;
        j["charging"]        = cached_charging_;
        j["nominal_voltage"] = nominal_voltage_;
        j["capacity_ah"]     = capacity_ah_;
        j["drain_rate"]      = drain_rate_;
        j["low_level"]       = low_level_;
        j["critical_level"]  = critical_level_;
        return j.dump();
    }

    // ─── Конфигурация ────────────────────────────────────────────────────────

    void from_config(const YAML::Node& node) override
    {
        if (node["initial_level"])
            initial_level_ = std::clamp(node["initial_level"].as<double>(), 0.0, 1.0);
        if (node["nominal_voltage"])
            nominal_voltage_     = node["nominal_voltage"].as<double>();
        if (node["capacity_ah"])
            capacity_ah_         = node["capacity_ah"].as<double>();
        if (node["design_capacity_ah"])
            design_capacity_ah_  = node["design_capacity_ah"].as<double>();
        if (node["location"])
            location_            = node["location"].as<std::string>();
        if (node["serial_number"])
            serial_number_       = node["serial_number"].as<std::string>();
        if (node["publish_rate_hz"])
            publish_rate_hz_     = node["publish_rate_hz"].as<double>();
        if (node["technology"])
            technology_          = parse_technology(node["technology"].as<std::string>());
        if (node["drain_rate"])
            drain_rate_          = node["drain_rate"].as<double>();
        if (node["low_level"])
            low_level_           = node["low_level"].as<double>();
        if (node["critical_level"])
            critical_level_      = node["critical_level"].as<double>();
    }

    nlohmann::json config_schema() const override
    {
        return nlohmann::json::array({
            {{"key","initial_level"},     {"label","Initial level [0-1]"}, {"type","number"}, {"default",1.0}},
            {{"key","nominal_voltage"},    {"label","Nominal voltage (V)"},  {"type","number"}, {"default",24.0}},
            {{"key","capacity_ah"},        {"label","Capacity (Ah)"},        {"type","number"}, {"default",10.0}},
            {{"key","design_capacity_ah"}, {"label","Design capacity (Ah)"}, {"type","number"}, {"default",10.0}},
            {{"key","publish_rate_hz"},    {"label","Publish rate (Hz)"},    {"type","number"}, {"default",1.0}}
        });
    }

private:
    // Параметры батареи
    double  initial_level_{1.0};
    double  nominal_voltage_{24.0};
    double  capacity_ah_{10.0};
    double  design_capacity_ah_{10.0};
    uint8_t technology_{2};           ///< POWER_SUPPLY_TECHNOLOGY_LION = 2
    std::string location_;
    std::string serial_number_;

    // Разряд и ограничения скорости
    double drain_rate_{0.01};         ///< Скорость разряда [уровень/с], по умолчанию 1%/с
    double low_level_{0.20};          ///< Ниже этого уровня — начинается замедление
    double critical_level_{0.05};     ///< На этом уровне и ниже — полная блокировка

    // Публикация
    double   publish_rate_hz_{1.0};
    double   publish_timer_{0.0};
    uint64_t seq_{0};

    // Кеш для to_json() (обновляется каждый тик)
    double cached_level_{1.0};
    bool   cached_charging_{false};

    // ─── Вспомогательный метод ───────────────────────────────────────────────

    /// Конвертировать строку технологии в константу POWER_SUPPLY_TECHNOLOGY_*
    static uint8_t parse_technology(const std::string& s)
    {
        if (s == "nimh") return 1;
        if (s == "lion") return 2;
        if (s == "lipo") return 3;
        if (s == "life") return 4;
        if (s == "nicd") return 5;
        if (s == "limn") return 6;
        return 0;  // unknown
    }
};

} // namespace plugins
} // namespace s2
