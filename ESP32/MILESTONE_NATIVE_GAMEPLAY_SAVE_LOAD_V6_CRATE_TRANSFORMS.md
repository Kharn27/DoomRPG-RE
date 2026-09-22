# Milestone — native checkpoint save/load V6 crate transforms

Status: **REAL-CYD PASS / merge-ready**

Hardware-tested code boundary:

```text
172055a8bc0f2430f9d5443972ee153aa1bf4ffc
```

Normal GitHub Actions reference:

```text
ESP32 CYD Build / run 35720525109 = SUCCESS
RAM static = 44832 B
Flash = 716177 B
```

Base main at branch creation:

```text
a356f4d5c2f94a8838babf21bb69911e3c36b54c
PR #144
```

Branch:

```text
agent/esp32-native-save-v6-crate-transforms
```

## Goal

Extend the hardware-proven V5 checkpoint format with exactly one bounded
persistent owner: transformed type-12/subtype-2 crates.

V5 already persists action-owned removals. A transformed crate is different:
the original BSP sprite still exists, but gameplay projects a replacement
EntityDef into renderer/topology/pickup consumers. Encoding such a sprite as
removed would lose the transformed pickup.

V6 therefore persists the compact crate transform overlay itself.

Permanent invariants remain:

```text
shapeData == NULL
mediaTexels == NULL
immutable EspMapRuntime / BSP sprite records
no legacy Entity_t serialization
native PAK/raw-flash gameplay backing
bounded pointer-free checkpoint sections
```

## Format

V6 is written as:

```text
DRPGSAV6
recordBytes = 1532

V5-compatible semantic prefix = 1356 B
crate transform section        = 176 B
```

The crate section is canonical and pointer-free:

```text
sourceArenaFNV1a
stateFNV1a
spriteCount
transformedCount
transformedBytes
targetMapId
128 B transformed-sprite bitset
32 B packed 4-bit target codes
```

The 4-bit code space represents only the nine allowed crate pickup targets.
The code bytes are ordered by ascending set sprite index.

V1-V5 remain read-compatible.

## Memory discipline

The new 176 B V6 section does **not** enlarge the persistent save workspace BSS.

The existing V5-sized shared save workspace remains static, while the crate
section is streamed as a separate bounded segment and uses only a small
temporary local snapshot during SAVE/LOAD.

Normal CI static RAM therefore remains:

```text
44832 B
```

which is identical to the hardware-proven crate gameplay boundary.

This is deliberate because the previous crate milestone proved that even a
small unnecessary BSS increase can break real-CYD startup contiguous heap.

## Validation / fail-closed rules

Before accepting a V6 record:

```text
magic/version/recordBytes must match V6
full streamed record CRC must match
runtime/map identity must match
spriteCount/bitset shape must be canonical
transformedCount <= 64
unused bit/code tails must be zero
each transformed source must be an original type12/subtype2 crate
each target code must resolve to one allowed pickup EntityDef
actionRemoved and crateTransforms must be disjoint
crate transform FNV must match
```

LOAD rebuilds the immutable map first, restores existing V2-V5 owners, then
restores the crate transform overlay before session resume.

Older V5 saves intentionally report a legacy crate persistence gap and rebuild
crate transforms fresh.

## Hardware witness — non-zero transform

The real classic CYD transformed Entrance sprite 11:

```text
[ACTIONENGINE] TRACE seq=28 ... sprite index=11 ... type=12 subtype=2
[CRATE] CONSEQUENCE seq=28 sprite=11
        first=16
        outcome=TRANSFORM
        effectiveDefTile=99
        rngCombat=2
        rngConsequence=1
        mutation=transform-overlay

[CRATE] COMMIT seq=28 sprite=11
        ammo=15->14
        outcome=TRANSFORM
        effective=4/25/def99
        removed=0
        transformed=1
```

The user identified the resulting pickup visually as the medkit produced by the
crate.

## Hardware witness — V6 SAVE

The transformed crate was saved before consuming the transformed pickup:

```text
[NATIVESAVE] SAVE
version=6
bytes=1532
map=1
runtimeFNV=c3882516
resources=6/43B
actionRemoved=1/43B/4a2aa797
crateTransforms=1/43B/1B/31d6c324
recordCrc=740b9bcd
atomic=temp+backup+rename
```

This proves the V6 writer captured one live transformed crate while preserving
the already-owned action-removal and resource sections.

## Hardware witness — V6 LOAD

After later world movement/state changes, LOAD rebuilt Entrance and restored:

```text
[CRATECHECKPOINT] RESTORE
arena=c3882516
map=1
sprites=344
transformed=1
bytes=43
codeBytes=1
stateFNV=31d6c324
mutation=transform-overlay-only
allocation=existing-owner
```

The top-level checkpoint confirmed the same transform count and fingerprint:

```text
[NATIVESAVE] LOAD
version=6
bytes=1532
runtimeFNV=c3882516
resources=restored/6/43B
actionRemoved=restored/1/43B/4a2aa797
crateTransforms=restored/1/31d6c324
world=resources+script+lines+action-removals+crate-transforms-restored+others-fresh
```

The restored effective world also reported one additional type-4 pickup:

```text
[PLAYERRES] CORPUS ... pickups=115 ... type4=7 ...
```

The user visually confirmed that the medkit produced by the crate reappeared
after LOAD rather than reverting to the original crate.

This is the required non-zero hardware witness for transformed crate
persistence.

## Removal-path compatibility witness

The same V6 branch was also tested with a crate consequence:

```text
first=233
outcome=BREAK_REMOVE
removed=1
transformed=0
```

SAVE then wrote:

```text
actionRemoved=1/43B/4a2aa797
crateTransforms=0/43B/0B/c3fb6f17
```

and LOAD restored both the action-removal owner and the empty crate-transform
snapshot successfully.

So V6 preserves the already hardware-proven V5 removal semantics while adding
the transformed-crate section.

## Scope not expanded by V6

This milestone does not serialize arbitrary mutable entities.

Still deferred include:

```text
other type-12 destructible subtypes
rocket/BFG radial crate damage
monster mutable-state persistence
generic entity persistence
trapped-crate explosion side effects beyond existing bounded behavior
unrelated world families not already owned by V1-V6
```

The crate owner's READY diagnostic may still print `persistence=deferred`
because persistence is intentionally implemented by the external checkpoint
layer, not by giving the crate owner direct SD/file ownership.
