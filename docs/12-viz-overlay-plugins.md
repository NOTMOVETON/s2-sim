# Задача 12 — Плагины визуализации данных робота

## Цель

Создать плагины, которые позволяют отображать в браузерном визуализаторе данные о состоянии робота: траекторию движения, планируемый путь от nav stack, эллипс ковариации локализации и подобные вещи.

Плагины **не влияют на физику симуляции** — они только собирают данные и передают их во фронтенд через механизм `to_json()` → `WorldSnapshot.plugins_data` → SSE → Three.js.

## Архитектура

```
Плагин (update/to_json)
    → WorldSnapshot.plugins_data["agent_0"]["trajectory_recorder"] = "{...}"
    → VizServer SSE stream
    → app.js читает plugins_data
    → THREE.Line рисует линии поверх сцены
```

## Плагин 1: `trajectory_recorder`

**Назначение:** записывает и отображает собственную траекторию робота.

Не требует ROS2. Работает автономно — просто записывает историю поз.

В UI добавлена кнопка включения и выключения работы плагина аналогично diff_drive или joint_vel.

### Конфиг YAML

```yaml
plugins:
  - type: "trajectory_recorder"
    record_interval_s: 0.5   # записывать позу каждые 0.5 секунды
    max_points: 200           # максимум точек в буфере (старые вытесняются)
    color: "#FFAA00"          # цвет линии в визуализаторе
```

### Поведение

- Каждые `record_interval_s` секунд добавляет текущую `agent.world_pose` в кольцевой буфер
- `to_json()` возвращает:
  ```json
  {
    "type": "trajectory",
    "points": [[x1,y1,z1], [x2,y2,z2], ...],
    "color": "#FFAA00"
  }
  ```

### Файлы

- `workspace/s2_plugins/include/s2/plugins/trajectory_recorder.hpp` — реализация
- `workspace/s2_plugins/src/plugins_registry.cpp` — регистрация

### Скелет

```cpp
class TrajectoryRecorderPlugin : public IAgentPlugin {
public:
    std::string type() const override { return "trajectory_recorder"; }

    void from_config(const YAML::Node& node) override {
        record_interval_s_ = node["record_interval_s"].as<double>(0.5);
        max_points_        = node["max_points"].as<int>(200);
        color_             = node["color"].as<std::string>("#FFAA00");
    }

    void update(double dt, Agent& agent) override {
        timer_ += dt;
        if (timer_ >= record_interval_s_) {
            timer_ = 0.0;
            points_.push_back({agent.world_pose.x, agent.world_pose.y, agent.world_pose.z});
            if ((int)points_.size() > max_points_)
                points_.erase(points_.begin());
        }
    }

    std::string to_json() const override {
        // сериализовать points_ в JSON
    }

private:
    double record_interval_s_{0.5};
    int    max_points_{200};
    std::string color_{"#FFAA00"};
    double timer_{0.0};
    std::vector<std::array<double,3>> points_;
};
```

---

## Плагин 2: `path_display`

**Назначение:** подписывается на ROS2-топик `nav_msgs/Path` и отображает планируемый путь от nav stack.

Требует ROS2. В stub-режиме — просто не отображает ничего.

Плагин отрисовывает последний путь который пришел. Сброс отрисовки осуществляется только если произошел reset симуляции, или при помощи кнопки в UI, которая закреплена за плагином - включения или выключения отображения пути плагином.

### Конфиг YAML

```yaml
plugins:
  - type: "path_display"
    topic: "/plan"        # ROS2-топик nav_msgs/Path (в домене агента)
    max_points: 500       # максимум точек пути
    color: "#00FF88"      # цвет линии
```

### Поведение

- При инициализации регистрирует подписку на `topic` в транспортном адаптере
- При поступлении нового сообщения обновляет внутренний буфер точек
- `to_json()` возвращает:
  ```json
  {
    "type": "path",
    "points": [[x1,y1,z1], [x2,y2,z2], ...],
    "color": "#00FF88"
  }
  ```
- При отсутствии данных — `"points": []` (линия не рисуется)

### Интеграция с транспортом

Плагин реализует новый опциональный метод интерфейса:

```cpp
// В IAgentPlugin (plugin_base.hpp) — опциональный метод:
virtual std::vector<std::string> subscribe_topics() const { return {}; }
```

`SimTransportBridge::init()` при обнаружении плагинов с `subscribe_topics()` регистрирует подписки через адаптер. Callback вызывает метод плагина:

```cpp
virtual void handle_subscription(const std::string& topic,
                                  const std::string& msg_json) {}
```

`Ros2TransportAdapter` создаёт `rclcpp::Subscription<nav_msgs::msg::Path>`, конвертирует сообщение в JSON и вызывает callback.

### Файлы

- `workspace/s2_plugins/include/s2/plugins/path_display.hpp` — реализация
- `workspace/s2_core/include/s2/plugin_base.hpp` — добавить `subscribe_topics()` и `handle_subscription()`
- `workspace/s2_transport/include/s2/sim_transport_bridge.hpp` — обработка `subscribe_topics()` в `init()`
- `workspace/s2_transport/include/s2/ros2_transport_adapter.hpp` — подписка на Path
- `workspace/s2_transport/include/s2/transport_adapter.hpp` — добавить `register_subscription()`
- `workspace/s2_plugins/src/plugins_registry.cpp` — регистрация

---

## Фронтенд: рендеринг линий

В `workspace/s2_visualizer/web/js/app.js` добавить обработку новых типов в `plugins_data`.

### Логика

При получении снапшота для каждого агента проверять `plugins_data`:
```js
const agentData = snapshot.plugins_data["agent_0"];

if (agentData.trajectory_recorder) {
    const d = JSON.parse(agentData.trajectory_recorder);
    renderOverlayLine("traj_agent_0", d.points, d.color);
}

if (agentData.path_display) {
    const d = JSON.parse(agentData.path_display);
    renderOverlayLine("path_agent_0", d.points, d.color);
}
```

### Функция `renderOverlayLine(id, points, color)`

```js
function renderOverlayLine(id, points, color) {
    // Удалить старую линию если есть
    if (overlayLines[id]) {
        scene.remove(overlayLines[id]);
    }
    if (!points || points.length < 2) return;

    const geometry = new THREE.BufferGeometry();
    const positions = new Float32Array(points.flatMap(p => [p[0], p[2], -p[1]])); // Y-up
    geometry.setAttribute('position', new THREE.BufferAttribute(positions, 3));

    const material = new THREE.LineBasicMaterial({ color: color, linewidth: 2 });
    const line = new THREE.Line(geometry, material);
    scene.add(line);
    overlayLines[id] = line;
}
```

Объект `overlayLines` — словарь активных линий, очищается при смене сцены или reset.

---

## Демо-сцена

Создать `workspace/s2_config/scenes/test_viz_overlay.yaml`:

```yaml
s2:
  update_rate: 100
  viz_rate: 30
  transport_rate: 100

  transport:
    type: ros2

  visualizer:
    enabled: true
    port: 8080

  world:
    surface: "flat"
    geo_origin:
      lat: 55.7522
      lon: 37.6156
      alt: 156.0

  agents:
    - name: "robot_0"
      id: 0
      domain_id: 50
      pose: {x: 0.0, y: 0.0, z: 0.0, yaw: 0.0}
      visual:
        type: "box"
        size: [0.8, 0.5, 0.3]
        color: "#FF6B35"

      plugins:
        - type: "diff_drive"
          max_linear: 2.0
          max_angular: 1.5

        - type: "trajectory_recorder"
          record_interval_s: 0.2
          max_points: 300
          color: "#FFAA00"

        - type: "path_display"
          topic: "/plan"
          color: "#00FF88"
```

## Проверка

- Робот движется → в браузере за ним тянется оранжевая линия траектории
- При reset → линия траектории очищается
- При публикации `nav_msgs/Path` в ROS2 на `/plan` в domain 50 → рисуется зелёный путь
- В stub-режиме → `path_display` не отображает ничего, `trajectory_recorder` работает
