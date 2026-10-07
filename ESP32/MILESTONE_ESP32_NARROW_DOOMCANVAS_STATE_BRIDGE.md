# Milestone — Narrow DoomCanvas state bridge

Status: **REAL-CYD PASS**

Hardware-tested code boundary:
`63a684d9c30bb20a9962a842c3551dc88aaa64c0`

Branch:
`agent/esp32-doomcanvas-bridge`

## Goal

Remove inherited DoomCanvas state-machine ownership that no longer belongs to
the ESP32 compatibility object, while preserving only the state handoffs still
used by the native firmware.

## Permanent state boundary

Accepted bridge states:

```text
ST_MENU
ST_PLAYING
ST_INTRO
```

Everything else fails closed with `STATE-REJECT`. Native owners already handle
dialogs, combat, death, automap, loading, credits, epilogue and related
presentation/state.

## Retired layout and ABI

```text
obsolete field payload 113 B
ESP32 layout reclaimed 116 B
DoomCanvas_t           512 B -> 396 B
total reclaimed        3344 B / 3740 B (~89.4%)
ABI exports            14 -> 11
```

Removed source ABI exports:

```text
DoomCanvas_initCredits
DoomCanvas_loadEpilogueText
DoomCanvas_renderScene
```

## CI

Normal `esp32-cyd` CI #1650: **SUCCESS**.

```text
RAM:   45432 B
Flash: 772949 B
esp32-cyd SUCCESS
```

Relative to the prior hardware-proven cut:

```text
static RAM: 45464 -> 45432  (-32 B)
Flash:     779181 -> 772949 (-6232 B)
```

## Real-CYD acceptance

Boot:

```text
[DOOMCANVASBRIDGE] INIT exports=11 desktopTU=no bytes=396 ... retiredStateLayout=116 clip=160x120
[CORE] DoomCanvas     used=412 heap=186468 largest=110580
[CORE] READY objects=5 heap used=6380 remaining=180880 largest=110580 clip=160x120
```

Stable checkpoints gain 148 bytes versus the 512-byte boundary because the
116-byte heap-object reduction and the 32-byte static-RAM reduction are both
visible to free heap.

Fresh session witnesses include:

```text
[ENGINESESSION] FIRST_FRAME map=1 angle=64 frame=71ca7465 walls=8 pixels=4430 presented=1
[DIALOG] OPEN event=79 cmd=0 resume=1 opcode=26 string=3 ...
[DIALOGCHAIN] RESUME event=79 start=1 handled=1 ... state=1 ... mutation=1
[ENGINESESSION] READY ... shapeData=0x0 mediaTexels=0x0
```

The run also validates crate actions, resource pickup, normal/deferred door
animation, HUB/System and clean Exit To Menu.

Checkpoint resume validates version 11 restore, movement/pickup continuation,
monster activation, ordered attack visualization and committed retaliation:

```text
[MONSTERRETAL] COMMIT ... playerHP=34->31 armor=15->12 ... rollback=closed
```

Options/Back then returns through the native opaque menu path without
`MenuSystem_back` or legacy menu rendering.

`shapeData` and `mediaTexels` remain NULL throughout.

The known compact-renderer guard recovery remains accepted and unrelated.

## Cosmetic note

The current INIT witness is missing one space between
`retiredDormantText=428` and `retiredStateLayout=116`. This is log-only and
does not alter the hardware-tested boundary.

## Closure

Hardware-tested code ends at
`63a684d9c30bb20a9962a842c3551dc88aaa64c0`.

This closure commit is documentation-only.
