# Technology Stack

**Analysis Date:** 2026-04-25

## Languages

**Primary:**
- C++17 - Core simulation engine, plugins, transport, visualizer backend (`workspace/s2_core/`, `workspace/s2_plugins/`, `workspace/s2_transport/`, `workspace/s2_visualizer/`)
- Standard set via `set(CMAKE_CXX_STANDARD 17)` in `workspace/CMakeLists.txt`

**Secondary:**
- JavaScript (ES Modules) - Web visualizer frontend (`workspace/s2_visualizer/web/js/app.js`, ~2753 lines)
- HTML/CSS - Visualizer UI (`workspace/s2_visualizer/web/index.html`)
- YAML - Scene configuration (`workspace/s2_config/scenes/*.yaml`)
- XML (URDF) - Robot model definitions (`workspace/s2_config/robots/*.urdf`)

## Runtime

**Environment:**
- Docker containers based on `ubuntu:22.04` (primary, `docker/Dockerfile`) and `ubuntu:noble` (ROS2, `docker/Dockerfile.ros2`)
- All builds, tests, and runs execute inside Docker (see `CLAUDE.md` section 4)

**Package Manager:**
- System packages via `apt-get` (no language-level package manager like npm/pip/cargo)
- C++ dependencies installed via Ubuntu system packages
- No lockfile mechanism (apt pin versions not enforced)

## Frameworks

**Core:**
- ROS2 Jazzy - Optional robotics middleware for transport layer (`docker/Dockerfile.ros2`, line 39: `ros-jazzy-ros-base`)
- Eigen3 (>=3.3) - Linear algebra, spatial math, SIMD (`workspace/CMakeLists.txt` line 9)
- CMake (>=3.16) - Build system (`workspace/CMakeLists.txt` line 1)

**Testing:**
- Google Test (GTest) - Unit testing framework (`workspace/s2_core/CMakeLists.txt` line 69)
- Google Mock (GMock) - Available but not heavily used (installed in Dockerfile)

**Build/Dev:**
- CMake - Build orchestration, all subprojects use `add_subdirectory()`
- colcon - ROS2 workspace build (`docker/docker-compose.yml` line 28)
- Docker Compose - Service orchestration (`docker/docker-compose.yml`)

## Key Dependencies

**Critical (C++ libraries):**
- `Eigen3` (>=3.3) - All spatial math: `Vec3`, `Transform3D`, `Pose3D` rotation matrices. Aliased as `s2::Vec3 = Eigen::Vector3d` in `workspace/s2_core/include/s2/types.hpp`
- `yaml-cpp` - Scene loading from YAML. Used in `workspace/s2_core/include/s2/scene_loader.hpp`
- `nlohmann_json` (>=3.0) - JSON serialization for viz snapshots, plugin data, transport. Used throughout `workspace/s2_core/` and `workspace/s2_visualizer/`
- `GeographicLib` - WGS84 coordinate conversions for GNSS plugin and earth-to-map TF. Used in `workspace/s2_core/include/s2/geo_origin.hpp` and `workspace/s2_plugins/`
- `tinyxml2` - URDF XML parsing. Used in `workspace/s2_core/src/urdf_loader.cpp`
- `OpenSSL` - WebSocket handshake (SHA-1 + Base64). Used in `workspace/s2_visualizer/src/viz_server.cpp`
- `uWebSockets` (uSockets) - NOT used as a library API. Only uSockets static lib is linked; the viz server uses raw POSIX sockets. Installed from git in Dockerfile.

**ROS2 packages (optional, when `S2_WITH_ROS2=ON`):**
- `rclcpp` - ROS2 C++ client library
- `geometry_msgs` - Twist messages for `/cmd_vel`
- `sensor_msgs` - NavSatFix, Imu, LaserScan, BatteryState
- `nav_msgs` - Odometry, Path
- `std_srvs` - Trigger service (fallback when s2_msgs unavailable)
- `std_msgs` - String messages for events
- `tf2_ros` - Transform broadcasting (odom->base_link, earth->map)
- `s2_msgs` - Custom service type `PluginCall.srv` (built separately via colcon, `workspace/s2_msgs/`)

**Frontend (browser):**
- `Three.js` (imported as ES module) - 3D rendering in browser (`workspace/s2_visualizer/web/js/app.js` line 1)
- `OrbitControls` (Three.js addon) - Camera controls
- `TransformControls` (Three.js addon) - Interactive object manipulation

## Configuration

**Scene Configuration:**
- YAML files in `workspace/s2_config/scenes/` define worlds, agents, plugins, zones
- Key fields: `s2.update_rate`, `s2.viz_rate`, `s2.transport_rate`, `s2.transport.type`, `s2.visualizer.port`
- Robot URDF models in `workspace/s2_config/robots/`

**Build Configuration:**
- `workspace/CMakeLists.txt` - Root CMake config
- `docker/docker-compose.yml` - Docker service definitions (5 services: dev, build, tests, sim, sim_ros2)
- `docker/Dockerfile` - Base image (Ubuntu 22.04, no ROS2)
- `docker/Dockerfile.ros2` - Full image with ROS2 Jazzy (Ubuntu Noble)
- `docker/fastdds.xml` - FastDDS transport config for ROS2 networking

**Compile-time flags:**
- `S2_WITH_ROS2` (CMake option, default OFF) - Enables ROS2 transport. Set via `-DS2_WITH_ROS2=ON` in docker-compose
- `S2_WITH_S2_MSGS` (auto-detected) - Enables custom PluginCall service when s2_msgs package is found
- `CMAKE_BUILD_TYPE=Debug` - Default build type in docker-compose

**Environment Variables (runtime):**
- `RMW_IMPLEMENTATION=rmw_fastrtps_cpp` - ROS2 middleware selection (set in docker-compose)
- `FASTRTPS_DEFAULT_PROFILES_FILE` - FastDDS config path (set in sim_ros2 service)
- `ROS_DISTRO=jazzy` - ROS2 distribution (set in Dockerfile.ros2)

## Platform Requirements

**Development:**
- Docker and Docker Compose required
- No native build supported (all dependencies live in Docker images)
- Source code mounted as volume: `../workspace:/workspace`

**Production:**
- Runs as Docker container
- Visualizer accessible at `http://localhost:1937` (port mapped in docker-compose)
- Default scene: `workspace/s2_config/scenes/test_two_robots.yaml`

**Host OS:**
- Linux recommended (Docker with volume mounts)
- Port 1937 must be available for visualizer

## Build Commands

```bash
# Full build with ROS2 and tests
docker compose --project-directory docker up --build build

# Run tests
docker compose --project-directory docker up --build tests

# Run simulation with visualizer
docker compose --project-directory docker up --build sim

# Run simulation with ROS2 networking (host mode)
docker compose --project-directory docker up --build sim_ros2
```

## Subproject Structure

| Subproject | Type | CMake Target | Dependencies |
|---|---|---|---|
| `workspace/s2_core` | Static lib | `s2_core` | Eigen3, yaml-cpp, nlohmann_json, GeographicLib, tinyxml2 |
| `workspace/s2_plugins` | Static lib | `s2_plugins` | s2_core, GeographicLib |
| `workspace/s2_transport` | Static lib | `s2_transport` | s2_core, (ROS2 if S2_WITH_ROS2) |
| `workspace/s2_visualizer` | Executable | `s2_sim` | s2_core, s2_plugins, s2_transport, nlohmann_json, OpenSSL, pthread |
| `workspace/s2_msgs` | ROS2 package | via colcon | ament_cmake, rosidl |

---

*Stack analysis: 2026-04-25*
