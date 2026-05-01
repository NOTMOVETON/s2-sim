# Project State

## Project Reference

See: .paul/PROJECT.md (updated 2026-05-02)

**Core value:** Разработчик роботов тестирует верхнеуровневую логику стека без физического симулятора, дописывая плагины без изменений ядра.
**Current focus:** Milestone v1.0 — Architecture Migration

## Current Position

Milestone: v1.0 Architecture Migration
Phase: 3 of 13 (KernelCommands Queue) — Not started
Plan: Ready for Phase 3
Status: Phase 2 COMPLETE; Phase 3 next
Last activity: 2026-05-02 — Phase 2 unified (02-01 + 02-02, 100% tests)

Progress:
- Milestone v1.0: [██░░░░░░░░] 15% (2/13 phases)
- Phase 2: [██████████] 100% ✅ COMPLETE
- Milestone v2.0: [░░░░░░░░░░] 0% (0/14 phases)

## Loop Position

```
PLAN ──▶ APPLY ──▶ UNIFY
  ✓        ✓        ✓     [Phase 2 loop complete - ready for Phase 3 PLAN]
```

## Accumulated Context

### Decisions

| Decision | Detail | Status |
|----------|--------|--------|
| Entity model Variant A | Полное объединение Agent/Actor/Prop в один Entity | Active |
| Flat structs (не наследование) | C++20 designated initializers требуют прямых полей | Active |
| KernelCommands сразу | Thread-safety, hot reload с шага 3 | Active |
| per-agent transport сразу | TransportPool + HttpAdapter в Milestone 1 | Active |
| transport_type/domain_id в tags | Временное хранение до Phase 8 (TransportPool) | Active |
| turn_rate_scale добавлен | Угловая скорость масштабируется отдельно от линейной | Active |
| max_angular_cap добавлен | Абсолютный лимит угловой скорости (MIN merge) | Active |
| OWN_EFFECT_REMOVE action | Новый action type для ремонтных зон | Active |
| RemoveOwnEffect command | KernelCommand для удаления own_effects плагинами | Active |
| zone.strength в EffectContext | IceModifier и подобные используют силу зоны | Active |
| IVizAdapter отделён | VizServer → WebVizAdapter; REST отдельно | Active |
| ScriptedBehavior в pre_resolve | Пишет desired velocity до DiffDrive.update() | Active |
| Entity.tags["behavior"] авто | При инициализации актора — тег из behavior.type() | Active |

### Deferred Issues

Пока нет.

### Blockers/Concerns

- `agent.domain_id` (legacy поле) остаётся в struct до Phase 8

## Session Continuity

Last session: 2026-05-02
Stopped at: Phase 2 fully complete and unified (02-01 entity.hpp + SimWorld; 02-02 SceneLoader + role() + tests)
Next action: /paul:plan для Phase 3 (KernelCommands Queue)
Resume file: .paul/phases/02-unified-entity-model/02-02-SUMMARY.md

---
*STATE.md — Updated after every significant action*
