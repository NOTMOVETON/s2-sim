---
phase: 00-core-architecture-foundation
reviewed: 2026-04-26T10:00:00Z
depth: standard
files_reviewed: 2
files_reviewed_list:
  - workspace/s2_core/src/zone_system.cpp
  - workspace/s2_core/tests/test_zone_system.cpp
findings:
  critical: 0
  warning: 2
  info: 3
  total: 5
status: issues_found
---

# Phase 00: Code Review Report (00-06 — ZoneEntered / ZoneExited)

**Reviewed:** 2026-04-26T10:00:00Z
**Depth:** standard
**Files Reviewed:** 2
**Status:** issues_found

## Summary

Ревью охватывает изменения фазы 00-06: публикацию `event::ZoneEntered` и `event::ZoneExited`
в `ZoneSystem::on_agent_enter()` / `on_agent_exit()`, а также новые тесты для этих событий.

Критических проблем не найдено. Публикация новых событий реализована корректно — типы
совпадают с объявлениями в `event_bus.hpp`, поля заполняются правильно. Обнаружено
два предупреждения: `sim_time` и `dt` не передаются в `on_agent_exit` (плагины получат
нулевой контекст), а тесты не проверяют, что события **не** публикуются при уже
активных подписчиках на `ZoneEntered`/`ZoneExited` при disabled-зоне.

---

## Warnings

### WR-01: `on_agent_exit` не передаёт `sim_time` и `dt` плагинам

**File:** `workspace/s2_core/src/zone_system.cpp:224-243`

**Issue:** Метод `on_agent_exit(Agent&, Zone&, SimBus&)` не принимает `sim_time` и `dt`.
В результате `EffectContext`, передаваемый плагинам через `desc.plugin->on_agent_exit()`,
всегда содержит `sim_time = 0.0` и `dt = 0.0` (значения по умолчанию из `EffectContext`).
Если CONTINUOUS-плагин использует `ctx.sim_time` для логирования, анимации или
time-based сброса при выходе агента — он получит некорректные данные.
Контраст: `on_agent_enter` принимает `sim_time` и `dt` и правильно заполняет контекст.

**Fix:** Добавить параметры `sim_time` и `dt` в сигнатуру и пробросить их в контекст:

```cpp
// zone_system.hpp
void on_agent_exit(Agent& agent, Zone& zone, SimBus& bus,
                   double sim_time, double dt);

// zone_system.cpp
void ZoneSystem::on_agent_exit(Agent& agent, Zone& zone, SimBus& bus,
                                double sim_time, double dt)
{
    bus.publish(event::AgentExitedZone{.agent = agent.id, .zone = zone.id});
    bus.publish(event::ZoneExited{.zone_id = zone.id, .entity_id = agent.id});

    for (auto& desc : zone.effects) {
        if (!desc.enabled || !desc.plugin) continue;
        if (!capabilities_match(agent, desc.required_capabilities)) continue;

        EffectContext ctx;
        ctx.sim_time       = sim_time;   // <-- добавить
        ctx.dt             = dt;         // <-- добавить
        ctx.zone_id        = zone.id;
        // ...
    }
}
```

Вызывающий код в `tick()` (строки 71 и 89) передаёт `sim_time` и `dt` как параметры,
поэтому они доступны на месте вызова.

---

### WR-02: Тест `DisabledZone_NoEnterEvent` не проверяет новые события `ZoneEntered`/`ZoneExited`

**File:** `workspace/s2_core/tests/test_zone_system.cpp:237-255`

**Issue:** Тест 8 подписывается только на `event::AgentEnteredZone`, но не проверяет
`event::ZoneEntered`. После добавления публикации `ZoneEntered` при входе агента — тест
не гарантирует, что disabled-зона не публикует и новый event-тип. Аналогично для exit:
при `toggle_zone(false)` во время tick система вызывает `on_agent_exit`, который теперь
публикует и `event::ZoneExited`, но этот путь кода нигде в тестах не покрыт на предмет
корректности новых событий.

**Fix:** Расширить тест 8 для проверки `ZoneEntered`:

```cpp
int zone_entered_count = 0;
bus.subscribe<event::ZoneEntered>([&](const event::ZoneEntered&) { ++zone_entered_count; });
// ... tick ...
EXPECT_EQ(enter_count,        0);
EXPECT_EQ(zone_entered_count, 0);  // <-- новая проверка
```

Добавить отдельный тест для случая `toggle_zone(false)` при наличии агентов внутри:
проверить, что `ZoneExited` публикуется для каждого агента из `inside_agents`.

---

## Info

### IN-01: Sentinel-значение `0` для `EntityId` совпадает с валидным AgentId

**File:** `workspace/s2_core/tests/test_zone_system.cpp:393, 425`

**Issue:** `EntityId received_entity = 0` используется как sentinel «событие не получено».
`AgentId` — это `uint32_t`, и значение `0` является валидным идентификатором (Тест 7
использует `AgentId{0}`). Если бы тест случайно использовал агента с id=0, условие
`EXPECT_EQ(received_entity, 7u)` завершалось бы неверным диагнозом ошибки.
В текущих тестах агент имеет id=7 — проблемы нет, но паттерн ненадёжен.

**Fix:** Использовать явно невалидное значение как sentinel (например, `UINT32_MAX`),
или добавить отдельный `bool event_received = false`:

```cpp
bool event_received = false;
ZoneId received_zone;
EntityId received_entity = std::numeric_limits<EntityId>::max();

bus.subscribe<event::ZoneEntered>([&](const event::ZoneEntered& e) {
    event_received  = true;
    received_zone   = e.zone_id;
    received_entity = e.entity_id;
});
// ...
EXPECT_TRUE(event_received);
```

---

### IN-02: Комментарий `// Новый event` — временная пометка

**File:** `workspace/s2_core/src/zone_system.cpp:203, 227`

**Issue:** Комментарии `// Новый event — entity-уровень (для Phase 1+ подписчиков)`
описывают временный статус изменения, а не назначение кода. После интеграции эти
комментарии теряют смысл и становятся шумом.

**Fix:** Заменить на описание назначения:

```cpp
// entity-level event для подписчиков Phase 1+ (charging, transport и др.)
bus.publish(event::ZoneEntered{.zone_id = zone.id, .entity_id = agent.id});
```

---

### IN-03: Тесты `ZoneSystem_ZoneEnteredEvent` и `ZoneSystem_ZoneExitedEvent` используют `// ─── Тест N` без нумерации

**File:** `workspace/s2_core/tests/test_zone_system.cpp:384, 409`

**Issue:** Заголовки тестов содержат `Тест N` и `Тест N+1` вместо реальных номеров.
В файле уже используются номера 1–11. Это затрудняет навигацию и нарушает единый стиль.

**Fix:** Заменить на `Тест 12` и `Тест 13` соответственно.

---

_Reviewed: 2026-04-26T10:00:00Z_
_Reviewer: Claude (gsd-code-reviewer)_
_Depth: standard_
