# Doom RPG ESP32 CYD porting status

Authoritative recovery/status file for the classic ESP32-2432S028R port.
Repository state wins over chat history. Serial logs from the real classic CYD
are the final runtime authority.

## Git boundary — LOCKED milestone

```text
main = 854ade1a7110fff44926099bdae418ca47e55365
branch = agent/esp32-native-player-resources
base main = 854ade1a7110fff44926099bdae418ca47e55365
hardware-tested code boundary = 8152cf8d233067a44b2d705edcaf315845b7744a
status = REAL-CYD GENERIC PLAYER RESOURCES + PROJECTION + GIB FX PASS
branch policy = LOCKED; docs-only tail only
```

`8152cf8d...` is the last code commit exercised on the real CYD. Commits after
that boundary must remain documentation-only until merge.

After merge, read the real GitHub `main` SHA again before creating the next
`agent/*` branch.

## Permanent architecture and hard invariants

```text
A NEW BSP IS NOT A NEW ENGINE.
A NEW MONSTER IS NOT A NEW COMBAT BACKEND.
A NEW PICKUP MUST NOT BECOME A NEW MINI-OWNER.
```

Production path:

```text
/DoomRPG-ESP32.pak
 -> native parsers/catalog
 -> compact immutable EspMapRuntime
 -> small explicit mutable owners
 -> native event/action/gameplay
 -> native renderer
```

Hard invariants:

```text
board       = ESP32-2432S028R classic CYD
MCU         = ESP32-D0WD-V3 dual core 240 MHz
flash       = 4 MB
PSRAM       = none
framebuffer = 160x120 RGB565 = 38400 B
shapeData   = NULL
mediaTexels = NULL
backing     = /DoomRPG-ESP32.pak
```

Do not reintroduce map-wide legacy texels, desktop pointer-heavy world ownership,
or runtime ZIP dependence for migrated gameplay/map data.

## Entrance canonical witness

```text
resourceMapId = 1
resource = /intro.bsp
name = Entrance
sourceBytes = 21823
crc32 = 623f34e4
sourceFNV = d5cc751f
runtime arena = 14095 B
runtimeFNV = c3882516
resident payload = 17891 B
spawn tile = 904
spawn direction = 64
spawn position = 544,1824
nodes = 223
lines = 480
sprites = 344
events = 93
byteCodes = 265
strings = 94
native topology entities = 220
enemies = 30
destructibles = 13
```

Core owner fingerprints retained as regression witnesses:

```text
mapStateFNV  = cd99b98e
scriptFNV    = f9e3d9df
lineFNV      = e5e74861
textureFNV   = f1fc1875
automapFNV   = 669b1aa7
topologyFNV  = 3f321e43
```

## Selected resident asset-cache baseline

Current hardware-selected cache remains unchanged:

```text
owner = 23592 B
payload = 19456 B (19 KiB)
range records = 288
range record = 12 B
resident entry slots = 24
large exact range = 2048 B
```

The global cache-reset cliff remains separate performance work. Preserve this
baseline while correctness work advances.

## Generic native monster state/combat — hardware PASS

Detailed milestone:

[`MILESTONE_NATIVE_MONSTER_COMBAT.md`](MILESTONE_NATIVE_MONSTER_COMBAT.md)

Permanent engine split:

```text
MonsterTrace
 -> CombatMath
 -> MonsterState + PlayerState
 -> MonsterCombat transaction
 -> renderer/liveness overlays
```

Entrance monster owner:

```text
30 enemies
16 B / enemy
480 B total
180 legacy-compatible RNG calls during initialization
```

The combat backend is generic for `type=1`; ordinary monsters remain data-driven
by `subtype/mType` and compact stats. Hellhound and Zombie have both been used as
hardware corpus witnesses through the same backend.

Owned direct-combat semantics include:

```text
persistent HP / armor
legacy integer hit / crit / damage math
mType resistance/weakness table
transactional gameplay RNG
pain visual 6 / 250 ms
ordinary death visual 4 -> corpse visual 2
recovered generic overkill/gib classification
XP application into shared PlayerState
full monster/player/RNG rollback on render failure
```

## Shared native PlayerState

`EspNativeGameplayPlayerState` is the one 52 B player-facing owner for:

```text
HP / max HP
armor / max armor
defense / strength / agility / accuracy
XP / level / next-level XP
keys
credits
ammo[6]
inventory[5]
weapon bits / selected weapon
```

Combat, pickups, ammo and progression must continue to share this owner. Do not
create per-feature or per-item player state islands.

## Generic player resources / pickups — COMPLETE hardware milestone

Detailed milestone:

[`MILESTONE_NATIVE_PLAYER_RESOURCES.md`](MILESTONE_NATIVE_PLAYER_RESOURCES.md)

The historical weapon-only bring-up path has been superseded by one generic
resource executor backed by EntityDef metadata and PlayerState.

### EntityDef metadata

Native metadata now retains compact `{tile,type,subtype,parm}` records. The
catalog is allocated when native gameplay metadata is built rather than as a
large boot-time BSS table.

Real-CYD Entrance witness:

```text
[ENTITYDEFTYPE] READY defs=115 metadata=115 cache=920B recordBytes=8 ...
```

This change also fixed the temporary boot fragmentation regression that had made
`menu.bsp` fail its 10992 B inflate-state allocation. Normal boot is hardware
validated again.

### Generic corpus

```text
[PLAYERRES] READY map=1 arena=c3882516 sprites=344 consumedBytes=43 playerBytes=52 ... families=3/4/5/6/16
[PLAYERRES] CORPUS map=1 arena=c3882516 pickups=114 type3=84 type4=6 type5=3 type6=17 type16=4 routes=all-generic playerOwner=shared
```

One map-local consumed bitset owns pickup disappearance; Entrance uses 43 B for
344 sprites. Player values live in PlayerState, not in that bitset owner.

Hardware-validated resource families:

```text
type 3  health / armor / credits / keys
type 4  inventory
type 5  weapons
type 6  ammo
type 16 alternate ammo entries
```

Real display witnesses include:

```text
armor shard: 0 -> 4 -> 8 armor, both sprites disappear
medkit/inventory: value 0 -> 1, sprite disappears
extinguisher pickup: selected weapon becomes 1, sprite disappears
extinguisher recharge: ammo0 10 -> 13, sprite disappears
credits: 0 -> 1, sprite disappears
```

HUD health/armor/weapon/ammo projection now reads the same shared PlayerState.

## Extinguisher ammo transaction — hardware PASS

Fire removal now consumes ammo from PlayerState inside the same transactional
world update:

```text
[ACTIONENGINE] FIRE-COMMIT seq=76 sprite=74 ammoType=0 ammo=10->9 playerFNV=a6e115a7->15cb16e4 xp=2-deferred sound=5045-deferred turnAdvance=deferred rollback=closed
```

The real HUD decremented from 10 to 9 when the fire was extinguished.

Fire +2 XP remains deferred and is not yet migrated into the permanent
progression owner. The historical jammed-door +1 XP is also still deferred.

## Generic monster presentation repairs — hardware PASS

Validation of pickups exposed a projection bug where mutable visual state could
be logged correctly but overwritten before sprite drawing. The projection chain
is now composed at the final topology/render boundary.

### Pain

Nonlethal hits show visual 6 briefly and return to the normal sprite. This was
observed physically on the Hellhound.

### Ordinary death

A non-gib ordinary death uses:

```text
death visual 4 / 250 ms
 -> stable corpse visual 2
 -> entity remains unlinked/dead
```

This is a generic monster presentation state machine, not a subtype-specific
route.

### Overkill / gib

Legacy overkill classification is also generic. A gibbed monster has no corpse:
after the death pose it becomes hidden and a bounded native presentation-only
burst replaces it.

Current native gib FX:

```text
owner = 148 B
chunks = 5
particle count = bounded by monster max health
lease = 350 ms
visual RNG = local deterministic xorshift
gameplay RNG = untouched
legacy ParticleSystem = not owned
```

Final hardware witness at the locked code boundary:

```text
[MONSTERCOMBAT] DEATH-SETTLE sprite=179 visual=4->hidden delayMs=250 gib=1 gibFX=deferred immutableSprite=yes
[GIBFX] PAINT sprite=179 subtype=1 particles=17 chunks=5 pixels=86 center=80,52 ownerBytes=148 leaseMs=350 visualRng=local gameplayRng=untouched legacyParticleSystem=no
[GIBFX] EXPIRE sprite=179 leaseMs=350 frame=1426a13d presented=1 restored=world-redraw gameplayRng=untouched
```

The user confirmed the burst disappears automatically without movement or touch.
Zombie subtype 0 was also exercised through the same generic gib family on the
preceding code boundary.

## Representative final RAM witness

Latest gameplay logs around the final combat test:

```text
heap8 = 19788 B
largest8 = 14324 B
shapeData = NULL
mediaTexels = NULL
PSRAM = none
```

The selected resident asset-cache baseline remains unchanged.

## Existing gameplay boundary retained

Previously hardware-validated systems remain intact:

```text
TURN_LEFT / TURN_RIGHT
FORWARD / BACK / STRAFE
native collision/topology
SELECT event-first routing
EV_SHOW / EV_HIDE / EV_UNLOCK
EV_OPENLINE / EV_CLOSELINE
EV_DIALOG / EV_DIALOGNOBACK
EV_FORCEMESSAGE / EV_NOTE
state ops 11 / 19 / 20
regular door open/close animation
mutable line texture variants
native idle weapon rendering
generic attack frame presentation
move-event state mutation with rollback/commit
jammed-door destructible subtype-3 destruction and traversal
generic monster state + combat
generic player resources / pickup disappearance
shared PlayerState HUD projection
extinguisher ammo consumption
generic death / corpse / gib presentation
```

## Intentionally deferred families

Still explicit/deferred:

```text
pickup sounds/messages/got-face presentation
combat MISS/HIT/CRIT text feedback
fire +2 XP and jammed-door +1 XP migration into PlayerState
materialized monster drops
corpse-pile trimming
enemy retaliation / native monster AI turn
turn advancement
audio playback
multi-loop chaingun/plasma presentation/commit
rocket/BFG radius damage
special death consequences for subtypes 7, 8, 12, 13
Kronos-specific semantics
password input
SAVEGAME / CHANGEMAP production transition consumer
```

These are mechanical family boundaries, never an item-by-item or monster-by-
monster implementation ladder.

## CHANGEMAP remains deferred

Entrance event 1 / tile 69 remains recovered but intentionally not live yet:

```text
SAVEGAME -> /junction.bsp, targetMapId 9, savePos 992,1888 angle 64
CHANGEMAP -> /junction.bsp, targetMapId 9, showStats 1, spawnParam 0
OPENLINE -> third eligible command
```

Do not force the transition before enough native gameplay exists to complete the
map normally.

## Next direction after merge

Do **not** continue code on this locked branch.

After merge:

1. read actual GitHub `main` and exact SHA;
2. create a fresh coherent `agent/*` branch from that SHA;
3. recover the next gameplay family from legacy behavior;
4. prefer engine-level consequences that close existing deferred seams rather
   than per-entity milestones.

Strong candidates now include:

```text
shared combat/action feedback: MISS / HIT / CRIT + pickup messages
migrate deferred action XP into PlayerState
standard ammo-consuming direct-fire weapon family
monster turn / retaliation owner
materialized monster drops
```

Choose the next bounded family only after re-reading the merged `main`.

## Development workflow

```text
recover true main + PORTING_STATUS + DOCUMENTATION + latest milestone
 -> choose one bounded behavior FAMILY
 -> recover exact legacy semantics
 -> implement permanent compact native API/owner
 -> keep materially unsupported families fail-closed
 -> commit + push agent/*
 -> test normal esp32-cyd on real CYD
 -> Serial is hardware truth
 -> fix failures and push without inventing results
 -> after PASS, docs-only tail
 -> declare merge-ready
```

Never merge to `main` without explicit user request. After the user announces a
merge, re-read true `main`, recover its exact SHA, and branch the next milestone
from that SHA.
