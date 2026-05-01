# Phase 2 Context: Unified Entity Model

Date: 2026-05-02
Status: Ready for planning

## Goals

1. **Правильный фундамент для Phases 3–27** — все последующие фазы (KernelCommands, WorldQuery, IActorBehavior, EffectPlugin, Transport) строятся поверх Entity model. Закладываем сразу правильно.

2. **Cache-friendly layout** — путь к SoA, KinematicKernel, GPU batch без переписывания хранилища.

3. **Unified EntityId namespace** — KernelCommands (SpawnEntity/DespawnEntity), WorldQuery (find_in_radius), Interact работают с одним типом EntityId. Никаких AgentId/ActorId/ObjectId в публичном API.

4. **Props без SharedState по структуре** — не runtime assertion, а отсутствие поля. Нельзя случайно добавить.

## Approach: Variant D (typed vectors + index registry)

### Хранилище

```cpp
// Contiguous per-type storage (cache-friendly для tick loop)
std::vector<AgentData> agents_;
std::vector<ActorData> actors_;
std::vector<PropData>  props_;

// Unified lookup O(1)
std::unordered_map<EntityId, EntityType> entity_type_;
std::unordered_map<EntityId, size_t>     agent_idx_;
std::unordered_map<EntityId, size_t>     actor_idx_;
std::unordered_map<EntityId, size_t>     prop_idx_;
```

### Структуры данных

```cpp
struct EntityBase {
    EntityId id;
    EntityType type;              // AGENT / ACTOR / PROP
    std::string name;
    Pose3D world_pose;
    CollisionShape collision;
    VisualDesc visual;
    std::unordered_set<std::string> capabilities;
    std::map<std::string, std::string> tags;
    std::unordered_set<std::string> immune_to_effects;
    std::vector<Signal> signals;  // заглушка (Phase 14)
    bool enabled{true};
    std::vector<ZoneId> owned_zones;  // заглушка (Phase 12)
};

struct AgentData : EntityBase {
    // Полный набор слоёв
    PluginHost plugin_host;           // vector<unique_ptr<IAgentPlugin>>
    SharedState shared_state;         // contributions + resolver
    TransportLink transport_link;     // тип + config (domain_id здесь)
    std::optional<LinkTree> link_tree; // URDF (nullable)
    Velocity world_velocity;
    OwnEffects own_effects;           // заглушка (Phase 7)
};

struct ActorData : EntityBase {
    BehaviorSlot behavior;                    // IActorBehavior
    std::optional<PluginHost> plugin_host;    // если нужны сенсоры
    std::optional<SharedState> shared_state;  // если подвержен эффектам зон
    OwnEffects own_effects;                   // заглушка (Phase 7)
};

struct PropData : EntityBase {
    bool movable{false};
    std::map<std::string, std::string> properties;
    // NO SharedState — enforcement через отсутствие поля
};
```

### Что меняется в API

| Компонент | Было | Станет |
|-----------|------|--------|
| IAgentPlugin lifecycle | `Agent&` | `Entity&` (= AgentData&) |
| ZoneSystem.tick() | `vector<Agent>&, vector<Actor>&` | entity-агностичный диапазон |
| EffectContext | `AgentId agent_id` | `EntityId entity_id` |
| SceneData | `vector<Agent>, vector<Prop>, vector<Actor>` | `vector<AgentData>, vector<ActorData>, vector<PropData>` |
| SimWorld | `get_agent(AgentId)` | `get_agent(EntityId)`, `get_entity_type(EntityId)` |
| YAML | `domain_id: 50` на агенте | `transport: ros2` + `ros2: {domain_id: 50}` |

### Что НЕ меняется

- Порядок фаз тика (Phase 10 займётся)
- SharedState внутренняя структура (Phase 7)
- EffectPlugin.apply() сигнатура — только EffectContext.agent_id → entity_id
- Тесты на plugin lifecycle (Phase 1 — остаются рабочими)
- KinematicTree — переезжает в AgentData.link_tree без изменений

### Технические детали

**Despawn (swap-and-pop):**
```cpp
void remove_agent(EntityId id) {
    size_t idx = agent_idx_[id];
    size_t last = agents_.size() - 1;
    if (idx != last) {
        std::swap(agents_[idx], agents_[last]);
        agent_idx_[agents_[idx].id] = idx;  // обновить индекс последнего
    }
    agents_.pop_back();
    agent_idx_.erase(id);
    entity_type_.erase(id);
}
```

**WorldQuery combined iteration:**
```cpp
// find_in_radius итерирует все три вектора последовательно
// EntityFilter фильтрует по типу и capabilities
```

**YAML backward compatibility:**
- Поле `transport` — опциональное, default = "ros2"
- Поле `domain_id` на верхнем уровне — deprecated but supported (fallback)

## Boundaries (что Phase 2 НЕ делает)

- Не вводит KinematicKernel как отдельную структуру (Phase 10+)
- Не меняет порядок тика
- Не реализует BehaviorSlot логику (Phase 5)
- Не реализует TransportPool (Phase 8)
- Не реализует OwnEffects (Phase 7)
- Signals, owned_zones — заглушки (пустые векторы)

## Research

Детали из исследования кодовой базы:
- `.paul/phases/02-unified-entity-model/RESEARCH.md`
- `.paul/phases/02-unified-entity-model/research/agent-class-hierarchy.md`
- `.paul/phases/02-unified-entity-model/research/sim-engine-structure.md`
- `.paul/phases/02-unified-entity-model/research/zone-system-scene-loader.md`

## Файлы для изменения

| Файл | Изменение |
|------|-----------|
| `s2/agent.hpp` | AgentData (новое имя или рефакторинг) |
| `s2/actor.hpp` | ActorData |
| `s2/prop.hpp` | PropData |
| `s2/world.hpp` | 3 вектора + 4 index map |
| `s2/sim_engine.hpp` | обращения к world_, initial_states_ |
| `s2/zone_system.hpp/.cpp` | tick() сигнатура |
| `s2/interfaces/effect_plugin.hpp` | EffectContext.agent_id → entity_id |
| `s2/plugin_base.hpp` | Agent& → Entity& в lifecycle |
| `s2/scene_loader.hpp` | SceneData, новый YAML парсинг |
| `s2/world_snapshot.hpp` | build_snapshot() + fix PropSnapshot bug |

---
*Context created: 2026-05-02*
*Ready for: /paul:plan*
