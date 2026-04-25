# Phase 1: Zone Visual & Control Layer - Discussion Log

> **Audit trail only.** Do not use as input to planning, research, or execution agents.
> Decisions captured in CONTEXT.md — this log preserves the alternatives considered.

**Date:** 2026-04-26
**Phase:** 01-zone-visual-control-layer
**Areas discussed:** UI Zone Inspector, VisualHint анимация, Spawn triggers полнота, owned_zones глубина

---

## UI Zone Inspector (ZONE-01)

| Option | Description | Selected |
|--------|-------------|----------|
| Dropdown со статикой | Hardcoded список типов эффектов; в Phase 5 заменяется на dynamic registry | ✓ |
| Text input | Свободный ввод строки, нет валидации | |

**Выбор типа эффекта:** Dropdown со статикой.

| Option | Description | Selected |
|--------|-------------|----------|
| Минимальный CRUD | Форма, позиция, размер, цвет, список эффектов без параметров | ✓ |
| Полный инспектор | Форма + YAML-параметры каждого эффекта | |
| Список + кнопка выбора | Visual список всех зон сцены | |

**Объём формы:** Минимальный CRUD.

**Расположение:** Freeform ответ — "есть кнопка edit scene, давай сделаем все там".
Инспектор зон — раздел "Zones" внутри существующей панели Edit Scene.

| Option | Description | Selected |
|--------|-------------|----------|
| Кнопка Delete в панели | В форме редактирования, без подтверждения | ✓ |
| Список + иконка корзины | Delete рядом с каждой зоной в списке | |

**Удаление:** Кнопка Delete в панели.

---

## VisualHint анимация (ZONE-02)

| Option | Description | Selected |
|--------|-------------|----------|
| Пульс границы зоны | Flash прозрачности при ZoneEntered | |
| Окрашивание агента | AgentSnapshot + inside_zones тег | |
| Цвет/прозрачность достаточно | ZoneSnapshot.color/opacity уже есть | ✓ |

**Анимация активации:** Цвет/прозрачность достаточно.

| Option | Description | Selected |
|--------|-------------|----------|
| glow и arrows | Для charging/fog + conveyor/wind | |
| Только цвет/прозрачность | VisualHint не рендерится в Phase 1 | |
| Все 4 типа | glow, arrows, particles, grid все в Phase 1 | ✓ |

**VisualHint типы:** Все 4 (glow, arrows, particles, grid).

---

## Spawn Triggers полнота (ZONE-08)

| Option | Description | Selected |
|--------|-------------|----------|
| Все 4 | command, event, timer, state_change | ✓ |
| command + event | timer и state_change в Phase 2+ | |

**Количество триггеров:** Все 4 в Phase 1.

**Уточнение state_change:** Freeform ответ пользователя — описал сценарий
"плагин агента посылает ивент → зона спавнится". Из RESULT_DISCUSS.md §9.6
уточнено: `event` = любое EventBus событие от плагина; `state_change` = конкретно
ActorStateChanged (FSM актора). В Phase 1 state_change реализуется как механизм
через EventBus.ActorStateChanged, реальные сценарии появятся в Phase 2.

---

## owned_zones глубина (ZONE-09)

| Option | Description | Selected |
|--------|-------------|----------|
| Entity-level достаточно | Зона следует за центром Entity | |
| Per-link сразу | attached_to_link: <link_name> в Phase 1 | ✓ |

**Per-link attachment:** Реализуется в Phase 1.

| Option | Description | Selected |
|--------|-------------|----------|
| И YAML и runtime | SceneLoader + SpawnZone{attached_to} KernelCommand | ✓ |
| Только runtime | Без YAML объявления | |

**Хранение owned_zones:** И в YAML сцены, и через KernelCommand в рантайме.

---

## Claude's Discretion

- Структура ZoneSpawnSystem (отдельный класс или метод SimEngine)
- YAML синтаксис spawn_trigger секции
- Three.js параметры 4 типов VisualHint анимации
- Порядок итерации kinematic_tree при per_link detection

## Deferred Ideas

- Параметры эффектов в UI (config_schema) — Phase 5
- Actor FSM реальные сценарии state_change — Phase 2
- VisualHint кастомные JS-модули Level 2 — Phase 8
