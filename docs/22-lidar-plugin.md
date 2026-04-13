# Задача 22 — Плагин лидара (LidarPlugin)

## Цель

Реализовать плагин `lidar` типа Sensor, который симулирует однослойный 2D лидар.
Плагин бросает N лучей по кругу в горизонтальной плоскости на заданной высоте,
определяет расстояния до препятствий и публикует результат.

Лидар видит:
- Статические примитивы сцены (`static_geometry`)
- Других агентов, у которых задана коллизия (`has_collision = true`)
- НЕ видит самого агента-владельца лидара

## Зависимости

- Требует: задача 14 (примитивы сцены существуют)
- Требует: задача 20 (агенты с коллизиями — для их добавления в raycast-сцену)
- `RaycastEngine` уже реализован в `raycast_engine.hpp`

## Конфиг YAML

```yaml
plugins:
  - type: lidar
    name: front_lidar
    num_rays: 360           # количество лучей (360 = 1 луч на градус)
    min_range: 0.1          # минимальная дальность, метры
    max_range: 10.0         # максимальная дальность, метры
    angle_min: -3.14159     # начальный угол (рад), по умолчанию -π (полный круг)
    angle_max:  3.14159     # конечный угол (рад)
    height_offset: 0.2      # высота сенсора над base_link агента (метры)
    publish_rate_hz: 10.0   # частота публикации
    visualize: true         # отображать лучи в визуализаторе
    viz_color: "#00FFFF"    # цвет точек/лучей в визуализаторе
```

## Архитектура

```
LidarPlugin::update(agent, dt):
    1. Сформировать N лучей в мировых координатах
    2. RaycastEngine::cast_batch(rays)
    3. Собрать дистанции и точки
    4. Сохранить в буфер для публикации и визуализации

LidarPlugin::to_json():
    → plugins_data["agent_0"]["lidar"] = "{...}"
    → VizServer SSE → app.js рендерит точки

LidarPlugin::publish(transport):
    → sensor_msgs/LaserScan (ROS2)
    OR
    → ничего (stub)
```

## Добавление агентов в raycast-сцену

Проблема: `RaycastEngine` работает только по `static_geometry`. Нужно добавить
бounding shapes других агентов как динамическую геометрию.

### Решение: динамический список для raycast

`RaycastEngine` расширяется поддержкой динамических объектов:

```cpp
class RaycastEngine {
public:
    void set_static_geometry(const std::vector<WorldPrimitive>& prims);

    /// Добавить динамические объекты (агенты) для текущего тика.
    /// Вызывается перед cast() каждый тик, очищается после.
    void set_dynamic_agents(const std::vector<WorldPrimitive>& agent_bounds);

private:
    std::vector<WorldPrimitive> static_prims_;
    std::vector<WorldPrimitive> dynamic_prims_;  // текущие агенты
};
```

В `cast()` перебираем и `static_prims_`, и `dynamic_prims_`.

### Построение dynamic_prims в SimEngine

**Файл:** `workspace/s2_core/include/s2/sim_engine.hpp`

Перед фазой `3e` (plugin->update), для каждого агента с лидаром:

```cpp
// Подготовить dynamic bounds для raycast (все агенты с коллизией кроме текущего)
std::vector<WorldPrimitive> agent_bounds;
for (const auto& other : world_.agents()) {
    if (&other == &agent) continue;  // пропустить себя
    if (!other.has_collision) continue;

    WorldPrimitive wp;
    wp.pose = other.world_pose;
    if (other.bounding.type == ShapeType::SPHERE) {
        wp.type = "sphere";
        wp.radius = other.bounding.radius;
    } else if (other.bounding.type == ShapeType::BOX) {
        wp.type = "box";
        wp.size = other.bounding.size * 2.0;  // half-extents → full extents
    }
    agent_bounds.push_back(wp);
}
raycast_engine_.set_dynamic_agents(agent_bounds);
```

## Реализация плагина

**Файл:** `workspace/s2_plugins/include/s2/plugins/lidar.hpp`

```cpp
#pragma once
#include <s2/plugin_base.hpp>
#include <s2/raycast_engine.hpp>
#include <vector>
#include <array>
#include <cmath>

namespace s2::plugins {

class LidarPlugin : public IAgentPlugin {
public:
    std::string type() const override { return "lidar"; }

    void from_config(const YAML::Node& node) override {
        num_rays_         = node["num_rays"].as<int>(360);
        min_range_        = node["min_range"].as<double>(0.1);
        max_range_        = node["max_range"].as<double>(10.0);
        angle_min_        = node["angle_min"].as<double>(-M_PI);
        angle_max_        = node["angle_max"].as<double>( M_PI);
        height_offset_    = node["height_offset"].as<double>(0.2);
        visualize_        = node["visualize"].as<bool>(true);
        viz_color_        = node["viz_color"].as<std::string>("#00FFFF");
        base_rate_hz_     = node["publish_rate_hz"].as<double>(10.0);
    }

    void set_raycast_engine(const RaycastEngine* re) { raycast_ = re; }

    void update(Agent& agent, double dt) override {
        if (!raycast_) return;
        if (!should_publish(dt)) return;

        // Позиция сенсора в мировых координатах
        const double sx = agent.world_pose.x;
        const double sy = agent.world_pose.y;
        const double sz = agent.world_pose.z + height_offset_;
        const double yaw = agent.world_pose.yaw;

        // Сформировать лучи
        std::vector<Ray> rays;
        rays.reserve(num_rays_);

        const double angle_step = (angle_max_ - angle_min_) / num_rays_;
        for (int i = 0; i < num_rays_; ++i) {
            double angle = yaw + angle_min_ + i * angle_step;
            Ray ray;
            ray.origin    = Vec3{sx, sy, sz};
            ray.direction = Vec3{std::cos(angle), std::sin(angle), 0.0};
            ray.max_range = max_range_;
            rays.push_back(ray);
        }

        // Бросить лучи
        auto results = raycast_->cast_batch(rays);

        // Собрать дистанции и точки попаданий
        scan_distances_.resize(num_rays_);
        hit_points_.clear();

        for (int i = 0; i < num_rays_; ++i) {
            if (results[i].hit && results[i].distance >= min_range_) {
                scan_distances_[i] = results[i].distance;
                if (visualize_) {
                    hit_points_.push_back(results[i].point);
                }
            } else {
                scan_distances_[i] = max_range_;  // нет попадания → max_range
            }
        }
    }

    nlohmann::json to_json(const Agent& agent) const override {
        if (!visualize_) return nullptr;

        nlohmann::json pts = nlohmann::json::array();
        for (const auto& p : hit_points_) {
            pts.push_back({p.x(), p.y(), p.z()});
        }

        return {
            {"type",      "lidar_points"},
            {"points",    pts},
            {"color",     viz_color_},
            {"num_rays",  num_rays_},
            {"max_range", max_range_},
        };
    }

    // ROS2 публикация
    void publish_to_transport(ITransportAdapter& transport,
                               const Agent& agent) const override {
        if (scan_distances_.empty()) return;
        // Сформировать sensor_msgs/LaserScan
        transport.publish_laser_scan(
            agent,
            sensor_name_,
            scan_distances_,
            angle_min_,
            angle_max_,
            (angle_max_ - angle_min_) / num_rays_,
            min_range_,
            max_range_
        );
    }

    std::unique_ptr<IAgentPlugin> clone() const override {
        return std::make_unique<LidarPlugin>(*this);
    }

private:
    int num_rays_       = 360;
    double min_range_   = 0.1;
    double max_range_   = 10.0;
    double angle_min_   = -M_PI;
    double angle_max_   =  M_PI;
    double height_offset_ = 0.2;
    bool visualize_     = true;
    std::string viz_color_ = "#00FFFF";

    std::vector<double> scan_distances_;
    std::vector<Vec3>   hit_points_;

    const RaycastEngine* raycast_ = nullptr;
};

} // namespace s2::plugins
```

## Публикация LaserScan через транспорт

### ITransportAdapter: новый метод

**Файл:** `workspace/s2_core/include/s2/transport_adapter.hpp`

```cpp
virtual void publish_laser_scan(
    const Agent& agent,
    const std::string& sensor_name,
    const std::vector<double>& ranges,
    double angle_min,
    double angle_max,
    double angle_increment,
    double range_min,
    double range_max
) {}  // no-op по умолчанию
```

### Ros2TransportAdapter

Реализует `publish_laser_scan()` через `sensor_msgs/msg/LaserScan`.

Топик: `/<robot_name>/<sensor_name>` (например `/robot_0/front_lidar`)

```cpp
void Ros2TransportAdapter::publish_laser_scan(
    const Agent& agent,
    const std::string& sensor_name,
    const std::vector<double>& ranges,
    double angle_min, double angle_max, double angle_increment,
    double range_min, double range_max)
{
    auto msg = sensor_msgs::msg::LaserScan{};
    msg.header.stamp = node_->now();
    msg.header.frame_id = agent.name + "/base_link";

    msg.angle_min       = static_cast<float>(angle_min);
    msg.angle_max       = static_cast<float>(angle_max);
    msg.angle_increment = static_cast<float>(angle_increment);
    msg.range_min       = static_cast<float>(range_min);
    msg.range_max       = static_cast<float>(range_max);
    msg.scan_time       = 1.0f / 10.0f;
    msg.time_increment  = 0.0f;

    msg.ranges.reserve(ranges.size());
    for (double r : ranges)
        msg.ranges.push_back(static_cast<float>(r));

    laser_scan_pub_->publish(msg);
}
```

## Визуализация в браузере

**Файл:** `workspace/s2_visualizer/web/js/app.js`

В `updateScene()` → обработка `plugins_data`, добавить кейс `lidar_points`:

```js
if (agentPlugins.lidar) {
    const d = typeof agentPlugins.lidar === 'string'
        ? JSON.parse(agentPlugins.lidar)
        : agentPlugins.lidar;

    renderLidarPoints(`lidar_${agentKey}`, d.points, d.color);
}
```

```js
const lidarPointObjects = {};  // agentKey -> THREE.Points

function renderLidarPoints(id, points, color) {
    // Удалить старые
    if (lidarPointObjects[id]) {
        scene.remove(lidarPointObjects[id]);
        lidarPointObjects[id].geometry.dispose();
        lidarPointObjects[id].material.dispose();
        delete lidarPointObjects[id];
    }
    if (!points || points.length === 0) return;

    // Создать новые
    const positions = new Float32Array(points.flatMap(p => [p[0], p[2], -p[1]]));
    const geometry = new THREE.BufferGeometry();
    geometry.setAttribute('position', new THREE.BufferAttribute(positions, 3));

    const material = new THREE.PointsMaterial({
        color: color || '#00FFFF',
        size: 0.08,
        sizeAttenuation: true,
    });

    const pts = new THREE.Points(geometry, material);
    scene.add(pts);
    lidarPointObjects[id] = pts;
}
```

## Тестовая сцена с лидаром

**Файл:** `workspace/s2_config/scenes/test_lidar.yaml`

```yaml
s2:
  update_rate: 50
  visualizer:
    enabled: true
    port: 1937
  transport:
    type: stub

  world:
    geometry:
      # Пол
      - type: box
        pose: { x: 0.0, y: 0.0, z: -0.025 }
        size: [20.0, 20.0, 0.05]
        color: "#222222"

      # Несколько стен
      - type: box
        pose: { x: 4.0, y: 0.0, z: 0.5 }
        size: [0.2, 4.0, 1.0]
        color: "#4444AA"

      - type: box
        pose: { x: -3.0, y: 2.0, z: 0.5 }
        size: [2.0, 0.2, 1.0]
        color: "#AA4444"

      - type: cylinder
        pose: { x: 1.0, y: 3.0, z: 0.5 }
        radius: 0.3
        height: 1.0
        color: "#44AA44"

  agents:
    - name: robot_0
      pose: { x: 0.0, y: 0.0 }
      collision:
        bounding:
          type: sphere
          radius: 0.3
      visual:
        type: box
        size: [0.5, 0.35, 0.25]
        color: "#FF6B35"
      plugins:
        - type: diff_drive
          wheel_base: 0.4
          max_linear_vel: 1.5
        - type: lidar
          name: front_lidar
          num_rays: 360
          max_range: 8.0
          height_offset: 0.2
          publish_rate_hz: 10
          visualize: true
          viz_color: "#00FFFF"

    - name: robot_1
      pose: { x: 2.0, y: 1.0 }
      collision:
        bounding:
          type: sphere
          radius: 0.3
      visual:
        type: box
        size: [0.5, 0.35, 0.25]
        color: "#35B5FF"
      plugins:
        - type: diff_drive
          wheel_base: 0.4
          max_linear_vel: 1.5
```

## Критерии завершения

- [ ] Лидар robot_0 видит статические стены (box, cylinder) — точки отображаются в UI
- [ ] Лидар robot_0 видит robot_1 (другой агент с коллизией)
- [ ] При движении robot_1 точки лидара обновляются
- [ ] При отсутствии препятствия в направлении — точка не рисуется (max_range)
- [ ] Угол `angle_min` / `angle_max` ограничивает сектор сканирования
- [ ] `min_range` игнорирует слишком близкие попадания
- [ ] В ROS2 режиме публикуется `sensor_msgs/LaserScan` на топик `/<name>/front_lidar`
- [ ] При `visualize: false` точки не передаются в plugins_data

## Регистрация плагина

**Файл:** `workspace/s2_plugins/src/plugins_registry.cpp`

```cpp
static PluginRegistrar<LidarPlugin> register_lidar("lidar");
```

## Тесты

**Файл:** `workspace/s2_core/tests/test_lidar_plugin.cpp`

- `LidarPlugin_HitsStaticBox` — луч в направлении box → distance корректна
- `LidarPlugin_MissesOutOfRange` — box за max_range → distance = max_range
- `LidarPlugin_HitsAgent` — другой агент с коллизией → hit
- `LidarPlugin_IgnoresSelf` — агент не видит себя
- `LidarPlugin_AngleSector` — только лучи в [angle_min, angle_max]
- `LidarPlugin_MinRange` — попадание ближе min_range → игнорируется
- `LidarPlugin_ToJson` — `to_json()` содержит массив points с корректными координатами
