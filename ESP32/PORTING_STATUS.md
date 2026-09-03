# Doom RPG ESP32 CYD porting status

Authoritative recovery/status file for the classic ESP32-2432S028R port.
Repository state wins over chat history. Serial logs from the real classic CYD
are the final runtime authority.

## Git boundary — LOCKED milestone

```text
main = 8b9f23324ff314bf207cc3ffab01f11f76438515
branch = agent/esp32-native-weapon-control
base main = 8b9f23324ff314bf207cc3ffab01f11f76438515
hardware-tested code boundary = 17c30561bd7c080cdd55436caa6d67cae7250970
status = REAL-CYD GENERIC NEXT WEAPON CONTROL + LIVE PISTOL FIRE PASS
branch policy = LOCKED; docs-only tail only
```

`17c30561...` is the last code commit exercised on the real CYD. Commits after
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

Owned player-attack semantics include persistent monster HP/armor, exact integer
hit/crit/damage math, mType resistance/weakness, transactional gameplay RNG,
pain/death/gib presentation, XP application into PlayerState, and rollback on
render failure.

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

Combat, pickups, ammo, progression and monster retaliation share this owner. Do
not create per-feature or per-item player-state islands.

## Generic player resources / pickups — hardware PASS

Detailed milestone:

[`MILESTONE_NATIVE_PLAYER_RESOURCES.md`](MILESTONE_NATIVE_PLAYER_RESOURCES.md)

Native EntityDef metadata retains compact `{tile,type,subtype,parm}` records and
is allocated only when gameplay metadata is built.

Real-CYD Entrance witness:

```text
[ENTITYDEFTYPE] READY defs=115 metadata=115 cache=920B recordBytes=8 ...
[PLAYERRES] READY map=1 arena=c3882516 sprites=344 consumedBytes=43 playerBytes=52 ... families=3/4/5/6/16
[PLAYERRES] CORPUS map=1 arena=c3882516 pickups=114 type3=84 type4=6 type5=3 type6=17 type16=4 routes=all-generic playerOwner=shared
```

Hardware-validated families:

```text
type 3  health / armor / credits / keys
type 4  inventory
type 5  weapons
type 6  ammo
type 16 alternate ammo entries
```

HUD health/armor/weapon/ammo projection reads the same PlayerState.

## Extinguisher ammo transaction — hardware PASS

Fire removal consumes ammo from PlayerState transactionally:

```text
[ACTIONENGINE] FIRE-COMMIT ... ammoType=0 ammo=10->9 ... rollback=closed
```

The real HUD decremented with the same owner. Fire +2 XP and historical
jammed-door +1 XP remain deferred.

## Generic native weapon control — hardware PASS

Detailed milestone:

[`MILESTONE_NATIVE_WEAPON_CONTROL.md`](MILESTONE_NATIVE_WEAPON_CONTROL.md)

The resident input path now owns a permanent allocation-free circular weapon
selector over the historical 12 player slots. Ownership, selected slot and ammo
remain in the shared 52 B PlayerState. Selection follows the exact legacy gate:
owned plus `ammoUsage == 0 || ammo[ammoType] > 0`. Weapon cycling itself does not
schedule a monster turn, and redraw failure rolls PlayerState back exactly.

Real-CYD NEXT witness with `weapons=0007`:

```text
[WEAPONCONTROL] COMMIT seq=107 action=NEXT_WEAPON
  weapon=0->1 ammoType=0 ammo=10 inspected=1
  playerFNV=fa441c23->1e469366 redraw=yes turn=no rollback=closed
[WEAPONCONTROL] COMMIT seq=108 action=NEXT_WEAPON
  weapon=1->2 ammoType=1 ammo=12 inspected=1
  playerFNV=1e469366->464910f5 redraw=yes turn=no rollback=closed
```

The selected Pistol then fired through the existing generic type-1 combat
transaction against moved Hellhound sprite 179 at tile 718 / distance 2:

```text
[MONSTERCOMBAT] ROLL seq=110 weapon=2 worldDist=16384
  loops=1 hitLoops=1 firstRandHit=22 firstRandDamage=63
  totalDamage=5 armorDamage=3 crit=0 rngCalls=4
[WEAPON] DRAW weapon=2 logical=242 actual=611 frame=1 pose=attack
[MONSTERCOMBAT] COMMIT seq=110 sprite=179 weapon=2
  hp=6->0 armor=2->0 alive=1->0 ammo=12->11
  xp=5-applied rollback=closed
```

The submitted hardware log independently exercises NEXT, not PREV; PREV is
implemented by the same circular selector in reverse but is not claimed here as
a separate real-CYD witness. Direct generic single-target weapon combat currently
owns Axe, Pistol, Shotgun and Super Shotgun. Chaingun/Plasma multi-loop and
Rocket/BFG radial mechanics remain separate fail-closed families.

After the kill, the activation owner still reported `activeCount=1` while no
living candidate remained, producing conservative `active-order-not-owned`. This
is a monster activation/order cleanup boundary, not a weapon-combat failure.

## Generic monster presentation — hardware PASS

```text
nonlethal hit
 -> pain visual 6 / 250 ms
 -> normal visual

ordinary non-gib death
 -> death visual 4 / 250 ms
 -> corpse visual 2, unlinked

overkill/gib death
 -> death visual 4 / 250 ms
 -> hidden + bounded native gib burst
 -> burst expires after 350 ms by world redraw
```

The gib layer is presentation-only and uses local deterministic visual RNG; it
does not consume gameplay RNG or revive legacy ParticleSystem ownership.

## Native monster turn / retaliation — hardware PASS

Detailed milestone:

[`MILESTONE_NATIVE_MONSTER_TURN.md`](MILESTONE_NATIVE_MONSTER_TURN.md)

This is the first live enemy-turn family. It is generic, not dog/zombie-specific.
A committed MOVE, ROTATE, or PLAYER_ATTACK can schedule a native turn. The
bounded executor accepts one unambiguous active `type=1` attacker with cardinal
native LOS and a recovered supported monster weapon.

Owned live consequences:

```text
stationary cardinal attacker selection
native tile / line / sprite LOS
legacy-compatible monster weapon selection
legacy-compatible AI decision byte where applicable
legacy-compatible hit / crit / damage / armor math
nonlethal HP + armor mutation in shared PlayerState
transactional native redraw
miss gameplay-RNG commit path
render/RNG failure rollback
```

Still fail-closed inside retaliation:

```text
multiple ambiguous attackers / activation ordering
special subtype-10 AI
player lethal/death transition
dog-familiar damage redirection
attack visual / player pain FX / damage text / sound
```

## Native PASS TURN — hardware PASS

Detailed milestone:

[`MILESTONE_NATIVE_PASS_TURN.md`](MILESTONE_NATIVE_PASS_TURN.md)

The native resident gameplay dispatcher now owns the bounded legacy PASS TURN
route without moving or rotating the player. It first checks the recovered
`Game_touchTile(..., false)` precondition and fails closed if a linked type 10 or
11 entity occupies the player's current tile. When that unowned touch family is
absent, it queues the exact transient top-bar message and schedules the existing
generic monster-turn backend:

```text
PASS_TURN input
 -> current player tile / settled view validation
 -> type 10/11 current-tile touch precondition check
 -> queue "Turn passed." in existing ActionEngine top-bar owner
 -> request MonsterTurn reason=PASS_TURN
 -> generic movement or retaliation transaction
```

The feedback path is allocation-free and reuses the already hardware-proven
160x20 top-bar renderer. The message is visible for 1200 ms and is explicitly
cleared back to the normal top bar. The player position/angle is not mutated by
PASS TURN itself.

Decisive real-CYD witness:

```text
[PASSTURN] REQUEST seq=86 tile=654 pos=928,1312 angle=192
  tileTouch=none type10/11=absent message="Turn passed."-queued
  monsterTurn=requested playerMutation=no
[ACTIONFEEDBACK] PAINT kind=4 text="Turn passed." chars=12
  reads=35 bytes=9928 durationMs=1200
[MONSTERTURN] SCHEDULE n=35 reason=PASS_TURN passSeq=86
[MONSTERMOVELIVE] COMMIT sprite=179 tile=750->718
  pos=928,1504->928,1440 rngCalls=1 randomCommitted=yes
  positionFNV=61296c4a->10cf73aa topologyFNV=bb1d78a4->b40ad9d9
  renderer=snap-destination projected=yes rollback=closed
[ACTIONFEEDBACK] EXPIRE kind=4 elapsedMs=1204 targetMs=1200
  restored=topbar-only
```

A second PASS TURN moved the same Hellhound `718->686`. Subsequent PASS TURNs
then exercised both retaliation outcomes while the player remained fixed at
`928,1312`, angle `192`:

```text
MISS: probe=1 reason=PASS_TURN firstRandHit=235 rngCalls=2
      playerHP=30 armor=8 playerMutation=no gameplayRngCommitted=yes
HIT : probe=2 reason=PASS_TURN firstRandHit=80 firstRandDamage=111
      totalDamage=2 armorDamage=2 playerHP=30->28 armor=8->6
      frame=798a3e8f presented=1 rollback=closed
```

The refill-boundary movement path was also crossed on the first PASS TURN:
`PROBE-REFILL -> MONSTERMOVE -> PROBE-COMMIT -> MONSTERMOVELIVE`, preserving the
existing exact gameplay-RNG transaction contract.

Still deliberately deferred:

```text
current-tile type 10/11 Entity_touched semantics
monster attack animation / player pain FX / damage text / sound
player lethal/death transition
multiple-monster activation/movement ordering
full legacy turn orchestration outside the now-owned PASS TURN route
```

The missing Hellhound attack animation is therefore not part of this PASS. The
hit/miss consequences are live; visual attack presentation remains a separate
bounded family.

## Native monster position + movement planner — hardware PASS

Detailed probe milestone:

[`MILESTONE_NATIVE_MONSTER_MOVEMENT.md`](MILESTONE_NATIVE_MONSTER_MOVEMENT.md)

Detailed live-publication milestone:

[`MILESTONE_NATIVE_MONSTER_MOVEMENT_LIVE.md`](MILESTONE_NATIVE_MONSTER_MOVEMENT_LIVE.md)

A permanent compact spatial owner exists independently of immutable BSP sprite
coordinates:

```text
record = {spriteIndex,tileIndex,worldX,worldY}
recordBytes = 8
Entrance records = 30
payload = 240 B
initial source = native topology tile center
allocation = one load/session allocation
```

The recovered generic movement planner owns the first bounded
`Entity_aiThink`/`Entity_aiGoal_MOVE` behavior:

```text
attack LOS mask = 0x5687
movement mask = 0xff87 with legacy subtype adjustments
2-step calcPath greedy look-ahead
legacy east/west/south/north ordering
legacy north closestPathDist quirk preserved
visit choice = visitOrder[(rand & 3) % visitCount]
NO-IMMEDIATE-ATTACK and RANGED-AI producer paths
```

The planner remains the rollback-only preflight, but a successful unambiguous
ordinary-monster move may now be published live. Publication commits the exact
legacy gameplay RNG bytes, the 8 B `MonsterPosition` record, and a compact
SpriteTopology relink, then redraws the complete native frame. Immutable BSP
coordinates are not mutated.

Renderer projection is bounded by a static no-allocation published-position
bitset: an enemy keeps its historical immutable-runtime render coordinates until
its first committed native move. Interpolation remains deferred, so the current
visual behavior is a deliberate destination snap.

Decisive real-CYD live witness:

```text
sprite=179 subtype=1 weapon=13
sourceTile=750 source=928,1504
destTile=718 delta=0,-64
tieRand=204 choice=0 mask=ff87 rngCalls=1
probe positionFNV=61296c4a->10cf73aa->61296c4a
probe positionRollback=yes randomLiveUntouched=yes

[RNGGUARD] PROBE-COMMIT refill=1 bytes=1 leaseMs=1000
  hiddenGenerator=advanced-once-total reservation=consumed
  rollbackReplay=armed sequenceExact=yes

[MONSTERMOVELIVE] COMMIT trigger=NO-IMMEDIATE-ATTACK
  sprite=179 tile=750->718 pos=928,1504->928,1440
  rngCalls=1 randomCommitted=yes
  positionFNV=61296c4a->10cf73aa
  topologyFNV=bb1d78a4->b40ad9d9
  linkOrder=103->211
  renderer=snap-destination projected=yes
  frame=5fb03085 presented=1
  interpolation=deferred rollback=closed
```

The dog was visibly observed moving toward the player. More importantly, the
immediately following `SELECT` traced `sprite=179` on its new **tile 718** and the
generic combat backend armed against that same moved tile. This proves a real
gameplay consumer observed the relocated topology, not only a screen-space
sprite offset.

A direct player-FORWARD collision attempt into occupied tile 718 was not made in
this run, so that specific consumer is not claimed as independently exercised.

## RNG refill transaction + movement reservation — hardware PASS

Hardware testing proved that `Random_t` alone is not the full legacy RNG state.
`DoomRPG_setRand()` regenerates the 128-byte table using hidden file-static
generator state.

The generic guard now supports bounded rollback/replay plus live movement commit:

```text
ordinary rollback lease = 1000 ms
persistent movement-probe reservation = exact live Random_t pointer + pre/post table
live movement boundary commit = consume reserved table exactly once
hidden generator = advances once per logical refill
allocation = none
current static guard owner = bounded static state
```

The movement reservation may survive beyond the ordinary 1000 ms lease. A probe
temporarily borrows the reserved post-refill state. If publication does not
commit, live `Random_t` can still be restored exactly. If the live move commits,
`PROBE-COMMIT` closes the reservation while retaining the ordinary rollback lease
needed by downstream transactional consumers.

Decisive live-movement boundary chain:

```text
[RNGGUARD] PROBE-REFILL refill=1 ... hiddenGenerator=advanced-once ...
[MONSTERMOVE] PROBE ... tieRand=204 ... randomLiveUntouched=yes ... positionRollback=yes
[RNGGUARD] PROBE-COMMIT refill=1 bytes=1 leaseMs=1000 ... hiddenGenerator=advanced-once-total ... sequenceExact=yes
[MONSTERMOVELIVE] COMMIT ... rngCalls=1 randomCommitted=yes ... rollback=closed
[MONSTERMOVERNG] COMMIT ... reservation=consumed-by-live-move randomLive=advanced-exactly
```

The earlier probe/retaliation boundary sequence remains a retained regression
witness for rollback/replay behavior.

## Representative final RAM witness

Latest real-CYD gameplay logs at the locked code boundary:

```text
heap = 84608 B
heap8 = 18844 B
largest8 = 13812 B
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
generic monster state + player-attack combat
generic player resources / pickup disappearance
shared PlayerState HUD projection
extinguisher ammo consumption
generic death / corpse / gib presentation
bounded generic stationary monster retaliation
native PASS_TURN scheduling + exact "Turn passed." top-bar feedback
PASS_TURN-driven live monster movement / hit / miss consequences
compact mutable monster-position ownership
bounded legacy-compatible monster movement planner probe
persistent boundary-safe RNG reservation/replay
live one-monster movement RNG commit
live MonsterPosition commit + SpriteTopology relink
native renderer projection of committed moved position
generic NEXT weapon cycling through owned usable weapons
live selected-weapon PlayerState + HUD/weapon redraw
live Pistol ammo consumption + generic combat commit
```

## Intentionally deferred families

Still explicit/deferred:

```text
PASS_TURN current-tile type10/11 Entity_touched semantics
pickup sounds/messages/got-face presentation
combat/retaliation MISS/HIT/CRIT text feedback
fire +2 XP and jammed-door +1 XP migration into PlayerState
materialized monster drops
corpse-pile trimming
monster movement interpolation/animation
multiple-monster activation/movement ordering
unsupported special calcPath plane corpus
special subtype-10 AI
player death/lethal retaliation transition
monster attack/pain presentation and sound
full native turn advancement orchestration
independent PREV_WEAPON real-CYD witness
multi-loop chaingun/plasma presentation/commit
rocket/BFG radius damage
familiar weapon attack semantics for slots 9..11
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
3. re-read this status, `DOCUMENTATION.md`, and both movement milestones;
4. choose the next bounded gameplay family from the merged frontier.

Strong candidates now include:

```text
remaining generic weapon families: chaingun/plasma multi-loop then rocket/BFG radial
monster movement interpolation / animation
multiple-monster activation + movement ordering
monster attack + player-pain presentation and combat feedback
player lethal/death transition
action XP migration into PlayerState
materialized monster drops
```

Choose only after re-reading merged `main`.

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
