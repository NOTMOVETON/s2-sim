# Task 01 — Базовые типы и структуры данных

> **Предыдущий шаг:** `00-infrastructure.md` (Docker + CMake работают)
> **Следующий шаг:** `02-tick-loop.md`
> **Контекст:** `ARCHITECTURE.md` разделы 3, 4, 7

## Цель

Определить все базовые типы данных проекта. После этого шага: типы компилируются, тесты на базовые операции проходят.

## Что сделать

### 1. types.hpp — базовые типы

```
s2_core/include/s2/types.hpp
```

Содержит:

```cpp
#pragma once
#include <Eigen/Dense>
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <optional>

namespace s2 {

// === ID types ===
using AgentId = uint32_t;
using ActorId = uint32_t;
using ObjectId = uint32_t;  // Prop
using ZoneId = std::string;  // строковый id для читаемости конфигов
using EntityId = uint32_t;   // generic

// === Spatial ===
struct Vec3 {
    double x{0}, y{0}, z{0};
    // базовые операции: +, -, *, length, normalized
};

struct Pose3D {
    double x{0}, y{0}, z{0};
    double roll{0}, pitch{0}, yaw{0};
};

struct Velocity {
    double vx{0}, vy{0}, vz{0};  // linear
    double wx{0}, wy{0}, wz{0};  // angular
};

struct Transform3D {
    Eigen::Vector3d translation{0, 0, 0};
    Eigen::Matrix3d rotation{Eigen::Matrix3d::Identity()};
};

// === Collision primitives ===
enum class ShapeType { SPHERE, BOX, CAPSULE, CYLINDER };

struct CollisionShape {
    ShapeType type{ShapeType::SPHERE};
    double radius{0.5};           // SPHERE, CAPSULE, CYLINDER
    double height{1.0};           // CAPSULE, CYLINDER
    Vec3 size{1.0, 1.0, 1.0};    // BOX
};

// === Visual ===
struct VisualDesc {
    std::string type{"box"};      // "box", "cylinder", "sphere", "capsule"
    Vec3 size{1.0, 1.0, 1.0};
    double radius{0.5};
    double height{1.0};
    std::string color{"#FF6B35"};
};

// === Desired velocity (output of Actuation) ===
struct DesiredVelocity {
    double vx{0}, vy{0}, vz{0};
    double wx{0}, wy{0}, wz{0};
    bool valid{true};   // false = нет команды
};

// === Zone shape ===
enum class ZoneShapeType { SPHERE, AABB, INFINITE };

struct ZoneShape {
    ZoneShapeType type{ZoneShapeType::SPHERE};
    Vec3 center{0, 0, 0};
    double radius{1.0};          // SPHERE
    Vec3 half_size{1, 1, 1};     // AABB

    bool contains(const Vec3& point) const;
};

// === Effect types ===
enum class EffectType { MODIFIER, CONTINUOUS, MUTATION, SENSOR };

// === Actor state (for FSM) ===
using ActorState = std::string;  // "closed", "opening", "open", ...

} // namespace s2
```

**Важно:**
- Vec3 — своя простая структура, НЕ Eigen. Eigen используется внутри для тяжёлых вычислений (kinematic tree). Vec3 — для API, простой и удобный.
- Реализовать `ZoneShape::contains()` для всех трёх типов.
- Реализовать базовые операции Vec3 (+, -, scalar multiply, length, normalized).

### 2. shared_state.hpp — Shared State + Contributions + Resolver

```
s2_core/include/s2/shared_state.hpp
```

Это ключевая структура. Три уровня:

```cpp
#pragma once
#include <s2/types.hpp>
#include <any>
#include <typeindex>
#include <functional>

namespace s2 {

// === Contribution ===
struct ScaleContribution {
    double value{1.0};
    std::string source;  // "battery", "ice_zone_3", "payload"
};

struct AdditiveContribution {
    Vec3 value{0, 0, 0};
    std::string source;
};

struct LockContribution {
    bool locked{false};
    std::string source;
};

// === Resolved effective state ===
struct EffectiveConstraints {
    double speed_scale{1.0};       // product of all scale contributions
    bool motion_locked{false};      // OR of all lock contributions
    Vec3 velocity_addition{0,0,0};  // sum of all additive contributions

    // Для debug/визуализации: сохраняем все contributions
    std::vector<ScaleContribution> scale_sources;
    std::vector<LockContribution> lock_sources;
    std::vector<AdditiveContribution> additive_sources;
};

// === Shared State ===
class SharedState {
public:
    // --- Single-owner fields ---
    // Type-safe storage. Модуль регистрирует при init, читает/пишет по типу.
    template<typename T, typename... Args>
    T& emplace(Args&&... args);

    template<typename T>
    T* get();

    template<typename T>
    const T* get() const;

    template<typename T>
    bool has() const;

    // --- Contributions ---
    void add_scale(double value, const std::string& source);
    void add_velocity_addition(const Vec3& value, const std::string& source);
    void add_lock(bool locked, const std::string& source);

    // --- Resolver ---
    void resolve();  // вычисляет effective из contributions
    void clear_contributions();  // очистить перед новым тиком

    const EffectiveConstraints& effective() const { return effective_; }

private:
    // Single-owner: type-indexed
    std::unordered_map<std::type_index, std::any> fields_;

    // Contributions (заполняются за тик, очищаются в начале следующего)
    std::vector<ScaleContribution> scale_contribs_;
    std::vector<AdditiveContribution> additive_contribs_;
    std::vector<LockContribution> lock_contribs_;

    // Resolved
    EffectiveConstraints effective_;
};

} // namespace s2
```

**Resolver логика:**
```
effective_speed_scale = clamp(product(all scale values), 0.0, 10.0)
effective_motion_locked = any(lock.locked for lock in locks)
effective_velocity_addition = sum(all additive values)
```

### 3. sim_bus.hpp — Event Bus

```
s2_core/include/s2/sim_bus.hpp
```

Типизированная шина событий:

```cpp
#pragma once
#include <s2/types.hpp>
#include <functional>
#include <vector>
#include <typeindex>
#include <any>
#include <unordered_map>

namespace s2 {

// Стандартные события
namespace event {
    struct AgentEnteredZone   { AgentId agent; ZoneId zone; };
    struct AgentExitedZone    { AgentId agent; ZoneId zone; };
    struct ObjectAttached     { ObjectId obj; AgentId agent; std::string link; };
    struct ObjectReleased     { ObjectId obj; AgentId agent; };
    struct ActorStateChanged  { ActorId actor; ActorState old_state; ActorState new_state; };
    struct AgentCollision     { AgentId agent; Vec3 point; };
}

class SimBus {
public:
    template<typename EventT>
    void subscribe(std::function<void(const EventT&)> handler);

    template<typename EventT>
    void publish(const EventT& event);

private:
    std::unordered_map<std::type_index,
        std::vector<std::function<void(const std::any&)>>> handlers_;
};

} // namespace s2
```

Dispatch синхронный — вызов подписчиков в момент publish.

## Тесты

```
s2_core/tests/test_types.cpp
s2_core/tests/test_shared_state.cpp
s2_core/tests/test_sim_bus.cpp
```

### test_types.cpp
- Vec3: операции (+, -, *, length, normalized)
- ZoneShape::contains() для SPHERE, AABB, INFINITE
- Pose3D конструктор по умолчанию = нули

### test_shared_state.cpp
- emplace/get работают (BatteryComponent с level)
- has() возвращает true/false корректно
- Contributions: добавить 3 scale (0.8, 0.5, 1.2), resolve → effective = 0.48
- Contributions: добавить lock(true) → effective.motion_locked = true
- Contributions: additive Vec3(1,0,0) + Vec3(0,1,0) → effective = (1,1,0)
- clear_contributions() очищает, resolve после clear → defaults (1.0, false, 0,0,0)
- effective().scale_sources содержит все источники с именами

### test_sim_bus.cpp
- subscribe + publish → handler вызван
- два подписчика на один тип → оба вызваны
- publish без подписчиков → не падает
- разные типы событий не пересекаются

## Критерии приёмки

```bash
docker compose run tests   # все тесты проходят
```

- types.hpp компилируется без warnings
- SharedState работает с resolve
- SimBus доставляет события

## Чего НЕ делать

- Не реализовывать Agent, World, Engine — это следующие задачи
- Не оптимизировать SharedState (std::any нормально для 100 агентов)
- Не добавлять сериализацию
