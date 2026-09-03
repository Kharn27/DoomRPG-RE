# Native PASS TURN milestone

Status: **REAL-CYD HARDWARE PASS** for bounded native PASS TURN scheduling,
transient top-bar feedback, live monster movement and nonlethal retaliation.

Hardware-tested code boundary:

```text
branch = agent/esp32-native-pass-turn
base main = 7f799ab9bac2bb60f7cffb65719131f0f9877ae0
code boundary = 40a19ba56ee946477ba107d237e644b7bac2e9ef
CI = PlatformIO esp32-cyd SUCCESS
```

Commits after that boundary are documentation-only.

## Goal

Own the real PASS TURN control path without creating a parallel turn engine or
importing legacy mutable world ownership. The action must leave player position
and facing unchanged, preserve the recovered current-tile touch precondition,
show the historical `"Turn passed."` message, then feed the already-native
monster movement/retaliation machinery.

Permanent composition:

```text
resident PASS_TURN input
 -> settled PlayerView/current tile
 -> bounded legacy Game_touchTile(..., false) precondition check
 -> existing ActionEngine transient top-bar queue
 -> MonsterTurn request reason=PASS_TURN
 -> existing generic movement / retaliation backends
```

No new heap allocation, legacy `Hud*`, legacy `Entity_t*`, desktop world owner or
map-wide media ownership is introduced.

## Recovered legacy boundary

Legacy PASS TURN first performs `Game_touchTile(..., false)`. For this call, the
relevant unowned consequence is `Entity_touched()` on a linked type 10 or 11
entity occupying the player's current tile. The native route therefore detects
that condition and fails closed before queuing feedback or requesting a monster
turn.

When no such entity is present, the bounded native route owns:

```text
player view must be settled
current tile = viewX/viewY >> 6
player position/facing mutation = none
message = "Turn passed."
message owner = existing ActionEngine top-bar feedback
display duration = 1200 ms
monster turn reason = PASS_TURN
```

If the feedback queue cannot arm, no turn is requested. If the turn request is
busy after feedback was queued, the same pending feedback is cancelled
transactionally.

## Feedback reuse

PASS TURN does not add a second HUD renderer. `EspNativeGameplayActionEngine` now
exposes a compact semantic feedback queue around the already hardware-proven
160x20 top-bar paint path used by native SELECT consequences.

If a monster movement or hit redraw presents immediately, that presentation
consumes the queued message. If no gameplay redraw occurs first, the resident
ActionEngine service presents the existing framebuffer with only the top-bar
decoration. Expiry repaints the normal top bar after 1200 ms.

## Real-CYD movement witness

The first tested PASS TURN occurred with Hellhound sprite 179 active three tiles
ahead while the player remained fixed at `928,1312`, angle `192`:

```text
[PASSTURN] REQUEST seq=86 tile=654 pos=928,1312 angle=192
  tileTouch=none type10/11=absent message="Turn passed."-queued
  monsterTurn=requested playerMutation=no
[ACTIONFEEDBACK] PAINT kind=4 text="Turn passed." chars=12
  reads=35 bytes=9928 present=caller durationMs=1200
[MONSTERTURN] SCHEDULE n=35 reason=PASS_TURN passSeq=86
```

That turn crossed the 128-byte RNG refill boundary and committed one real live
monster move:

```text
[RNGGUARD] PROBE-REFILL refill=1 next=127->0
  hiddenGenerator=advanced-once reservation=until-live-replay
[MONSTERMOVE] PROBE trigger=NO-IMMEDIATE-ATTACK sprite=179
  sourceTile=750 source=928,1504 destTile=718 delta=0,-64
  tieRand=151 rngCalls=1 randomLiveUntouched=yes
  positionFNV=61296c4a->10cf73aa->61296c4a positionRollback=yes
[RNGGUARD] PROBE-COMMIT refill=1 bytes=1 leaseMs=1000
  hiddenGenerator=advanced-once-total reservation=consumed sequenceExact=yes
[MONSTERMOVELIVE] COMMIT sprite=179 tile=750->718
  pos=928,1504->928,1440 rngCalls=1 randomCommitted=yes
  positionFNV=61296c4a->10cf73aa
  topologyFNV=bb1d78a4->b40ad9d9 linkOrder=103->211
  renderer=snap-destination projected=yes rollback=closed
```

The message then expired normally:

```text
[ACTIONFEEDBACK] EXPIRE kind=4 elapsedMs=1204 targetMs=1200
  restored=topbar-only
```

A second PASS TURN committed the same dog from tile `718->686`, position
`928,1440->928,1376`, with the player still unchanged.

## Real-CYD retaliation witness

Once adjacent, PASS TURN drove the existing generic retaliation path without any
MOVE/ROTATE/player-attack surrogate trigger. Both miss and hit were observed.

Miss:

```text
[MONSTERTURN] ATTACK-PROBE reason=PASS_TURN sprite=179 subtype=1
  weapon=13 firstRandHit=235 firstCalcHit=193 rngCalls=2
  playerHP=30->30 armor=8->8 rngRollback=yes playerExact=yes
[MONSTERRETAL] MISS-COMMIT probe=1 reason=PASS_TURN
  gameplayRngCommitted=yes playerMutation=no turn=closed
```

Hit:

```text
[MONSTERTURN] ATTACK-PROBE reason=PASS_TURN sprite=179 subtype=1
  weapon=13 firstRandHit=80 firstRandDamage=111
  totalDamage=2 armorDamage=2 playerHP=30->28 armor=8->6
  rngRollback=yes playerExact=yes
[MONSTERRETAL] COMMIT probe=2 reason=PASS_TURN
  playerHP=30->28 armor=8->6
  frame=798a3e8f presented=1 rollback=closed
```

The earlier diagnostic `reason=NONE` was only a missing string case in the
retaliation logger; the turn owner had preserved numeric reason 4. The tested
boundary now logs `reason=PASS_TURN` end-to-end.

## Final RAM witness

Normal real-CYD `esp32-cyd` logs remained stable through repeated PASS TURN,
movement and retaliation:

```text
heap = 84608 B
heap8 = 18844 B
largest8 = 13812 B
PSRAM = none
shapeData = NULL
mediaTexels = NULL
```

## Explicitly deferred

```text
current-tile type 10/11 Entity_touched behavior
monster attack animation
player pain FX / damage text
sound
player lethal/death transition
multiple-monster activation/movement ordering
monster movement interpolation
full legacy turn orchestration outside PASS TURN
```

The Hellhound therefore visibly walks into range and its hit/miss gameplay
consequences are live, but its attack sprite animation is intentionally not
claimed by this milestone. That presentation family should be migrated
separately rather than being smuggled into PASS TURN.

## Merge boundary

`40a19ba56ee946477ba107d237e644b7bac2e9ef` is the last code commit exercised
by the real hardware for this milestone. The branch is locked to docs-only
changes until merge.

After merge, re-read the real GitHub `main` SHA before creating the next
`agent/*` branch.
