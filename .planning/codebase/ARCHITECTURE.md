# Architecture

**Analysis Date:** 2026-04-25

## Pattern Overview

**Overall:** Entity-Component-System (ECS) hybrid with plugin architecture and fixed-timestep simulation loop.

**Key Characteristics:**
- Header-heavy C++17 codebase: most core logic lives in `.hpp` headers with inline implementations
- Fixed-rate tick loop (`SimEngine`) drives all simulation updates
- Plugin system for extensible agent behaviors (`IAgentPlugin`) and zone effects (`EffectPlugin`)
- Contribution-based state resolution: multiple modules write contributions, a central resolver computes effective state
- Transport-agnostic design: `ITransportAdapter` abstracts ROS2/stub/future transports
- Scene-driven configuration: all entities loaded from YAML scene files

## Layers

**Core Types (`s2_core/include/s2/types.hpp`):**
- Purpose: Fundamental data types shared across the entire codebase
- Location: `workspace/s2_core/include/s2/types.hpp`
- Contains: `Vec3` (Eigen::Vector3d alias), `Pose3D`, `Velocity`, `CollisionShape`, `VisualDesc`, `ZoneShape`, `EffectType`, ID types (`AgentId`, `ActorId`, `ObjectId`, `ZoneId`)
- Depends on: Eigen3
- Used by: Every other module

**Simulation Engine (`s2_core`):**
- Purpose: Fixed-timestep simulation loop, physics integration, collision detection
- Location: `workspace/s2_core/include/s2/sim_engine.hpp`
- Contains: `SimEngine` (tick loop, agent iteration, kinematics, collision response), `SimWorld` (entity container), `SimBus` (event bus), `SharedState` (contribution-based state), `CollisionSystem`, `RaycastEngine`, `ZoneSystem`
- Depends on: Core types, Eigen3, yaml-cpp, nlohmann_json, tinyxml2, GeographicLib
- Used by: Plugins, Transport, Visualizer

**Plugin System (`s2_plugins`):**
- Purpose: Extensible agent behaviors (sensors, actuators, resource modules) and zone effects
- Location: `workspace/s2_plugins/`
- Contains: Agent plugins (`DiffDrivePlugin`, `GnssPlugin`, `ImuPlugin`, `LidarPlugin`, `BatteryPlugin`, `GravityPlugin`, `ColorPlugin`, `JointVelPlugin`, `TrajectoryRecorderPlugin`, `PathDisplayPlugin`, `TopicDisplayPlugin`), Effect plugins (`IceModifier`, `BoostZone`, `MotionLockZone`, `ConveyorEffect`, `WindEffect`, `ChargingEffect`, `TirePunctureEffect`, `TeleportEffect`), Registry/factory functions
- Depends on: `s2_core`
- Used by: `SimEngine` (via tick callbacks), `SimTransportBridge` (via sensor output)

**Transport Layer (`s2_transport`):**
- Purpose: Bidirectional bridge between simulation and external systems (ROS2 or stub)
- Location: `workspace/s2_transport/`
- Contains: `ITransportAdapter` (abstract interface), `Ros2TransportAdapter` (real or stub), `SimTransportBridge` (orchestrates registration and per-tick publish)
- Depends on: `s2_core`, optionally ROS2 (rclcpp, geometry_msgs, sensor_msgs, nav_msgs, tf2_ros, s2_msgs)
- Used by: `main.cpp` (initialization), `SimEngine` (post-tick callback)

**Visualizer (`s2_visualizer`):**
- Purpose: Web-based 3D visualization via WebSocket/SSE + HTTP static serving
- Location: `workspace/s2_visualizer/`
- Contains: `VizServer` (HTTP+SSE server), `VizCommandHandler` (abstract command interface), `SimEngineCommandAdapter` (concrete adapter connecting viz to engine), scene editor support (save/load/new scene, update agents/geometry)
- Depends on: `s2_core`, `s2_plugins`, `s2_transport`, OpenSSL
- Used by: End user via browser at `http://localhost:1937`

**Scene Configuration (`s2_config`):**
- Purpose: YAML scene definitions and robot models
- Location: `workspace/s2_config/`
- Contains: Scene YAML files (`workspace/s2_config/scenes/*.yaml`), URDF robot models (`workspace/s2_config/robots/*.urdf`)
- Depends on: Nothing (data only)
- Used by: `SceneLoader`, `main.cpp`

## Data Flow

**Main Simulation Tick (per tick at `update_rate` Hz):**

1. `SimEngine::tick()` increments `sim_time += dt`
2. `ZoneSystem::tick()` checks agent enter/exit for all zones, applies MUTATION on enter, publishes `AgentEnteredZone`/`AgentExitedZone` events on `SimBus`
3. For each agent:
   - `plugin->pre_resolve(dt, agent)` -- resource modules publish contributions (e.g. `BatteryPlugin` adds scale/lock)
   - `agent.state.resolve()` -- computes `EffectiveConstraints` from all contributions (product of scales, OR of locks, sum of additives)
   - `plugin->update(dt, agent)` for each plugin -- actuators set velocity, sensors compute data
   - Kinematics: body-frame velocity transformed to world frame via rotation matrix, position integrated
   - Collision detection/response (sphere-vs-static-geometry with walkable slope/step detection)
   - Surface alignment (pitch/roll from support surface normal)
   - Pending teleport application (deferred from `TeleportEffect`)
   - `agent.state.clear_contributions()` -- reset for next tick
4. Viz publish at `viz_rate` Hz: `SimEngine::build_snapshot()` -> `VizServer::publish()`
5. Transport publish at `transport_rate` Hz: `post_tick_cb_` -> `SimTransportBridge::on_post_tick()` -> `ITransportAdapter::publish_agent_frame()`

**Scene Loading Flow:**

1. `main.cpp` calls `SceneLoader::load(yaml_path, plugin_factory)`
2. `SceneLoader` parses YAML: engine config, world (heightmap, geometry), agents (with plugins), props, actors, zones (with effect descriptors)
3. `SimWorld` populated with entities
4. `SimEngine::set_effect_factory(create_effect)` sets zone effect factory
5. `SimEngine::load_world(world)` transfers zones to `ZoneSystem`, initializes `CollisionSystem` and `RaycastEngine`
6. `SimTransportBridge::init()` registers agents, sensors, command topics, services in the transport adapter
7. `SimEngine::run()` starts the tick loop

**Plugin Input Flow (external command -> plugin):**

1. External source (VizUI / ROS2 topic / service) sends command
2. Command reaches `SimEngine::handle_plugin_input(agent_id, plugin_type, json_input)` or `plugin->handle_service()`
3. Plugin stores command internally (e.g. `DiffDrivePlugin` updates desired velocity)
4. Next `plugin->update(dt, agent)` applies stored command

**State Management:**

- `SharedState` on each `Agent`: type-indexed `std::any` storage for single-owner fields + contribution lists (scale, lock, additive)
- Per-tick cycle: modules publish contributions -> `resolve()` computes effective -> actuation reads effective -> `clear_contributions()`
- `SimBus`: synchronous publish/subscribe event bus (typed via `std::any_cast`), zero-allocation hot path
- `WorldSnapshot`: read-only serialization of entire world state for viz/transport, built by `SimEngine::build_snapshot()`

## Key Abstractions

**Agent (`workspace/s2_core/include/s2/agent.hpp`):**
- Purpose: The primary controllable entity (robot)
- Contains: ID, pose, velocity, `SharedState`, capabilities set, vector of `unique_ptr<IAgentPlugin>`, collision shape, visual, optional `KinematicTree`
- Pattern: Data struct with embedded state machine (`SharedState`) and polymorphic plugins

**IAgentPlugin (`workspace/s2_core/include/s2/plugin_base.hpp`):**
- Purpose: Extensible per-agent behavior module
- Key methods: `type()`, `update(dt, agent)`, `pre_resolve(dt, agent)`, `from_config(yaml)`, `to_json()`, `handle_input(json)`, `has_inputs()`, `inputs_schema()`, `command_topics()`, `service_names()`, `poll_events()`, `contribute_snapshot(json, agent)`
- Examples: `workspace/s2_plugins/include/s2/plugins/diff_drive.hpp`, `workspace/s2_plugins/include/s2/plugins/gnss.hpp`
- Pattern: Template method with optional overrides for sensor/actuator/resource behavior

**EffectPlugin (`workspace/s2_core/include/s2/interfaces/effect_plugin.hpp`):**
- Purpose: Zone-triggered behavior (ice, boost, charging, teleport, etc.)
- Key methods: `on_init(yaml)`, `effect_type()`, `apply_modifier(state, ctx)`, `apply_continuous(state, ctx)`, `apply_mutation(state, ctx)`, `on_agent_exit(state, ctx)`
- Examples: `workspace/s2_plugins/include/s2/effects/ice_modifier.hpp`, `workspace/s2_plugins/include/s2/effects/charging_effect.hpp`
- Pattern: Strategy pattern with lifecycle hooks (enter/tick/exit)

**ITransportAdapter (`workspace/s2_core/include/s2/transport_adapter.hpp`):**
- Purpose: Abstract interface for external communication (ROS2, stub, future MQTT)
- Key methods: `register_agent()`, `register_sensor()`, `register_input_topic()`, `register_service()`, `publish_agent_frame()`, `emit_event()`
- Implementations: `workspace/s2_transport/include/s2/ros2_transport_adapter.hpp` (real), `workspace/s2_transport/src/ros2_transport_adapter_stub.cpp` (stub)
- Pattern: Bridge pattern -- `SimTransportBridge` orchestrates, adapter handles transport specifics

**SimBus (`workspace/s2_core/include/s2/sim_bus.hpp`):**
- Purpose: Typed synchronous event bus for decoupled intra-tick communication
- Key events: `AgentEnteredZone`, `AgentExitedZone`, `ObjectAttached`, `ObjectReleased`, `ActorStateChanged`, `AgentCollision`, `TeleportAgentCommand`, `SetZoneTeleportTargetCommand`
- Pattern: Observer with type-erased dispatch via `std::any`

**SharedState (`workspace/s2_core/include/s2/shared_state.hpp`):**
- Purpose: Per-agent state store with contribution-based resolution
- Two mechanisms: (1) Single-owner type-indexed storage via `emplace<T>()`/`get<T>()`, (2) Multi-contributor resolution via `add_scale()`/`add_lock()`/`add_velocity_addition()` -> `resolve()` -> `effective()`
- Pattern: Blackboard pattern with aggregation rules (product for scale, OR for lock, sum for additive)

## Entry Points

**`workspace/s2_visualizer/src/main.cpp`:**
- Location: `workspace/s2_visualizer/src/main.cpp`
- Triggers: Docker container starts `./s2_visualizer/s2_sim <scene.yaml>`
- Responsibilities: Load scene via `SceneLoader`, create `VizServer`, build `SimWorld`, create `SimEngine`, create `SimTransportBridge`, wire everything together, call `engine.run()`

**Plugin Factory (`workspace/s2_plugins/src/plugins_registry.cpp`):**
- Location: `workspace/s2_plugins/src/plugins_registry.cpp`
- Triggers: `SceneLoader::load()` calls `plugin_factory(type, yaml_node)` for each agent plugin in YAML
- Responsibilities: Static registrar pattern maps string type -> factory function -> `IAgentPlugin` instance

**Effect Factory (`workspace/s2_plugins/src/effects_registry.cpp`):**
- Location: `workspace/s2_plugins/src/effects_registry.cpp`
- Triggers: `ZoneSystem::add_zone()` calls `effect_factory_(type, params)` for each effect in zone
- Responsibilities: Maps string type -> `EffectPlugin` instance, calls `on_init(params)`

**Scene Loader (`workspace/s2_core/include/s2/scene_loader.hpp`):**
- Location: `workspace/s2_core/include/s2/scene_loader.hpp`
- Triggers: `main.cpp` or `SimEngineCommandAdapter::on_load_scene()`
- Responsibilities: Parse YAML into `SceneData` struct containing engine config, world entities, agents with plugins, zones with effect descriptors

## Error Handling

**Strategy:** Exception-based for initialization (scene loading, URDF parsing), graceful degradation at runtime.

**Patterns:**
- `SceneLoader::load()` throws `std::runtime_error` or `YAML::Exception` on invalid scene files
- `main.cpp` catches exceptions during scene load and exits with error message
- Plugin factory returns `nullptr` for unknown plugin types (silently skipped)
- Effect factory returns `nullptr` for unknown effect types (silently skipped)
- `SimEngine::handle_plugin_input()` returns `false` if agent or plugin not found (no exception)
- `VizCommandHandler::on_load_scene()` catches exceptions and returns `SaveSceneResult{false, error_msg}`

## Cross-Cutting Concerns

**Logging:** `std::cout`/`std::cerr` with `[Module]` prefix (e.g. `[Main]`, `[VizServer]`). No structured logging framework.

**Validation:** YAML scene files validated implicitly by yaml-cpp during load. Plugin `from_config()` methods read optional fields with defaults. No explicit schema validation layer.

**Authentication:** None. VizServer serves HTTP/SSE without auth. ROS2 transport relies on DDS domain isolation.

**Serialization:** `nlohmann::json` for all JSON serialization (snapshots, plugin data, transport messages). `yaml-cpp` for scene configuration. Custom `WorldSnapshot::to_json()` for viz protocol.

**Threading Model:** Single simulation thread runs the tick loop. `VizServer` runs HTTP/SSE in a separate thread with mutex-guarded snapshot exchange. ROS2 adapter may spin in its own thread. `std::atomic<bool>` for `running_`/`paused_` flags. `TripleBuffer` available for lock-free data exchange.

**Build Modes:** `S2_WITH_ROS2=ON` enables real ROS2 transport; `OFF` (default) uses stubs. `S2_WITH_S2_MSGS` enables custom service types.

---

*Architecture analysis: 2026-04-25*
