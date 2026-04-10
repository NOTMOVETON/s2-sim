# Задача 11 — Конфигурация транспорта и визуализатора из YAML

## Цель

Вынести выбор транспортного адаптера и параметры визуализатора из захардкоженного `main.cpp` в YAML-файл сцены. После этого смена транспорта или отключение визуализатора не требует перекомпиляции.

## Мотивация

Сейчас в `workspace/s2_visualizer/src/main.cpp`:
```cpp
auto adapter = std::make_shared<s2::Ros2TransportAdapter>();  // захардкожено
auto viz = std::make_unique<s2::VizServer>(0, port, web_path); // порт из аргумента CLI
```

Нет возможности:
- Запустить симуляцию без ROS2 (headless или stub-режим) через конфиг
- Выбрать тип транспорта без перекомпиляции
- Переопределить порт визуализатора из конфига сцены

## Новые секции YAML

### Секция `transport:`

```yaml
s2:
  transport:
    type: ros2          # "ros2" | "stub"
                        # stub — без реального транспорта (для тестов, CI)
    default_domain_id: 0  # домен по умолчанию для агентов без явного domain_id
```

- Если секция отсутствует — поведение как раньше (`type: ros2`)
- Если `S2_WITH_ROS2` не определён и `type: ros2` — предупреждение в лог, автоматический fallback на `stub`

### Секция `visualizer:`

```yaml
s2:
  visualizer:
    enabled: true   # false = headless: VizServer не создаётся
    port: 8080      # порт HTTP/SSE сервера (по умолчанию 8080)
```

- Если секция отсутствует — `enabled: true`, порт 8080
- `enabled: false` полезен для CI, автотестов и серверных запусков без браузера

## Что нужно сделать

### 1. `workspace/s2_core/include/s2/scene_loader.hpp`

Добавить структуры в `SceneData`:

```cpp
struct TransportConfig {
    std::string type = "ros2";  // "ros2" | "stub"
    int default_domain_id = 0;
};

struct VizConfig {
    bool enabled = true;
    int  port    = 8080;
};

struct SceneData {
    SimEngine::Config engine_config;
    TransportConfig   transport_config;   // новое
    VizConfig         viz_config;         // новое
    // ... остальные поля без изменений
};
```

Добавить парсинг в `SceneLoader::load()` (в секции `if (const auto& s2 = root["s2"])`):

```cpp
if (const auto& tr = s2["transport"]) {
    if (tr["type"])              scene.transport_config.type              = tr["type"].as<std::string>();
    if (tr["default_domain_id"]) scene.transport_config.default_domain_id = tr["default_domain_id"].as<int>();
}

if (const auto& viz = s2["visualizer"]) {
    if (viz["enabled"]) scene.viz_config.enabled = viz["enabled"].as<bool>();
    if (viz["port"])    scene.viz_config.port    = viz["port"].as<int>();
}
```

### 2. `workspace/s2_visualizer/src/main.cpp`

Заменить создание адаптера:

```cpp
std::shared_ptr<s2::ITransportAdapter> adapter;
if (scene_data.transport_config.type == "stub") {
    adapter = std::make_shared<s2::Ros2TransportAdapter>();  // stub-режим внутри
    std::cout << "[Main] Transport: stub mode" << std::endl;
} else {
#ifndef S2_WITH_ROS2
    std::cerr << "[Main] WARNING: transport type=ros2 but S2_WITH_ROS2 not set, falling back to stub" << std::endl;
#endif
    adapter = std::make_shared<s2::Ros2TransportAdapter>();
    std::cout << "[Main] Transport: ros2 (domain default=" 
              << scene_data.transport_config.default_domain_id << ")" << std::endl;
}
```

Обернуть создание VizServer в условие:

```cpp
std::unique_ptr<s2::VizServer> viz;
if (scene_data.viz_config.enabled) {
    viz = std::make_unique<s2::VizServer>(0, scene_data.viz_config.port, web_path);
    viz->start();
    g_viz = viz.get();
} else {
    std::cout << "[Main] Visualizer disabled (headless mode)" << std::endl;
}
```

### 3. Обновить YAML-сцены

Добавить секции в каждый файл конфига:

- `workspace/s2_config/scenes/test_basic.yaml`
- `workspace/s2_config/scenes/test_dozer.yaml`
- `workspace/s2_config/scenes/test_ros2_full.yaml`

Пример добавления:
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
    # ... без изменений
```

## Проверка

- `type: stub` → нет ROS2 подключений, симуляция работает
- `type: ros2` → поведение как до задачи
- `enabled: false` → нет HTTP-сервера, браузер не нужен, симуляция работает
- `port: 9090` → визуализатор доступен на другом порту
- Отсутствие секций в старых YAML-сценах → поведение не изменилось
