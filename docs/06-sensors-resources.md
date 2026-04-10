# Task 06 — Lidar2D сенсор + Resource модуль Battery

> **Предыдущий шаг:** `05-zones-effects.md` (зоны и эффекты работают)
> **Следующий шаг:** `07-actors-door.md`
> **Контекст:** `ARCHITECTURE.md` разделы 5.1 (Sensor), 5.1 (Resource), 15

## Цель

Первые сенсоры и ресурсы. После этого шага: лидар бросает лучи и генерирует LaserScan, батарея разряжается и публикует contributions, данные доступны в snapshot агента.

## Что сделать

### 1. Интерфейсы SensorModule и ResourceModule

```cpp
// interfaces/sensor_module.hpp
class SensorModule {
public:
    virtual ~SensorModule() = default;
    virtual void on_init(const YAML::Node& config) = 0;
    virtual void on_tick(const Pose3D& agent_pose, 
                          const SharedState& state,
                          double dt) = 0;
    virtual std::vector<std::string> capabilities() const = 0;

    // Последние сгенерированные данные (для snapshot)
    virtual const std::any& last_output() const = 0;
    virtual std::string output_type() const = 0;  // "laser_scan", "nav_sat_fix", ...
};

// interfaces/resource_module.hpp
class ResourceModule {
public:
    virtual ~ResourceModule() = default;
    virtual void on_init(const YAML::Node& config, SharedState& state) = 0;
    virtual void on_tick(SharedState& state, const Velocity& velocity, double dt) = 0;
    virtual std::string telemetry_type() const = 0;
    virtual const std::any& telemetry() const = 0;
};
```

### 2. Lidar2DModule (Sensor)

```
s2_plugins/sensors/lidar_2d.hpp / .cpp
```

- on_init: num_rays, angle_min, angle_max, range_min, range_max, noise_std, publish_rate
- on_tick: 
  1. Накопить dt. Если < 1/publish_rate → return
  2. Построить 360 лучей из позы агента (yaw учитывается)
  3. cast_batch через RaycastEngine (передаётся через on_init или World Query)
  4. Добавить gaussian noise
  5. Сохранить как LaserScan output

```cpp
struct LaserScan {
    std::vector<float> ranges;
    float angle_min, angle_max, angle_increment;
    float range_min, range_max;
    double timestamp;
};
```

**[СПРОСИТЬ]** Как Lidar2D получает доступ к RaycastEngine? Варианты:
- Передать указатель при init (просто, но связность)
- World Query API: agent запрашивает cast_batch у ядра (чище)

Пока — передать указатель. World Query API формализуем позже.

### 3. BatteryResource (Resource)

```
s2_plugins/resources/battery.hpp / .cpp
```

- on_init: регистрирует BatteryComponent в SharedState. Параметры: initial_level, drain_rate, slow_threshold, stop_threshold, min_speed_factor
- on_tick:
  1. Считать скорость: если двигается → drain
  2. `battery.level -= drain_rate * speed * dt`
  3. Publish contributions:
     - Если level < stop_threshold: add_lock(true, "battery")
     - Если level < slow_threshold: add_scale(calculated_factor, "battery")
  4. Сохранить телеметрию

```cpp
struct BatteryState {
    double level;        // 0..1
    bool charging;
    double drain_rate;
};
```

### 4. Обновить Agent

```cpp
struct Agent {
    // ... existing ...
    std::vector<std::unique_ptr<SensorModule>> sensors;
    std::vector<std::unique_ptr<ResourceModule>> resources;
};
```

### 5. Обновить tick()

```cpp
// Фаза 3a: Resource модули
for (auto& res : agent.resources) {
    res->on_tick(agent.state, agent.world_velocity, dt_);
}

// ... resolver, actuation, kinematics, collisions ...

// Фаза 3k: Sensor модули
for (auto& sensor : agent.sensors) {
    sensor->on_tick(agent.world_pose, agent.state, dt_);
}
```

### 6. Agent Snapshot

Добавить структуру для вывода состояния:

```cpp
struct AgentSnapshot {
    AgentId id;
    std::string name;
    Pose3D pose;
    Velocity velocity;
    EffectiveConstraints constraints;

    // Sensor outputs
    std::unordered_map<std::string, std::any> sensor_data;
    // Resource telemetry
    std::unordered_map<std::string, std::any> resource_data;
};
```

После всех фаз — заполнить snapshot из текущего состояния. Пока без тройного буфера (добавим с транспортом).

## Тесты

```
s2_core/tests/test_lidar.cpp
s2_core/tests/test_battery.cpp
```

### test_lidar.cpp
- Агент в центре комнаты, лидар 360 лучей: все hit, расстояния > 0
- Агент рядом со стеной: лучи в сторону стены — короткие, в другую — длинные
- publish_rate: при rate=10 и tick rate=100, output обновляется каждые 10 тиков

### test_battery.cpp
- Battery drain: после 1000 тиков с движением level < initial
- Battery стоит на месте: level не меняется (drain только при движении)
- Battery < slow_threshold: effective speed_scale < 1.0
- Battery < stop_threshold: effective motion_locked = true
- Battery + ChargingEffect zone: level растёт пока в зоне

## Критерии приёмки

- Lidar генерирует LaserScan данные
- Battery разряжается при движении
- Battery contributions влияют на скорость через resolver
- Sensor data доступна в snapshot
