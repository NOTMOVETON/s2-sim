# Задача 21 — Плагин гравитации (GravityPlugin)

## Цель

Реализовать плагин `gravity` типа Resource, который симулирует гравитацию для агента.
При отсутствии опоры снизу агент падает вертикально вниз с ускорением `g` до тех пор,
пока не достигнет поверхности (статического примитива снизу).

## Зависимости

- Требует: задача 20 (CollisionSystem, `find_support_surface()`)
- Требует: задача 14 (примитивы сцены существуют)

## Поведение

```
Каждый тик:
  1. Запросить find_support_surface(agent.pose, bounding_radius)
  2. Если опора найдена (hit):
     a. Если агент выше поверхности больше чем на epsilon:
        - Он ещё падает: сохранить вертикальную скорость, применить гравитацию
     b. Если агент на поверхности (z ≈ surface_z + bounding_radius):
        - Обнулить вертикальную скорость
        - Зафиксировать z = surface_z + bounding_radius
  3. Если опора не найдена:
     - Агент в воздухе: velocity.z -= g * dt
     - pose.z += velocity.z * dt
```

## Конфиг YAML

```yaml
plugins:
  - type: gravity
    gravity_accel: 9.81    # ускорение свободного падения, м/с² (по умолчанию 9.81)
    max_fall_speed: 20.0   # максимальная скорость падения, м/с (clamp)
    grounded_epsilon: 0.02 # допуск "на поверхности", метры
```

## Реализация

**Файл:** `workspace/s2_plugins/include/s2/plugins/gravity.hpp`

```cpp
#pragma once
#include <s2/plugin_base.hpp>
#include <s2/collision_system.hpp>

namespace s2::plugins {

class GravityPlugin : public IAgentPlugin {
public:
    std::string type() const override { return "gravity"; }

    void from_config(const YAML::Node& node) override {
        gravity_accel_  = node["gravity_accel"].as<double>(9.81);
        max_fall_speed_ = node["max_fall_speed"].as<double>(20.0);
        epsilon_        = node["grounded_epsilon"].as<double>(0.02);
    }

    /// Вызывается SimEngine перед update() — передать ссылку на CollisionSystem.
    /// (CollisionSystem инжектируется через initialize() или специальный метод)
    void set_collision_system(const CollisionSystem* cs) { collision_ = cs; }

    void update(Agent& agent, double dt) override {
        if (!collision_) return;

        const double bounding_radius = agent.bounding.radius;  // для sphere
        auto support = collision_->find_support_surface(agent.world_pose, bounding_radius);

        const double ground_z = support.has_value()
            ? *support + bounding_radius
            : -std::numeric_limits<double>::infinity();

        const bool grounded = support.has_value()
            && (agent.world_pose.z <= ground_z + epsilon_);

        if (grounded) {
            // Агент на поверхности
            fall_velocity_ = 0.0;
            agent.world_pose.z = ground_z;
            // Убрать вертикальную компоненту из world_velocity (другие плагины не должны двигать вниз)
            agent.world_velocity.linear.z() = 0.0;
        } else {
            // Свободное падение
            fall_velocity_ -= gravity_accel_ * dt;
            fall_velocity_ = std::max(fall_velocity_, -max_fall_speed_);
            agent.world_pose.z += fall_velocity_ * dt;
            agent.world_velocity.linear.z() = fall_velocity_;

            // Если упали ниже поверхности (чрезмерный dt или резкий подъём поверхности)
            if (support.has_value() && agent.world_pose.z < ground_z) {
                agent.world_pose.z = ground_z;
                fall_velocity_ = 0.0;
                agent.world_velocity.linear.z() = 0.0;
            }
        }
    }

    nlohmann::json to_json(const Agent& agent) const override {
        return {
            {"grounded", fall_velocity_ == 0.0},
            {"fall_velocity", fall_velocity_},
        };
    }

    std::unique_ptr<IAgentPlugin> clone() const override {
        return std::make_unique<GravityPlugin>(*this);
    }

private:
    double gravity_accel_  = 9.81;
    double max_fall_speed_ = 20.0;
    double epsilon_        = 0.02;
    double fall_velocity_  = 0.0;   // текущая вертикальная скорость падения (< 0 = вниз)

    const CollisionSystem* collision_ = nullptr;
};

} // namespace s2::plugins
```

## Инжекция CollisionSystem в плагин

### Вариант: через SimEngine при тике

В `sim_engine.hpp`, в цикле по агентам, перед вызовом `plugin->update()`:

```cpp
for (auto& plugin : agent.plugins) {
    // Инжектировать CollisionSystem если плагин это поддерживает
    if (auto* grav = dynamic_cast<GravityPlugin*>(plugin.get())) {
        grav->set_collision_system(&collision_system_);
    }
    plugin->update(agent, dt_);
}
```

Альтернатива: добавить метод `set_collision_system(const CollisionSystem*)` в `IAgentPlugin`
как no-op по умолчанию. Это чище, но добавляет зависимость плагинов на `collision_system.hpp`.

Рекомендуется: `dynamic_cast` в SimEngine — плагины остаются независимыми от ядра коллизий.

## Порядок фаз при наличии гравитации

```
3e. plugin->update() для всех плагинов (в том числе GravityPlugin обновляет pose.z)
3f. Кинематика (интеграция pose.x, pose.y из world_velocity.linear.x/y)
3h. Коллизии (lateral) — применить slide для горизонтальных стен
```

Гравитация применяется в `3e`, не в `3h`. Это важно: GravityPlugin сам управляет
вертикальной позицией агента, коллизионная система `3h` управляет горизонтальными стенами.

## Тестовая сцена с гравитацией

**Файл:** `workspace/s2_config/scenes/test_gravity.yaml`

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
        size: [40.0, 40.0, 0.05]
        color: "#333333"

      # Платформа на высоте 1м (6x2, по центру X)
      - type: box
        pose: { x: 0.0, y: 5.0, z: 1.025 }
        size: [6.0, 2.0, 0.05]
        color: "#555555"

  agents:
    - name: robot_0
      pose: { x: 0.0, y: 5.0, z: 3.0 }   # стартует в воздухе над платформой
      collision:
        bounding:
          type: sphere
          radius: 0.2
      visual:
        type: box
        size: [0.4, 0.3, 0.2]
        color: "#FF6B35"
      plugins:
        - type: diff_drive
          wheel_base: 0.3
          max_linear_vel: 1.0
        - type: gravity
          gravity_accel: 9.81
```

**Ожидаемое поведение:**
1. Robot_0 стартует на z=3.0 — в воздухе над платформой (z=1.05)
2. Свободно падает вниз
3. Приземляется на платформу (z = 1.05 + 0.2 = 1.25)
4. Можно управлять движением; если свалится с платформы — упадёт на пол

## Критерии завершения

- [x] Агент с плагином `gravity` падает вниз при z > floor
- [x] Агент приземляется на пол (box z≈0) и не проваливается
- [x] Агент приземляется на платформу и стоит на ней
- [x] Если агент съезжает с платформы — падает на пол
- [x] Вертикальная скорость не превышает `max_fall_speed`
- [x] `to_json()` возвращает `grounded: true` когда агент на поверхности
- [x] Плагин не влияет на горизонтальное движение (diff_drive работает как обычно)
- [x] Агент без плагина `gravity` — поведение не меняется (z остаётся неизменным)

## Регистрация плагина

**Файл:** `workspace/s2_plugins/src/plugins_registry.cpp`

```cpp
static PluginRegistrar<GravityPlugin> register_gravity("gravity");
```

## Тесты

**Файл:** `workspace/s2_core/tests/test_gravity_plugin.cpp`

- `GravityPlugin_FreeFall` — агент с z=5.0, нет поверхности → z уменьшается каждый тик
- `GravityPlugin_LandsOnFloor` — агент падает, пол на z=0 → агент останавливается на z=bounding_radius
- `GravityPlugin_StaysOnGround` — агент на поверхности, gravity → z не меняется
- `GravityPlugin_FallsOffPlatform` — агент на платформе z=1, выезжает за край → начинает падать
- `GravityPlugin_MaxFallSpeed` — при длинном падении скорость не превышает max_fall_speed
