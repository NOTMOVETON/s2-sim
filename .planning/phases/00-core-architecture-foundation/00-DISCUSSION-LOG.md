# Phase 0: Core Architecture Foundation - Discussion Log

> **Audit trail only.** Do not use as input to planning, research, or execution agents.
> Decisions captured in CONTEXT.md — this log preserves the alternatives considered.

**Date:** 2026-04-25
**Phase:** 00-core-architecture-foundation
**Areas discussed:** WorldQuery → плагины, KernelCommand хранение, SimBus → EventBus rename, Роли плагинов (ARCH-02)

---

## WorldQuery → плагины

| Option | Description | Selected |
|--------|-------------|----------|
| PluginContext в update() | update(dt, agent, ctx) где PluginContext{WorldQuery, EventBus, KernelCommandQueue}. Один параметр, расширяемо. | ✓ |
| set_world_query() при init | on_spawn передаёт WorldQuery*, старая сигнатура update() | |
| Глобальный WorldQuery | SimEngine::world_query() статически | |

**User's choice:** PluginContext в update()

**Дополнительный вопрос: pre_resolve() тоже получает ctx?**

| Option | Selected |
|--------|----------|
| pre_resolve без ctx (Resource-плагинам WorldQuery не нужен) | ✓ |
| pre_resolve тоже с ctx | |

---

## KernelCommand хранение

| Option | Description | Selected |
|--------|-------------|----------|
| std::variant очередь | using KernelCommand = std::variant<...>. Инспектируемо, сериализуемо. | ✓ |
| Lambda-очередь | std::function<void(SimEngine&)> — гибко, но не инспектируемо | |

**Дополнительный вопрос: REST API тоже через очередь?**

| Option | Selected |
|--------|----------|
| Да, единая точка входа (mutex-protected) | ✓ |
| Нет, REST остаётся прямым | |

---

## SimBus → EventBus rename

| Option | Description | Selected |
|--------|-------------|----------|
| Переименовать SimBus → EventBus | sim_bus.hpp → event_bus.hpp, class EventBus. Все вхождения обновить. | ✓ |
| using EventBus = SimBus алиас | Обратная совместимость, но два имени | |

**Дополнительный вопрос: куда поместить новые event:: типы?**

| Option | Selected |
|--------|----------|
| Все в event_bus.hpp | ✓ |
| Отдельные файлы по группам | |

---

## Роли плагинов (ARCH-02)

**Уточняющий вопрос пользователя:** "у агента же може быть больше одного плагина управления? например для джоинтов и для скоростей?"

Выяснено: DiffDrive пишет в agent.velocity (поза тела), JointVelPlugin пишет в KinematicTree (суставы) — разные цели, не конфликтуют.

| Option | Description | Selected |
|--------|-------------|----------|
| JointVel = UTILITY | ACTUATION = только agent.velocity. JointVel → UTILITY. Один DiffDrive + много JointVel = valid. | ✓ |
| JointVel = ACTUATION | Запрет DiffDrive + JointVel вместе | |

**Валидация правила «один ACTUATION»:**

| Option | Selected |
|--------|----------|
| throw при load/AddPlugin | ✓ |
| std::cerr предупреждение без остановки | |

---

## Claude's Discretion

- Конкретная реализация KernelCommandQueue (std::vector + mutex vs lockless)
- Формат config_schema()
- Порядок migrate-коммитов

## Deferred Ideas

- Signals на Actor/Prop — Phase 2
- Entity base model — Phase 6
- provided_capabilities() auto-add в initialize_entity() — Phase 6
