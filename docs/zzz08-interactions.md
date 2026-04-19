# Task 08 — Interaction модули: Grabber + DoorOpener

> **Предыдущий шаг:** `07-actors-door.md` (акторы и FSM)
> **Следующий шаг:** `09-visualizer.md`
> **Контекст:** `ARCHITECTURE.md` разделы 5.3, 11, 12

## Цель

Агент взаимодействует с миром. После этого шага: агент может подобрать/отпустить Prop, агент может открыть дверь через Interaction модуль. Attachment работает.

## Что сделать

### 1. InteractionModule интерфейс

```cpp
class InteractionModule {
public:
    virtual ~InteractionModule() = default;
    virtual void on_init(const YAML::Node& config) = 0;
    virtual void on_tick(Agent& agent, SimWorld& world, SimBus& bus, double dt) = 0;

    // Входящая команда от стека робота
    virtual void handle_command(const std::string& command, const YAML::Node& params) = 0;

    // Последний результат (для snapshot)
    virtual std::optional<std::string> last_result() const = 0;
};
```

**Примечание:** InteractionModule получает доступ к SimWorld (World Query API) и SimBus (Event Bus + Kernel Commands). Это осознанное решение — interaction модулям нужно видеть мир и менять его.

### 2. AttachmentSystem

```
s2_core/include/s2/attachment.hpp
```

```cpp
struct Attachment {
    ObjectId object_id;
    AgentId agent_id;
    std::string link_name;  // пока "base_link" для всех (kinematic tree позже)
    Vec3 offset;
};

class AttachmentSystem {
public:
    bool attach(ObjectId obj, AgentId agent, const std::string& link, Vec3 offset);
    bool detach(ObjectId obj);
    bool transfer(ObjectId obj, AgentId from, AgentId to, const std::string& new_link);

    // Обновить позы привязанных объектов (каждый тик)
    void update(const std::vector<Agent>& agents, std::vector<Prop>& props);

    std::vector<ObjectId> objects_held_by(AgentId agent) const;
    std::optional<AgentId> holder_of(ObjectId obj) const;

private:
    std::vector<Attachment> attachments_;
};
```

### 3. GrabberModule (Interaction)

```
s2_plugins/interactions/grabber.hpp / .cpp
```

handle_command("grab", {object_id: "box_1"}):
1. Найти объект в мире (World Query)
2. Проверить расстояние <= grab_radius
3. Проверить held_objects.size() < max_held
4. attachment_system.attach(obj, agent, link, offset)
5. Обновить held_objects в SharedState
6. Результат: "ok" или "too_far" / "hands_full" / "not_found"

handle_command("release", {object_id: "box_1"}):
1. Проверить что объект held
2. attachment_system.detach(obj)
3. Обновить held_objects
4. Результат: "ok" или "not_held"

Параметры: grab_radius, max_held, attach_link.

### 4. DoorOpenerModule (Interaction)

```
s2_plugins/interactions/door_opener.hpp / .cpp
```

handle_command("open_door", {door_id: "door_1"}):
1. Найти дверь-актор в мире
2. Найти trigger zone двери (по конвенции: zone id = "door_1_trigger")
3. Проверить что агент в trigger zone (World Query: zones_containing)
4. Publish DoorCommand{door_id, OPEN} в Event Bus
5. Подписаться на ActorStateChanged для этой двери
6. Когда дверь откроется: результат "ok"
7. Таймаут: результат "timeout"

handle_command("close_door", {door_id: "door_1"}):
- Аналогично с CLOSE

**[СПРОСИТЬ]** Async результат (ждём пока дверь откроется): как реализовать? Варианты:
- Простой: проверяем состояние двери каждый tick, возвращаем результат когда дверь открылась
- Callback через Event Bus
Пока — простой (polling state каждый tick).

### 5. Обновить Agent

```cpp
struct Agent {
    // ... existing ...
    std::vector<std::unique_ptr<InteractionModule>> interactions;
};
```

### 6. Обновить tick() — фаза 3m

```cpp
// Фаза 3m: Interactions
for (auto& interaction : agent.interactions) {
    interaction->on_tick(agent, world_, bus_, dt_);
}
```

### 7. Обновить tick() — фаза 4

```cpp
// Фаза 4: Attachments
attachment_system_.update(world_.agents(), world_.props());
```

### 8. Trigger zones для дверей

Добавить в сцену trigger zone рядом с каждой дверью:

```yaml
zones:
  - id: "door_1_trigger"
    shape:
      type: "sphere"
      center: {x: 5.0, y: 0.0, z: 0.0}
      radius: 2.0
    effects: []   # пустой — только триггер
```

## Тесты

```
s2_core/tests/test_attachment.cpp
s2_core/tests/test_grabber.cpp
s2_core/tests/test_door_opener.cpp
```

### test_attachment.cpp
- attach → object follows agent
- detach → object stays
- transfer → object moves from A to B
- objects_held_by returns correct list
- update() пересчитывает позы

### test_grabber.cpp
- grab ближнего объекта → ok, held_objects=[obj]
- grab далёкого объекта → "too_far"
- grab при полных руках → "hands_full"
- release → ok, held_objects=[]
- release не-held → "not_held"
- Объект следует за агентом после grab

### test_door_opener.cpp
- Агент в trigger zone + open_door → дверь открывается
- Агент вне trigger zone + open_door → "not_in_range"
- Дверь уже открыта + open_door → результат зависит от FSM
- close_door → дверь закрывается

## Критерии приёмки

- Grabber подбирает/отпускает props
- Attachment: объект следует за агентом
- DoorOpener открывает дверь через Event Bus
- Всё работает вместе: агент едет к двери → открывает → проезжает → подбирает ящик
