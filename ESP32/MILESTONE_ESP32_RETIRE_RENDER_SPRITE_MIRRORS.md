# Milestone — Render legacy custom/drop sprite mirrors retired (100 B)

Status: **REAL-CYD PASS**, 2026-10-08

Hardware-tested production code SHA: `1a2bdf3edb6cfbd399a30d509e3718346ced0e57`

Branch: `agent/esp32-render-mapflags-retirement`

Main starting SHA: `02baa3bddd3b52c2399e33725aa4147a3840ad6e`

## Scope / permanent boundary

In normal `esp32-cyd`, remove from `Render_t` the legacy
`customSprites[16]`, `dropSprites[8]` pointer arrays and
`firstDropSprite` counter: 16*4 + 8*4 + 4 = **100 bytes**.
Only the already production-rejected `Render_beginLoadMapData`
legacy BSP loader populated these mirrors. The native world sprite
topology, drops and renderer remain authoritative and unchanged.

Preserve the fields in the desktop and explicit
`DOOMRPG_ESP32_BRINGUP_PROBES` layouts. The ESP32 generator
asserts the precise three historical source accesses, all within
the rejected legacy map loader. Production startup bridge asserts
`sizeof(Render_t)==1576`.

This is the fourth distinct Render ownership cut:
`5040 -> 4016 -> 1968 -> 1676 -> 1576` bytes, cumulatively
**3464 bytes** reclaimed in the compatibility object.

## CI

GitHub Actions `esp32-cyd` run **#1694** at the exact hardware
code SHA: **SUCCESS**; static RAM 45056 B, flash 772109 B.

## Actual classic CYD tests

| Checkpoint | Previous heap8 | This heap8 | Improvement |
| --- | ---: | ---: | ---: |
| CORE READY | 184972 | 185072 | +100 |
| LAYOUT | 178364 | 178464 | +100 |
| Mappings | 159204 | 159304 | +100 |
| Resident gameplay | 118152 | 118252 | +100 |
| Back to main menu | 164048 | 164148 | +100 |

Full tested route: boot, MENU_MAIN, fresh START, full fitted intro
and disposal, native Entrance map load, first world frame,
movements/turning, crate hit and Armor Shard pickups, HUB->SYS,
**checkpoint v11 LOAD during active session**, native resident
teardown, exact checkpoint reconstruction, movement with 12
restored activated monsters (turn sequence correctly fail-closed:
`activeCount=12 delivered=0`), HUB->SYS->double-confirm EXIT,
full teardown and return to main menu.

```text
Engine structs: Render=1576 Game=4 Canvas=44 Total=2384 bytes
[CORE] READY objects=5 heap used=2564 remaining=185072 largest=110580
[INTRO1] READY ... FNV=ade0195d
[MAPRT] READY arenaBytes=14095 ... arenaFNV=c3882516
[ENGINESESSION] FIRST_FRAME map=1 angle=64 frame=71ca7465 walls=8 pixels=4430 presented=1
[NATIVESAVE] LOAD ... version=11 bytes=5604 ... runtimeFNV=c3882516 ... monsterDrops=restored/0/8d1747e5/serial1/next1 ... session=reprime-pending
[MONSTERSTATE] RESTORE ... stateFNV=22a9b038 rngCalls=0 source=checkpoint-v8
[MONSTERPOS] RESTORE ... stateFNV=149e39dd ... topologyExact=yes
[ENGINESESSION] RESUME-VISIBLE map=1 ... finalWorldPresent=yes
[RESIDENTRESET] heap8=146140->164148 released=18008 ... after=0/0/0/0/0/0/0 empty=1
[SYSEXIT] MENU-READY frame=522dc605 session=off resident=empty saveWrite=no checkpoint=unchanged
```

The existing `[NATIVEFRAME] LEGACY_GUARD -> RETRY -> RECOVERED`
path recovered on both fresh and resumed worlds. Normal
firmware maintained `shapeData==NULL` and `mediaTexels==NULL`.
No new checkpoint write occurred. This test is a structural and
workflow regression test, not proof of exhaustive monster AI.

No source changes after hardware-tested SHA belong to this
closure; only docs-only commits may follow. Any fifth code cut
requires separate CI and independent hardware validation.
