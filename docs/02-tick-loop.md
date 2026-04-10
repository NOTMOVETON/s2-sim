# Task 02 — Главный цикл (SimEngine) + World + Agent shell

> **Предыдущий шаг:** `01-core-types.md` (типы, SharedState, SimBus)
> **Следующий шаг:** `03-world-geometry.md`
> **Контекст:** `ARCHITECTURE.md` разделы 16, 17, 19

## Цель

Работающий тиковый цикл. Пустые агенты тикаются в правильном порядке. После этого шага: SimEngine запускается, тикает N раз, агенты существуют в мире, resolver вызывается каждый тик, sim_time растёт.

## Что сделать

### 1. agent.hpp — структура агента (shell)

Минимальный Agent — пока без модулей, без kinematic tree. Только id, pose, shared_state.

```cpp
namespace s2 {

struct Agent {
    AgentId id;
    std::string name;
    int domain_id{0};

    Pose3D world_pose;
    Velocity world_velocity;
    SharedState state;

    std::unordered_set<std::string> capabilities;

    CollisionShape bounding;
    VisualDesc visual;

    // Модули добавятся в следующих задачах
    // Эффекты добавятся в задаче 05
};

} // namespace s2
```

### 2. prop.hpp и actor.hpp — заглушки

Минимальные структуры, просто чтобы World мог их хранить:

```cpp
struct Prop {
    ObjectId id;
    std::string type;
    Pose3D world_pose;
    bool movable{false};
    CollisionShape collision;
    VisualDesc visual;
    std::unordered_map<std::string, std::string> properties;
};

struct Actor {
    ActorId id;
    std::string name;
    Pose3D world_pose;
    ActorState current_state;
    CollisionShape collision;
    VisualDesc visual;
    // FSM добавится в задаче 07
};
```

### 3. world.hpp — SimWorld

Контейнер для всех сущностей:

```cpp
class SimWorld {
public:
    void add_agent(Agent agent);
    void add_prop(Prop prop);
    void add_actor(Actor actor);

    Agent* get_agent(AgentId id);
    Prop* get_prop(ObjectId id);
    Actor* get_actor(ActorId id);

    std::vector<Agent>& agents() { return agents_; }
    std::vector<Prop>& props() { return props_; }
    std::vector<Actor>& actors() { return actors_; }

private:
    std::vector<Agent> agents_;
    std::vector<Prop> props_;
    std::vector<Actor> actors_;
};
```

### 4. sim_engine.hpp — главный цикл

```cpp
class SimEngine {
public:
    struct Config {
        double update_rate{100.0};  // Hz
        double viz_rate{30.0};      // Hz
    };

    explicit SimEngine(Config config);

    void load_world(SimWorld world);

    // Запустить N тиков (для тестов)
    void step(int n = 1);

    // Запустить бесконечный цикл (для runtime)
    void run();
    void stop();

    double sim_time() const { return sim_time_; }
    double dt() const { return dt_; }
    const SimWorld& world() const { return world_; }
    SimBus& bus() { return bus_; }

private:
    void tick();

    Config config_;
    SimWorld world_;
    SimBus bus_;

    double sim_time_{0.0};
    double dt_{0.0};
    bool running_{false};
};
```

### 5. Реализация tick()

Fixed dt. Каждый тик = 1/update_rate секунд. Sim_time растёт на dt каждый тик.

```cpp
void SimEngine::tick() {
    dt_ = 1.0 / config_.update_rate;
    sim_time_ += dt_;

    // === Фаза 1: Акторы (пока пусто) ===

    // === Фаза 2: Зоны (пока пусто) ===

    // === Фаза 3: Для каждого агента ===
    for (auto& agent : world_.agents()) {
        // 3a. Resource модули (пока пусто)
        // 3b. Own effects CONTINUOUS (пока пусто)
        // 3c. Zone effects CONTINUOUS (пока пусто)

        // 3d. RESOLVER
        agent.state.resolve();

        // 3e. Actuation (пока пусто)
        // 3f. Кинематика (пока пусто)
        // 3g. Surface snap (пока пусто)
        // 3h. Коллизии (пока пусто)
        // 3i. Joints (пока пусто)
        // 3j. Kinematic tree update (пока пусто)
        // 3k. Sensors (пока пусто)
        // 3l. Interactions (пока пусто)

        // Очистить contributions для следующего тика
        agent.state.clear_contributions();
    }

    // === Фаза 4: Attachments (пока пусто) ===
    // === Фаза 5: Snapshot (пока пусто) ===
    // === Фаза 6: Viz publish (пока пусто) ===
}
```

**Важно:** step(n) — для тестов, run() — для runtime с sleep. Оба вызывают tick().

**[СПРОСИТЬ]** Fixed dt решено предварительно. Если пользователь хочет variable dt — изменить. Пока реализуем fixed.

## Тесты

```
s2_core/tests/test_sim_engine.cpp
```

### test_sim_engine.cpp
- Создать SimEngine с rate=100. step(1). sim_time() == 0.01
- step(100). sim_time() == 1.0
- Добавить 3 агента в world. step(1). Все три в world().agents()
- Agent shared state: добавить contribution перед step, после step resolver вызван и effective вычислен, contributions очищены
- dt() == 0.01 при rate=100
- stop() прерывает run() (запустить в отдельном потоке, проверить что остановился)

## Критерии приёмки

```bash
docker compose run tests   # все тесты включая новые
```

- SimEngine тикает с правильным dt
- sim_time растёт линейно
- Resolver вызывается каждый тик
- Contributions очищаются каждый тик

## Чего НЕ делать

- Не реализовывать движение агентов (задача 04)
- Не реализовывать зоны (задача 05)
- Не реализовывать визуализатор (задача 09)
- Не добавлять тройную буферизацию (позже, когда появится транспорт)
