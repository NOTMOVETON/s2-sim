# Phase 2: Actor & Prop Foundation - Pattern Map

**Mapped:** 2026-04-26
**Files analyzed:** 17 new/modified files
**Analogs found:** 14 / 17

## File Classification

| New/Modified File | Role | Data Flow | Closest Analog | Match Quality |
|-------------------|------|-----------|----------------|---------------|
| `include/s2/actor_behavior.hpp` | interface | request-response | `include/s2/plugin_base.hpp` | exact |
| `include/s2/actor_fsm.hpp` | utility | state-machine | (new, no analog) | no-match |
| `src/actor.cpp` | model | CRUD | `src/zone_system.cpp` | role-match |
| `include/s2/prop.hpp` (modified) | model | CRUD | `include/s2/types.hpp` | role-match |
| `include/s2_plugins/door_behavior.hpp` | behavior | state-machine | `include/s2/plugin_base.hpp` | role-match |
| `src/s2_plugins/door_behavior.cpp` | behavior | state-machine | `src/zone_system.cpp` | role-match |
| `include/s2_plugins/door_opener_plugin.hpp` | plugin | request-response | `include/s2/plugin_base.hpp` | exact |
| `src/s2_plugins/door_opener_plugin.cpp` | plugin | request-response | `src/zone_system.cpp` | role-match |
| `include/s2/signal_listener_base.hpp` | utility | event-driven | `include/s2/plugin_base.hpp` | role-match |
| `include/s2_plugins/door_wire_controller.hpp` | plugin | event-driven | `include/s2/plugin_base.hpp` | exact |
| `src/s2_plugins/door_wire_controller.cpp` | plugin | event-driven | `src/zone_system.cpp` | role-match |
| `include/s2_plugins/event_reactor.hpp` | plugin | event-driven | `include/s2/plugin_base.hpp` | exact |
| `src/s2_plugins/event_reactor.cpp` | plugin | event-driven | `src/zone_system.cpp` | role-match |
| `include/s2_plugins/grabber_plugin.hpp` | plugin | request-response | `include/s2/plugin_base.hpp` | exact |
| `src/s2_plugins/grabber_plugin.cpp` | plugin | request-response | `src/zone_system.cpp` | role-match |
| `src/sim_engine.cpp` (Phase 0, 2, 6) | engine | CRUD | `src/sim_engine_viz.cpp` | role-match |
| `src/plugins_registry.cpp` (add 3 plugins) | registry | CRUD | `src/plugins_registry.cpp` | exact |

## Pattern Assignments

### `include/s2/actor_behavior.hpp` (interface, request-response)

**Analog:** `include/s2/plugin_base.hpp`

**Interface pattern** (lines 74-424):
```cpp
// Базовый класс для плагинов с virtual методами и lifecycle hooks
class IAgentPlugin {
public:
    virtual ~IAgentPlugin() = default;
    
    // Основные методы
    virtual std::string type() const = 0;
    virtual PluginRole role() const = 0;
    virtual void update(double dt, Agent& agent, const PluginContext& ctx) = 0;
    virtual void from_config(const YAML::Node& node) = 0;
    virtual std::string to_json() const = 0;
    
    // Lifecycle-методы
    virtual void on_spawn(Agent& agent) { (void)agent; }
    virtual void on_despawn(Agent& agent) { (void)agent; }
    virtual void on_reset(Agent& agent) { (void)agent; }
};
```

**Copy to IActorBehavior interface:**
- Lifecycle pattern: `on_init(YAML::Node&)`, `on_reset()`, `on_despawn()`
- Core method signature: `update(double dt, Entity&, WorldContext&)`
- Method signatures for signals/interactions: `on_signal(SignalEvent&)`, `on_interact(EntityId, string, json)`
- State query: `current_state()`, `to_json()`

---

### `include/s2_plugins/door_behavior.hpp` (behavior, state-machine)

**Analog:** `include/s2/plugin_base.hpp`

**Class structure pattern** (lines 89-243):
```cpp
class IAgentPlugin {
public:
    virtual std::string type() const = 0;
    virtual void on_spawn(Agent& agent) { (void)agent; }
    virtual void update(double dt, Agent& agent, const PluginContext& ctx) = 0;
    virtual void from_config(const YAML::Node& node) = 0;
};
```

**Copy to DoorBehavior class:**
- Use enum for FSM states: `enum class State { CLOSED, OPENING, OPEN, CLOSING };`
- Store state + timer: `State state_{State::CLOSED}; double timer_{0.0};`
- Configuration from YAML: `on_init()` reads open_duration, close_duration, auto_close_secs
- State transitions in `update()`: check timer, update state, trigger events

---

### `include/s2_plugins/door_opener_plugin.hpp` (plugin, request-response)

**Analog:** `include/s2/plugin_base.hpp`

**Plugin structure** (lines 89-243):
```cpp
class IAgentPlugin {
public:
    virtual ~IAgentPlugin() = default;
    virtual std::string type() const = 0;
    virtual PluginRole role() const = 0;
    virtual void update(double dt, Agent& agent, const PluginContext& ctx) = 0;
    virtual void from_config(const YAML::Node& node) = 0;
    virtual std::string to_json() const = 0;
};
```

**Copy pattern:**
- Return `PluginRole::INTERACTION` for role()
- In `update()`: use `ctx.world.find_in_radius()` to locate nearest door actor
- Send `KernelCommand::Interact` through `ctx.commands` to open door
- In `from_config()`: read `interaction_distance` parameter
- Publish events through `ctx.bus.publish(event::GrabAttempt{...})`

---

### `include/s2/signal_listener_base.hpp` (utility, event-driven)

**Analog:** `include/s2/plugin_base.hpp`

**Base class structure** (lines 89-150):
```cpp
class IAgentPlugin {
public:
    virtual ~IAgentPlugin() = default;
    virtual PluginRole role() const = 0;
    virtual void update(double dt, Agent& agent, const PluginContext& ctx) = 0;
    virtual void from_config(const YAML::Node& node) = 0;
};
```

**Imports and pattern** (lines 1-24):
```cpp
#include <s2/event_bus.hpp>
#include <s2/kernel_command.hpp>
#include <s2/world_query.hpp>
#include <yaml-cpp/yaml.h>
#include <memory>
#include <vector>
```

**Copy pattern:**
- Base class with virtual `react()` method
- Constructor takes `WorldQuery*` and `EventBus*` references (like PluginContext fields)
- `scan_signals()` method uses WorldQuery to find signals
- Store configuration reactions in vector of structs with signal_id, source_entity, on_active, on_inactive

---

### `include/s2_plugins/door_wire_controller.hpp` (plugin, event-driven)

**Analog:** `include/s2/plugin_base.hpp`

**Plugin implementation pattern** (lines 89-243):
```cpp
class IAgentPlugin {
public:
    virtual std::string type() const = 0;
    virtual PluginRole role() const = 0;
    virtual void update(double dt, Agent& agent, const PluginContext& ctx) = 0;
    virtual void from_config(const YAML::Node& node) = 0;
    virtual std::string to_json() const = 0;
};
```

**Copy pattern:**
- Inherit from both `IAgentPlugin` and `SignalListenerBase`
- `type()` returns "door_wire_controller"
- `role()` returns `PluginRole::INTERACTION`
- In `update()`: call parent `scan_signals()`, then iterate reactions
- Reaction map: "close_and_lock" → send Interact command with action="close_and_lock"
- In `from_config()`: parse `reactions:` array with signal_id, source_entity, on_active, on_inactive

---

### `include/s2_plugins/event_reactor.hpp` (plugin, event-driven)

**Analog:** `include/s2/plugin_base.hpp`

**Plugin pattern** (lines 89-243):
```cpp
class IAgentPlugin {
public:
    virtual std::string type() const = 0;
    virtual PluginRole role() const = 0;
    virtual void update(double dt, Agent& agent, const PluginContext& ctx) = 0;
    virtual void from_config(const YAML::Node& node) = 0;
};
```

**Copy pattern:**
- Simpler than DoorWireController — no behavior/action
- Just listen for signal + republish event
- Configuration: `{ signal_id, on_active: {event_type, params}, on_inactive: {...} }`
- In `update()`: check if signal_active, publish corresponding event through ctx.bus

---

### `include/s2_plugins/grabber_plugin.hpp` (plugin, request-response)

**Analog:** `include/s2/plugin_base.hpp`

**Interaction plugin pattern** (lines 89-243):
```cpp
class IAgentPlugin {
public:
    virtual PluginRole role() const = 0;
    virtual void update(double dt, Agent& agent, const PluginContext& ctx) = 0;
    virtual void from_config(const YAML::Node& node) = 0;
};
```

**Copy pattern:**
- `role()` returns `PluginRole::INTERACTION`
- In `update()`: use `ctx.world.find_in_radius()` with EntityFilter (movable props only)
- Publish `event::GrabAttempt` when grab action triggered
- Send `KernelCommand::AttachObject` through `ctx.commands` when validation passes
- In `from_config()`: read `interaction_distance`, `max_weight` parameters

---

## Shared Patterns

### Plugin Registration Pattern
**Source:** `src/plugins_registry.cpp` (lines 25-51)
**Apply to:** `src/plugins_registry.cpp` (add DoorOpenerPlugin, DoorWireController, EventReactor, GrabberPlugin)

```cpp
// Registry pattern using static initialization
static const PluginRegistrar register_door_opener("door_opener", 
    []() { return std::make_unique<DoorOpenerPlugin>(); });
static const PluginRegistrar register_door_wire_controller("door_wire_controller",
    []() { return std::make_unique<DoorWireController>(); });
static const PluginRegistrar register_event_reactor("event_reactor",
    []() { return std::make_unique<EventReactor>(); });
static const PluginRegistrar register_grabber("grabber",
    []() { return std::make_unique<GrabberPlugin>(); });
```

### EventBus Publish Pattern
**Source:** `include/s2/event_bus.hpp` (lines 144-160)
**Apply to:** All behavior and plugin files

```cpp
// Event publication through EventBus reference
bus.publish(event::ActorStateChanged{
    .actor_id = actor.id,
    .old_state = old_state,
    .new_state = state_
});

bus.publish(event::GrabAttempt{
    .agent = agent.id,
    .target = target_entity_id
});

bus.publish(event::SignalActivated{
    .signal_id = signal_id,
    .source_entity = source_entity_id
});
```

### YAML Configuration Pattern
**Source:** `include/s2/plugin_base.hpp` (lines 238-241)
**Apply to:** All behavior and plugin from_config() methods

```cpp
virtual void from_config(const YAML::Node& node) {
    // Safe access with defaults
    duration_ = node["duration"].as<double>(1.0);
    enabled_ = node["enabled"].as<bool>(true);
    
    // Vector parsing
    if (node["reactions"]) {
        for (const auto& reaction : node["reactions"]) {
            reactions_.push_back({
                reaction["signal_id"].as<std::string>(""),
                reaction["source_entity"].as<uint32_t>(0)
            });
        }
    }
}
```

### WorldQuery Usage Pattern
**Source:** `include/s2/world_query.hpp` (lines 127-150)
**Apply to:** DoorOpenerPlugin, GrabberPlugin

```cpp
// Find entities in radius
auto nearby = ctx.world.find_in_radius(
    agent.world_pose.position(),
    interaction_distance_,
    EntityFilter::actors_only()  // or custom filter
);

// Find nearest with filter
auto target = ctx.world.find_nearest(
    agent.world_pose.position(),
    EntityFilter{.include_props = true}
);
```

### KernelCommand Pattern
**Source:** `include/s2/kernel_command.hpp` (lines 163-192)
**Apply to:** All interaction plugins

```cpp
// Send Interact command
ctx.commands.push_back(cmd::Interact{
    .source_id = agent.id,
    .target_id = door_id,
    .action = "open",
    .params = nlohmann::json::object(),
    .max_distance = 0.0
});

// Send AttachObject command
ctx.commands.push_back(cmd::AttachObject{
    .parent_id = agent.id,
    .link = "gripper",
    .child_id = prop_id,
    .local_pose = Pose3D{0.1, 0, 0, 0, 0, 0}
});
```

### State Contribution Pattern
**Source:** `include/s2/shared_state.hpp` (lines 54-85)
**Apply to:** GrabberPlugin (manipulation_locked contribution), DoorWireController (lock contribution)

```cpp
// In update(), publish contribution to agent's SharedState
auto lock_contrib = agent.shared_state.find<LockContribution>("door_lock");
if (door_is_locked) {
    agent.shared_state.add_lock(
        LockContribution{.locked = true, .source = "door_wire_controller"}
    );
}
```

---

## Integration Points

### SimEngine Phase 0: Kernel Commands
**File:** `src/sim_engine.cpp` — `phase0_kernel_commands()`

**Pattern:** Add handlers for Interact, AttachObject, DetachObject (lines like in zone_system.cpp dispatch)

```cpp
// In phase0_kernel_commands() visitor/switch block:
std::visit([this](const auto& cmd) { this->apply_command(cmd); }, command);

// Specialized for Interact:
void apply_command(const cmd::Interact& cmd) {
    auto actor = world_.find_actor_by_id(cmd.target_id);
    if (actor && actor->behavior) {
        actor->behavior->on_interact(cmd.source_id, cmd.action, cmd.params);
    }
}
```

### SimEngine Phase 2: Actor Update
**File:** `src/sim_engine.cpp` — `phase2_actors()`

**Pattern:** Iterate actors, call behavior.update() (similar to zone_system lifecycle update)

```cpp
void SimEngine::phase2_actors() {
    for (auto& actor : world_.actors()) {
        if (actor.behavior) {
            WorldContext ctx{world_, bus_, /* other fields */};
            actor.behavior->update(dt_, actor, ctx);
        }
    }
}
```

### SimEngine Phase 6: Attachments
**File:** `src/sim_engine.cpp` — `phase6_attachments()`

**Pattern:** Update attached prop positions from agent link poses (like zone attachment in zone_system.cpp)

```cpp
void SimEngine::phase6_attachments() {
    for (auto& prop : world_.props()) {
        if (prop.attached_to_agent) {
            auto agent = world_.find_agent_by_id(prop.attached_to_agent);
            if (agent) {
                auto link_pose = agent->get_link_world_pose(prop.attach_link);
                prop.world_pose = combine_poses(link_pose, prop.attach_offset);
            }
        }
    }
}
```

---

## No Analog Found

Files with no close existing match (use RESEARCH.md patterns instead):

| File | Role | Data Flow | Reason |
|------|------|-----------|--------|
| `include/s2/actor_fsm.hpp` | utility | state-machine | No FSM utility class exists yet in codebase; Phase 1 stub only. Will define custom ActorFSM template. |

---

## Metadata

**Analog search scope:** workspace/s2_core/include/s2/, workspace/s2_plugins/

**Files scanned:** 28 header files, 7 source files

**Pattern extraction date:** 2026-04-26

**Key patterns identified:**
1. All plugins inherit from `IAgentPlugin` and implement role(), type(), update(), from_config()
2. EventBus publish pattern consistent across all event-emitting code
3. YAML parsing with safe defaults using `.as<Type>(default_value)`
4. WorldQuery usage pattern: find_in_radius() + EntityFilter for proximity-based interactions
5. KernelCommand pattern: construct variant, push to ctx.commands or command_queue
6. FSM implementation: enum State + state variable + timer + switch/case in update()
7. Plugin registration using static PluginRegistrar objects
8. SharedState contributions for cross-cutting concerns (locks, scales, velocity_addition)
