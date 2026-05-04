#pragma once

/**
 * @file behavior_registry.hpp
 * BehaviorRegistry — фабрика типов поведений акторов (Phase 5).
 *
 * Регистрация: registry.register_type("DoorBehavior", factory_fn)
 * Создание:    registry.create("DoorBehavior", yaml_config)
 */

#include <s2/actor_behavior.hpp>
#include <yaml-cpp/yaml.h>

#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace s2 {

using BehaviorFactory =
    std::function<std::unique_ptr<IActorBehavior>(const YAML::Node&)>;

class BehaviorRegistry {
public:
    void register_type(const std::string& name, BehaviorFactory factory) {
        factories_[name] = std::move(factory);
    }

    std::unique_ptr<IActorBehavior> create(const std::string& type,
                                           const YAML::Node& config) const {
        auto it = factories_.find(type);
        if (it == factories_.end()) {
            throw std::runtime_error("BehaviorRegistry: unknown type '" + type + "'");
        }
        return it->second(config);
    }

    bool has_type(const std::string& name) const {
        return factories_.count(name) > 0;
    }

private:
    std::unordered_map<std::string, BehaviorFactory> factories_;
};

} // namespace s2
