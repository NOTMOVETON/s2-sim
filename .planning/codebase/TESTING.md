# Testing Patterns

**Analysis Date:** 2026-04-25

## Test Framework

**Runner:**
- Google Test (GTest) через CMake `find_package(GTest)`
- Конфиг: `workspace/s2_core/CMakeLists.txt`, секция `add_executable(s2_core_tests ...)`

**Assertion Library:**
- GTest встроенный: `EXPECT_*`, `ASSERT_*`
- Основные матчеры: `EXPECT_EQ`, `EXPECT_NE`, `EXPECT_NEAR`, `EXPECT_DOUBLE_EQ`, `EXPECT_TRUE`, `EXPECT_FALSE`, `EXPECT_GT`, `EXPECT_LE`, `EXPECT_GE`, `ASSERT_EQ`, `ASSERT_NE`, `ASSERT_EQ`
- Для проверки отсутствия исключений: `EXPECT_NO_THROW`

**Команды запуска:**
```bash
# Из корня проекта (предпочтительный способ)
docker compose --project-directory docker up --build tests   # Собрать и запустить все тесты

# Альтернативно, если контейнер уже запущен
docker compose --project-directory docker run tests ./s2_core_tests
docker compose --project-directory docker run tests ./s2_editor_tests
```

## Организация тестовых файлов

**Расположение:**
- Все тесты в `workspace/s2_core/tests/` — отдельная директория, не рядом с исходниками
- Тестовые данные (XML) рядом с тестами: `workspace/s2_core/tests/test_urdf.xml`

**Именование:**
- Паттерн: `test_<имя_модуля>.cpp` (e.g., `test_zone_system.cpp`, `test_shared_state.cpp`)
- Тесты эффектов: `test_effect_<тип>.cpp` (e.g., `test_effect_modifier.cpp`, `test_effect_charging.cpp`)
- Тесты плагинов: `test_<имя>_plugin.cpp` (e.g., `test_gravity_plugin.cpp`, `test_battery_plugin.cpp`)

**Структура:**
```
workspace/s2_core/
  include/s2/
    shared_state.hpp
    sim_bus.hpp
    zone_system.hpp
    ...
  tests/
    test_smoke.cpp
    test_shared_state.cpp
    test_sim_bus.cpp
    test_sim_engine.cpp
    test_zone_system.cpp
    test_effect_modifier.cpp
    test_effect_charging.cpp
    test_effect_mutation.cpp
    test_effect_teleport.cpp
    test_effect_velocity_addition.cpp
    test_battery_plugin.cpp
    test_gravity_plugin.cpp
    test_collision_system.cpp
    ...
```

**Два тестовых бинарника:**
- `s2_core_tests` — основной тест-сьют (линкуется с `s2_core`, `s2_plugins`, `s2_transport`, `GTest::gtest_main`)
- `s2_editor_tests` — тесты редактора сцены (только `s2_core`, `s2_plugins`, без транспорта)

## Структура тестов

**Организация сьютов:**
```cpp
// Паттерн без describe — TEST(SuiteName, TestName)
TEST(ZoneSystem_AgentEnterSphere, EventPublished)
{
    // Arrange: создаём объекты
    ZoneSystem zs;
    zs.add_zone(make_sphere_zone("zone1", Vec3{0.0, 0.0, 0.0}, 2.0));

    SimBus bus;
    AgentId entered_agent = 999;
    bus.subscribe<event::AgentEnteredZone>([&](const event::AgentEnteredZone& e) {
        entered_agent = e.agent;
    });

    // Act: выполняем действие
    std::vector<Agent> agents;
    agents.push_back(make_agent(1, 1.0, 0.0));
    std::vector<Actor> actors;
    zs.tick(agents, actors, bus, 0.0, 0.01);

    // Assert: проверяем результат
    EXPECT_EQ(entered_agent, 1u);
}
```

**Именование тестов:**
- Суит именует подсистему и сценарий: `ZoneSystem_AgentEnterSphere`
- Тест именует ожидаемый исход: `EventPublished`, `NoEvents`, `FollowsAgent`
- Либо: суит именует подсистему (`SharedState`), тест именует поведение (`ScaleProduct`, `LockMultipleOneTrue`)
- Либо: суит именует эффект (`EffectModifier`), тест именует сценарий (`IceModifier_SlowsAgent`, `DiffDrive_MotionLocked_StopsAgent`)

**Паттерны setup/teardown:**
- Нет `SetUp`/`TearDown` — каждый тест создаёт объекты локально
- Нет `TEST_F` с fixture-классом — используются статические helper-функции
- Нет `beforeEach/afterEach`

**Секционные разделители в файлах:**
```cpp
// ─── Тест 1: Название сценария ────────────────────────────────────────
TEST(Suite, Test1) { ... }

// ─── Тест 2: Следующий сценарий ──────────────────────────────────────
TEST(Suite, Test2) { ... }
```

## Моки и стабы

**Подход:** Нет внешней библиотеки для моков. Стабы — конкретные классы, реализующие интерфейс.

**Паттерн stub-плагина:**
```cpp
// Stub наследует EffectPlugin и считает вызовы
class StubModifierPlugin : public EffectPlugin
{
public:
    int apply_count = 0;

    void on_init(const YAML::Node&) override {}
    EffectType effect_type() const override { return EffectType::MODIFIER; }
    std::vector<std::string> required_capabilities() const override
    {
        return {"special_capability"};
    }
    void apply_modifier(SharedState&, const EffectContext&) override
    {
        ++apply_count;
    }
};

// Использование в тесте
auto stub = std::make_unique<StubModifierPlugin>();
StubModifierPlugin* raw_stub = stub.get();
desc.plugin = std::move(stub);
// ... выполняем тик ...
EXPECT_EQ(raw_stub->apply_count, 1);
```

**Что мокируется:**
- Плагины эффектов (`EffectPlugin`) — stub-классы в теле теста
- Внешние зависимости (ROS2 транспорт) — используется stub-адаптер

**Что НЕ мокируется:**
- `SimBus` — используется реальный экземпляр
- `SharedState` — используется реальный экземпляр
- `ZoneSystem` — используется реальный экземпляр
- `Agent`, `Zone` — создаются через helper-функции с реальными типами

## Фикстуры и фабрики

**Паттерн статических helper-функций (основной):**
```cpp
// Файл с тестами: статические helper-функции в начале
static Agent make_agent(AgentId id, double x = 0.0, double y = 0.0)
{
    Agent a;
    a.id = id;
    a.world_pose.x = x;
    a.world_pose.y = y;
    a.world_pose.z = 0.0;
    return a;
}

static Zone make_sphere_zone_with_effect(
    const ZoneId& zone_id,
    std::unique_ptr<EffectPlugin> plugin,
    const std::vector<std::string>& required_caps = {})
{
    Zone z;
    z.id           = zone_id;
    z.enabled      = true;
    z.shape.type   = ZoneShapeType::SPHERE;
    z.shape.center = Vec3::Zero();
    z.shape.radius = 10.0;  // большой радиус — агент всегда внутри
    // ... собираем EffectDesc ...
    return z;
}

static void run_tick(ZoneSystem& zs, std::vector<Agent>& agents, double sim_time = 0.0)
{
    SimBus bus;
    std::vector<Actor> actors;
    zs.tick(agents, actors, bus, sim_time, 0.01);
}
```

**Расположение:**
- Все helper-функции определены статически в том же `.cpp`-файле что и тесты
- Нет общих fixture-файлов или factories/ директории
- Каждый тест-файл дублирует базовые helpers (`make_agent`, `make_sphere_zone`)
- Файл тестового URDF: `workspace/s2_core/tests/test_urdf.xml`, путь передаётся через `S2_TEST_URDF_PATH` compile definition

**YAML-конфигурация в тестах:**
```cpp
// Конфигурация плагина через YAML::Load со строкой
plugin->on_init(YAML::Load("traction_coefficient: 0.2"));

// Или через YAML::Node builder
YAML::Node cfg;
cfg["initial_level"] = 0.6;
plugin.from_config(cfg);
```

## Покрытие

**Требования:**
- Явного target покрытия нет
- Правило из CLAUDE.md: «Любое поведение, которое можно покрыть тестом, должно быть проверено тестом»

**Что покрывается:**
- Каждый новый EffectPlugin — отдельный `test_effect_<тип>.cpp`
- Каждый новый IAgentPlugin — добавляется в существующий файл или создаётся `test_<имя>_plugin.cpp`
- Каждый новый метод публичного API — тест в соответствующем `test_<модуль>.cpp`
- Граничные случаи: пустые входные данные, выход за пределы, отрицательные значения

**Конфигурация:**
- Нет встроенного coverage-инструмента в конфигурации CMake
- Покрытие визуально не измеряется, только review-based

## Типы тестов

**Unit-тесты (основной тип):**
- Проверяют один компонент в изоляции
- Например: `test_shared_state.cpp` тестирует только `SharedState`, `test_sim_bus.cpp` — только `SimBus`
- Реальные зависимости — никаких сторонних моков

**Интеграционные тесты:**
- Тесты эффектов: реальный `ZoneSystem` + реальный `EffectPlugin` + реальный `SharedState`
- Тесты `SimEngine`: полный цикл тика с агентами, зонами, плагинами
- Паттерн: `test_effect_charging.cpp`, тест `ChargingEffect_ComponentAccessibleViaState` — создаёт `SimEngine` с зоной и агентом, вызывает `step()`

**Smoke-тесты:**
- `test_smoke.cpp`: проверка компилятора и наличия Eigen
- Запускаются первыми в сьюте

**E2E-тесты:**
- Отсутствуют в автоматическом виде
- Визуальная проверка через браузер `http://localhost:1937` после `docker compose up sim`

## Общие паттерны

**Тестирование событийной системы (SimBus):**
```cpp
// Подписываемся, фиксируем результат в локальные переменные
SimBus bus;
AgentId entered_agent = 999;
ZoneId  entered_zone;
bus.subscribe<event::AgentEnteredZone>([&](const event::AgentEnteredZone& e) {
    entered_agent = e.agent;
    entered_zone  = e.zone;
});

// Выполняем действие
zs.tick(agents, actors, bus, 0.0, 0.01);

// Проверяем что событие пришло и данные корректны
EXPECT_EQ(entered_agent, 1u);
EXPECT_EQ(entered_zone, "zone1");
```

**Тестирование contribution-based состояния (SharedState):**
```cpp
// Добавляем contributions вручную
agent.state.add_scale(0.5, "ice_zone");
agent.state.add_lock(true, "estop");

// Вызываем resolve() вручную (в реальной системе вызывает SimEngine)
agent.state.resolve();

// Проверяем effective()
EXPECT_NEAR(agent.state.effective().speed_scale, 0.5, 1e-9);
EXPECT_TRUE(agent.state.effective().motion_locked);
```

**Тестирование ZoneSystem с несколькими тиками:**
```cpp
// Первый тик: агент входит в зону
zs.tick(agents, actors, bus, 0.0, 0.01);
EXPECT_EQ(zs.all_zones()[0].inside_agents.count(1u), 1u);

// Перемещаем агента
agents[0].world_pose.x = 5.0;

// Второй тик: агент выходит из зоны
zs.tick(agents, actors, bus, 0.01, 0.01);
EXPECT_EQ(zs.all_zones()[0].inside_agents.count(1u), 0u);
```

**Тестирование с проверкой указателей:**
```cpp
// Всегда ASSERT_NE перед разыменованием
const auto* bat = agent.state.get<BatteryComponent>();
ASSERT_NE(bat, nullptr) << "BatteryComponent должен быть создан";
EXPECT_NEAR(bat->level, 0.6, 1e-9);
```

**Тестирование с числовой точностью:**
```cpp
EXPECT_NEAR(value, expected, 1e-9);    // для double, стандартная точность
EXPECT_DOUBLE_EQ(value, expected);     // строгое сравнение double
EXPECT_NEAR(value, expected, 0.01);    // для углов и физики (менее строго)
```

**Тестирование через SimEngine (интеграция):**
```cpp
SimEngine engine{{.update_rate = 100.0, .viz_rate = 30.0}};
engine.set_effect_factory(s2::create_effect);  // если нужны эффекты из реестра

SimWorld world;
world.add_agent(std::move(agent));
world.add_zone(std::move(zone));
engine.load_world(std::move(world));

engine.step(1);  // или engine.step(N) для нескольких тиков

const Agent* a = engine.world().get_agent(1);
ASSERT_NE(a, nullptr);
EXPECT_GT(a->state.get<BatteryComponent>()->level, 0.7);
```

**Тестирование необратимых мутаций:**
```cpp
// Применяем мутацию через ZoneSystem
zs.tick(agents, actors, bus, 0.0, 0.01);
EXPECT_TRUE(agents[0].state.get<TirePunctureData>()->punctured);

// Выходим из зоны
agents[0].world_pose.x = 200.0;
zs.tick(agents, actors, bus, 0.01, 0.01);

// Мутация сохраняется после выхода
EXPECT_TRUE(agents[0].state.get<TirePunctureData>()->punctured);
```

## Добавление новых тестов

**При добавлении нового EffectPlugin:**
1. Создать `workspace/s2_core/tests/test_effect_<тип>.cpp`
2. Добавить файл в список `add_executable(s2_core_tests ...)` в `CMakeLists.txt`
3. Покрыть: инициализацию через `on_init(YAML::Load(...))`, основное поведение, граничные случаи, проверку capability-фильтра, lifecycle (enter/continuous/exit)

**При добавлении нового IAgentPlugin:**
1. Создать `workspace/s2_core/tests/test_<имя>_plugin.cpp`
2. Добавить в `CMakeLists.txt`
3. Покрыть: `from_config()`, `initialize()`, `update()`, `pre_resolve()`, `to_json()` если нетривиальный

**При добавлении нового события SimBus:**
1. Добавить тест в `test_sim_bus.cpp`, тест `AllEventTypesDelivered`
2. Убедиться что событие корректно типизировано

---

*Testing analysis: 2026-04-25*
*Update when test patterns change*
