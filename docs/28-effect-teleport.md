# Задача 28 — Эффект MUTATION: TeleportEffect

## Цель

Зона-телепорт перемещает агентов (или пропы, или всех) в заданную точку.
Три режима триггера: сразу при входе, после N секунд нахождения, по явной команде.
Destination может быть фиксированной или задаваться в рантайме.

## Зависимости

- Задача 27 (MUTATION-механизм в ZoneSystem)
- `sim_bus.hpp` — TeleportAgentCommand (Kernel Command)

---

## Что сделать

### 1. TeleportTargetData — динамическая точка назначения

**Файл:** `workspace/s2_core/include/s2/components/teleport_target_data.hpp` (новый)

```cpp
#pragma once
#include <s2/types.hpp>

namespace s2 {

/// Данные о точке назначения для телепорта, задаваемые в рантайме.
/// Хранится в ZoneSystem (не в SharedState агента).
struct TeleportTargetData {
    Vec3 destination{Vec3::Zero()};
    double destination_yaw{0.0};
    bool has_destination{false};
};

} // namespace s2
```

### 2. Kernel Command: TeleportAgentCommand

**Файл:** `workspace/s2_core/include/s2/sim_bus.hpp`

Добавить события:

```cpp
/// Команда: телепортировать агента в точку.
struct TeleportAgentCommand {
    AgentId agent_id;
    Vec3 destination;
    double yaw{0.0};
};

/// Команда: установить runtime-точку назначения зоны телепорта.
struct SetZoneTeleportTargetCommand {
    ZoneId zone_id;
    Vec3 destination;
    double yaw{0.0};
};
```

### 3. TeleportEffect

**Файл:** `workspace/s2_plugins/effects/teleport_effect.hpp` (новый)

```cpp
#pragma once
#include <s2/interfaces/effect_plugin.hpp>
#include <s2/sim_bus.hpp>
#include <unordered_map>

namespace s2::effects {

/// Телепортирует агентов при выполнении условия триггера.
///
/// trigger_mode:
///   "immediate"     — сразу при входе в зону (однократно)
///   "after_seconds" — после N секунд нахождения внутри
///   "on_command"    — по явной команде (агент с TeleportActivatorPlugin)
///
/// target_mode:
///   "fixed"   — destination фиксирован в конфиге
///   "runtime" — destination задаётся через SetZoneTeleportTargetCommand
///
/// target_entity: "agent" | "prop" | "all"
class TeleportEffect : public EffectPlugin {
public:
    void on_init(const YAML::Node& params) override {
        trigger_mode_  = params["trigger_mode"].as<std::string>("immediate");
        target_mode_   = params["target_mode"].as<std::string>("fixed");
        target_entity_ = params["target_entity"].as<std::string>("agent");
        trigger_secs_  = params["after_seconds"].as<double>(3.0);

        if (params["destination"]) {
            const auto& d = params["destination"];
            fixed_destination_.x() = d["x"].as<double>(0);
            fixed_destination_.y() = d["y"].as<double>(0);
            fixed_destination_.z() = d["z"].as<double>(0);
            fixed_yaw_ = d["yaw"].as<double>(0.0);
            has_fixed_ = true;
        }
    }

    EffectType effect_type() const override {
        // Для "immediate" — MUTATION (одноразово при входе).
        // Для "after_seconds" и "on_command" — CONTINUOUS (отслеживаем время/команду).
        return (trigger_mode_ == "immediate") ? EffectType::MUTATION : EffectType::CONTINUOUS;
    }

    std::vector<std::string> required_capabilities() const override {
        return {};
    }

    /// Вызывается для trigger_mode = "immediate"
    void apply_mutation(SharedState& state, const EffectContext& ctx) override {
        if (!should_teleport_entity(ctx)) return;
        do_teleport(state, ctx);
    }

    /// Вызывается каждый тик для trigger_mode = "after_seconds"
    void apply_continuous(SharedState& state, const EffectContext& ctx) override {
        if (trigger_mode_ == "after_seconds") {
            time_inside_[ctx.agent_id] += ctx.dt;
            if (time_inside_[ctx.agent_id] >= trigger_secs_) {
                time_inside_[ctx.agent_id] = 0.0;
                if (!teleported_.count(ctx.agent_id)) {
                    teleported_.insert(ctx.agent_id);
                    do_teleport(state, ctx);
                }
            }
        }
    }

    /// Вызывается ZoneSystem при выходе агента (для сброса счётчика).
    void on_agent_exit(AgentId agent_id) {
        time_inside_.erase(agent_id);
        teleported_.erase(agent_id);
    }

    std::optional<VisualHint> visual_hint() const override {
        return VisualHint{
            "glow",
            {{"color", "#CC44FF"}, {"pulse_rate", 3.0}, {"intensity", 1.0}}
        };
    }

private:
    bool should_teleport_entity(const EffectContext& ctx) const {
        // target_entity фильтрация (prop — задача 35, пока только agent)
        return target_entity_ == "agent" || target_entity_ == "all";
    }

    void do_teleport(SharedState& state, const EffectContext& ctx) {
        Vec3 dest = has_fixed_ ? fixed_destination_ : runtime_destination_;
        double yaw = has_fixed_ ? fixed_yaw_ : runtime_yaw_;
        // Публикуем TeleportAgentCommand — SimEngine обрабатывает в следующем тике
        // Прямой доступ к SimBus через контекст (нужно добавить bus в EffectContext)
        // Временный механизм: сохранить pending teleport в SharedState
        state.emplace<PendingTeleport>(PendingTeleport{dest, yaw, true});
    }

    std::string trigger_mode_{"immediate"};
    std::string target_mode_{"fixed"};
    std::string target_entity_{"agent"};
    double trigger_secs_{3.0};

    Vec3 fixed_destination_{Vec3::Zero()};
    double fixed_yaw_{0.0};
    bool has_fixed_{false};

    Vec3 runtime_destination_{Vec3::Zero()};
    double runtime_yaw_{0.0};

    // Состояние на агента
    std::unordered_map<AgentId, double> time_inside_;
    std::unordered_set<AgentId> teleported_;
};

} // namespace s2::effects
```

### 4. PendingTeleport в SharedState

**Файл:** `workspace/s2_core/include/s2/components/pending_teleport.hpp` (новый)

```cpp
#pragma once
#include <s2/types.hpp>

namespace s2 {

/// Отложенный телепорт агента — обрабатывается SimEngine в конце тика.
struct PendingTeleport {
    Vec3 destination;
    double yaw{0.0};
    bool pending{false};
};

} // namespace s2
```

### 5. SimEngine: обработка PendingTeleport

**Файл:** `workspace/s2_core/include/s2/sim_engine.hpp`

В конце фазы 3 (после collision, перед snapshot):

```cpp
// Применить отложенные телепорты
auto* pt = agent.state.get<PendingTeleport>();
if (pt && pt->pending) {
    agent.world_pose.x = pt->destination.x();
    agent.world_pose.y = pt->destination.y();
    agent.world_pose.z = pt->destination.z();
    agent.world_pose.yaw = pt->yaw;
    // Обнулить скорость (иначе сразу улетит снова)
    agent.world_velocity.linear = Vec3::Zero();
    agent.world_velocity.angular = Vec3::Zero();
    pt->pending = false;
}
```

### 6. EffectPlugin::on_agent_exit callback

**Файл:** `workspace/s2_core/include/s2/interfaces/effect_plugin.hpp`

Добавить метод для уведомления о выходе:

```cpp
/// Вызывается ZoneSystem при выходе агента из зоны.
/// Плагины могут использовать для сброса per-agent состояния.
virtual void on_agent_exit(AgentId agent_id) {}
```

ZoneSystem вызывает его в `on_agent_exit()` для каждого плагина зоны.

### 7. Пример YAML

```yaml
zones:
  # Немедленный телепорт к origin
  - id: "portal_in"
    shape:
      type: cylinder
      center: {x: 8.0, y: 0.0, z: 0.5}
      radius: 0.8
      half_height: 1.0
    color: "#CC44FF"
    label: "Портал"
    effects:
      - type: teleport
        params:
          trigger_mode: immediate
          target_mode: fixed
          target_entity: agent
          destination: {x: -8.0, y: 0.0, z: 0.0, yaw: 3.14}

  # Телепорт после 3 секунд стояния
  - id: "slow_portal"
    shape:
      type: sphere
      center: {x: 0.0, y: 8.0, z: 0.0}
      radius: 1.5
    color: "#8844FF"
    label: "Медленный портал"
    effects:
      - type: teleport
        params:
          trigger_mode: after_seconds
          after_seconds: 3.0
          destination: {x: 0.0, y: -8.0, z: 0.0, yaw: 0.0}
```

---

## Тесты

**Файл:** `workspace/s2_core/tests/test_effect_teleport.cpp`

- `TeleportImmediate_OnEntry` — агент входит в зону: в следующем тике позиция = destination
- `TeleportImmediate_VelocityClearedAfterTeleport` — скорость после телепорта = 0
- `TeleportImmediate_OncePerEntry` — агент телепортируется только при первом входе
- `TeleportAfterSeconds_TimerAccumulates` — агент 2 сек в зоне: не телепортирован; 4 сек: телепортирован
- `TeleportAfterSeconds_ExitResetsTimer` — агент вышел из зоны → таймер сброшен → новый вход счёт заново
- `TeleportAfterSeconds_TeleportedOnlyOnce` — агент в зоне 10 сек: телепортируется один раз (не каждые 3 сек)
- `TeleportEntityFilter_AgentOnly` — target_entity="agent": агент телепортируется; проп — нет

---

## Критерии завершения

- [ ] TeleportEffect.apply_mutation() создаёт PendingTeleport в SharedState
- [ ] SimEngine применяет PendingTeleport в конце тика (сброс позиции + скорости)
- [ ] trigger_mode "immediate" работает (MUTATION, однократно)
- [ ] trigger_mode "after_seconds" работает (CONTINUOUS, таймер сбрасывается при выходе)
- [ ] Все тесты проходят в Docker
