# Milestone — DIALOGCHAIN journal session ownership

Status: **REAL-CYD HARDWARE PASS — DOCS-ONLY CLOSURE**

Base main `91e1d8412c98fc10ff7b6a2cff6c97323496ecaf` (PR #199).
Branch `agent/esp32-dialogchain-owner-lifecycle`. Hardware-tested code SHA `2aa4ebcb8b932db4f5b803db920fbaae612a088d`.
Normal production `esp32-cyd` CI #1709 **SUCCESS**:
static RAM 45056 B; flash 772369 B.

## Permanent ownership contract

`ChainTransaction` is lazy gameplay-session-owned state, not one journal
per dialog. `EspNativeGameplayEventChain_reset()` frees any optional
`topologyBytes` snapshot before the journal, nulls its owner and is
idempotent. `EspNativeGameplaySession_reset()` calls it only after the
resident gameplay consumer resets. EXIT / LOAD / CHANGEMAP call the
existing session reset boundary. The legacy script/event and committed
world behavior are unchanged; the normal first frame uses no extra buffer.

## Physical CYD witness (2026-10-08)

The normal firmware, not bringup, was flashed. User executed fresh MAP_INTRO,
two moves, one rotation, native SELECT event 88, complete dialogue and
resume with opcode 19 (`state=1 mutation=1 topologySnapshot=0B`), then
HUB -> SYSTEM -> EXIT double confirmation.

```text
[DIALOGCHAIN] OWNER bytes=1020 allocation=lazy-gameplay
[DIALOGCHAIN] RESUME event=88 start=1 handled=1 ... state=1 ... mutation=1 topologySnapshot=0B automapSnapshot=no
[DIALOGCHAIN] OWNER-RELEASE journal=1020 topologyCapacity=0 activeAtTeardown=1 heap8=118356->119392 recovered=1036 owner=none
[RESIDENTRESET] heap8=146176->164184 released=18008 before=1/1/1/1/1/1/1 after=0/0/0/0/0/0/0 empty=1
[MAINMENU] ... heap8=164184->164184 ... largest8=110580->110580 ... shapeData=0x0 mediaTexels=0x0
[SYSEXIT] MENU-READY frame=522dc605 session=off resident=empty saveWrite=no checkpoint=unchanged
```

Exact **1036 B** recovery matches the journal's measured heap8 footprint.
No snapshot allocation was involved on this path. This is an actual
release, not a new allocation or a synthetic counter. The session and
resident-map owners are empty and menu framebuffer fingerprint unchanged.
`activeAtTeardown=1` records that a successful continuation kept the
rollback journal active until end-of-session; the call is at the intended
teardown boundary rather than an assertion that rollback happened.

## Explicit limits and merge boundary

Cold-exit without opening a dialogue, second-session reacquisition,
active-session LOAD, CHANGEMAP and topology-snapshot release have not been
physically exercised on this code SHA. Do not infer broader hardware PASS.
The **event-88 -> SYS EXIT** acceptance path has passed, CI is green,
and closure after code SHA `2aa4ebcb8b932db4f5b803db920fbaae612a088d` is documentation-only. Ready for
user review/merge; never merge main automatically.
