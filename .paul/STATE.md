# Project State

## Project Reference

See: .paul/PROJECT.md (updated 2026-04-27)

**Core value:** Разработчик роботов тестирует верхнеуровневую логику стека без физического симулятора, дописывая плагины без изменений ядра.
**Current focus:** Milestone v1.0 — Architecture Migration

## Current Position

Milestone: v1.0 Architecture Migration
Phase: 2 of 13 (Unified Entity Model) — Not started
Plan: None
Status: Ready to plan Phase 2
Last activity: 2026-04-28 — Phase 1 complete, transitioned to Phase 2

Progress:
- Milestone v1.0: [█░░░░░░░░░] 8% (1/13 phases)
- Milestone v2.0: [░░░░░░░░░░] 0% (0/14 phases)

## Loop Position

```
PLAN ──▶ APPLY ──▶ UNIFY
  ✓        ✓        ✓     [Phase 1 complete — ready for next PLAN]
```

## Accumulated Context

### Decisions

| Decision | Detail | Status |
|----------|--------|--------|
| Entity model Variant A | Полное объединение Agent/Actor/Prop в один Entity | Active |
| KernelCommands сразу | Thread-safety, hot reload с шага 3 | Active |
| per-agent transport сразу | TransportPool + HttpAdapter в Milestone 1 | Active |
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

Пока нет.

## Session Continuity

Last session: 2026-04-27
Stopped at: Plan 01-01 создан
Next action: /paul:plan для Phase 2 (Unified Entity Model)
Resume file: .paul/ROADMAP.md

---
*STATE.md — Updated after every significant action*
