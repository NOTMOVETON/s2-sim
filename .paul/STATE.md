# Project State

## Project Reference

See: .paul/PROJECT.md (updated 2026-05-02)

**Core value:** Разработчик роботов тестирует верхнеуровневую логику стека без физического симулятора, дописывая плагины без изменений ядра.
**Current focus:** Milestone v1.0 — Architecture Migration

## Current Position

Milestone: v1.0 Architecture Migration
Phase: 4 of 13 (REST API + IVizAdapter) — Not started
Plan: Not started
Status: Ready to plan
Last activity: 2026-05-02 — Phase 3 complete, transitioned to Phase 4

Progress:
- Milestone v1.0: [███░░░░░░░] 23% (3/13 phases)
- Phase 4: [░░░░░░░░░░] 0%
- Milestone v2.0: [░░░░░░░░░░] 0% (0/14 phases)

## Loop Position

```
PLAN ──▶ APPLY ──▶ UNIFY
  ✓        ✓        ✓     [Loop complete — ready for next PLAN]
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
| PHASE 0 до if(paused_) guard | SetPose/ResumeSim работают даже когда sim на паузе | Active |
| cmd-типы в namespace s2::cmd | cmd::SetPose{} вместо длинного квалификатора | Active |
| drain() swap под мьютексом | Мьютекс не держится во время обработки команд | Active |

### Deferred Issues

Пока нет.

### Blockers/Concerns

- `agent.domain_id` (legacy поле) остаётся в struct до Phase 8

## Session Continuity

Last session: 2026-05-02
Stopped at: Phase 3 complete, loop closed, transitioned to Phase 4
Next action: /paul:plan для Phase 4 (REST API + IVizAdapter)
Resume file: .paul/ROADMAP.md

---
*STATE.md — Updated after every significant action*
