# Project State

## Project Reference

See: .paul/PROJECT.md (updated 2026-05-02)

**Core value:** Разработчик роботов тестирует верхнеуровневую логику стека без физического симулятора, дописывая плагины без изменений ядра.
**Current focus:** Milestone v1.0 — Architecture Migration

## Current Position

Milestone: v1.0 Architecture Migration
Phase: 6 of 13 (EffectPlugin Trigger×Action) — Not started
Plan: Not started
Status: Ready to plan
Last activity: 2026-05-02 — Phase 5 complete (IActorBehavior + BehaviorRegistry)

Progress:
- Milestone v1.0: [█████░░░░░] 38% (5/13 phases)
- Phase 6: [░░░░░░░░░░] 0% (not started)
- Milestone v2.0: [░░░░░░░░░░] 0% (0/14 phases)

## Loop Position

```
PLAN ──▶ APPLY ──▶ UNIFY
  ✓        ✓        ✓     [Phase 5 complete — ready for Phase 6]
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
| RestApiServer порт viz+1 | Автоматически 1938, конфигурируется через viz_config.port | Active |
| VizServer = только SSE/статика | VizCommandHandler удалён полностью | Active |
| ScriptedBehavior в pre_resolve | Пишет desired velocity до DiffDrive.update() | Active |
| Entity.tags["behavior"] авто | При инициализации актора — тег из behavior.type() | Active |
| PHASE 0 до if(paused_) guard | SetPose/ResumeSim работают даже когда sim на паузе | Active |
| cmd-типы в namespace s2::cmd | cmd::SetPose{} вместо длинного квалификатора | Active |
| drain() swap под мьютексом | Мьютекс не держится во время обработки команд | Active |
| Forward-declare Actor в actor_behavior.hpp | Разрыв цикл. зависимости entity.hpp↔actor_behavior.hpp | Active |
| WorldContext stub (sim_time+dt) | Phase 9 добавит WorldQuery*; API стабилен | Active |
| BehaviorRegistry явный параметр | Не singleton; SceneLoader::load() backward-compatible | Active |

### Deferred Issues

Пока нет.

### Blockers/Concerns

- `agent.domain_id` (legacy поле) остаётся в struct до Phase 8

## Session Continuity

Last session: 2026-05-02
Stopped at: Phase 5 complete — loop closed
Next action: /paul:plan для Phase 6 (EffectPlugin Trigger×Action)
Resume file: .paul/ROADMAP.md

---
*STATE.md — Updated after every significant action*
