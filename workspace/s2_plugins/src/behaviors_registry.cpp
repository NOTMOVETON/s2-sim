/**
 * @file behaviors_registry.cpp
 * Реестр behavior-типов акторов (аналог plugins_registry для агентов).
 *
 * Позволяет SceneLoader создавать behavior по строковому типу из YAML.
 * Паттерн: статическая регистрация через BehaviorRegistrar.
 */

#include <s2/behaviors/door_behavior.hpp>

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

namespace s2
{

// Тип фабрики behavior: (config) -> unique_ptr<IActorBehavior>
using BehaviorFactoryFn = std::function<std::unique_ptr<IActorBehavior>(const YAML::Node&)>;
using BehaviorFactoryMap = std::unordered_map<std::string, BehaviorFactoryFn>;

static BehaviorFactoryMap& behavior_factories()
{
    static BehaviorFactoryMap map;
    return map;
}

/// Регистратор behavior (статическая инициализация)
struct BehaviorRegistrar
{
    BehaviorRegistrar(const std::string& type, BehaviorFactoryFn fn)
    {
        behavior_factories()[type] = std::move(fn);
    }
};

// ── Регистрация конкретных behavior'ов ──────────────────────────────────────

static const BehaviorRegistrar register_door("door",
    [](const YAML::Node& cfg) {
        auto b = std::make_unique<DoorBehavior>();
        b->on_init(cfg);
        return b;
    });

// ── Публичный API ───────────────────────────────────────────────────────────

/// Создать behavior по типу. Возвращает nullptr если тип неизвестен.
std::unique_ptr<IActorBehavior> create_behavior(const std::string& type,
                                                const YAML::Node& config)
{
    auto& map = behavior_factories();
    auto it = map.find(type);
    if (it == map.end()) return nullptr;
    return it->second(config);
}

} // namespace s2
