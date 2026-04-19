# Задача 34 — Actor: ConveyorActor (лента-конвейер)

## Цель

Конвейер — визуальный актор с коллизионными бортами и attached-зоной `ConveyorEffect`.
Анимация ленты отображается в визуализаторе (стрелки на поверхности).
Скорость ленты можно изменять в рантайме через команду к актору.

После задачи: робот, въезжающий на конвейер, сносится в направлении ленты,
даже не двигаясь своими колёсами.

## Зависимости

- Задача 25 (ConveyorEffect — уже реализован)
- Задача 32 (IActorBehavior, Actor struct)
- Задача 23 (ZoneSystem, attached zones)
- Задача 30 (VisualHint `arrows` в зоне)

---

## Что сделать

### 1. ConveyorBehavior

**Файл:** `workspace/s2_plugins/behaviors/conveyor_behavior.hpp` (новый)

```cpp
#pragma once
#include <s2/interfaces/actor_behavior.hpp>
#include <s2/actor.hpp>
#include <s2/sim_bus.hpp>
#include <s2/world.hpp>

namespace s2::behaviors {

/// Поведение конвейерной ленты.
///
/// По умолчанию пассивен — только управляет параметрами attached-зоны.
/// На команду "SET_SPEED {speed}" обновляет belt_speed.
/// На команду "SET_DIRECTION {dx} {dy} {dz}" обновляет belt_direction.
/// На команды "START" / "STOP" включает/выключает зону с ConveyorEffect.
class ConveyorBehavior : public IActorBehavior {
public:
    void on_init(const YAML::Node& params) override {
        belt_zone_id_ = params["belt_zone_id"].as<std::string>("");
        belt_speed_   = params["speed"].as<double>(1.0);

        if (params["direction"]) {
            const auto& d = params["direction"];
            belt_direction_.x() = d["x"].as<double>(1.0);
            belt_direction_.y() = d["y"].as<double>(0.0);
            belt_direction_.z() = d["z"].as<double>(0.0);
        }
    }

    void tick(Actor& /*actor*/, SimWorld& world, SimBus& bus, double /*dt*/) override {
        // Ничего — конвейер пассивен пока нет команды
    }

    void on_command(Actor& actor, const std::string& cmd,
                    const std::string& params_json) override {
        if (cmd == "STOP") {
            bus_cache_->publish(ZoneToggleCommand{belt_zone_id_, false});
        } else if (cmd == "START") {
            bus_cache_->publish(ZoneToggleCommand{belt_zone_id_, true});
        } else if (cmd == "SET_SPEED") {
            // params_json: {"speed": 2.5}
            try {
                auto j = nlohmann::json::parse(params_json);
                belt_speed_ = j["speed"].get<double>();
                update_belt_zone_effect();
            } catch (...) {}
        }
    }

    std::string current_state() const override {
        return running_ ? "running" : "stopped";
    }

    // Инициализируется SimEngine — нужен доступ к SimBus для команд
    void set_bus(SimBus* bus) { bus_cache_ = bus; }

private:
    void update_belt_zone_effect() {
        // Пересобрать ConveyorEffect с новыми параметрами.
        // Самый простой механизм: ZoneResizeCommand с теми же параметрами → эффект пересоздаётся.
        // В реальности нужен отдельный Kernel Command для обновления параметров эффекта.
        // Пока оставляем TODO: ZoneUpdateEffectParamsCommand{zone_id, effect_index, new_params}
    }

    std::string belt_zone_id_;
    double      belt_speed_{1.0};
    Vec3        belt_direction_{1.0, 0.0, 0.0};
    bool        running_{true};
    SimBus*     bus_cache_{nullptr};
};

} // namespace s2::behaviors
```

### 2. Структура сцены конвейера

Конвейер состоит из:
1. **Actor** — визуальное тело (прямоугольный ящик), коллизионные борта.
2. **Attached Zone** — зона поверх ленты с `ConveyorEffect`.

```
        [attached zone: ConveyorEffect]
    ___________________________________________
   |  ===>  ===>  ===>  ===>  ===>  ===>      |   ← визуальная поверхность ленты
   |___________________________________________|
   |   (боковые борта — коллизия актора)       |
```

### 3. Описание сцены в YAML

```yaml
actors:
  - id: 20
    name: conveyor_main
    type: conveyor
    pose: {x: 0.0, y: 0.0, z: 0.0, yaw: 0.0}
    has_collision: true
    collision:
      type: aabb
      half_size: {x: 3.0, y: 0.05, z: 0.5}   # узкие борта по Y
    behavior: conveyor
    behavior_params:
      belt_zone_id: "conveyor_main_belt"
      speed: 1.5
      direction: {x: 1.0, y: 0.0, z: 0.0}
    visual:
      type: box
      size: {x: 6.0, y: 1.0, z: 0.2}
      color: "#444444"
    attached_zone:
      id: "conveyor_main_belt"
      shape:
        type: aabb
        center: {x: 0.0, y: 0.0, z: 0.15}    # чуть выше поверхности ленты
        half_size: {x: 3.0, y: 0.5, z: 0.15}
      color: "#FF8800"
      opacity: 0.2
      visible: true
      label: "Конвейер"
      effects:
        - type: conveyor
          params:
            direction: {x: 1.0, y: 0.0, z: 0.0}
            speed: 1.5
```

### 4. SceneLoader: разбор `attached_zone` с эффектами

**Файл:** `workspace/s2_core/include/s2/scene_loader.hpp`

В функции создания актора парсить `attached_zone` вместе с эффектами:

```cpp
if (actor_node["attached_zone"]) {
    const auto& az_node = actor_node["attached_zone"];
    Zone zone;
    zone.id             = az_node["id"].as<std::string>("");
    zone.color          = az_node["color"].as<std::string>("#4488FF");
    zone.opacity        = az_node["opacity"].as<double>(0.3);
    zone.visible        = az_node["visible"].as<bool>(true);
    zone.label          = az_node["label"].as<std::string>("");
    zone.shape          = parse_zone_shape(az_node["shape"]);
    zone.attached_to_actor = actor.id;
    zone.attachment_offset = Vec3::Zero();

    // Эффекты attached-зоны
    if (az_node["effects"]) {
        for (const auto& eff_node : az_node["effects"]) {
            Zone::EffectDesc desc;
            desc.type    = eff_node["type"].as<std::string>("");
            desc.enabled = eff_node["enabled"].as<bool>(true);
            desc.params  = eff_node["params"];
            zone.effects.push_back(std::move(desc));
        }
    }

    scene.zones.push_back(std::move(zone));
}
```

### 5. Регистрация в фабрике поведений

**Файл:** `workspace/s2_plugins/src/behaviors_registry.cpp`

```cpp
else if (type == "conveyor") b = std::make_unique<behaviors::ConveyorBehavior>();
```

### 6. Визуализация ленты в Three.js

Анимация конвейера реализуется через `arrows` VisualHint из ConveyorEffect (задача 30).
Никакой дополнительной Three.js логики не требуется — ZoneFxManager автоматически
создаст ArrowsEffect для зоны с VisualHint типа `arrows`.

Дополнительно: поверхность ленты у актора можно анимировать через UV scroll shader.
Это опциональное улучшение — отдельная задача по визуализации.

### 7. Команды к конвейеру (Kernel Commands)

Расширить Kernel Command API:

```cpp
/// Команда к актору из внешней системы.
struct SendActorCommandKernelCmd {
    ActorId actor_id;
    std::string command;
    std::string params_json;
};
```

SimEngine обрабатывает в фазе 1:

```cpp
bus_.subscribe<SendActorCommandKernelCmd>([this](const SendActorCommandKernelCmd& cmd) {
    bus_.publish(ActorCommandEvent{cmd.actor_id, cmd.command, cmd.params_json});
});
```

---

## Тесты

**Файл:** `workspace/s2_core/tests/test_actor_conveyor.cpp`

- `ConveyorBehavior_DefaultState_Running` — начальное состояние = "running"
- `ConveyorBehavior_StopCommand_DisablesZone` — команда "STOP" → ZoneToggleCommand{false}
- `ConveyorBehavior_StartCommand_EnablesZone` — команда "START" → ZoneToggleCommand{true}
- `ConveyorZone_AttachedToActor` — зона привязана к актору, следует за ним при перемещении
- `ConveyorEffect_InAttachedZone_DriftsAgent` — агент в зоне конвейера дрейфует
  в направлении direction (тест интеграции с ZoneSystem + ConveyorEffect)
- `ConveyorEffect_OutsideZone_NoDrift` — агент за пределами зоны не дрейфует

---

## Критерии завершения

- [ ] ConveyorBehavior обрабатывает команды START/STOP
- [ ] Attached-зона с ConveyorEffect создаётся через SceneLoader
- [ ] Зона следует за актором (обновляется в ZoneSystem)
- [ ] Агент на конвейере дрейфует в заданном направлении
- [ ] VisualHint `arrows` отображается в зоне (через задачу 30)
- [ ] Все тесты проходят в Docker
