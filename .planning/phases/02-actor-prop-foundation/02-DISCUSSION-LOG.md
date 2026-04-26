# Phase 2: Actor & Prop Foundation - Discussion Log

> **Audit trail only.** Do not use as input to planning, research, or execution agents.
> Decisions captured in CONTEXT.md — this log preserves the analysis.

**Date:** 2026-04-26
**Phase:** 02-actor-prop-foundation
**Mode:** document-extraction (decisions extracted from existing project docs)
**Areas analyzed:** IActorBehavior, DoorBehavior, SignalListenerBase, Prop structure, AttachObject/DetachObject, GrabberPlugin

---

## Source Documents

Все решения Phase 2 были предварительно задокументированы в следующих файлах:

| Document | Sections Used |
|----------|---------------|
| `RESULT_DISCUSS.md` | §4.1-4.4 (Entity model), §7.1-7.5 (IActorBehavior), §10.1-10.7 (Signals, Wire, Controllers) |
| `docs/32-actor-base-door.md` | IActorBehavior interface, Actor struct, DoorBehavior FSM, DoorOpenerPlugin |
| `docs/35-props-attachment.md` | Prop struct, Attachment commands, GrabberPlugin |

## Decision Extraction Summary

Интерактивное обсуждение не проводилось — пользователь указал что все решения уже описаны в предыдущих документах. CONTEXT.md составлен путём извлечения решений из существующей документации.

### Key Decisions Extracted

| Area | Decision | Source |
|------|----------|--------|
| Actor SharedState | Да, акторы имеют SharedState с contribution/resolver | RESULT_DISCUSS.md §4.2 |
| Behavior geom control | Императивное — behavior двигает геометрию напрямую | RESULT_DISCUSS.md §7.5 |
| ActorStateChanged | Behavior публикует явно через EventBus | docs/32 line 171 |
| Wire signal delivery | SignalListenerBase.scan_signals() + EventBus подписки | RESULT_DISCUSS.md §10.6 |
| Prop collision on grab | Отключается (skip в CollisionSystem) | docs/35 line 209 |
| GrabberPlugin | Proximity-based, INTERACTION role | docs/35 §7 |
| Controllers | Плагины роли INTERACTION на акторе | RESULT_DISCUSS.md §10.6 |

## Corrections Made

No corrections — all assumptions confirmed from existing documentation.
