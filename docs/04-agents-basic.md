# Фича 04 — Плагины робота

## Цель
Создать модульную систему плагинов для агента. Каждый плагин — отдельный модуль, подключаемый через конфигурацию YAML и вызываемый каждый тик симуляции.

## Архитектура

### Директория структуры
```
workspace/s2_plugins/include/s2/plugins/
├── plugin_base.hpp    // базовый интерфейс IAgentPlugin
├── diff_drive.hpp     // кинематика дифференциального привода
├── gnss.hpp           // GPS/GNSS сенсор
├── imu.hpp            // инерциальный измерительный блок
└── lidar.hpp          // упрощённый лидар

workspace/s2_plugins/src/
├── diff_drive.cpp
├── gnss.cpp
├── imu.cpp
└── lidar.cpp
```

### IAgentPlugin интерфейс
```cpp
class IAgentPlugin {
public:
    virtual ~IAgentPlugin() = default;
    virtual std::string type() const = 0;
    
    /// Вызывается каждый тик. Может менять velocity агента, публиковать данные.
    virtual void update(double dt, Agent& agent) = 0;
    
    /// Загрузка из YAML-конфига агента.
    virtual void from_config(const YAML::Node& node) = 0;
    
    /// JSON-сериализация текущих выходов плагина (для визуализатора).
    virtual std::string to_json() const = 0;
};
```

### Плагин DiffDrive
- **Назначение:** кинематика дифференциального привода
- **Входы:** desired_linear_x, desired_angular_z (желаемые скорости)
- **Действие:** устанавливает agent.world_velocity.linear.x и agent.world_velocity.angular.z
- **Конфиг:**
  ```yaml
  plugins:
    - type: "diff_drive"
      max_linear: 2.0
      max_angular: 1.5
  ```

### Плагин GNSS
- **Назначение:** имитация GPS-позиции с шумом
- **Действие:** публикует данные в SharedState агента
- **Конфиг:**
  ```yaml
  plugins:
    - type: "gnss"
      noise_std: 0.5  # стандартное отклонение шума (метры)
  ```
- **Выход JSON:**
  ```json
  {"plugin": "gnss", "lat": 55.75, "lon": 37.62, "accuracy": 0.5}
  ```

### Плагин IMU
- **Назначение:** инерциальный измерительный блок
- **Выход JSON:**
  ```json
  {"plugin": "imu", "gyro_z": 0.3, "accel_x": 0.1, "accel_y": 0.0, "yaw": 0.15}
  ```

### Плагин Lidar (упрощённый)
- **Назначение:** упрощённый лидар (raycast лучи вокруг робота)
- **Выход JSON:**
  ```json
  {"plugin": "lidar", "ranges": [2.5, 1.8, 3.2, ...], "angle_step": 0.1}
  ```

### Интеграция в Agent
Добавить в struct Agent:
```cpp
std::vector<std::unique_ptr<IAgentPlugin>> plugins;
```

### Интеграция в SimEngine tick()
Перед кинематикой:
```cpp
for (auto& plugin : agent.plugins) {
    plugin->update(dt_, agent);
}
```

### Конфигурация в YAML
Расширить agents:
```yaml
agents:
  - name: "robot_0"
    pose: {x: 0.0, y: 0.0, z: 0.0, yaw: 0.0}
    plugins:
      - type: "diff_drive"
        max_linear: 2.0
        max_angular: 1.5
      - type: "gnss"
        noise_std: 0.5
      - type: "imu"
```

### Тесты
- Тест DiffDrive: задать desired_{linear,angular} → проверить что velocity обновляется
- Тест GNSS: проверить что выход валидный JSON
- Тест IMU: проверить корректность yawrate

## Порядок реализации
1. Создать IAgentPlugin интерфейс (plugin_base.hpp)
2. Реализовать DiffDrive плагин
3. Добавить массив plugins в Agent
4. Обновить SimEngine tick() для вызова плагинов
5. Обновить scene_loader.hpp для чтения plugins: из YAML
6. Реализовать GNSS плагин
7. Реализовать IMU плагин
8. Написать тесты
9. Добавить Lidar плагин (опционально)