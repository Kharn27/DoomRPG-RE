# Milestone — native gameplay SAVE/LOAD V5 action removals

Status: **REAL-CYD PASS / merge-ready**

Hardware-tested code boundary:

```text
d65e5b9be9947e92c700b2296790b003ff7b7df0
```

Normal GitHub Actions reference:

```text
esp32-cyd run #387 / 35704985512 = SUCCESS
```

Base main at branch creation:

```text
1ae140d6082f70b947426ae02893246601400416
PR #142
```

Branch:

```text
agent/esp32-native-save-v5-world-removals
```

## Goal

Extend the bounded native checkpoint one owner at a time. V5 adds persistence
for the action-engine sprite-removal overlay, initially covering the
hardware-owned fire-clear path, without serializing legacy entities or a
pointer-heavy world graph.

The native runtime invariants remain:

```text
shapeData == NULL
mediaTexels == NULL
runtime BSP backing = /DoomRPG-ESP32.pak
immutable EspMapRuntime rebuilt fresh on LOAD
mutable state restored only through explicit compact owners
```

## Format

V5 keeps the V1/V2/V3/V4 sections and appends exactly one bounded
`EspNativeGameplayActionRemovedSnapshot`.

```text
magic = DRPGSAV5
version = 5
recordBytes = 1356
V1/V2/V3/V4 read compatibility = retained
write format = V5
max action-removal payload = 128 B / 1024 sprites
Entrance sprites = 344
Entrance used payload = 43 B
```

The snapshot contains pointer-free identity and semantics only:

```text
sourceArenaFNV1a
stateFNV1a
spriteCount
removedBytes
removedCount
targetMapId
removedBits[]
```

Restore validates map/runtime identity, exact sprite/byte counts, bit-count and
semantic FNV, and the unused tail. Unsupported/corrupt state fails closed.

## Memory correction discovered by hardware

The first V5 writer used separate static read and write record workspaces. The
real classic CYD then reset during startup while inflating `mappings.bin`;
the extra BSS had reduced the largest usable block enough to break the existing
startup path.

The correction shares one bounded save workspace union and verifies a V5 write
by streaming the file against the expected record with a 64-byte local compare
buffer. The large record remains off loopTask stack.

Relevant correction:

```text
20f6f493bd4da725729f0722c0e7b09e028b64c2
ESP32: share save v5 workspace to recover startup DRAM
```

The real CYD subsequently completed normal startup again with the permanent
renderer invariants intact.

## Checkpoint-resume corrections discovered by hardware

A restored checkpoint can contain mutable line state which a historical
fresh-map first-frame witness intentionally rejects. The V5 LOAD therefore uses
a dedicated checkpoint resume path rather than weakening that witness.

The resume path now:

```text
rebuild immutable runtime
 -> restore player/view and compact mutable checkpoint owners
 -> recreate settled HUD semantic owners
 -> skip only the historical fresh-map first-frame witness
 -> render HUD
 -> prime resident small/large caches through production gameplay renderer
 -> one-shot admit resident gameplay for checkpoint resume
 -> reinstall touch callback
 -> enter normal resident gameplay
```

The production dynamic-line wrapper remains the owner of restored open doors;
the old fresh first-frame contract is unchanged for normal startup.

Hardware exposed and closed three resume gates in sequence:

1. restored open line rejected by the historical fresh first-frame witness;
2. initial HUD lacked its settled checkpoint reprime owners;
3. resident gameplay still required `EspNativeFirstFrame_isReady()`, so touch
   input was never rearmed after deliberately skipping that witness.

Final admission is bounded and one-shot. Fresh startup still requires the
normal first-frame owner. Checkpoint resume is admitted only after HUD plus the
resident and large caches are ready.

## Real-CYD persistence witness

The player cleared fire sprite 74 with the native fire path:

```text
[ACTIONENGINE] TRACE ... weapon=1 ... sprite index=74 ... type=10 ... route=FIRE_CLEARED
[ACTIONENGINE] FIRE-COMMIT ... sprite=74 ammo=10->9 ... rollback=closed
```

The subsequent V5 SAVE recorded exactly one action-owned removed sprite:

```text
[NATIVESAVE] SAVE ... version=5 bytes=1356
             map=1 gameplayLoadMapId=1
             pos=288,1248 angle=0
             playerFNV=15cb16e4 runtimeFNV=c3882516
             resources=5/43B sprites=344
             script=93/265/81B scriptFNV=26f291e3
             lines=480/60B open=1 locked=6 texture10=1
             lineFNV=c50b0721 textureFNV=bda09634
             actionRemoved=1/43B/a54be373
             atomic=temp+backup+rename
```

After moving away, LOAD rebuilt the canonical runtime and restored the exact
checkpoint owners:

```text
[NATIVESAVE] REPRIME-HUD ... angle=0 refresh=pending clear=ready ...
[PLAYERRES] RESTORE ... consumed=5 bytes=43
[MAPLINECHECKPOINT] RESTORE ... open=1 locked=6 texture10=1
[NATIVESAVE] LOAD ... version=5 bytes=1356
             pos=288,1248 angle=0
             playerFNV=15cb16e4
             resources=restored/5/43B
             script=restored/93/265/81B/26f291e3
             lines=restored/480/60B/open1/locked6/tex101/c50b0721/bda09634
             actionRemoved=restored/1/43B/a54be373
```

The user visually confirmed on the real CYD that the fire cleared before SAVE
remained absent after LOAD, while a different fire that had never been cleared
remained lit. This proves selective V5 removal persistence through the normal
native renderer rather than merely restored bookkeeping.

A stronger mirror test — clear a second fire *after* SAVE and prove that LOAD
brings only that second fire back — was not exercised in this supplied log and
is not claimed here.

## Real-CYD gameplay-resume witness

The restored open line was rendered through the production dynamic-line path
while caches were reprised:

```text
[ENGINESESSION] RESUME checkpoint=restored freshFirstFrame=skipped dynamicLines=gameplay-wrapper
[DYNAMICLINES] FRAME angle=0 open=1 ... render=ok immutableRuntime=yes
```

The final checkpoint admission then rearmed resident gameplay and touch:

```text
[ENGINECACHE] PRIMED ... next=collision+input
[RAMBUDGET] LAZY_PRE ...
[RESIDENTGAMEPLAY] READY map=current entry=checkpoint-resume ...
[RAMBUDGET] LAZY_POST ...
[ENGINESESSION] READY map=1 angle=0 residentCache=yes largeCache=yes
                    touch=invisible-120ms TURN+MOVE=armed
                    shapeData=0x0 mediaTexels=0x0
```

No legacy world/entity/render mutation was introduced by the checkpoint resume
bridge.

## RAM observation

The final hardware LOAD completed with:

```text
heap8 = 13576
largest8 = 5364
```

This is below the existing advisory reserve targets and remains a
fragmentation/headroom review item. It did not prevent the V5 LOAD, cache
reprime, resident-gameplay activation or input rearm in this test, and it is
not treated as a new architectural memory target.

## V5 world boundary

Persisted:

```text
settled player pose
EspNativeGameplayPlayerState
EspNativeGameplayPlayerResources consumed overlay
EspMapScriptState event states + removed-command bits
EspMapLineState open/locked state
EspMapLineTextureState locked/unlocked texture variants
EspNativeGameplayActionEngine action-owned removed-sprite overlay
```

Still intentionally fresh / not yet persisted:

```text
automap reveal state
monster mutable state / position / activation / combat consequences
full entity/sprite dynamic state and transformed definitions
destructible transformed states such as crate -> pickup
power-coupling health/death globals
persistent GSprites
ceiling/floor color
other legacy player metadata not yet owned natively
```

Gameplay RNG is also rebuilt fresh; this is not classified as a missing
legacy-save field because the recovered original save format does not serialize
the RNG state.

## Next boundary

Do not broaden V5 into a generic entity dump. A coherent next gameplay family is
generic type-12 destructible behavior, especially crate subtype 2, with exact
legacy weapon-mask/RNG consequences and a compact mutable owner for transformed
crate/pickup state. That transformed state needs its own persistence design;
the V5 removed-bit overlay is intentionally insufficient for it.
