# Milestone — Retire legacy Render BSP view-node sentinel (44 B)

Status: **REAL-CYD PASS** — 2026-10-08

Code SHA validated on classic CYD:
`27b253cd7de0c29ea7095971296cf675dd91165d`

Branch: `agent/esp32-render-mapflags-retirement`

Base `main` SHA:
`02baa3bddd3b52c2399e33725aa4147a3840ad6e`

## Scope

Fifth bounded `Render_t` owner cut removes the 44-byte
`viewNodes` linked-list sentinel from production `esp32-cyd`.
The original `Render_renderBSP` / `Render_walkNode` traversals
are fail-closed in normal production. The former MENUWALL
diagnostic that reads this sentinel is restricted to bringup;
desktop and explicit `DOOMRPG_ESP32_BRINGUP_PROBES` retain
the original structure and algorithms. The generator
enforces the exact six historical `render->viewNodes`
consumers, all within the retired BSP traversal.
Native geometry, rendering, immutable BSP runtime,
map state and sprite topology are unchanged.

Production `sizeof(Render_t)==1532` is asserted at build time.
Five cuts: `5040 -> 4016 -> 1968 -> 1676 -> 1576 -> 1532 B`,
**3508 B** removed from the legacy Render object.
The active `sinTable[256]` must NOT be retired casually:
`esp_native_first_frame.c` still consumes it.

## CI

GitHub Actions normal `esp32-cyd` **#1701 SUCCESS**
at the exact code SHA. Static RAM 45056 B, flash 772161 B.

## Real classic CYD validation

| Stage | Prior heap8 | New heap8 | Measured change |
| --- | ---: | ---: | ---: |
| CORE READY | 185072 | 185116 | +44 B |
| LAYOUT READY | 178464 | 178500 | +36 B |
| Mappings installed | 159304 | 159340 | +36 B |
| Fresh gameplay ALIVE, before dialog | 118252 | 118288 | +36 B |
| MENU_MAIN after dialog then EXIT | 164148 (prior, no dialog) | 163148 | not comparable without accounting for dialog journal |

44-byte object shrink is hardware-proven. The later +36 B
heap8 differences reflect 8 B of additional layout-stage
allocator cost, not an unexpected restoration of the removed
Render field.

The successful test path was unusually broad:
- boot, native Help/About with page-down/page-up/Back and exact
  MENU_MAIN repaint;
- Options -> Back and exact main-menu repaint without legacy BSP;
- START -> fitted intro -> native MAP_INTRO runtime, FNV
  `c3882516`;
- world first frame `71ca7465`, movement, rotation,
  crate transform/Armor Shard pickup;
- event 88 native dialog open/page/close, then `DIALOGCHAIN`
  continuation and state-changing opcode 19;
- HUB inventory -> SYS -> double-confirm EXIT;
- seven resident map owners reset empty, no checkpoint write,
  and MENU_MAIN frame `522dc605`.

Representative serial witnesses:

```text
[CORE] READY objects=5 heap used=2520 remaining=185116 largest=110580
[MAINHELP] READY back=native mainFNV=522dc605 touch=armed
[OPTIONBACK] FAST End framebufferFNV=522dc605 expected=522dc605 ...
[MAPRT] READY arenaBytes=14095 ... arenaFNV=c3882516
[ENGINESESSION] FIRST_FRAME map=1 angle=64 frame=71ca7465 walls=8 pixels=4430 presented=1
[ENGINESESSION] READY ... shapeData=0x0 mediaTexels=0x0
[DIALOGCHAIN] OWNER bytes=1020 allocation=lazy-gameplay
[DIALOG] CLOSE event=88 resume=1 ... packClosed=yes
[DIALOGCHAIN] RESUME event=88 start=1 handled=1 ... state=1 ... mutation=1
[RESIDENTGAMEPLAY] DIALOG-RESUME ... opcode=19 stateMutation=1 redraw=yes
[RESIDENTRESET] heap8=145140->163148 released=18008 ... after=0/0/0/0/0/0/0 empty=1
[SYSEXIT] MENU-READY frame=522dc605 session=off resident=empty saveWrite=no checkpoint=unchanged
```

## Important memory ownership caveat

The dialog continuation journal `ChainTransaction` is
`SDL_calloc`-allocated once, lazily, in
`esp_native_gameplay_event_chain.c` (`transactionOwner` is a
singleton pointer in BSS). `ensureTransactionOwner()` reuses
the existing pointer on subsequent dialogs. The current
session reset does **not** release this owner. It therefore
survives EXIT as a bounded, process-lifetime allocation.

Hardware after first dialog shows gameplay heap8
`118288 -> 117252`, a **1036 B** reduction, matching
the journal payload 1020 B plus allocator overhead 16 B.
Consequently, comparing this test's final menu heap8
`163148` with the previous no-dialog test `164148`
as if only the 44-byte cut had changed would be misleading.
The equivalent expected no-dialog baseline for this code
is `164184` (= 164148 + 36); minus 1036 B journal
retention = observed `163148`. This does not affect the
seven resident-map owner teardown and is not evidence of
per-dialog unbounded growth. Nonetheless, the intended
lifetime/teardown of the journal should be audited and
owned explicitly in a future dedicated milestone, rather
than silently folding a behavior change into this
hardware-tested Render cut.

The known `LEGACY_GUARD` was not exercised in this last
trace; earlier cuts had validated recovery. Checkpoint
LOAD and exhaustive AI were tested only on earlier
SHA(s), **not** on this exact fifth-cut code SHA.

Freeze this code SHA after REAL-CYD PASS.
All closure commits must be documentation-only.
Do not merge into `main` automatically.
