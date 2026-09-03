# Native live monster movement publication milestone

Status: **REAL-CYD HARDWARE PASS** for bounded live publication of one ordinary
native monster movement into gameplay RNG, `MonsterPosition`, sprite topology and
the native renderer.

Hardware-tested code boundary:

```text
branch = agent/esp32-native-monster-movement-live
base main = c377f89d75bb9a3f8efe7398bd7e993757380700
code boundary = bc39044a2d6931899f3f10097d34522996897db0
```

Commits after that boundary are documentation-only.

## Goal

Promote the previously hardware-proven rollback-only movement planner into the
first real visible/live enemy movement while keeping the legacy split between
logical movement and interpolation.

The bounded publication chain is:

```text
MonsterTurn producer
 -> movement planner probe
 -> exact RNG replay
 -> MonsterPosition commit
 -> SpriteTopology relink
 -> sprite overlay projection
 -> complete native gameplay redraw / present
```

Interpolation is deliberately still deferred. The renderer snaps the published
monster to the destination tile for this milestone.

## Permanent ownership

No map-wide or pointer-heavy owner was added.

Retained owners:

```text
MonsterPosition record = 8 B / enemy
Entrance records = 30
MonsterPosition payload = 240 B
SpriteTopology = existing compact mutable owner
movement projection bits = bounded static bitset, no allocation
RNG guard = existing bounded static owner, no allocation
```

Immutable BSP sprite coordinates remain immutable. Only monsters that have
actually committed a live movement are projected from `MonsterPosition`; all
other map sprites retain their previous immutable-runtime rendering path.

## Topology relink transaction

The native topology now exposes prepare/commit/rollback for one enemy relink.
The operation is the compact native equivalent of the relevant legacy
`Game_unlinkEntity()` + `Game_linkEntity()` consequence:

```text
source linked/alive enemy required
destination tile must differ
nextLinkOrder advances exactly once on commit
rollback restores link state, link order and nextLinkOrder exactly
allocation = none
```

This keeps logical occupancy synchronized with the committed monster position.

## RNG publication transaction

The rollback-only planner still owns the path decision and remains the preflight.
When that probe succeeds, the publication layer replays the exact number of
legacy gameplay bytes that the movement path consumed and commits them only with
the live move.

At the 128-byte table boundary, the previously reserved post-refill table is
borrowed for the probe and then consumed by the live movement without a second
hidden-generator advance.

The real-CYD witness crossed that boundary:

```text
[RNGGUARD] PROBE-REFILL refill=1 next=127->0
  hiddenGenerator=advanced-once reservation=until-live-replay

[MONSTERMOVE] PROBE ... rngCalls=1 randomLiveUntouched=yes

[RNGGUARD] PROBE-COMMIT refill=1 bytes=1 leaseMs=1000
  hiddenGenerator=advanced-once-total
  reservation=consumed rollbackReplay=armed sequenceExact=yes

[MONSTERMOVERNG] COMMIT trigger=NO-IMMEDIATE-ATTACK rngCalls=1
  reservation=consumed-by-live-move randomLive=advanced-exactly
```

## Decisive real-CYD live movement witness

Hellhound sprite 179 moved one tile north toward the player and was visibly seen
to advance on the classic CYD:

```text
[MONSTERMOVE] PROBE trigger=NO-IMMEDIATE-ATTACK n=34 reason=1
  sprite=179 subtype=1 weapon=13
  sourceTile=750 source=928,1504
  target=928,1440
  destTile=718 delta=0,-64
  immediateAttack=no
  visitCount=1 tieRand=204 choice=0 mask=ff87 rngCalls=1
  randomLiveUntouched=yes
  positionFNV=61296c4a->10cf73aa->61296c4a
  positionRollback=yes

[MONSTERMOVELIVE] COMMIT trigger=NO-IMMEDIATE-ATTACK
  sprite=179 tile=750->718
  pos=928,1504->928,1440
  rngCalls=1 randomCommitted=yes
  positionFNV=61296c4a->10cf73aa
  topologyFNV=bb1d78a4->b40ad9d9
  linkOrder=103->211
  renderer=snap-destination projected=yes
  frame=5fb03085 presented=1
  interpolation=deferred rollback=closed
```

The user independently confirmed the dog visibly advanced toward the player.

## Post-move consumer proof

The strongest non-visual witness came immediately after the move. A real SELECT
at player tile 686 traced the same monster on its **new tile 718**:

```text
[ACTIONENGINE] TRACE seq=84 weapon=0 distance=1 tile=718
  target=sprite index=179 type=1 subtype=1 route=ENEMY_COMBAT_DEFERRED

[MONSTERCOMBAT] ARM seq=84 sprite=179 tile=718 subtype=1 ...
```

The generic combat backend then killed that same sprite and completed the normal
death/gib path. This proves the live movement was not only a screen-space offset:
subsequent gameplay lookup observed the relocated topology.

A direct player-FORWARD collision attempt into occupied tile 718 was **not** made
in this hardware run, so player collision against a moved monster is not claimed
as an independently exercised witness here.

## Real-CYD RAM witness

Normal `esp32-cyd` environment after live publication:

```text
heap = 84608 B
heap8 = 18844 B
largest8 = 13812 B
PSRAM = none
shapeData = NULL
mediaTexels = NULL
```

The selected resident cache geometry remains unchanged.

## Still deferred

This milestone does not attempt to complete the full legacy movement subsystem.
Still explicit/deferred:

```text
monster movement interpolation / animation
multiple-active-monster movement ordering
special subtype-10 AI
unsupported special calcPath plane corpus
legacy fallback attack when no movement path exists
player lethal/death transition
dog-familiar redirection
monster attack / player pain presentation
sound / combat text feedback
```

The live path remains conservative: one unambiguous active ordinary enemy at a
time, with fail-closed behavior for unsupported ordering or semantics.

## Merge boundary

`bc39044a2d6931899f3f10097d34522996897db0` is the last code commit exercised
by the real classic CYD for this milestone. The branch is locked to
documentation-only changes until merge.

After merge, recover the true GitHub `main` SHA before creating the next
`agent/*` branch.