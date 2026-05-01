# Research: SimEngine structure + entity registry

## Файлы

- `workspace/s2_core/include/s2/sim_engine.hpp` (строки 470-749)
- `workspace/s2_core/include/s2/world.hpp`

## SimEngine поля (sim_engine.hpp, строки 723-749)

```cpp
SimWorld world_;              // Контейнер сущностей
SimBus bus_;                  // Шина событий
CollisionSystem collision_system_;
RaycastEngine raycast_engine_;
ZoneSystem zone_system_;
EffectFactory effect_factory_;
VizServer* viz_server_ = nullptr;  // Не владеет
double sim_time_{0.0};
double dt_{0.0};
std::atomic<bool> running_{false};
bool paused_{false};
std::map<AgentId, AgentInitialState> initial_states_;
double viz_timer_{0.0};
double transport_timer_{0.0};
PostTickCallback post_tick_cb_;
```

## SimWorld (world.hpp)

Три отдельных вектора + зоны + статика:
```cpp
std::vector<Agent> agents_;
std::vector<Prop> props_;
std::vector<Actor> actors_;
std::vector<Zone> zones_;           // При load_world переезжают в ZoneSystem
std::vector<WorldPrimitive> static_geometry_;
Heightmap heightmap_;
```

**Нет единого реестра** — каждый тип в своём контейнере.

## Регистрация (world.hpp, строки 55-77)

Отдельные методы:
```cpp
void add_agent(Agent agent);
void add_prop(Prop prop);
void add_actor(Actor actor);
void add_zone(Zone zone);
void add_static_primitive(WorldPrimitive prim);
```

Поиск O(n) — линейный проход по вектору:
```cpp
Agent* get_agent(AgentId id);
Prop* get_prop(ObjectId id);
Actor* get_actor(ActorId id);
Zone* get_zone(const ZoneId& id);
```

**Для Phase 2:** нужен `unordered_map<EntityId, Entity>` → O(1) lookup.

## Порядок фаз tick() (sim_engine.hpp, строки 470-715)

```
0. Проверка паузы
   sim_time += dt

1. Акторы (FSM) — ПУСТО (задача 07)

2. Зоны
   zone_system_.tick(world_.agents(), world_.actors(), bus_, sim_time_, dt_)
   - Обновление позиций attached-зон
   - Проверка enter/exit
   - Применение MODIFIER и CONTINUOUS эффектов

3. Для каждого агента:
   a. Resource modules: plugin->pre_resolve(dt, agent)
   b. Own effects CONTINUOUS — ПУСТО
   c. Zone effects CONTINUOUS — применены в п.2
   d. Resolver: agent.state.resolve()
   e. Плагины: plugin->update(dt, agent)
      - Собрать bounding-примитивы других агентов
      - raycast_engine_.set_dynamic_agents(agent_bounds)
   f. Kinematics: world_pose обновляется по velocity
   g. Surface snap — ПУСТО
   h. Collision detection (сфера vs статика)
      - walkable: Z-only push-out
      - стены: horizontal slide + push-out
      - pitch/roll по нормали
   i-l. Joints, KinematicTree, Sensors, Interactions — ПУСТО
   clear_contributions()

4. Attachments — ПУСТО

5. Snapshot + Viz publish (по таймеру viz_timer_)

6. Transport publish (post_tick_cb_ по таймеру transport_timer_)
```

## Передача данных ZoneSystem

При load_world():
```cpp
for (auto& zone : world_.zones())
    zone_system_.add_zone(std::move(zone));
world_.zones().clear();
```

При tick():
```cpp
zone_system_.tick(world_.agents(), world_.actors(), bus_, sim_time_, dt_);
// ZoneSystem получает vector<Agent>& и const vector<Actor>&
```

ZoneSystem хранит копию зон (`std::vector<Zone> zones_`).

## Иерархия владения

```
SimEngine
  ├── SimWorld (данные)
  │     ├── vector<Agent>
  │     ├── vector<Prop>
  │     ├── vector<Actor>
  │     └── static_geometry, heightmap
  ├── ZoneSystem (владеет копией зон)
  ├── CollisionSystem
  ├── RaycastEngine
  └── SimBus (шина событий)
VizServer* (SimEngine не владеет — raw pointer)
```

## Вывод для Phase 2

- SimWorld → заменить 3 вектора на `unordered_map<EntityId, Entity>`
- Методы add_agent/add_prop/add_actor → единый `add_entity(Entity)`
- get_agent/get_prop/get_actor → `get_entity(EntityId)` O(1)
- ZoneSystem.tick() сигнатура → принимать Entity-агностичный контейнер
- Порядок фаз пока не меняется (Phase 10 займётся этим)
- initial_states_ привязан к AgentId → нужно переработать на EntityId
