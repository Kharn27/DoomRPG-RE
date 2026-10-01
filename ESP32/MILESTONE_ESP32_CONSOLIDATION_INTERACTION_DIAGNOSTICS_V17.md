# Milestone ESP32 consolidation — interaction diagnostics V17

Date: 2026-10-01  
Branch: `agent/esp32-consolidation-interaction-diagnostics-v17`  
Base main: `67950d6310ddb2fe4b48342200603674f6e71815`  
Hardware-tested code boundary: `53b548b5adf0d09c2d1e1ed4b673a3ae8054cac2`  
Status: **REAL-CYD PASS**

## Goal

Remove the remaining temporary interaction / CHANGEMAP recovery census from
the normal INFO firmware without changing the real native transition engine,
script semantics, renderer or gameplay ownership.

## Implementation

The production `esp32-cyd` linker no longer wraps
`EspNativeGameplayInteractionInventory_log`.

The player-resource session leaf calls the one-shot interaction inventory only
when `DOOMRPG_LOG_LEVEL >= DOOMRPG_LOG_DEBUG`. The historical
`esp_native_changemap_probe.c` implementation is likewise compiled only for
DEBUG/TRACE builds.

`esp32-cyd-bringup` remains TRACE and explicitly retains
`--wrap=EspNativeGameplayInteractionInventory_log`, so the old read-only
recovery witness is still available when deliberately requested.

No transition/gameplay implementation was modified.

## Production ownership preserved

The real production route remains:

```text
EspNativeGameplaySession
 -> EspNativeGameplayPlayerResources_sessionService
 -> EspNativeResidentGameplay
 -> EspNativeGameplayTransition
 -> EspNativeGameplayTransitionHandoff
```

The final normal ELF contains
`EspNativeGameplayTransitionHandoff_service`. It does not contain either
`EspNativeGameplayInteractionInventory_log` or its `__wrap_` symbol.

The older Junction exit census remains present and intentionally out of scope;
`INTERACTCORPUS` also remains a separate bounded diagnostic family.

## CI / ELF

CI #1214: **SUCCESS**

```text
static RAM       45072 B
linked Flash    764757 B
firmware.bin    765120 B
artifact id     11152203158
artifact digest sha256:3cf7e77078a257d81394e954f54ba3a12ad8ac3a505046b7f72ad88e03448adf
firmware sha256 06885150bee7ae651340a3b0cfb5557eae161bfc99a394bc9dae8355d2bf6425
ELF sha256      5b9bceb0f42f5391f522ac7a61bee9757310c534305b998c6195a72cafa23b37
active wraps    50
```

Versus merged V16 main:

```text
static RAM       45080 -> 45072 B   (-8 B)
linked Flash    767581 -> 764757 B  (-2824 B)
firmware.bin    767952 -> 765120 B  (-2832 B)
active wraps        51 -> 50        (-1)
```

The removed 8 B correspond to the two 4-byte one-shot arena-FNV diagnostic
owners that are no longer linked by the normal image.

Final ELF inspection confirms zero `CHANGEMAPPROBE` and zero `INTERACTMAP`
format strings. `INTERACTCORPUS` remains, as does
`JUNCTIONEXITCENSUS`, by design.

The local PlatformIO build on the hardware workstation reports the same
45072 B static RAM and 764773 B linked Flash / 765136 B firmware.bin, a
16-byte size difference versus CI.

## Real-CYD validation

The normal `esp32-cyd` image was built, flashed and exercised on the real
classic CYD.

Validated path:

```text
cold boot
 -> native MENU_MAIN
 -> START
 -> full intro
 -> bounded intro disposal
 -> Entrance native bootstrap
 -> exact FIRST_FRAME 71ca7465
 -> ENGINESESSION READY shapeData=0x0 mediaTexels=0x0
 -> FORWARD MOVE 904 -> 872
 -> TURN
 -> crate subtype-2 transform -> Armor Shard
 -> MOVE / resource pickup
 -> scientist DIALOG
 -> dialog resume through opcode 19
 -> further MOVE / second Armor Shard pickup
 -> healthy ALIVE
```

Representative readiness witness:

```text
[ENGINESESSION] READY map=1 angle=64 residentCache=yes largeCache=yes
touch=invisible-120ms TURN+MOVE=armed shapeData=0x0 mediaTexels=0x0
```

The first movement commits and renders successfully. The crate transaction
mutates to an Armor Shard and closes its rollback. Dialog event 88 opens,
pages, closes, and resumes the chain with opcode 19 state mutation. Two Armor
Shard pickups commit through the generic player-resource owner.

No `[CHANGEMAPPROBE]` or `[INTERACTMAP]` line appears anywhere in the
normal runtime transcript.

Observed memory:

```text
MENU_MAIN  heap8=35712 largest8=23540
intro      heap8=43256 largest8=12276
gameplay   heap8=27520 largest8=18420
```

Later dialog/resource owners legitimately lower heap8 as their bounded lazy
allocations materialize.

## Result

V17 is hardware-valid. The normal INFO firmware no longer executes or retains
the temporary interaction/CHANGEMAP recovery census, the historical witness
remains available in bringup, and the permanent native transition/gameplay
route is unchanged.

The next consolidation audit may treat `INTERACTCORPUS` and
`EspNativeGameplayTransition_probeJunctionExitCensus` as separate candidates;
neither was removed or reclassified here.
