# Milestone — Retire DoomCanvas object-graph mirrors

Status: **REAL-CYD PASS**

Hardware-tested code boundary:
`032c4d6eb8f1b9e3d5b4a3712499832cda96d4bc`

Branch:
`agent/esp32-doomcanvas-bridge`

## Goal

Remove inherited DoomCanvas backpointers whose current ESP32 owners already
live elsewhere and which are no longer consumed through the Canvas object.

## Retired mirrors

```text
player
game
entityDef
combat
hud
menuSystem
particleSystem
```

These seven pointers were only assigned during `DoomCanvas_startup()`. The
current compiled ESP32 closure never reads them through `DoomCanvas_t`.

The bridge keeps the two references it still consumes:

```text
doomRpg
render
```

## Layout

```text
DoomCanvas_t           396 B -> 368 B
reclaimed this step     28 B
total reclaimed       3372 B / 3740 B (~90.2%)
source ABI exports      11 -> 11
```

No replacement arena, static owner or transient allocation is introduced.

## CI / ELF

Normal `esp32-cyd` CI #1654: **SUCCESS**.

```text
RAM:   45432 B
Flash: 772925 B
esp32-cyd SUCCESS
```

ELF inspection:

```text
DoomCanvas_s byte_size = 368
DoomCanvas_* linked symbols = 11
```

## Real-CYD acceptance

Boot:

```text
Engine structs: Render=5040 Game=4 Canvas=368 Total=6172 bytes
[DOOMCANVASBRIDGE] INIT exports=11 desktopTU=no bytes=368 ... retiredGraphMirrors=28 clip=160x120
[CORE] DoomCanvas     used=384 heap=186496 largest=110580
[CORE] READY objects=5 heap used=6352 remaining=180908 largest=110580 clip=160x120
```

The expected 28-byte gain is visible exactly at deterministic startup
checkpoints relative to the preceding 396-byte hardware boundary:

```text
CORE READY   180880 -> 180908
LAYOUT       163296 -> 163324
mappings     144136 -> 144164
```

Fresh gameplay validates complete intro/disposal, MAP_INTRO load, canonical
first frame, pass turn, movement, crate action, pickups, door animation,
HUB/System and Exit To Menu.

Checkpoint resume validates save version 11 restoration, movement/pickups,
monster activation, ordered attack visualization and committed retaliation.

```text
[ENGINESESSION] FIRST_FRAME map=1 angle=64 frame=71ca7465 walls=8 pixels=4430 presented=1
[ENGINESESSION] READY ... shapeData=0x0 mediaTexels=0x0
[MONSTERRETAL] COMMIT ... playerHP=34->32 armor=19->17 ... rollback=closed
```

The renderer compact-guard recovery remains known, successful and unrelated.

## Closure

Hardware-tested code ends at
`032c4d6eb8f1b9e3d5b4a3712499832cda96d4bc`.

The closure commit is documentation-only.
