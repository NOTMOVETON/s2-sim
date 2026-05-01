---
phase: 02-unified-entity-model
plan: 01
status: complete
completed: 2026-05-02
---

# Summary: 02-01 — Core Entity Types + SimWorld + Consumer Updates

## Что сделано

### Task 1: entity.hpp — flat structs (не наследование)
- `types.hpp`: добавлен `enum class EntityType { AGENT, ACTOR, PROP };`
- Создан `entity.hpp`: плоские структуры Agent/Actor/Prop с Phase 2 полями в конце
- `agent.hpp`, `actor.hpp`, `prop.hpp`: переписаны как thin wrappers (`#include <s2/entity.hpp>`)
- Alias: `using AgentData = Agent;`, `using ActorData = Actor;`, `using PropData = Prop;`

**Ключевое решение**: C++20 designated initializers (`Agent{.id=1}`) требуют ПРЯМЫХ членов,
не унаследованных. Поэтому отказались от `struct AgentData : EntityBase` в пользу flat struct
с Phase 2 полями дописанными в конец. Тесты не ломаются, обратная совместимость сохранена.

### Task 2: world.hpp — Variant D storage
- `SimWorld` имеет 3 типизированных вектора + 4 индексные карты:
  - `agents_`, `actors_`, `props_` — cache-friendly layout
  - `entity_type_`, `agent_idx_`, `actor_idx_`, `prop_idx_` — O(1) lookup по EntityId
- `add_agent/actor/prop()` — обновляют все 4 картысде
- `get_agent/actor/prop(EntityId)` — O(1) через соответствующий `_idx_`
- `remove_agent()` — swap-and-pop, обновляет индекс перемещённого
- `agents()/actors()/props()` — ссылки на типизированные векторы (+ const overloads)

### Task 3: Consumer updates
- `effect_context.hpp`: добавлен `EntityId entity_id{0};` рядом с `agent_id`
- `zone_system.hpp`: добавлены 2 overloads tick():
  - `tick(SimWorld&, ...)` — для SimEngine (новый)
  - `tick(vector<Agent>&, const vector<Actor>&, ...)` — для тестов (сохранён)
- `zone_system.cpp`: SimWorld-версия делегирует в vector-версию
- `world_snapshot.hpp`: исправлен баг `PropSnapshot::id` — `ActorId` → `ObjectId`
- `sim_engine.hpp`: `zone_system_.tick(world_, bus_, sim_time_, dt_)` вместо векторов

## Acceptance Criteria

- [x] AC-1: AgentData/ActorData/PropData существуют; PropData без SharedState
- [x] AC-2: SimWorld.get_agent(id) — O(1) через agent_idx_
- [x] AC-3: ZoneSystem::tick() принимает SimWorld& (SimEngine не ломается)
- [x] AC-4: PropSnapshot.id = ObjectId (баг исправлен)
- [x] AC-5: 100% тестов проходят (2/2 test suites, 0 failures)

## Файлы изменены

- `workspace/s2_core/include/s2/types.hpp`
- `workspace/s2_core/include/s2/entity.hpp` (создан)
- `workspace/s2_core/include/s2/agent.hpp`
- `workspace/s2_core/include/s2/actor.hpp`
- `workspace/s2_core/include/s2/prop.hpp`
- `workspace/s2_core/include/s2/world.hpp`
- `workspace/s2_core/include/s2/effect_context.hpp`
- `workspace/s2_core/include/s2/zone_system.hpp`
- `workspace/s2_core/src/zone_system.cpp`
- `workspace/s2_core/include/s2/world_snapshot.hpp`
- `workspace/s2_core/include/s2/sim_engine.hpp`

## Известные ограничения / отложено на Phase 02-02

- SceneLoader не знает о новых полях (tags, collision, entity_type, immune_to_effects)
- `signals` и `owned_zones` — пустые заглушки (Phase 12/14)
- `remove_actor`, `remove_prop` — не реализованы (Phase 3, KernelCommands)
- transport-поля агентов живут в `tags["transport_type"]` временно (Phase 8 — TransportLink)
