# External Integrations

**Analysis Date:** 2026-04-25

## APIs & External Services

**ROS2 (Optional Transport Layer):**
- ROS2 Jazzy provides the external robotics integration
- SDK/Client: `rclcpp` (C++ ROS2 client library)
- Enabled via compile flag: `S2_WITH_ROS2=ON`
- Middleware: FastDDS (`RMW_IMPLEMENTATION=rmw_fastrtps_cpp`)
- Config: `docker/fastdds.xml` (UDPv4 transport, maxInitialPeersRange=400)
- Implementation: `workspace/s2_transport/src/ros2_transport_adapter.cpp`
- Stub fallback: `workspace/s2_transport/src/ros2_transport_adapter_stub.cpp` (all methods no-op when ROS2 disabled)

**No other external APIs, cloud services, or third-party SaaS integrations detected.**

## Data Storage

**Databases:**
- None. The simulator is entirely in-memory with no persistent storage.

**File Storage:**
- Local filesystem only
- Scene files read from `workspace/s2_config/scenes/*.yaml`
- URDF robot models read from `workspace/s2_config/robots/*.urdf`
- Scene editor saves modified YAML back to disk via `s2::SceneWriter` (`workspace/s2_core/include/s2/scene_writer.hpp`)

**Caching:**
- None. Simulation state is held in `SimWorld` struct in memory.
- Triple buffer (`workspace/s2_core/include/s2/triple_buffer.hpp`) used for lock-free snapshot transfer between simulation and visualization threads.

## Authentication & Identity

**Auth Provider:**
- None. No authentication on any endpoint.
- VizServer HTTP/WebSocket/SSE server has no auth (`workspace/s2_visualizer/src/viz_server.cpp`)
- ROS2 transport uses DDS security model (not configured, runs open)

## Communication Protocols

**WebSocket (Browser <-> SimEngine):**
- Custom WebSocket implementation using raw POSIX sockets + OpenSSL for handshake
- Implementation: `workspace/s2_visualizer/src/viz_server.cpp`
- Used for: bidirectional commands (pause/resume/reset/move_agent/plugin_input)
- Handshake uses `openssl sha1` CLI command via `popen()` for SHA-1 computation

**Server-Sent Events (SSE):**
- SimEngine publishes WorldSnapshot to connected browser clients via SSE
- Each SSE client runs in its own thread
- Snapshot throttled by `viz_rate` config (default: 30 Hz)

**HTTP Static File Server:**
- Serves `workspace/s2_visualizer/web/` directory (index.html, js/app.js)
- Same port as SSE/WebSocket (default: 1937)

**ROS2 Topics (published by simulator):**
- Per-agent (each agent gets its own ROS2 domain):
  - `/gnss/fix` or `/gnss/<name>/fix` - `sensor_msgs/NavSatFix`
  - `/imu/data` or `/imu/<name>/data` - `sensor_msgs/Imu`
  - `/odom` - `nav_msgs/Odometry`
  - `/<lidar_name>` - `sensor_msgs/LaserScan`
  - `/battery/state` - `sensor_msgs/BatteryState`
  - TF: `odom -> base_link` (dynamic), `earth -> map`, `map -> odom` (static)
  - Static TF for fixed joints and sensor mounting frames
- Interface: `workspace/s2_core/include/s2/transport_adapter.hpp`
- Implementation: `workspace/s2_transport/include/s2/ros2_transport_adapter.hpp`

**ROS2 Topics (subscribed by simulator):**
- `/cmd_vel` - `geometry_msgs/Twist` (velocity commands for DiffDrive plugin)
- `/plan` - `nav_msgs/Path` (path display)
- Additional input topics registered dynamically by plugins via `command_topics()`

**ROS2 Services (provided by simulator):**
- Plugin-specific services registered via `register_service()`
- Two service types supported:
  - `std_srvs/Trigger` (simple trigger)
  - `s2_msgs/PluginCall` (JSON request/response, defined in `workspace/s2_msgs/srv/PluginCall.srv`)
- Fallback to `std_srvs/Trigger` when `s2_msgs` package not built

**ROS2 Events (published by simulator):**
- Event publishers created lazily per-topic (`std_msgs/String`)
- Used for plugin events (ArUco detection, zone triggers)
- Interface: `TransportEvent` struct in `workspace/s2_core/include/s2/transport_adapter.hpp`

## Transport Architecture

**Abstraction Layer:**
- `ITransportAdapter` - abstract interface (`workspace/s2_core/include/s2/transport_adapter.hpp`)
- Designed for multiple transport implementations (ROS2, future: MQTT, gRPC)
- All methods thread-safe (called from sim-thread and transport-threads)

**Bridge Pattern:**
- `SimTransportBridge` (`workspace/s2_transport/include/s2/sim_transport_bridge.hpp`) connects SimEngine to transport adapter
- Registers agents, sensors, topics, services at init
- Installs `post_tick_callback` on SimEngine for periodic sensor publishing
- Publishing frequency controlled by `transport_rate` config (default: 30 Hz)

**ROS2 Domain Isolation:**
- Each agent operates in its own ROS2 domain (separate `rclcpp::Context` + `rclcpp::Node`)
- Domain ID configured per-agent in YAML: `domain_id: 0`, `domain_id: 1`, etc.
- Prevents topic collision between agents

## Monitoring & Observability

**Error Tracking:**
- None. Errors written to `std::cerr` / `std::cout`.

**Logs:**
- Console logging via `std::cout` / `std::cerr`
- Tagged with `[Main]`, `[s2_transport]` prefixes
- No structured logging framework

**Metrics:**
- None. No metrics collection or export.

## CI/CD & Deployment

**Hosting:**
- Docker containers, local deployment only
- No cloud hosting or CI/CD pipeline detected

**CI Pipeline:**
- None configured (no `.github/workflows/`, no `.gitlab-ci.yml`, no `Jenkinsfile`)

## Environment Configuration

**Required env vars (set in docker-compose):**
- `RMW_IMPLEMENTATION=rmw_fastrtps_cpp` - Required for ROS2 transport
- `FASTRTPS_DEFAULT_PROFILES_FILE` - Required for sim_ros2 service with custom DDS config

**Optional env vars:**
- `ROS_DISTRO=jazzy` - Set in Dockerfile.ros2, used by ROS2 tooling

**Secrets location:**
- No secrets required. Project has no authentication, no API keys, no cloud credentials.

**Config files existence:**
- No `.env` files present
- No credentials files present

## Webhooks & Callbacks

**Incoming:**
- WebSocket commands from browser: pause, resume, reset, move_agent, plugin_input, update_geometry, save_scene, load_scene, etc.
  - Handled by `VizCommandHandler` interface (`workspace/s2_visualizer/src/viz_server.hpp`)
  - Implemented by `SimEngineCommandAdapter` in `workspace/s2_visualizer/src/main.cpp`

**Outgoing:**
- SSE snapshots to browser clients (WorldSnapshot as JSON)
- ROS2 topic publications (sensor data, TF frames)
- ROS2 event publications (plugin events as `std_msgs/String`)

## Internal Event Bus

**SimBus:**
- Typed, synchronous, in-process event bus (`workspace/s2_core/include/s2/sim_bus.hpp`)
- NOT an external integration, but the primary internal communication mechanism
- Events: `AgentEnteredZone`, `AgentExitedZone`, `ObjectAttached`, `ObjectReleased`, `ActorStateChanged`, `AgentCollision`, `TeleportAgentCommand`, `SetZoneTeleportTargetCommand`
- Used by: ZoneSystem, plugins, effects

## Custom ROS2 Message Types

**s2_msgs package:**
- Location: `workspace/s2_msgs/`
- Built separately via colcon before main CMake build
- Service: `PluginCall.srv` - Generic JSON request/response for plugin services
  ```
  string request_json
  ---
  bool success
  string response_json
  ```
- Optional: falls back to `std_srvs/Trigger` when not available

---

*Integration audit: 2026-04-25*
