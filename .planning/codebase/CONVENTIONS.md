# Coding Conventions

**Analysis Date:** 2026-04-25

## Language

**Primary language for all documentation, comments, and commit messages:** Russian.
- All code comments, docstrings, and inline notes are written in Russian.
- Technical terms (API names, type names) are kept in English where natural.
- No emojis in code or documentation.

## Naming Patterns

**Files:**
- Headers: `snake_case.hpp` (e.g., `sim_engine.hpp`, `zone_system.hpp`, `shared_state.hpp`)
- Sources: `snake_case.cpp` (e.g., `zone_system.cpp`, `plugins_registry.cpp`)
- Test files: `test_snake_case.cpp` (e.g., `test_zone_system.cpp`, `test_effect_modifier.cpp`)
- Components/data: `snake_case.hpp` in dedicated subdirectory (e.g., `components/battery_component.hpp`)

**Namespaces:**
- Root namespace: `s2`
- Nested namespaces for subsystems: `s2::plugins`, `s2::effects`, `s2::event`
- C++17 nested namespace syntax used: `namespace s2::effects { ... }`

**Types (structs/classes):**
- PascalCase: `SimEngine`, `ZoneSystem`, `SharedState`, `AgentSnapshot`, `EffectPlugin`
- Interfaces prefixed with `I`: `IAgentPlugin`
- Enums use PascalCase: `ShapeType`, `ZoneShapeType`, `EffectType`
- Enum values use UPPER_SNAKE_CASE: `ShapeType::SPHERE`, `EffectType::MODIFIER`
- Type aliases use PascalCase: `AgentId`, `ZoneId`, `Vec3`

**Functions/Methods:**
- snake_case: `add_agent()`, `load_world()`, `check_sphere_collision()`, `clear_contributions()`
- Getters without `get_` prefix for simple accessors: `agents()`, `world()`, `sim_time()`, `dt()`
- Getters with `get_` prefix for lookup by ID: `get_agent(id)`, `get_prop(id)`, `get_zone(id)`
- Factory functions: `create_effect()`, `create_plugin()`

**Variables:**
- snake_case for local variables and parameters: `entered_agent`, `sim_time`, `clamped_linear`
- Private members with trailing underscore: `sim_time_`, `world_`, `dt_`, `max_linear_`, `effect_factory_`
- Constants use snake_case or `constexpr`: `constexpr int N = 10;`, `constexpr double dt = 1.0;`
- No Hungarian notation.

**Template parameters:**
- PascalCase: `EventT`, `T`, `Args`

## Code Style

**Formatting:**
- No `.clang-format` or `.clang-tidy` configured.
- Consistent manual formatting throughout the codebase.
- Indentation: 2 or 4 spaces (mixed; headers tend to use 2, some source files use 4).
- Braces: opening brace on same line for control flow, next line for namespace/class definitions.
- Brace style for namespaces:
  ```cpp
  namespace s2
  {
  // ...
  }  // namespace s2
  ```
- Closing namespace comment always present: `} // namespace s2`

**Linting:**
- No linter configured. Quality enforced through code review and convention.

**Header guards:**
- `#pragma once` exclusively (no `#ifndef` guards).

**Standard:**
- C++17 (`CMAKE_CXX_STANDARD 17` in root `CMakeLists.txt`).
- Designated initializers used: `{.agent = 1, .zone = "test_zone"}`.
- `std::optional`, `std::any`, `std::string_view` from C++17 used.

## Import Organization

**Order:**
1. Corresponding header (for `.cpp` files)
2. Project headers (`<s2/...>`) using angle brackets
3. Third-party library headers (`<Eigen/Dense>`, `<yaml-cpp/yaml.h>`, `<nlohmann/json.hpp>`, `<gtest/gtest.h>`)
4. Standard library headers (`<string>`, `<vector>`, `<memory>`, `<functional>`)

**Path style:**
- Angle brackets for all includes: `#include <s2/types.hpp>`, `#include <gtest/gtest.h>`
- Project headers use `s2/` prefix: `#include <s2/agent.hpp>`, `#include <s2/sim_bus.hpp>`
- Plugin headers: `#include <s2/plugins/diff_drive.hpp>`, `#include <s2/effects/ice_modifier.hpp>`
- Component headers: `#include <s2/components/battery_component.hpp>`

**Path Aliases:**
- No path aliases. All paths are relative from include directories declared in CMake.
- Include directories: `workspace/s2_core/include`, `workspace/s2_plugins/include`, `workspace/s2_transport/include`

## Error Handling

**Patterns:**
- Return `nullptr` for lookup failures: `get_agent()`, `get_prop()`, `get_zone()` return raw pointers.
- Return `bool` for operation success/failure: `set_agent_pose()`, `resize_zone()`, `toggle_zone()`.
- Silent ignore for invalid input: `handle_input()` catches exceptions and ignores malformed JSON.
- No exceptions thrown from core simulation code.
- `ASSERT_NE(ptr, nullptr)` in tests before dereferencing.

**Null safety:**
- Raw pointers for non-owning references: `VizServer* viz_server_ = nullptr`
- `std::unique_ptr<>` for owned resources: `std::unique_ptr<EffectPlugin>`, `std::unique_ptr<IAgentPlugin>`
- `std::optional<>` for optional values: `std::optional<ActorId>`, `std::optional<VisualHint>`

## Documentation

**Doxygen-style comments:**
- `@file` at top of headers with description in Russian.
- `@brief` for all public classes, structs, and methods.
- `@param` and `@return` for non-obvious parameters.
- `@code`/`@endcode` blocks for usage examples.
- `///` for single-line doc comments on struct members.
- Multi-line doc comments: `/** ... */`

**Section separators:**
- Visual separators using `// ========` lines for major sections within files.
- Labeled separators with `// --- Description ---` for subsections.
- Unicode box-drawing: `// ── Section ──────` used in newer code.

**When to comment:**
- Every public API has a doc comment.
- Architectural decisions documented inline (e.g., "Why not Eigen::AngleAxisd...").
- Comments explain "why" not "what" for non-obvious code.
- Russian language for all comments.

## Struct Design

**Pattern:**
- POD-like structs with in-class member initializers (aggregate initialization):
  ```cpp
  struct Pose3D
  {
    double x{0}, y{0}, z{0};
    double roll{0}, pitch{0}, yaw{0};
  };
  ```
- Move semantics through `std::move()` on function parameters:
  ```cpp
  void add_agent(Agent agent) { agents_.push_back(std::move(agent)); }
  ```
- Move-only types documented explicitly: `Zone` is move-only due to `unique_ptr` in `EffectDesc`.

**Classes:**
- Explicit constructors for classes with required configuration: `explicit SimEngine(Config config)`
- No copy constructors/assignment for complex types (implicitly deleted via `unique_ptr` members).
- Public methods first, then private helpers, then private data.

## Module Design

**Plugin architecture:**
- Two interface hierarchies:
  1. `IAgentPlugin` (`workspace/s2_core/include/s2/plugin_base.hpp`) -- agent-level plugins (sensors, actuation)
  2. `EffectPlugin` (`workspace/s2_core/include/s2/interfaces/effect_plugin.hpp`) -- zone effect plugins
- Concrete implementations in `s2_plugins`:
  - Agent plugins: `workspace/s2_plugins/include/s2/plugins/*.hpp`
  - Effect plugins: `workspace/s2_plugins/include/s2/effects/*.hpp`
- Registration through factory functions:
  - `workspace/s2_plugins/src/plugins_registry.cpp` -- agent plugin factory
  - `workspace/s2_plugins/src/effects_registry.cpp` -- effect plugin factory
  - `workspace/s2_plugins/include/s2/effects_registry.hpp` -- declares `create_effect()`

**Exports:**
- Header-only for most types (structs in headers).
- Implementation in `.cpp` only when necessary (e.g., `zone_system.cpp`, `world_snapshot.cpp`).
- Many plugins are header-only with all logic inline.

**Barrel files:**
- Not used. Each header is included individually.

## Configuration Pattern

**YAML-based configuration:**
- Plugins configure via `from_config(const YAML::Node& node)`.
- Effects configure via `on_init(const YAML::Node& params)`.
- Pattern for reading YAML with defaults:
  ```cpp
  traction_coeff_ = params["traction_coefficient"].as<double>(0.2);
  ```
- Multiple key names accepted for backward compatibility:
  ```cpp
  if (node["max_linear_vel"]) max_linear_ = node["max_linear_vel"].as<double>();
  else if (node["max_linear"]) max_linear_ = node["max_linear"].as<double>();
  ```

## JSON Serialization

**Pattern:**
- `to_json()` returns `std::string` (manual string concatenation, not nlohmann serializer).
- `nlohmann::json` used for structured data (snapshots, schemas).
- `YAML::Load(json_string)` used for parsing JSON input in some plugins (YAML is superset of JSON).

## Contribution/Resolver Pattern

**Core pattern for shared mutable state (`workspace/s2_core/include/s2/shared_state.hpp`):**
1. Modules publish contributions: `state.add_scale()`, `state.add_lock()`, `state.add_velocity_addition()`
2. `state.resolve()` computes effective values (product for scales, OR for locks, sum for additive)
3. Actuation reads `state.effective()`
4. `state.clear_contributions()` at end of tick

Use this pattern for any new system where multiple sources affect the same value.

## Event Bus Pattern

**Pattern for inter-module communication (`workspace/s2_core/include/s2/sim_bus.hpp`):**
- Events are POD structs in `s2::event` namespace.
- Subscribe: `bus.subscribe<event::AgentEnteredZone>([](const event::AgentEnteredZone& e) { ... });`
- Publish: `bus.publish(event::AgentEnteredZone{.agent = 1, .zone = "ice_zone"});`
- Synchronous dispatch (no queuing).

---

*Convention analysis: 2026-04-25*
