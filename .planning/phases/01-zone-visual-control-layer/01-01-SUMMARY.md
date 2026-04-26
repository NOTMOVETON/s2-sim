---
phase: 01-zone-visual-control-layer
plan: 01
subsystem: s2_core
tags: [zone, lifecycle, detection-mode, self-destruct, data-types, tdd]
dependency_graph:
  requires: []
  provides:
    - "Zone::strength, Zone::lifecycle, Zone::detection_mode_enum, Zone::self_destruct"
    - "EffectContext::zone_strength, EffectContext::contact_link"
    - "ZoneSnapshot::strength, ZoneSnapshot::visual_hints"
  affects:
    - "workspace/s2_core/include/s2/zone.hpp"
    - "workspace/s2_core/include/s2/effect_context.hpp"
    - "workspace/s2_core/include/s2/world_snapshot.hpp"
    - "workspace/s2_core/src/world_snapshot.cpp"
tech_stack:
  added: []
  patterns:
    - "TDD — RED (тесты написаны до реализации), GREEN (реализация), тесты компилируются и проходят"
key_files:
  created:
    - workspace/s2_core/tests/test_zone_lifecycle.cpp
    - workspace/s2_core/tests/test_zone_detection_mode.cpp
    - workspace/s2_core/tests/test_zone_self_destruct.cpp
  modified:
    - workspace/s2_core/include/s2/zone.hpp
    - workspace/s2_core/include/s2/effect_context.hpp
    - workspace/s2_core/include/s2/world_snapshot.hpp
    - workspace/s2_core/src/world_snapshot.cpp
    - workspace/s2_core/CMakeLists.txt
decisions:
  - "Старое поле Zone::detection_mode (string) оставлено с пометкой @deprecated для backward compat"
  - "ZoneSnapshot::Hint вложена в ZoneSnapshot (не глобальная struct) — не загрязняет namespace s2"
  - "zone_snapshot_to_json сериализует strength и visual_hints как ключи верхнего уровня в JSON объекте зоны"
metrics:
  duration: "358 секунд (~6 мин)"
  completed_date: "2026-04-26"
  tasks_completed: 2
  tasks_total: 2
  files_modified: 5
  files_created: 3
---

# Phase 01 Plan 01: Zone Data Types Foundation Summary

**One-liner:** Аддитивное расширение Zone/EffectContext/ZoneSnapshot новыми полями lifecycle, DetectionMode enum и SelfDestructPolicy с TDD-тестами (8 тестов) — все 406 тестов проходят.

## Tasks Completed

| Task | Name | Commit | Files |
|------|------|--------|-------|
| 1 | Расширить zone.hpp + EffectContext | 08dbf39 | zone.hpp, effect_context.hpp, CMakeLists.txt, 3 test файла |
| 2 | Расширить ZoneSnapshot | 7e245ff | world_snapshot.hpp, world_snapshot.cpp |

## What Was Built

**Задача 1 — zone.hpp + effect_context.hpp:**

Добавлены три новых типа данных в zone.hpp:
- `enum class DetectionMode { CENTER, BOUNDING, PER_LINK }` — режим обнаружения агентов (per D-12)
- `struct SelfDestructPolicy { enum class Type { NONE, ON_ANY_CONTACT, ON_EFFECT_APPLIED }; }` — политика самоуничтожения (per D-13)
- `struct ZoneLifecycle { initial_strength, growth_rate, max_strength, decay_delay, decay_rate, remove_threshold }` — параметры lifecycle (per D-09/D-10)

В `struct Zone` добавлены поля:
- `double strength{1.0}` — текущая сила зоны
- `ZoneLifecycle lifecycle` — параметры роста/затухания
- `DetectionMode detection_mode_enum{DetectionMode::CENTER}` — режим детекции
- `SelfDestructPolicy self_destruct` — политика удаления
- `std::string attached_to_entity_id` — generic entity attachment
- `std::optional<std::string> attached_to_link` — per-link attachment

В `struct EffectContext` добавлены поля:
- `double zone_strength{1.0}` — заполняется из zone.strength при создании контекста
- `std::string contact_link` — имя линка при PER_LINK detection

**Задача 2 — world_snapshot.hpp + world_snapshot.cpp:**

В `struct ZoneSnapshot` добавлены:
- `double strength{1.0}` — текущая сила для передачи визуализатору
- вложенная `struct Hint { std::string type; nlohmann::json params; }` — описание VisualHint
- `std::vector<Hint> visual_hints` — список подсказок для Three.js рендеринга

Функция `zone_snapshot_to_json` обновлена: теперь сериализует `strength` и `visual_hints` в JSON объект зоны.

## Test Results

- 3 новых тестовых файла: 8 тестов (ZoneLifecycle×3, ZoneDetectionMode×2, ZoneSelfDestruct×3)
- Все 8 новых тестов проходят
- Все 406 тестов s2_core_tests проходят (включая все legacy zone_system тесты)

## Deviations from Plan

None — план выполнен точно как написан.

## Known Stubs

None. Поля strength, visual_hints и zone_strength — полноценные данные-члены с правильными дефолтами. Заполнение zone_strength реальным значением из zone.strength (в ZoneSystem) — задача Plan 03 (lifecycle).

## Threat Flags

Нет новых доверительных границ. Zone::strength — внутреннее состояние симуляции (не принимается из REST в этом плане, T-01-01 accepted). Zone::attached_to_entity_id будет проверяться в Plan 04 (T-01-02 mitigate — запланировано).

## Self-Check

- workspace/s2_core/include/s2/zone.hpp содержит `enum class DetectionMode` — FOUND
- workspace/s2_core/include/s2/zone.hpp содержит `double strength{1.0}` — FOUND
- workspace/s2_core/include/s2/zone.hpp содержит `ZoneLifecycle lifecycle` — FOUND
- workspace/s2_core/include/s2/zone.hpp содержит `SelfDestructPolicy self_destruct` — FOUND
- workspace/s2_core/include/s2/zone.hpp содержит `std::optional<std::string> attached_to_link` — FOUND
- workspace/s2_core/include/s2/effect_context.hpp содержит `double zone_strength{1.0}` — FOUND
- workspace/s2_core/include/s2/effect_context.hpp содержит `std::string contact_link` — FOUND
- workspace/s2_core/include/s2/world_snapshot.hpp содержит `double strength{1.0}` в ZoneSnapshot — FOUND
- workspace/s2_core/include/s2/world_snapshot.hpp содержит `std::vector<Hint> visual_hints` — FOUND
- Коммит 08dbf39 — FOUND
- Коммит 7e245ff — FOUND

## Self-Check: PASSED
