# Codebase Concerns

**Analysis Date:** 2026-04-25

---

## 1. Security Issues

### 1.1 [CRITICAL] Shell injection via popen() in WebSocket handshake

**Location:** `workspace/s2_visualizer/src/viz_server.cpp`, function `compute_ws_accept()`

**Problem:** The WebSocket handshake computes the `Sec-WebSocket-Accept` header by constructing a shell command from a value extracted directly from an HTTP request header (`Sec-WebSocket-Key`) and passing it to `popen()`:

```cpp
std::string cmd = "echo -n '" + key + "258EAFA5-E914-47DA-95CA-5AB37E8E73AB' | openssl sha1 -binary | openssl base64";
FILE* pipe = popen(cmd.c_str(), "r");
```

If an attacker connects with a crafted `Sec-WebSocket-Key` header value containing shell metacharacters (single quote, semicolon, pipe, backtick, `$(...)`, etc.), arbitrary commands execute with the process's privileges.

**Severity:** High — the port (default 1937) is accessible from any host (`INADDR_ANY`), and the VizServer has no authentication.

**Fix:** Replace with an in-process SHA-1 + Base64 implementation. The `stub sha1()` function at the top of the same file is already a placeholder for this but was never implemented (it zeroes the hash and is not called). Use OpenSSL's EVP API directly in C++ instead of spawning a subprocess.

---

### 1.2 [HIGH] No authentication on any endpoint

**Location:** `workspace/s2_visualizer/src/viz_server.cpp` — all HTTP, SSE and WebSocket handlers.

**Problem:** The HTTP server listens on `INADDR_ANY` (all interfaces) and accepts all connections without any authentication. Every endpoint — including state-mutating operations (`POST /api/scene/save`, `POST /api/scene/load`, `POST /api/scene/new`, `?cmd=reset`, `?cmd=plugin_input`) — is accessible to any process that can reach the port.

In the Docker `sim` service the port is mapped to the host machine (`1937:1937`). On a shared network this means anyone on the same LAN can control the simulation, load arbitrary scenes from the filesystem, or write YAML files.

**Severity:** Medium for the simulator's intended local-dev use, but high if deployed on a shared or cloud environment.

**Fix:** At a minimum, bind to `localhost` (127.0.0.1) rather than `INADDR_ANY` for the non-ROS2 `sim` service. A token-based simple auth layer can be added later.

---

### 1.3 [MEDIUM] Path traversal in static file server

**Location:** `workspace/s2_visualizer/src/viz_server.cpp`, function `serve_http()` (static file serving block at the bottom).

**Problem:** Incoming request URL is used to construct a filesystem path with no validation:

```cpp
std::string rel = (!url.empty() && url[0] == '/') ? url.substr(1) : url;
std::string content = read_file_content(static_path_ + "/" + rel);
```

A request like `GET /../../etc/passwd` after URL normalization by the kernel will be rejected by the OS if the path exits the Docker filesystem root, but paths within the container (e.g., `../../workspace/s2_config/scenes/secret.yaml`) will be served without restriction.

**Fix:** Canonicalize `static_path_ + "/" + rel` with `std::filesystem::canonical()` and reject paths that do not start with `static_path_`.

---

### 1.4 [MEDIUM] Scene file injection via `POST /api/scene/load`

**Location:** `workspace/s2_visualizer/src/viz_server.cpp`, `/api/scene/load` handler; `workspace/s2_visualizer/src/main.cpp`, `SimEngineCommandAdapter::on_load_scene()`.

**Problem:** The `filename` parameter from the POST body is passed directly to `SceneLoader::load()` which calls `YAML::LoadFile(yaml_path)`. There is no validation that the path stays within the scenes directory. An attacker (or a local user with browser access) can load any YAML-parseable file from the container filesystem.

**Fix:** Validate that the resolved path is inside the allowed `scenes_dir_` before loading.

---

## 2. Known Bugs

All four items below are tracked in `docs/known_bugs.md`. They are reproduced here for completeness with additional diagnosis notes.

### 2.1 [OPEN] Robot jitter and floating lidar line on slopes

**Location:** `workspace/s2_core/include/s2/sim_engine.hpp` phases 3e/3f/3h; `workspace/s2_plugins/include/s2/plugins/gravity.hpp`.

**Root cause (best current hypothesis):** `GravityPlugin` (phase 3e) snaps `pose.z` for the agent's current XY, then kinematics (phase 3f) advances XY. The Z snap is therefore computed for the wrong XY position. `find_support_surface()` in phase 3h reads the surface normal for the new XY, but the Z correction already happened for the old XY. This causes a per-tick oscillation of `pitch`/`roll`, which in turn destabilizes `R.transpose() * slide_velocity_` in GravityPlugin and causes visible jitter.

**Consequence:** LidarPlugin scan plane oscillates; robot shakes when rotating on a ramp.

**Proposed direction:** Move the GravityPlugin Z-snap to phase 3g (currently empty "surface snap" phase), which runs after kinematics. Alternatively, predict the post-kinematics XY inside GravityPlugin.

---

### 2.2 [OPEN] New agent preview not shown immediately in scene editor

**Location:** `workspace/s2_visualizer/web/js/app.js`, functions `openAgentForm()`, `refreshNewAgentPreview()`, `updateScene()`.

**Root cause (unconfirmed):** `refreshNewAgentPreview()` is called before the browser renders the next frame, and `updateScene()` triggered by the incoming SSE snapshot removes or hides the preview mesh before the first paint.

---

### 2.3 [OPEN] Scene reload does not reinitialize ROS2 transport

**Location:** `workspace/s2_visualizer/src/main.cpp`, `SimEngineCommandAdapter::on_load_scene()`.

**Root cause:** `transport_bridge_` is created in `main()` but is not accessible inside `SimEngineCommandAdapter`. After `engine_->load_world()` the transport layer retains the ROS2 nodes and subscriptions from the previous scene.

**Workaround:** Full Docker container restart.

---

### 2.4 [OPEN] Stale TF frames and overlay lines after scene reload

**Location:** `workspace/s2_visualizer/web/js/app.js`, function `resetEditorState()`.

**Root cause:** `resetEditorState()` does not clear `tfFrames` or call `clearOverlayLines()`. These persist in the Three.js scene until the next SSE update overwrites them.

---

### 2.5 [OPEN] Severe latency with overlay plugins enabled

**Location:** `workspace/s2_visualizer/src/viz_server.cpp`, `handle_pending_snapshots()`; `workspace/s2_visualizer/web/js/app.js`, `updateScene()`.

**Problem:** Each SSE frame (30 fps) contains the full world snapshot including all trajectory points. With 3 agents each having up to 300 trajectory points, SSE messages grow large and block the socket send buffer, which is shared with the simulation and command threads via mutex.

Multiple attempted mitigations (plugin-data throttling, MSG_DONTWAIT, unlock-before-serialize, JS geometry cache) have not fully resolved the issue.

**Remaining directions:** Binary serialization (MessagePack/protobuf), separate SSE streams per data type (fast stream for poses, slow endpoint for overlay data), or hard cap on `max_points` in `TrajectoryRecorderPlugin`.

---

## 3. Technical Debt

### 3.1 [HIGH] SimEngine header contains ~500 lines of inline simulation logic

**Location:** `workspace/s2_core/include/s2/sim_engine.hpp` (entire file).

**Problem:** The full tick loop, snapshot builder, plugin input dispatch, pose management and reset logic all live in a single header as inline methods. This:

- Increases compilation time for every translation unit that includes `sim_engine.hpp` (currently: `scene_loader.hpp`, `main.cpp`, all test files).
- Keeps closely related subsystem code (viz, transport, collision, zones) in one monolithic class.
- Makes it impractical to unit-test individual phases in isolation.

**Recommended direction:** Move non-inline method bodies to `sim_engine.cpp`. Extract the tick phases into private helper methods. The `build_snapshot()` method (already partially split into `sim_engine_viz.cpp`) is a good model.

---

### 3.2 [MEDIUM] SceneLoader is a 400-line inline header

**Location:** `workspace/s2_core/include/s2/scene_loader.hpp`.

**Problem:** `SceneLoader::load()` and all `parse_*` helpers are implemented inline in the header, despite being used only from `main.cpp` and `SimEngineCommandAdapter`. Compilation cost is paid by every file that includes `sim_engine.hpp` (which includes `scene_loader.hpp`).

**Fix:** Move implementation to `scene_loader.cpp`.

---

### 3.3 [MEDIUM] Duplicate helper functions across test files

**Location:** Every `test_*.cpp` in `workspace/s2_core/tests/`.

**Problem:** `make_agent()`, `make_sphere_zone()`, and similar factory functions are copy-pasted across multiple test files. There is no shared `test_helpers.hpp` or fixture class. If `Agent` or `Zone` struct layout changes, all copies must be updated simultaneously.

**Fix:** Extract common helpers into `workspace/s2_core/tests/test_helpers.hpp` included by test files that need them.

---

### 3.4 [MEDIUM] SSE and WebSocket mixed in a single client set (`ws_clients_`)

**Location:** `workspace/s2_visualizer/src/viz_server.cpp`.

**Problem:** Both SSE connections (`/stream`) and WebSocket connections share the same `ws_clients_` set and receive snapshots via the same `send()` path in `handle_pending_snapshots()`. SSE uses plain `text/event-stream` framing (`data: ...\n\n`); WebSocket uses a binary frame format. The current code sends SSE-formatted text to WebSocket clients and vice versa, relying on the fact that the browser doesn't complain — but the code is semantically incorrect and fragile.

---

### 3.5 [MEDIUM] TripleBuffer is not a true lock-free structure

**Location:** `workspace/s2_core/include/s2/triple_buffer.hpp`.

**Problem:** Despite the docstring advertising "atomic swap without locks", the implementation uses three `std::mutex` objects. `publish()` acquires two mutexes sequentially; `acquire_read()` acquires two mutexes sequentially. This can produce priority inversion and is not lock-free. The docstring is misleading.

**Fix:** Either use `std::atomic<int>` index swaps (true lock-free triple buffer) or update the docstring to accurately describe the mutex-based implementation.

---

### 3.6 [LOW] Global mutable state (`g_viz_server`, `g_broadcast_server`)

**Location:** `workspace/s2_visualizer/src/viz_server.cpp`, lines 20 and 245.

**Problem:** Two file-scope globals (`g_viz_server`, `g_broadcast_server`) hold raw pointers to the `VizServer` instance. `g_broadcast_server` is set on every `/command` request inside a `handle_command()` call that runs on the accept thread. If two `/command` requests are handled concurrently this write is a data race. The globals also make unit testing the server difficult.

**Fix:** Pass the `VizServer*` as a parameter to `handle_command()` rather than through a global.

---

### 3.7 [LOW] No CI/CD pipeline

**Location:** No `.github/workflows/`, no `.gitlab-ci.yml`, no `Jenkinsfile`.

**Problem:** There is no automated build or test verification on commits/PRs. Test regressions are only caught locally via `docker compose up tests`. Pull requests are merged without automated verification.

**Fix:** Add a minimal GitHub Actions workflow that runs `docker compose --project-directory docker up --build tests` on every push to `main` and on every PR.

---

### 3.8 [LOW] No clang-format or clang-tidy configuration

**Location:** Repository root (missing `.clang-format`, `.clang-tidy`).

**Problem:** Formatting is manually enforced by convention. Mixed indentation (2 vs 4 spaces) already exists across headers and source files. Without an automated formatter, style drift will accumulate as more contributors or AI agents touch the codebase.

---

### 3.9 [LOW] apt dependencies not pinned

**Location:** `docker/Dockerfile`, `docker/Dockerfile.ros2`.

**Problem:** System packages are installed with `apt-get install` without version pinning. Rebuilding the Docker image at a future date may produce a different set of library versions, potentially breaking builds silently.

---

## 4. Performance Concerns

### 4.1 [HIGH] SSE snapshot serialization blocks the simulation thread

**Location:** `workspace/s2_visualizer/src/viz_server.cpp`, `handle_pending_snapshots()`; `workspace/s2_core/include/s2/sim_engine.hpp`, `publish_viz()`.

**Problem (detailed in bug 2.5):** The snapshot is copied under `snapshot_mutex_`. The simulation thread calls `publish()` which also acquires `snapshot_mutex_`. If the VizServer's SSE thread holds `snapshot_mutex_` for JSON serialization — even briefly — the simulation tick is stalled. The current fix (serialize outside the mutex after copying) mitigates but does not eliminate the issue because the copy itself is O(n) in snapshot size.

**Recommended improvement:** Use the existing `TripleBuffer<WorldSnapshot>` infrastructure (which is already implemented but underused) to decouple the simulation thread from the VizServer thread with zero-copy semantics.

---

### 4.2 [MEDIUM] O(n*m) loops in ZoneSystem::tick()

**Location:** `workspace/s2_core/src/zone_system.cpp`, `ZoneSystem::tick()`.

**Problem:** For each zone, all agents are iterated to check enter/exit. For applying active effects, all zones are iterated and for each zone all agents are iterated again (`inside_agents` look-up is O(agents)). Total complexity is O(zones * agents) per tick. With 10 zones and 50 agents this is 500+ iterations per tick at 100 Hz — still fast, but there is no spatial acceleration.

For the current scale this is acceptable. At 100+ agents or 100+ zones it becomes a bottleneck.

**Recommended improvement:** When scaling is needed, a spatial hash (zone AABB -> candidate agents) would reduce this to O(agents + active pairs).

---

### 4.3 [MEDIUM] O(n) linear search for agents in SimEngine

**Location:** `workspace/s2_core/include/s2/sim_engine.hpp`, methods `set_agent_pose()`, `handle_plugin_input()`, `get_plugin_inputs_schemas()`.

**Problem:** All three methods iterate `world_.agents()` linearly to find an agent by ID. `handle_plugin_input()` additionally iterates the plugin list. For small agent counts (~10) this is negligible, but it is called from the HTTP request path on every `cmd=plugin_input` and `cmd=move_agent` command, which means it runs while holding no lock (potentially racing with the simulation thread which also iterates agents).

**Recommended improvement:** Use an `std::unordered_map<AgentId, Agent*>` index maintained alongside `world_.agents()`.

---

### 4.4 [MEDIUM] SSE threads accumulate without cleanup

**Location:** `workspace/s2_visualizer/src/viz_server.cpp`, `sse_threads_` vector.

**Problem:** Every new `/stream` connection spawns a `std::thread` that is `push_back`-ed into `sse_threads_`. These threads are only joined in `run_server()` after `running_` becomes false (server shutdown). If a browser connects and disconnects many times over a long run, `sse_threads_` grows unboundedly, holding onto finished-thread handles and OS thread stacks until server shutdown.

**Fix:** Detach SSE threads or join them lazily when a new connection arrives (scan for joinable threads and join them before appending the new one).

---

### 4.5 [LOW] WorldSnapshot copy on every publish() call

**Location:** `workspace/s2_core/include/s2/sim_engine.hpp`, `publish_viz()`; `workspace/s2_visualizer/src/viz_server.cpp`, `publish()`.

**Problem:** `build_snapshot()` constructs a `WorldSnapshot` (heap-allocated vectors of agent/prop/zone structs), which is then `std::move`-d into `publish()`, which stores it by value under a mutex. On the VizServer side, `handle_pending_snapshots()` copies the snapshot again (`snap_copy = pending_snapshot_`). For a scene with many lidar points or large kinematic trees the copy cost is measurable.

---

## 5. Fragile Areas

### 5.1 [HIGH] Phase ordering in SimEngine::tick() is undocumented and order-dependent

**Location:** `workspace/s2_core/include/s2/sim_engine.hpp`, `tick()`.

**Problem:** The tick phases (3a pre_resolve, 3b zone effects, 3d resolve, 3e plugins, 3f kinematics, 3g surface snap, 3h collision) are semantically order-dependent:

- `pre_resolve()` (3a) must precede `resolve()` (3d) or contributions are missed.
- `GravityPlugin::update()` (3e) reads the surface below the agent, but kinematics (3f) moves the agent first — this is the root of bug 2.1.
- Phase 3g (surface snap) is currently empty but reserved for the gravity snap to be moved there.

The phase ordering is described only in inline comments, not in any document or enforced by type-level contracts. Any future change to plugin phase hooks (e.g., adding `post_resolve()`) requires careful reasoning about the ordering.

**Risk:** A new contributor or agent adding a plugin that calls `state.add_scale()` in `update()` instead of `pre_resolve()` will silently produce wrong physics — the contribution is resolved only on the next tick.

---

### 5.2 [HIGH] Concurrent access to world state from simulation and HTTP threads

**Location:** `workspace/s2_visualizer/src/main.cpp`, `SimEngineCommandAdapter`; `workspace/s2_core/include/s2/sim_engine.hpp`, `set_agent_pose()`, `handle_plugin_input()`.

**Problem:** `SimEngine::run()` iterates `world_.agents()` in the simulation thread. `SimEngineCommandAdapter::on_move_agent()` calls `engine_->set_agent_pose()`, which also mutates `world_.agents()`. These are called from different threads (simulation thread vs. HTTP handler thread) with no synchronization. This is an unguarded data race on the agent vector — undefined behavior in C++.

Currently the race is benign in practice (the mutation is a single `Pose3D` write and x86 cache coherency makes partial reads unlikely), but it is technically undefined behavior and will break under thread sanitizers or on non-x86 architectures.

**Fix:** Use a command queue: HTTP handlers post a mutation command; the simulation thread drains the queue at the start of each tick (before iterating agents).

---

### 5.3 [MEDIUM] effect_factory_ must be set before load_world() — unenforced precondition

**Location:** `workspace/s2_core/include/s2/sim_engine.hpp`, `set_effect_factory()` and `load_world()`.

**Problem:** The doc comment states "must be called before load_world(), otherwise effect plugins will not be created." There is no runtime check or assertion; silently calling `load_world()` first produces a world with no zone effects and no error message. In `main.cpp` this ordering is correct, but the test setup in some test files also relies on getting the order right.

**Fix:** Either enforce in `load_world()` with an assertion (`assert(effect_factory_)`), or set the factory as a constructor parameter.

---

### 5.4 [MEDIUM] Plugin factory returns nullptr for unknown types — silently skipped

**Location:** `workspace/s2_plugins/src/plugins_registry.cpp`, `create_plugin()`; `workspace/s2_plugins/src/effects_registry.cpp`, `create_effect()`.

**Problem:** When a YAML scene references an unknown plugin type (e.g., a typo like `"baterry"` instead of `"battery"`), both factories return `nullptr`. `SceneLoader` silently skips `nullptr` plugins. The user sees no error message and the simulation runs with the plugin missing.

**Risk:** Silent misconfiguration — hard to diagnose when a new zone effect or plugin has no visible effect.

**Fix:** Log a warning with the unknown type name when the factory returns nullptr.

---

### 5.5 [MEDIUM] SSE thread sends to all clients while holding clients_mutex_

**Location:** `workspace/s2_visualizer/src/viz_server.cpp`, `handle_pending_snapshots()`.

**Problem:** `handle_pending_snapshots()` acquires `clients_mutex_` for the entire duration of the send loop over all clients. If a client's TCP send buffer is full and `MSG_DONTWAIT` is not set (or the kernel decides to block briefly), other clients and the accept thread are stalled while holding the mutex. Although `MSG_DONTWAIT` is used, the loop itself is not O(1) and slow clients can increase latency for all others.

---

### 5.6 [LOW] ZoneSystem::detection_point always ignores the mode parameter

**Location:** `workspace/s2_core/src/zone_system.cpp`, `ZoneSystem::detection_point()`.

```cpp
Vec3 ZoneSystem::detection_point(const Agent& agent, const std::string& mode)
{
    // "bounding" — TODO: bounding overlap в будущей задаче. Fallback на center.
    (void)mode;
    return agent.world_pose.position();
}
```

**Problem:** The `detection_mode` field on `Zone` (which can be `"center"` or `"bounding"`) has no effect. All zones use center-point detection regardless of configuration. A scene file setting `detection_mode: bounding` will appear to work but will silently behave as `center`.

---

### 5.7 [LOW] SimBus does not support unsubscription during dispatch

**Location:** `workspace/s2_core/include/s2/sim_bus.hpp`.

**Problem:** The docstring explicitly states: "does not support unsubscription during dispatch — this is undefined behavior." If any subscriber calls `bus.subscribe()` or needs to remove itself inside its handler, the handler vector is modified while being iterated. There is no guard (e.g., iterating a copy of the handler list).

For the current subscriber set (ZoneSystem, TeleportEffect callback) this is not triggered. It becomes a risk when more complex plugin interactions are added.

---

### 5.8 [LOW] SENSOR EffectType registered but never dispatched

**Location:** `workspace/s2_core/src/zone_system.cpp`, `apply_active_effects()`; `workspace/s2_core/include/s2/types.hpp`.

**Problem:** `EffectType::SENSOR` exists in the enum and is mentioned in comments as "задача 31", but the `switch` in `apply_active_effects()` has a no-op `case EffectType::SENSOR:`. Any effect plugin that declares `effect_type() == EffectType::SENSOR` will be silently ignored even if it is in a zone with an agent inside it.

---

## Summary Table

| # | Area | Severity | Category |
|---|------|----------|----------|
| 1.1 | Shell injection in WebSocket handshake | Critical | Security |
| 1.2 | No authentication on any endpoint | High | Security |
| 1.3 | Path traversal in static file server | Medium | Security |
| 1.4 | Scene file injection via load API | Medium | Security |
| 2.1 | Robot jitter on slopes | High | Bug |
| 2.2 | Agent preview not shown immediately | Low | Bug |
| 2.3 | Scene reload skips ROS2 reinit | Medium | Bug |
| 2.4 | Stale TF frames after scene reload | Low | Bug |
| 2.5 | Latency with overlay plugins | High | Bug/Perf |
| 3.1 | SimEngine header has inline simulation logic | High | Tech Debt |
| 3.2 | SceneLoader is a large inline header | Medium | Tech Debt |
| 3.3 | Duplicate test helper functions | Medium | Tech Debt |
| 3.4 | SSE and WebSocket mixed in one client set | Medium | Tech Debt |
| 3.5 | TripleBuffer is not lock-free (misleading doc) | Medium | Tech Debt |
| 3.6 | Global mutable pointers in VizServer | Low | Tech Debt |
| 3.7 | No CI/CD pipeline | Low | Tech Debt |
| 3.8 | No clang-format/clang-tidy | Low | Tech Debt |
| 3.9 | apt dependencies not pinned | Low | Tech Debt |
| 4.1 | Snapshot serialization can stall sim thread | High | Performance |
| 4.2 | O(n*m) zone-agent loops per tick | Medium | Performance |
| 4.3 | O(n) agent search in command path | Medium | Performance |
| 4.4 | SSE threads accumulate without cleanup | Medium | Performance |
| 4.5 | WorldSnapshot copied twice per publish | Low | Performance |
| 5.1 | Tick phase ordering undocumented and fragile | High | Fragile |
| 5.2 | Data race: HTTP thread mutates world during tick | High | Fragile |
| 5.3 | Unenforced precondition: set_effect_factory before load_world | Medium | Fragile |
| 5.4 | Unknown plugin types silently skipped | Medium | Fragile |
| 5.5 | clients_mutex_ held during entire send loop | Medium | Fragile |
| 5.6 | detection_mode field has no effect | Low | Fragile |
| 5.7 | SimBus unsafe if handler modifies subscriber list | Low | Fragile |
| 5.8 | SENSOR EffectType declared but never dispatched | Low | Fragile |

---

*Concerns analysis: 2026-04-25*
