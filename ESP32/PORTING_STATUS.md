# Doom RPG ESP32 CYD porting status

Authoritative recovery/status file for the classic ESP32-2432S028R port.
Repository state wins over chat history. Serial logs from the real classic CYD
are the final runtime authority.

## Git boundary — LOCKED milestone

```text
main = a74a6f067cc7a5304a1f940f78317804659c37e8
branch = agent/esp32-native-hazard-touch
base main = a74a6f067cc7a5304a1f940f78317804659c37e8
hardware-tested code boundary = bae9d1a78f40db31c35a8a0aa9a1875692cf5c9e
status = REAL-CYD MOVEMENT HAZARD TOUCH + RED DAMAGE FEEDBACK PASS
branch policy = LOCKED; docs-only tail only
```

`bae9d1a7...` is the exact code boundary exercised on the real CYD. Commits
after that SHA must remain documentation-only until merge.

Normal GitHub Actions `esp32-cyd` run `33845214033` completed successfully for
this exact SHA. CI is a compile/link gate only; the hardware logs remain
authoritative.

After merge, read the real GitHub `main` SHA again before creating the next
`agent/*` branch.

## Permanent architecture and hard invariants

```text
A NEW BSP IS NOT A NEW ENGINE.
A NEW MONSTER IS NOT A NEW COMBAT BACKEND.
A NEW PICKUP MUST NOT BECOME A NEW MINI-OWNER.
```

Target production path:

```text
/DoomRPG-ESP32.pak
 -> native parsers/catalog
 -> compact immutable EspMapRuntime
 -> small explicit mutable owners
 -> native event/script engine
 -> native gameplay
 -> native renderer
```

Hardware / memory invariants:

```text
board       = ESP32-2432S028R classic CYD
MCU         = ESP32-D0WD-V3 dual core 240 MHz
flash       = 4 MB
PSRAM       = none
framebuffer = 160x120 RGB565 = 38400 B
shapeData   = NULL
mediaTexels = NULL
native backing = /DoomRPG-ESP32.pak
```

Do not reintroduce map-wide legacy texels, desktop pointer-heavy world ownership,
or runtime ZIP dependence for migrated gameplay/map data. The current firmware
still has a transitional `/DoomRPG.zip` startup dependency for legacy HUD/layout
resources; removing that debt remains part of the native migration.

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

Retained owner fingerprints:

```text
mapStateFNV = cd99b98e
scriptFNV   = f9e3d9df
lineFNV     = e5e74861
textureFNV  = f1fc1875
automapFNV  = 669b1aa7
topologyFNV = 3f321e43
```

## Selected resident asset-cache baseline

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

## Current hardware-owned gameplay frontier

Previously validated behavior retained on the current branch includes:

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
native idle weapon rendering + generic attack pose
move-event state mutation with rollback/commit
jammed-door subtype-3 destruction and traversal
generic monster state + type-1 player attack combat
generic player resources / consumed pickup removal
shared 52 B PlayerState + HUD projection
extinguisher ammo consumption + fire removal
pain / corpse / gib presentation
bounded stationary monster retaliation
native PASS_TURN + exact top-bar feedback
compact mutable MonsterPosition owner
legacy-compatible movement planner + RNG reservation/replay
live one-monster movement publication + topology relink
renderer projection of committed moved monster position
generic NEXT and PREV weapon cycling
selected-weapon HUD / first-person redraw without turn advance
live Pistol ammo consumption + generic combat commit
live pickup messages + white pickup flash
adaptive floor/ceiling texture cache under memory pressure
movement-side linked type10/type11 hazard touch into shared PlayerState
live bounded damage text + red viewport flash for movement hazards
```

Relevant detailed records:

- [`MILESTONE_NATIVE_JAMMED_DOOR.md`](MILESTONE_NATIVE_JAMMED_DOOR.md)
- [`MILESTONE_NATIVE_MONSTER_COMBAT.md`](MILESTONE_NATIVE_MONSTER_COMBAT.md)
- [`MILESTONE_NATIVE_PLAYER_RESOURCES.md`](MILESTONE_NATIVE_PLAYER_RESOURCES.md)
- [`MILESTONE_NATIVE_MONSTER_TURN.md`](MILESTONE_NATIVE_MONSTER_TURN.md)
- [`MILESTONE_NATIVE_PASS_TURN.md`](MILESTONE_NATIVE_PASS_TURN.md)
- [`MILESTONE_NATIVE_WEAPON_CONTROL.md`](MILESTONE_NATIVE_WEAPON_CONTROL.md)
- [`MILESTONE_NATIVE_MONSTER_MOVEMENT.md`](MILESTONE_NATIVE_MONSTER_MOVEMENT.md)
- [`MILESTONE_NATIVE_MONSTER_MOVEMENT_LIVE.md`](MILESTONE_NATIVE_MONSTER_MOVEMENT_LIVE.md)
- [`MILESTONE_NATIVE_PICKUP_FEEDBACK.md`](MILESTONE_NATIVE_PICKUP_FEEDBACK.md)
- [`MILESTONE_NATIVE_HAZARD_TOUCH.md`](MILESTONE_NATIVE_HAZARD_TOUCH.md)

## Shared native PlayerState

`EspNativeGameplayPlayerState` remains the single 52 B player-facing owner for:

```text
HP / max HP
armor / max armor
defense / strength / agility / accuracy
XP / level / next-level XP
keys / credits
ammo[6]
inventory[5]
weapon bits / selected weapon
```

Combat, pickups, HUD, ammo, progression and monster retaliation share this owner.
Do not create per-feature or per-item state islands.

## Generic player resources + live pickup feedback — hardware PASS

Entrance native EntityDef metadata remains compact and data-driven:

```text
[ENTITYDEFTYPE] READY defs=115 metadata=115 cache=920B recordBytes=8
[PLAYERRES] READY map=1 arena=c3882516 sprites=344 consumedBytes=43 playerBytes=52
[PLAYERRES] CORPUS map=1 arena=c3882516 pickups=114
  type3=84 type4=6 type5=3 type6=17 type16=4
  routes=all-generic playerOwner=shared
```

Hardware-owned families:

```text
type 3  health / armor / credits / keys
type 4  inventory
type 5  weapons
type 6  ammo
type 16 alternate ammo entries
```

Current real-CYD witnesses include:

```text
Armor Shard #1  armor 0->4   message="Got Armor Shard"
Armor Shard #2  armor 4->8   message="Got Armor Shard"
Bullet Clip      ammo1 8->12  message="Got Bullet Clip"
Fire Ext         weapon=1     message="Got Fire Ext"
1 credit         credits 0->1 message="Got 1 credit"
```

Pickup presentation is bounded:

```text
top-bar message = 1200 ms
white RGB565 ffff viewport-border flash = 500 ms
viewport = 0,20,160,80
border = 2 px / 944 pixels
snapshot = bounded
```

The final hardware run continued after multiple pickups with no gameplay failure.
Pickup sound playback and got-face presentation remain deferred.

## Movement hazard touch — hardware PASS

Movement-side `Game_touchTile(..., true)` now owns linked type 10/11 hazard
damage through the shared 52 B PlayerState without a new gameplay owner.
Recovered amounts are `pain(1,2)` for type 10 and `pain(10,10)` for type 11.

The real CYD crossed two independent type-10 flames:

```text
tile=613 sprite=74 rawDamage=1+2 hp=30->29 armor=8->6
tile=616 sprite=110 rawDamage=1+2 hp=30->29 armor=6->4
```

Both produced the bounded dynamic message `"3 damage!"` plus the recovered red
viewport-border flash:

```text
color565 = b800
duration = 500 ms
viewport = 0,20,160,80
border = 2 px / 944 pixels
text duration = 1200 ms
```

Representative expiry witnesses were 513/523 ms for the red border and
1205/1214 ms for the text lease. The same session continued through a door,
a Health Vial pickup and a second flame, then scheduled the normal MOVE monster
turn after each committed hazard touch.

Mixed resource+hazard tiles, familiar redirection, lethal player transition,
PASS_TURN current-tile type10/11 touch, secondary burn text, pain face/shake and
sound remain intentionally fail-closed or deferred.

A Hellhound PASS_TURN hit on this exact firmware also confirms monster retaliation
damage presentation is still separate: PlayerState damage commits, while the log
explicitly reports `painFX=deferred damageText=deferred`. No red border or damage
message is claimed for monster attacks yet.

## Adaptive native plane cache — hardware PASS

A real dialog continuation exposed a memory-pressure failure after allocating a
2408 B topology rollback snapshot. The floor/ceiling renderer previously required
all six independent 2048 B texture leases and failed the whole frame if any lease
could not be acquired.

The permanent bounded policy is now:

```text
max slots = 6
slot bytes = 2048 B
minimum usable slots = 1
fewer slots = more PAK misses/reads, not different pixels
```

The final real CYD exercised the degraded path repeatedly:

```text
[NATIVEPLANE] CACHE-FALLBACK slots=5/6 leaseBytes=2048 totalLeaseBytes=10240
[NATIVEPLANE] rows=80 pixels=12800 textures=12 ...
[PLANEPROFILE] ... ok=1
```

This occurred during jammed-door destruction, weapon cycling, Pistol combat,
movement and later pickups. The player reported no recurrence of the soldier
dialog crash on the final firmware.

## Generic native weapon control — hardware PASS

The allocation-free circular selector uses the shared PlayerState and the exact
legacy usability gate `owned && (ammoUsage == 0 || ammo[ammoType] > 0)`.
Weapon cycling never advances the turn and redraw failure rolls PlayerState back.

Earlier NEXT hardware witness remains valid. The current run independently adds
PREV:

```text
[WEAPONCONTROL] COMMIT seq=86 action=PREV_WEAPON
  weapon=1->0 weapons=0007 redraw=yes turn=no rollback=closed
[WEAPONCONTROL] COMMIT seq=87 action=PREV_WEAPON
  weapon=0->2 weapons=0007 ammoType=1 ammo=12 redraw=yes turn=no rollback=closed
```

Direct generic single-target combat currently owns Axe, Pistol, Shotgun and Super
Shotgun. Chaingun/Plasma multi-loop and Rocket/BFG radial behavior remain separate
families.

## Combat regression chain at locked boundary — hardware PASS

The final run crossed the RNG refill boundary and killed Hellhound sprite 179 with
the Pistol while the plane cache was already degraded to 5/6:

```text
[ACTIONENGINE] TRACE seq=88 weapon=2 distance=3 tile=750 target=sprite index=179
[RNGGUARD] REFILL refill=1 ... hiddenGenerator=advanced-once rollbackReplay=armed
[MONSTERCOMBAT] ROLL seq=88
  firstRandHit=245 firstRandDamage=79
  totalDamage=5 armorDamage=3 crit=0 rngCalls=4
[NATIVEPLANE] CACHE-FALLBACK slots=5/6 ...
[PLANEPROFILE] ... ok=1
[WEAPON] DRAW weapon=2 logical=242 actual=611 frame=1 pose=attack
[MONSTERCOMBAT] COMMIT seq=88 sprite=179
  hp=6->0 armor=2->0 alive=1->0 ammo=12->11
  xp=5-applied rollback=closed
```

The next world frame also rendered successfully, the player moved, and the same
session later consumed a credit. This closes the earlier renderer/stack regression
for this tested path.

The jammed-door Axe route was independently healthy in the same run:

```text
[DESTRUCTIBLE] HIT seq=84 ... open=0->1 rngConsumed=1
[PLANEPROFILE] ... ok=1
[WEAPON] DRAW weapon=0 ... pose=attack
[DESTRUCTIBLE] COMMIT seq=84 ... message="Door cleared!" rollback=closed
[ACTIONENGINE] ATTACK seq=84 weapon=0 ... worldCommitted=yes
```

Historical jammed-door +1 XP and extinguisher +2 XP remain deferred PlayerState
consequences.

## Monster position / turn / movement ownership retained

Permanent position owner:

```text
record = {spriteIndex,tileIndex,worldX,worldY}
recordBytes = 8
Entrance records = 30
payload = 240 B
initial source = native topology tile center
```

The movement planner retains exact legacy ordering/masks and speculative RNG
semantics. A successful unambiguous ordinary-monster move can commit one
MonsterPosition record, SpriteTopology relink and exact gameplay RNG bytes, then
redraw the complete native frame. Renderer projection currently snaps to the
destination; interpolation remains deferred.

PASS_TURN and committed MOVE/ROTATE/PLAYER_ATTACK can schedule the bounded native
monster-turn path. Multiple ambiguous attackers and unowned activation ordering
still fail closed.

## Representative final RAM witness

Final real-CYD `ALIVE` lines at the locked boundary repeatedly reported:

```text
heap = 82516 B
heap8 = 16784 B
largest8 = 14324 B
SD/ZIP/VIDEO/CORE/LAYOUT/PRERENDER/RENDER/MAPPINGS/MENUBSP = ready
```

The submitted excerpt does not reprint `shapeData` or `mediaTexels`; their NULL
values remain hard project invariants and retained earlier regression witnesses,
not a newly claimed pointer-value observation from this log.

## Intentionally deferred families

```text
PASS_TURN current-tile type10/11 Entity_touched semantics
pickup sound playback / got-face presentation
combat/retaliation MISS/HIT/CRIT text feedback + monster-hit red flash
movement-hazard secondary burn text / pain face / shake / sound
action XP migration: extinguisher +2, jammed door +1
materialized monster drops
corpse-pile trimming
monster movement interpolation/animation
multiple-monster activation/movement ordering
unsupported special calcPath plane corpus
special subtype-10 AI
player lethal/death retaliation transition
monster attack/player-pain animation and FX
actual sound playback
chaingun/plasma multi-loop presentation/commit
rocket/BFG radius damage
familiar weapon attack semantics for slots 9..11
special death consequences for subtypes 7, 8, 12, 13
Kronos-specific semantics
password input
SAVEGAME / CHANGEMAP production route
```

These are mechanical family boundaries, never item-by-item or monster-by-monster
implementation ladders.

## CHANGEMAP remains deferred

Entrance event 1 / tile 69 remains recovered but intentionally not live:

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
2. re-read this file, `DOCUMENTATION.md` and the latest relevant milestone(s);
3. create a fresh coherent `agent/*` branch from that exact SHA;
4. choose one bounded gameplay family from the merged frontier.

Strong candidates include:

```text
chaingun/plasma multi-loop mechanics
rocket/BFG radial damage
multiple-monster activation + movement ordering
monster movement interpolation / animation
monster attack + player-pain presentation / combat feedback
player lethal/death transition
action XP migration into PlayerState
materialized monster drops
```

Choose only after re-reading merged `main`.

## Development workflow

```text
recover true main + docs
 -> choose one bounded behavior FAMILY
 -> recover exact legacy behavior
 -> design small permanent native owner/API
 -> keep genuinely different families fail-closed
 -> commit/push agent/*
 -> test normal esp32-cyd on real CYD
 -> Serial is truth
 -> fix failures directly
 -> after PASS, docs-only tail
 -> verify post-test commits are docs-only
 -> merge-ready
```

Never merge into `main` without explicit user request.
