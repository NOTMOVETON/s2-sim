# S2 Simulator

Kinematic multi-agent robot fleet simulator. C++17, CMake, Docker-first.

## Quick Start

```bash
docker compose --project-directory docker up --build tests   # run tests
docker compose --project-directory docker up --build sim     # run sim → http://localhost:1937
```

## Project Planning (GSD)

```bash
/gsd-plan-phase 1      # plan next phase
/gsd-execute-plan      # execute current phase plan
/gsd-verify-phase      # verify phase goal achieved
/gsd-map-codebase      # refresh codebase map
```

**Current state:** `.planning/STATE.md` — Phase 1 (Zone Visual & Control Layer) not started.
**Roadmap:** `.planning/ROADMAP.md` — 8 phases.
**Requirements:** `.planning/REQUIREMENTS.md` — 36 requirements (ZONE, ACTR, PROP, PERC, TRAN, ENTY, MATL, VIZL).

## Rules

- **Language:** Russian in all code comments, docs, and responses. Don't translate API/technical terms.
- **Docker-first:** all builds and tests inside Docker only.
- **Architecture:** no plugin knows about another plugin — everything through SharedState contributions.
- **s2_core** must not know about domain types from s2_plugins.
- **No emojis** in code or docs.
- Small steps: each change is buildable, testable, understandable on its own.

## Workspace Layout

```
workspace/
  s2_core/        # SimEngine, SharedState, ZoneSystem, CollisionSystem
  s2_plugins/     # IAgentPlugin implementations (DiffDrive, Battery, Lidar, ...)
  s2_transport/   # ROS2 transport adapter
  s2_visualizer/  # Three.js web visualizer + SSE server
  s2_config/      # YAML scene configs
  s2_msgs/        # ROS2 message definitions
.planning/        # GSD planning artifacts
docs/             # task files (37.1-zone-flat.md, etc.)
```
