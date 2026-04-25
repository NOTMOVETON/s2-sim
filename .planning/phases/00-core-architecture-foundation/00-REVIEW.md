---
phase: 00-core-architecture-foundation
reviewed: 2026-04-25T12:00:00Z
depth: standard
files_reviewed: 12
files_reviewed_list:
  - workspace/s2_core/include/s2/event_bus.hpp
  - workspace/s2_core/include/s2/types.hpp
  - workspace/s2_core/include/s2/world_query.hpp
  - workspace/s2_core/include/s2/plugin_base.hpp
  - workspace/s2_core/include/s2/kernel_command.hpp
  - workspace/s2_core/include/s2/sim_engine.hpp
  - workspace/s2_core/include/s2/scene_loader.hpp
  - workspace/s2_core/include/s2/agent.hpp
  - workspace/s2_core/include/s2/sim_bus.hpp
  - workspace/s2_plugins/include/s2/plugins/diff_drive.hpp
  - workspace/s2_plugins/include/s2/plugins/battery.hpp
  - workspace/s2_plugins/src/plugins_registry.cpp
findings:
  critical: 2
  warning: 6
  info: 4
  total: 12
status: issues_found
---

# Phase 00: Code Review Report

**Reviewed:** 2026-04-25T12:00:00Z
**Depth:** standard
**Files Reviewed:** 12
**Status:** issues_found

## Summary

Ревью охватывает всю архитектурную основу Phase 0: шину событий, типы,
WorldQuery, интерфейс плагинов, команды ядра, SimEngine, SceneLoader, Agent,
два плагина (DiffDrive, Battery) и реестр плагинов.

Общая архитектура — чистая и хорошо документированная. Основные паттерны
(однонаправленный поток команд через KernelCommand, swap-under-mutex для очереди
команд, PluginContext без прямого доступа к SimWorld) выбраны правильно.

Найдено два критических дефекта: гонка данных при вызове `pause()`/`resume()`
из внешнего треда, и ссылки в `PluginContext` на локальные переменные стека,
которые инвалидируются при перемещении агентов в векторе. Шесть предупреждений
касаются логических ошибок и отсутствия проверок граничных условий.

---

## Critical Issues

### CR-01: Гонка данных на `paused_` между HTTP-тредом и sim-тредом

**File:** `workspace/s2_core/include/s2/sim_engine.hpp:210-215, 492, 1012`

**Issue:** `paused_` объявлен как `bool` (не `std::atomic<bool>`), тогда как
`running_` объявлен атомиком. При этом `pause()`, `resume()` и `is_paused()`
вызываются из HTTP-треда (внешний API), а `tick()` читает `paused_` в
симуляционном треде. Это классическая гонка данных — неопределённое поведение
по стандарту C++11 и выше.

```cpp
// sim_engine.hpp:1011-1012
std::atomic<bool> running_{false};
bool paused_{false};  // BUG: должен быть std::atomic<bool>
```

**Fix:**
```cpp
std::atomic<bool> running_{false};
std::atomic<bool> paused_{false};
```

Все чтения/записи `paused_` уже тривиальны (bool), поэтому переход к
`std::atomic<bool>` не требует изменений в местах использования.

---

### CR-02: Ссылки в `PluginContext` указывают на локальный стек — потенциальный dangling reference

**File:** `workspace/s2_core/include/s2/sim_engine.hpp:609-611, 768-770, 796-798`

**Issue:** В трёх фазах (3, 4, 5) создаётся локальная `KernelCommandQueue tick_cmds`,
и `PluginContext` формируется со ссылкой на неё. Затем `ctx` передаётся в
`plugin->update(dt_, agent, ctx)`. Если плагин сохраняет `ctx` (или ссылку на
`ctx.commands`) за пределами вызова `update()` — получается dangling reference.

Сам интерфейс `IAgentPlugin::update()` не запрещает это явно; плагин может
сохранить `&ctx.commands` в поле класса.

```cpp
// phase3_agents() — то же самое в phase4 и phase5
KernelCommandQueue tick_cmds;
PluginContext ctx{null_world_query_, bus_, tick_cmds};  // tick_cmds — локальная переменная стека
for (auto& plugin : agent.plugins)
    plugin->update(dt_, agent, ctx);
// tick_cmds уничтожается в конце блока; любой сохранённый &ctx.commands — dangling
```

**Fix:** Задокументировать контракт в `IAgentPlugin::update()` явно, что `ctx`
действителен только на время вызова:

```cpp
// В plugin_base.hpp, перед объявлением update():
// ВАЖНО: ctx действителен только в течение вызова update().
// Плагины НЕ должны сохранять ссылки на ctx или ctx.commands за пределами вызова.
virtual void update(double dt, Agent& agent, const PluginContext& ctx) = 0;
```

Дополнительно можно рассмотреть передачу `PluginContext` по значению (структура
содержит только три ссылки, копирование дешёво) чтобы явно сигнализировать о
временном характере контекста. Это не устранит дефект полностью, но уберёт
иллюзию хранимого объекта.

---

## Warnings

### WR-01: `update_rate = 0` вызывает деление на ноль в конструкторе SimEngine

**File:** `workspace/s2_core/include/s2/sim_engine.hpp:78`

**Issue:** `dt_ = 1.0 / config_.update_rate;` — если `update_rate` равен 0 (или
отрицательный, например, при неверном YAML), результат — +infinity или NaN.
Все последующие обновления позиций (`pos += vel * dt_`) молча дадут NaN.

```cpp
explicit SimEngine(Config config)
    : config_(std::move(config))
{
  dt_ = 1.0 / config_.update_rate;  // UB/NaN если update_rate <= 0
}
```

**Fix:**
```cpp
explicit SimEngine(Config config)
    : config_(std::move(config))
{
  if (config_.update_rate <= 0.0)
    throw std::invalid_argument("SimEngine: update_rate должен быть > 0");
  dt_ = 1.0 / config_.update_rate;
}
```

---

### WR-02: `handle_plugin_input()` вызывается из HTTP-треда без mutex

**File:** `workspace/s2_core/include/s2/sim_engine.hpp:304-317`

**Issue:** `handle_plugin_input()` итерирует `world_.agents()` и вызывает
`plugin->handle_input()` напрямую из вызывающего треда (HTTP). При этом
sim-тред может одновременно вызывать `plugin->update()` того же плагина.
`push_command()` защищён mutex (строки 154-157), а `handle_plugin_input()` —
нет.

```cpp
bool handle_plugin_input(AgentId agent_id, const std::string& plugin_type,
                          const std::string& json_input)
{
  for (auto& agent : world_.agents()) {   // читается из HTTP-треда без lock
    if (agent.id == agent_id) {
      for (auto& plugin : agent.plugins) {
        if (...) {
          plugin->handle_input(json_input);  // записывает external_linear_velocity_ без lock
          return true;
```

В `DiffDrivePlugin::handle_input()` записывается `external_linear_velocity_`,
которую `update()` читает в sim-треде без синхронизации.

**Fix:** Использовать `push_command()` для передачи внешних команд плагинам,
либо защитить чтение/запись внешних команд отдельным mutex внутри плагина,
либо добавить общий lock на операции с `world_` из HTTP-треда.

---

### WR-03: `EventBus::publish()` итерирует `handlers_` без защиты от модификации во время dispatch

**File:** `workspace/s2_core/include/s2/event_bus.hpp:168-175`

**Issue:** Документация (`event_bus.hpp:18`) явно указывает: «НЕ поддерживает
отписку во время dispatch». Однако при реентерабельном dispatch (подписчик
вызывает `publish()` для другого события, чей обработчик подписывается на тот
же тип) итератор `for (const auto& handler : it->second)` инвалидируется из-за
потенциального `push_back` в `handlers_[typeid(EventT)]`.

```cpp
template <typename EventT>
void publish(const EventT& event)
{
  auto it = handlers_.find(typeid(EventT));
  if (it == handlers_.end()) return;
  for (const auto& handler : it->second)  // итератор может инвалидироваться
    handler(event);                        // если handler вызывает subscribe<EventT>
}
```

**Fix:** Перед итерацией скопировать вектор обработчиков:
```cpp
template <typename EventT>
void publish(const EventT& event)
{
  auto it = handlers_.find(typeid(EventT));
  if (it == handlers_.end()) return;
  // Копируем локально: защита от реентерабельного subscribe во время dispatch
  auto local_handlers = it->second;
  for (const auto& handler : local_handlers)
    handler(event);
}
```

---

### WR-04: Yaw нормализуется с использованием магических констант вместо `M_PI`

**File:** `workspace/s2_core/include/s2/sim_engine.hpp:649-652`

**Issue:** Нормализация yaw использует жёстко зашитый литерал
`3.14159265358979323846` вместо `M_PI` или `std::numbers::pi`. При этом в
`scene_loader.hpp:148` используется стандартный `M_PI`. Несоответствие стилей,
и константа повторяется дважды в пределах 4 строк.

```cpp
agent.world_pose.yaw = std::fmod(agent.world_pose.yaw,
                                  2.0 * 3.14159265358979323846);
if (agent.world_pose.yaw < 0) {
  agent.world_pose.yaw += 2.0 * 3.14159265358979323846;
}
```

**Fix:**
```cpp
constexpr double two_pi = 2.0 * M_PI;
agent.world_pose.yaw = std::fmod(agent.world_pose.yaw, two_pi);
if (agent.world_pose.yaw < 0)
  agent.world_pose.yaw += two_pi;
```

---

### WR-05: `SceneLoader::load()` вызывает `root["s2"]` дважды без проверки null

**File:** `workspace/s2_core/include/s2/scene_loader.hpp:87, 104`

**Issue:** Первый доступ `if (const auto& s2 = root["s2"])` проверяет
существование секции. Второй доступ `if (const auto& world = root["s2"]["world"])`
на строке 104 повторяет `root["s2"]` без проверки: если `root["s2"]` не
существует, yaml-cpp вернёт `Null` node, и `["world"]` вернёт тоже `Null`
node — поведение безопасно для yaml-cpp, но семантически неожиданно.
Однако критичнее: в `root["s2"]["agents"]` (строка 122) при отсутствующем `s2`
формируется пустой узел и цикл не выполняется — это молчаливо проглатывает
невалидный YAML вместо выдачи ошибки.

**Fix:** Вынести `root["s2"]` в переменную и передавать её вниз:
```cpp
const auto& s2_node = root["s2"];
if (!s2_node) {
    throw std::runtime_error("SceneLoader: отсутствует секция 's2' в YAML");
}
// Использовать s2_node["world"], s2_node["agents"], s2_node["zones"]
```

---

### WR-06: `plugin_bus_` объявлен в SimEngine, но нигде не используется в PluginContext

**File:** `workspace/s2_core/include/s2/sim_engine.hpp:1034`

**Issue:** В `sim_engine.hpp` объявлено поле `EventBus plugin_bus_`
(«Шина для плагинов (отдельная от bus_)»), однако во всех трёх фазах (3, 4, 5)
в `PluginContext` передаётся `bus_` (общая шина зоновой системы), а не
`plugin_bus_`. Это нарушает предполагаемую изоляцию и делает `plugin_bus_`
мёртвым кодом.

```cpp
// phase3_agents():
PluginContext ctx{null_world_query_, bus_, tick_cmds};  // bus_, не plugin_bus_

// Поле никогда не используется:
EventBus plugin_bus_;  // строка 1034
```

**Fix:** Либо использовать `plugin_bus_` в `PluginContext` (если изоляция нужна),
либо удалить `plugin_bus_` и оставить `bus_`. Текущее смешение нарушает
архитектурный принцип разделения шин.

---

## Info

### IN-01: `Signal::range` имеет противоречивый комментарий по умолчанию

**File:** `workspace/s2_core/include/s2/types.hpp:376`

**Issue:** Поле `range{0.0}` имеет комментарий «0 = нет ограничений не по умолчанию»
— текст явно содержит опечатку («не по умолчанию» вместо «по умолчанию») и
неоднозначен. В описании типа указано: «wire-сигнал — `range = infinity`», но
значение по умолчанию `0.0`, а не `infinity`. Неясно, интерпретирует ли
`find_signals_of_type` значение 0 как «без ограничений» или как «нулевая дальность».

**Fix:** Уточнить семантику нуля в комментарии и/или изменить значение по
умолчанию на `std::numeric_limits<double>::infinity()` для согласованности с
описанием wire-сигнала.

---

### IN-02: `list_plugin_schemas()` использует O(n²) алгоритм для дедупликации

**File:** `workspace/s2_plugins/src/plugins_registry.cpp:98-113`

**Issue:** Внешний цикл по `map` + внутренний цикл по `order` для проверки
«уже добавлен» — O(n*m). При текущем размере реестра (~11 плагинов) это
незначительно, но паттерн неидиоматичен.

**Fix:** Использовать `std::unordered_set<std::string>` для O(1) проверки:
```cpp
std::unordered_set<std::string> added(order.begin(), order.end());
for (const auto& [type_name, factory] : map) {
    if (added.count(type_name)) continue;
    // ...
}
```

---

### IN-03: `parse_heightmap()` игнорирует поле `path:` при наличии в YAML

**File:** `workspace/s2_core/include/s2/scene_loader.hpp:475-477`

**Issue:** Ветка `if (const auto& hm = node["path"])` определяет, что путь к
heightmap задан, но при этом возвращает `Heightmap::flat(40.0, 40.0, 0.0)` —
т.е. настоящий heightmap никогда не загружается. Это молчаливое игнорирование
конфигурации без предупреждения пользователя.

```cpp
if (const auto& hm = node["path"]) {
    return Heightmap::flat(40.0, 40.0, 0.0);  // TODO: загрузить файл
}
```

**Fix:** Добавить хотя бы предупреждение в лог:
```cpp
if (const auto& hm = node["path"]) {
    std::cerr << "[SceneLoader] WARNING: heightmap path '" << hm.as<std::string>()
              << "' указан, но загрузка из файла не реализована (TODO).\n";
    return Heightmap::flat(40.0, 40.0, 0.0);
}
```

---

### IN-04: `DiffDrivePlugin::handle_input()` парсит JSON через YAML::Load

**File:** `workspace/s2_plugins/include/s2/plugins/diff_drive.hpp:167`

**Issue:** `handle_input(json_input)` получает JSON-строку (по контракту
`IAgentPlugin`), но парсит её через `YAML::Load(json_input)`. YAML является
надмножеством JSON, поэтому большинство корректных JSON-объектов парсируются
корректно. Однако это неочевидно и может сломаться на JSON с unicode escapes
(`\uXXXX`) или числами в экспоненциальном формате в зависимости от версии yaml-cpp.

**Fix:** Использовать `nlohmann::json::parse()` для консистентности с остальным кодом:
```cpp
void handle_input(const std::string& json_input) override
{
    try {
        auto data = nlohmann::json::parse(json_input);
        if (data.contains("linear_velocity")) {
            external_linear_velocity_ = std::clamp(
                data["linear_velocity"].get<double>(), -max_linear_, max_linear_);
            has_external_input_ = true;
        }
        if (data.contains("angular_velocity")) {
            external_angular_velocity_ = std::clamp(
                data["angular_velocity"].get<double>(), -max_angular_, max_angular_);
            has_external_input_ = true;
        }
    } catch (const std::exception&) {
        // Игнорируем некорректный ввод
    }
}
```

---

_Reviewed: 2026-04-25T12:00:00Z_
_Reviewer: Claude (gsd-code-reviewer)_
_Depth: standard_
