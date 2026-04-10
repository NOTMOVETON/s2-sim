# Task 09 — Визуализатор: WebSocket сервер + Three.js клиент

> **Предыдущий шаг:** `08-interactions.md` (взаимодействия работают)
> **Следующий шаг:** `10-transport-ros2.md`
> **Контекст:** `ARCHITECTURE.md` раздел 21

## Цель

Визуальное отображение симуляции в браузере. После этого шага: открываешь localhost:8080 — видишь 3D сцену с роботами, стенами, зонами. Сцена обновляется в реальном времени.

## Что сделать

### 1. WorldSnapshot — формат данных

```
s2_core/include/s2/world_snapshot.hpp
```

```cpp
struct WorldSnapshot {
    double sim_time;

    struct AgentInfo {
        AgentId id;
        std::string name;
        Pose3D pose;
        Velocity velocity;
        VisualDesc visual;
        std::optional<double> battery_level;
        std::vector<ObjectId> held_objects;
        double effective_speed_scale;
        bool motion_locked;
    };

    struct PropInfo {
        ObjectId id;
        std::string type;
        Pose3D pose;
        VisualDesc visual;
        std::optional<AgentId> attached_to;
    };

    struct ActorInfo {
        ActorId id;
        std::string name;
        Pose3D pose;
        VisualDesc visual;
        ActorState state;
    };

    struct ZoneInfo {
        ZoneId id;
        bool enabled;
        ZoneShape shape;
        std::vector<AgentId> inside_agents;
    };

    struct GeometryInfo {
        std::string type;
        Pose3D pose;
        Vec3 size;
        double radius;
        double height;
    };

    std::vector<AgentInfo> agents;
    std::vector<PropInfo> props;
    std::vector<ActorInfo> actors;
    std::vector<ZoneInfo> zones;
    std::vector<GeometryInfo> static_geometry;  // только при первом подключении
};
```

### 2. Сериализация в JSON

```cpp
// Используем nlohmann/json
nlohmann::json snapshot_to_json(const WorldSnapshot& snap, bool include_geometry = false);
```

Формат:
```json
{
  "sim_time": 1.234,
  "agents": [
    {"id": 0, "name": "robot_0", "pose": {"x": 1.0, "y": 2.0, "z": 0.0, "yaw": 0.5},
     "visual": {"type": "box", "size": [0.8, 0.5, 0.3], "color": "#FF6B35"},
     "battery": 0.85, "held_objects": [], "speed_scale": 1.0, "locked": false}
  ],
  "props": [...],
  "actors": [...],
  "zones": [...]
}
```

### 3. VizServer — WebSocket сервер (C++)

```
s2_visualizer/src/viz_server.hpp / .cpp
```

```cpp
class VizServer {
public:
    VizServer(int ws_port = 8765, int http_port = 8080);

    void start();  // запуск в отдельном потоке
    void stop();

    // Вызывается из SimEngine каждые 1/viz_rate секунд
    void publish(const WorldSnapshot& snapshot);

    // Команды от визуализатора (poll)
    struct VizCommand {
        std::string type;         // "toggle_zone", "teleport_agent", ...
        nlohmann::json params;
    };
    std::vector<VizCommand> poll_commands();

private:
    // uWebSockets event loop в отдельном потоке
    // HTTP для отдачи static файлов (web/)
    // WebSocket для стрима данных и приёма команд
};
```

**[СПРОСИТЬ]** Если uWebSockets не работает или слишком сложно — можно использовать простой WebSocket на boost::beast или даже libwebsocketpp. Главное — рабочий WebSocket + HTTP для статики.

**Альтернатива:** использовать `cpp-httplib` (header-only) для HTTP + простой WebSocket. Или даже Python сервер для прототипа.

### 4. Three.js клиент (минимальный)

```
s2_visualizer/web/index.html
s2_visualizer/web/js/app.js
s2_visualizer/web/js/scene.js
```

Минимальный клиент:

```javascript
// app.js
const ws = new WebSocket('ws://localhost:8765');
const scene = new THREE.Scene();
const camera = new THREE.PerspectiveCamera(75, w/h, 0.1, 1000);
const renderer = new THREE.WebGLRenderer();

// Объекты по id
const meshes = {};

ws.onmessage = (event) => {
    const data = JSON.parse(event.data);
    updateScene(data);
};

function updateScene(snapshot) {
    // Agents
    for (const agent of snapshot.agents) {
        if (!meshes[`agent_${agent.id}`]) {
            createAgentMesh(agent);
        }
        updateMesh(meshes[`agent_${agent.id}`], agent.pose);
    }
    // Props, Actors аналогично
    // Zones — полупрозрачные
}

function createAgentMesh(agent) {
    const {type, size, color} = agent.visual;
    let geometry;
    if (type === "box") geometry = new THREE.BoxGeometry(size[0], size[2], size[1]);
    else if (type === "cylinder") geometry = new THREE.CylinderGeometry(/*...*/);
    else geometry = new THREE.SphereGeometry(0.5);

    const material = new THREE.MeshLambertMaterial({color: color});
    const mesh = new THREE.Mesh(geometry, material);
    scene.add(mesh);
    meshes[`agent_${agent.id}`] = mesh;
}
```

Функциональность:
- 3D сцена с OrbitControls (вращение камеры мышью)
- Пол (grid или плоскость)
- Стены (из static_geometry при первом подключении)
- Роботы — примитивы с цветами
- Акторы — примитивы (дверь — другой цвет для open/closed)
- Props — примитивы
- Зоны — полупрозрачные wireframe
- Освещение (ambient + directional)
- Обновление позиций по WebSocket

НЕ делать в этой задаче:
- Панель управления (кнопки)
- Панель состояния (батарея, held objects)
- Лучи лидара
- Команды от визуализатора (только просмотр)

### 5. Интеграция с SimEngine

```cpp
// В SimEngine::tick(), фаза 6:
viz_accum_ += dt_;
if (viz_accum_ >= 1.0 / config_.viz_rate) {
    viz_accum_ = 0.0;
    if (viz_server_) {
        auto snapshot = build_world_snapshot();
        viz_server_->publish(snapshot);
    }
}
```

### 6. Main executable

```
workspace/s2_core/src/main.cpp
```

```cpp
int main(int argc, char** argv) {
    std::string scene_path = "s2_config/scenes/test_basic.yaml";
    if (argc > 1) scene_path = argv[1];

    auto scene = SceneLoader::load(scene_path);
    
    SimEngine engine(scene.engine_config);
    engine.load_world(/* ... */);

    VizServer viz(8765, 8080);
    viz.start();
    engine.set_viz_server(&viz);

    // Для теста: задать команду скорости первому роботу
    if (auto* agent = engine.world().get_agent(0)) {
        if (agent->actuation) {
            static_cast<DiffDriveModule*>(agent->actuation.get())
                ->set_command(0.5, 0.1);
        }
    }

    engine.run();
    return 0;
}
```

### 7. Обновить docker-compose.yml

Добавить сервис `sim`:

```yaml
  sim:
    build: ...
    volumes:
      - ../workspace:/workspace
    ports:
      - "8765:8765"
      - "8080:8080"
    command: >
      bash -c "cd /workspace/build && ./s2_sim ../s2_config/scenes/test_basic.yaml"
```

## Тесты

### Автоматические
```
s2_core/tests/test_snapshot.cpp
```
- build_world_snapshot() содержит всех агентов, props, actors, zones
- snapshot_to_json() → валидный JSON, парсится обратно

### Ручные
```bash
docker compose up sim
# Открыть http://localhost:8080 в браузере
# Видеть: комнату, робота, стены
# Робот медленно двигается (set_command)
```

## Критерии приёмки

- WebSocket сервер отправляет JSON snapshot 30 раз/сек
- Браузер показывает 3D сцену
- Роботы двигаются в реальном времени
- Стены видны
- При подключении клиент получает статическую геометрию

## Чего НЕ делать

- Не делать красивый UI (позже)
- Не делать команды из визуализатора (позже)
- Не делать лучи лидара в визуализаторе (позже)
- Не оптимизировать (JSON при 100 агентах — позже)
