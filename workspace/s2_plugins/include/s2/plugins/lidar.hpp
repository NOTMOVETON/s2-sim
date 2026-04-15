#pragma once

/**
 * @file lidar.hpp
 * Плагин 2D лидара.
 *
 * Бросает N лучей в горизонтальной плоскости, использует RaycastEngine
 * для определения расстояний до статической геометрии и агентов с коллизией.
 *
 * Конфиг YAML:
 *   - type: lidar
 *     name: front_lidar       # sensor_name; определяет имя топика LaserScan
 *     num_rays: 360
 *     min_range: 0.1          # минимальная дальность [м]
 *     max_range: 10.0         # максимальная дальность [м]
 *     start_angle: -3.14159   # начальный угол скана [рад]; 0 = вперёд по X
 *     end_angle:   3.14159    # конечный угол скана [рад]
 *     mount_link: ""          # имя линка URDF; пусто = base_link агента
 *     publish_rate_hz: 10.0   # устанавливается SceneLoader через set_base_rate()
 */

#include <s2/plugins/plugin_base.hpp>
#include <s2/raycast_engine.hpp>
#include <s2/sensor_data.hpp>
#include <s2/agent.hpp>

#include <cmath>
#include <string>
#include <vector>

namespace s2
{
namespace plugins
{

class LidarPlugin : public IAgentPlugin
{
public:
    std::string type() const override { return "lidar"; }
    double default_publish_rate_hz() const override { return 10.0; }
    std::string display_label() const override { return "Lidar"; }

    // ─── Конфигурация ────────────────────────────────────────────────────────

    void from_config(const YAML::Node& node) override
    {
        if (node["num_rays"])    num_rays_    = node["num_rays"].as<int>(360);
        if (node["min_range"])   min_range_   = node["min_range"].as<double>(0.1);
        if (node["max_range"])   max_range_   = node["max_range"].as<double>(10.0);
        if (node["start_angle"]) start_angle_ = node["start_angle"].as<double>(-M_PI);
        if (node["end_angle"])   end_angle_   = node["end_angle"].as<double>(M_PI);
        if (node["mount_link"])  mount_link_  = node["mount_link"].as<std::string>("");
        if (node["viz_color"])   viz_color_   = node["viz_color"].as<std::string>("#00FFFF");
    }

    std::string config_schema() const override
    {
        return "["
            "{\"key\":\"num_rays\",     \"label\":\"Количество лучей\",  \"type\":\"number\",\"default\":360},"
            "{\"key\":\"min_range\",    \"label\":\"Мин. дальность (м)\",\"type\":\"number\",\"default\":0.1},"
            "{\"key\":\"max_range\",    \"label\":\"Макс. дальность (м)\",\"type\":\"number\",\"default\":10.0},"
            "{\"key\":\"start_angle\",  \"label\":\"Нач. угол (рад)\",    \"type\":\"number\",\"default\":-3.14159},"
            "{\"key\":\"end_angle\",    \"label\":\"Кон. угол (рад)\",    \"type\":\"number\",\"default\":3.14159},"
            "{\"key\":\"mount_link\",   \"label\":\"Монтажный линк\",     \"type\":\"text\",  \"default\":\"\"},"
            "{\"key\":\"viz_color\",    \"label\":\"Цвет лучей\",         \"type\":\"color\", \"default\":\"#00FFFF\"},"
            "{\"key\":\"publish_rate_hz\",\"label\":\"Частота (Гц)\",     \"type\":\"number\",\"default\":10}"
            "]";
    }

    // ─── Управляющая кнопка (паттерн TrajectoryRecorderPlugin) ───────────────

    bool has_inputs() const override { return true; }

    std::string inputs_schema() const override
    {
        return R"({"visible": {"type": "boolean", "default": false, "label": "Показывать лучи"}})";
    }

    void handle_input(const std::string& json_input) override
    {
        try {
            YAML::Node data = YAML::Load(json_input);
            if (data["visible"]) visible_ = data["visible"].as<bool>();
        } catch (const std::exception&) {}
    }

    // ─── Системные зависимости ────────────────────────────────────────────────

    void set_raycast_engine(const RaycastEngine* e) override { raycast_ = e; }

    /**
     * @brief TF-фрейм лидара для ROS2-заголовка LaserScan.
     * Если задан mount_link — возвращает его. Иначе возвращает "base_link"
     * (лидар смонтирован на базовом фрейме агента).
     */
    std::string sensor_frame_id() const override
    {
        return mount_link_.empty() ? "base_link" : mount_link_;
    }

    // ─── Основной цикл ────────────────────────────────────────────────────────

    void update(double dt, Agent& agent) override
    {
        if (!raycast_) return;

        // Управляем таймером публикации (паттерн GnssPlugin)
        publish_timer_ += dt;
        double rate = publish_rate_hz();
        double interval = (rate > 0.0) ? (1.0 / rate) : 0.0;
        if (interval > 0.0 && publish_timer_ < interval - 1e-9) return;
        publish_timer_ -= interval;

        // Определить позу монтажной точки
        Pose3D mount_pose = agent.world_pose;
        if (!mount_link_.empty() && agent.kinematic_tree != nullptr) {
            mount_pose = agent.kinematic_tree->compute_world_pose(mount_link_, agent.world_pose);
        }

        // Сформировать лучи
        const int n = num_rays_;
        if (n <= 0) return;

        const double angle_step = (n > 1)
            ? (end_angle_ - start_angle_) / static_cast<double>(n)
            : 0.0;

        // Матрица вращения R = Rz(yaw)*Ry(pitch)*Rx(roll) для трансформации
        // локального направления луча (cos a, sin a, 0) в мировые координаты.
        // При pitch=roll=0 результат совпадает с прежним поведением.
        const double cy = std::cos(mount_pose.yaw),   sy = std::sin(mount_pose.yaw);
        const double cp = std::cos(mount_pose.pitch), sp = std::sin(mount_pose.pitch);
        const double cr = std::cos(mount_pose.roll),  sr = std::sin(mount_pose.roll);

        // Первые два столбца R (умножаются на lx=cos(a) и ly=sin(a) соответственно)
        const double r00 = cy * cp,              r01 = cy * sp * sr - sy * cr;
        const double r10 = sy * cp,              r11 = sy * sp * sr + cy * cr;
        const double r20 = -sp,                  r21 = cp * sr;

        std::vector<Ray> rays;
        rays.reserve(n);
        for (int i = 0; i < n; ++i) {
            const double a  = start_angle_ + i * angle_step; // угол в системе сенсора
            const double lx = std::cos(a);
            const double ly = std::sin(a);
            Ray ray;
            ray.origin    = Vec3{mount_pose.x, mount_pose.y, mount_pose.z};
            ray.direction = Vec3{r00 * lx + r01 * ly,
                                 r10 * lx + r11 * ly,
                                 r20 * lx + r21 * ly};
            ray.max_range = max_range_;
            rays.push_back(ray);
        }

        // Бросить лучи
        const auto results = raycast_->cast_batch(rays);

        // Заполнить LidarScanData
        LidarScanData data;
        data.seq            = ++seq_;
        data.angle_min      = static_cast<float>(start_angle_);
        data.angle_max      = static_cast<float>(end_angle_);
        data.angle_increment = static_cast<float>(angle_step);
        data.time_increment = 0.0f;
        data.scan_time      = (rate > 0.0f) ? static_cast<float>(1.0 / rate) : 0.1f;
        data.range_min      = static_cast<float>(min_range_);
        data.range_max      = static_cast<float>(max_range_);
        data.ranges.resize(n);

        scan_points_.clear();

        for (int i = 0; i < n; ++i) {
            if (results[i].hit && results[i].distance >= min_range_) {
                data.ranges[i] = static_cast<float>(results[i].distance);
                if (visible_) {
                    scan_points_.push_back(results[i].point);
                }
            } else {
                data.ranges[i] = static_cast<float>(max_range_);
            }
        }

        agent.state.emplace<LidarScanData>(data);
    }

    // ─── Визуализация ────────────────────────────────────────────────────────

    std::string to_json() const override
    {
        std::string json = "{\"type\":\"lidar_points\",\"visible\":";
        json += visible_ ? "true" : "false";
        json += ",\"color\":\"" + viz_color_ + "\",\"points\":[";
        if (visible_) {
            for (std::size_t i = 0; i < scan_points_.size(); ++i) {
                if (i > 0) json += ",";
                json += "[" + std::to_string(scan_points_[i].x()) + ","
                            + std::to_string(scan_points_[i].y()) + ","
                            + std::to_string(scan_points_[i].z()) + "]";
            }
        }
        json += "]}";
        return json;
    }

private:
    // Параметры конфига
    int         num_rays_    = 360;
    double      min_range_   = 0.1;
    double      max_range_   = 10.0;
    double      start_angle_ = -M_PI;
    double      end_angle_   =  M_PI;
    std::string mount_link_;
    std::string viz_color_   = "#00FFFF";

    // Состояние плагина
    bool        visible_      = false;  ///< управляется кнопкой из UI
    double      publish_timer_{0.0};
    uint64_t    seq_{0};

    // Точки попаданий для визуализатора (обновляются при visible_=true)
    std::vector<Vec3> scan_points_;

    // Системная зависимость (устанавливается SimEngine перед update)
    const RaycastEngine* raycast_ = nullptr;
};

} // namespace plugins
} // namespace s2
