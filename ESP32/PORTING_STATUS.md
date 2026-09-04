# Doom RPG ESP32 CYD porting status

Authoritative recovery/status file for the classic ESP32-2432S028R port.
Repository state wins over chat history. Serial logs from the real classic CYD
are the final runtime authority.

## Git boundary — LOCKED milestone

```text
main = 4b95d382ab9b120dcd7e020d4614a48d01001d1c
branch = agent/esp32-native-monster-pain-feedback
base main = 4b95d382ab9b120dcd7e020d4614a48d01001d1c
hardware-tested code boundary = b7bf6bb692f5987f9307a7c02a42601fcf3232e1
status = REAL-CYD MONSTER RETALIATION PLAYER-PAIN FEEDBACK + DIALOG PAK-LEASE FIX PASS
branch policy = LOCKED; docs-only tail only
```

`b7bf6bb6...` is the exact code boundary exercised on the real CYD. Commits
after that SHA must remain documentation-only until merge.

Normal GitHub Actions `esp32-cyd` run `33854099003` completed successfully for
this exact SHA and uploaded
`doom-rpg-esp32-cyd-b7bf6bb692f5987f9307a7c02a42601fcf3232e1`.
CI is a compile/link gate only; the hardware logs remain authoritative.

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

The global cache-reset/performance cliff remains separate work. Preserve this
baseline while correctness work advances.

## Current hardware-owned gameplay frontier

Validated behavior retained at the locked boundary includes:

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
live nonlethal monster-retaliation raw damage text + red viewport flash
feedback expiry safely deferred while a native dialog owns the PAK
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
- [`MILESTONE_NATIVE_MONSTER_PAIN_FEEDBACK.md`](MILESTONE_NATIVE_MONSTER_PAIN_FEEDBACK.md)

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

Combat, pickups, HUD, ammo, progression, hazards and monster retaliation share
this owner. Do not create per-feature or per-item state islands.

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

Pickup presentation is bounded:

```text
top-bar message = 1200 ms
white RGB565 ffff viewport-border flash = 500 ms
viewport = 0,20,160,80
border = 2 px / 944 pixels
snapshot = bounded
```

The final boundary independently revalidated:

```text
Armor Shard: armor 5->9, message="Got Armor Shard"
Halon Can: ammo0 7->10, message="Got Halon Can"
```

Pickup sound playback and got-face presentation remain deferred.

## Movement hazard touch — hardware PASS

Movement-side `Game_touchTile(..., true)` owns linked type 10/11 hazard damage
through the shared PlayerState without a new gameplay owner.

```text
type10 = pain(1,2)
type11 = pain(10,10)
feedback text = total raw pair, 1200 ms
flash = red RGB565 b800 / 500 ms
viewport = 0,20,160,80
border = 2 px / 944 pixels
```

Earlier real-CYD type-10 witnesses remain canonical:

```text
tile=613 sprite=74 rawDamage=1+2 hp=30->29 armor=8->6
tile=616 sprite=110 rawDamage=1+2 hp=30->29 armor=6->4
message="3 damage!"
```

Mixed resource+hazard tiles, familiar redirection, lethal player transition,
PASS_TURN current-tile type10/11 touch, secondary burn text, pain face/shake and
sound remain intentionally fail-closed or deferred.

## Monster retaliation player-pain feedback — hardware PASS

Legacy `Player_pain(player, totalDamage, totalArmorDamage)` displays the raw pair
sum. Native retaliation now reuses the existing bounded ActionEngine DAMAGE
feedback owner after a nonlethal hit commits.

Final real-CYD witness on `b7bf6bb6...`:

```text
[MONSTERTURN] ATTACK-PROBE reason=PASS_TURN sprite=179
  firstRandHit=13 firstCalcHit=193 firstRandDamage=243
  totalDamage=3 armorDamage=3 crit=0
  playerHP=30->27 armor=8->5

[ACTIONFEEDBACK] PAINT kind=6 text="6 damage!" durationMs=1200
[VIEWFLASH] PAINT color565=b800 viewport=0,20,160,80
  thickness=2 pixels=944 durationMs=500

[MONSTERRETAL] COMMIT
  playerHP=30->27 armor=8->5
  message="6 damage!" damageTotal=6
  redFlash=b800/500ms
  passMessage=legacy-superseded
  rollback=closed
  attackVisual=deferred
  painFace=deferred
  shake=deferred
  sound=deferred
  playerDeath=fail-closed

[VIEWFLASH] EXPIRE elapsedMs=514 targetMs=500 color565=b800
```

The player physically observed the red border. Monster attack animation is still
explicitly deferred and its absence is expected at this boundary.

## Dialog / feedback PAK-owner conflict — hardware PASS

The first retaliation-feedback code commit
`18a76296d9739489cf7806e7dc7beb6a8bd09d1d` passed the new damage presentation,
but the same hardware session exposed a pre-existing lease conflict:

```text
pickup top-bar feedback still visible
 -> native dialog opens and owns PAK
 -> feedback reaches expiry
 -> top-bar painter tries to acquire PAK
 -> ACTIONFEEDBACK FAILED kind=0
 -> gameplay fatal
```

Final code boundary `b7bf6bb6...` adds only bounded `EspAssetPack_isOpen()`
guards: expired feedback waits while a native dialog owns the PAK and is cleaned
as soon as the dialog releases it. No new allocation or owner was introduced.

Exact final reproduction:

```text
[PLAYERRES] FEEDBACK tile=782 message="Got Halon Can"
[VIEWFLASH] EXPIRE elapsedMs=501 targetMs=500 color565=ffff
[DIALOG] OPEN event=83 cmd=1 resume=2 opcode=26 ... pack=open
# dialog remains active beyond the 1200 ms top-bar lease
# no ACTIONFEEDBACK FAILED / ACTIONENGINE FAILED
[DIALOG] FASTFORWARD pageStart=0 lines=3
[DIALOG] CLOSE event=83 resume=2 mode=resume ... packClosed=yes
[DIALOGCHAIN] RESUME event=83 start=2 handled=1 state=1 mutation=1
[RESIDENTGAMEPLAY] DIALOG-RESUME ... redraw=yes dialog=closed
[ACTIONFEEDBACK] EXPIRE kind=5 elapsedMs=2911 targetMs=1200 restored=topbar-only
[DIALOG] OPEN event=83 cmd=3 resume=4 opcode=8 ... pack=open
```

The second dialog proves the gameplay/dialog chain remains live after the delayed
feedback cleanup.

## Adaptive native plane cache — hardware PASS

The floor/ceiling renderer accepts any successful prefix of 1..6 independent
2048 B texture leases:

```text
max slots = 6
slot bytes = 2048 B
minimum usable slots = 1
fewer slots = more PAK misses/reads, not different pixels
```

The final run continued to exercise the 5/6 fallback in normal gameplay. During
dialog resume one transient frame reached 4/6:

```text
[NATIVEPLANE] CACHE-FALLBACK slots=4/6 leaseBytes=2048 totalLeaseBytes=8192
[NATIVEPLANE] ... reads=55296B
[PLANEPROFILE] us=230457 ok=1
```

The frame still rendered successfully. This is a performance signal, not a
correctness failure.

## Generic native weapon/combat regression — hardware PASS

Direct generic single-target combat currently owns Axe, Pistol, Shotgun and Super
Shotgun. Chaingun/Plasma multi-loop and Rocket/BFG radial behavior remain separate
families.

The final boundary independently killed Hellhound sprite 179 with the Axe:

```text
[MONSTERCOMBAT] ROLL seq=88 sprite=179 weapon=0
  totalDamage=7 armorDamage=1 crit=0 rngCalls=4
[WEAPON] DRAW weapon=0 ... frame=1 pose=attack
[MONSTERCOMBAT] COMMIT seq=88 sprite=179
  hp=6->0 armor=2->1 alive=1->0
  xp=5-applied level=1->1
  rollback=closed
[MONSTERCOMBAT] ATTACK seq=88 weapon=0 genericMonster=yes worldCommitted=yes
```

The player then moved through the former monster tile and continued to pickups and
dialogue, giving a useful cross-family regression chain.

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
semantics. A successful unambiguous ordinary-monster move commits one
MonsterPosition record, SpriteTopology relink and exact gameplay RNG bytes, then
redraws the complete native frame. Renderer projection currently snaps to the
destination; interpolation remains deferred.

PASS_TURN and committed MOVE/ROTATE/PLAYER_ATTACK can schedule the bounded native
monster-turn path. Multiple ambiguous attackers and unowned activation ordering
still fail closed.

## RAM witnesses

Normal gameplay before lazy NOTE/dialog allocation repeatedly reported:

```text
heap = 82516 B
heap8 = 16784 B
largest8 = 14324 B
SD/ZIP/VIDEO/CORE/LAYOUT/PRERENDER/RENDER/MAPPINGS/MENUBSP = ready
```

The computer interaction lazily allocated the existing NOTE owner:

```text
[NOTE] OWNER bytes=1416 allocation=lazy-gameplay
```

Post-dialog `ALIVE` then reported:

```text
heap = 81084 B
heap8 = 15352 B
largest8 = 13300 B
```

This single run does not establish a leak; the delta is consistent with the
explicit lazy owner allocation. `shapeData == NULL` and `mediaTexels == NULL`
remain hard project invariants and retained earlier regression witnesses.

## Performance observation to investigate after merge

The user reported a general impression that the final firmware felt slower.
Serial timing provides a real but not yet explained lead:

```text
previous comparable SPRITEPROFILE samples ~= 22-25 ms
final boundary comparable SPRITEPROFILE samples ~= 46-49 ms
VIDEO present remains ~= 34.4 ms
normal plane samples remain broadly ~= 79-109 ms
one dialog-resume 4/6 plane-cache frame = 230457 us
```

The final PAK-lease hotfix adds only two `EspAssetPack_isOpen()` guards and cannot
plausibly account for a broad rendering slowdown by itself. Do not claim a cause
from this sample. Treat sprite-time/cache/SD behavior as a separate performance
investigation and keep correctness milestones bounded.

## Intentionally deferred families

```text
PASS_TURN current-tile type10/11 Entity_touched semantics
pickup sound playback / got-face presentation
movement-hazard secondary burn text / pain face / shake / sound
action XP migration: extinguisher +2, jammed door +1
materialized monster drops
corpse-pile trimming
monster movement interpolation/animation
multiple-monster activation/movement ordering
unsupported special calcPath plane corpus
special subtype-10 AI
player lethal/death retaliation transition
monster attack visual / player-pain animation and FX
monster attack sound / actual sound playback
status-warning presentation
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
monster attack visual / player-pain animation family
generic type-12 destructible combat
player lethal/death transition
multiple-monster activation + movement ordering
monster movement interpolation / animation
PASS_TURN current-tile hazards
action XP migration into PlayerState
materialized monster drops
chaingun/plasma multi-loop mechanics
rocket/BFG radial damage
performance investigation: sprite profile + plane-cache fallback
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