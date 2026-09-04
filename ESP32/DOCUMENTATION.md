# ESP32 documentation map

Recovery and development should start from:

1. current GitHub `main` and its exact SHA;
2. [`PORTING_STATUS.md`](PORTING_STATUS.md) — authoritative tested/candidate boundary;
3. [`ARCHITECTURE.md`](ARCHITECTURE.md) — permanent native engine design;
4. this file — build/layout/recovery pointers;
5. latest relevant milestone/source on the active branch.

Repository state wins over chat history. Serial logs from the real classic CYD
are the final runtime truth.

## Current locked branch

```text
main = 4b95d382ab9b120dcd7e020d4614a48d01001d1c
branch = agent/esp32-native-monster-pain-feedback
base main = 4b95d382ab9b120dcd7e020d4614a48d01001d1c
hardware-tested code boundary = b7bf6bb692f5987f9307a7c02a42601fcf3232e1
status = monster retaliation player-pain feedback + dialog PAK-lease fix PASS
branch policy = LOCKED; docs-only tail only
```

Do not treat commits after `b7bf6bb6...` as new hardware-tested code. The tail
must remain documentation-only until merge.

Normal GitHub Actions `esp32-cyd` run `33854099003` passed on the exact tested
SHA. CI is compile/link evidence only; real-CYD serial logs remain authoritative.

## Build environment

Normal hardware reference:

```text
pio run -e esp32-cyd
```

GitHub Actions builds this environment through `.github/workflows/esp32-cyd.yml`
and uploads firmware artifacts. Bring-up diagnostics perturb RAM and are not the
production memory canon. Never claim a local build or hardware pass that did not
occur.

## Hardware / permanent memory rules

```text
classic CYD = ESP32-2432S028R
MCU = ESP32-D0WD-V3 dual core 240 MHz
flash = 4 MB
PSRAM = none
logical framebuffer = 160x120 RGB565 = 38400 B
shapeData == NULL
mediaTexels == NULL
native backing store = /DoomRPG-ESP32.pak
```

Do not recreate map-wide texel ownership or migrate native gameplay/map data back
to ZIP. The current firmware still has a transitional `/DoomRPG.zip` startup
dependency for legacy HUD/layout resources; removal remains migration debt.

## Selected resident-cache baseline

```text
owner = 23592 B
payload = 19456 B (19 KiB)
range records = 288
range record = 12 B
resident entry slots = 24
large exact range = 2048 B
```

Cache recycle/stall behavior remains separate performance work.

## Current native gameplay frontier

Hardware-owned behavior now includes:

```text
movement / turn / strafe
native collision + compact sprite topology
event-first SELECT routing
SHOW / HIDE / UNLOCK
OPENLINE / CLOSELINE
DIALOG / DIALOGNOBACK
FORCEMESSAGE / NOTE
state ops 11 / 19 / 20
regular door animation + mutable line texture variants
native idle weapon rendering + attack frame presentation
jammed-door subtype-3 Axe destruction + traversal
generic compact monster-state initialization
generic type-1 player attack hit/miss/crit/HP/armor math
pain / corpse / overkill-gib presentation
shared 52 B PlayerState + XP/progression state
generic type 3/4/5/6/16 pickup/resource engine
consumed-pickup world removal
HUD projection from PlayerState
extinguisher ammo consumption + fire removal transaction
stationary monster-turn scheduling + LOS recovery
live nonlethal monster retaliation
native PASS_TURN + exact "Turn passed." top-bar feedback
transaction-safe gameplay RNG replay across 128-byte refill boundary
compact mutable MonsterPosition owner
legacy-compatible bounded movement planner
persistent movement RNG reservation/replay
live one-monster movement commit + topology relink
renderer projection of committed moved monster position
generic NEXT + PREV weapon cycling
live selected-weapon HUD/first-person redraw without turn advance
live Pistol ammo consumption + generic monster combat commit
live generic pickup messages + white viewport-border flash
adaptive native plane cache under memory pressure
movement-side linked type10/type11 hazard damage through shared PlayerState
live bounded `N damage!` top-bar text + red viewport-border hazard flash
live monster-retaliation raw damage text + red viewport-border flash
safe feedback expiry while a native dialog owns the PAK
```

Relevant milestone records:

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

## Shared PlayerState

`EspNativeGameplayPlayerState` remains the single 52 B player-facing owner for:

```text
HP / max HP
armor / max armor
defense / strength / agility / accuracy
XP / level / next-level XP
keys / credits
ammo[6]
inventory[5]
weapon ownership / selected weapon
```

Player attacks, resources, HUD projection, hazards and monster retaliation all use
this same owner.

## Generic player resources and feedback

```text
EntityDef {tile,type,subtype,parm}
 -> generic PlayerResources classifier
 -> shared PlayerState
 -> one consumed-sprite bitset
 -> HUD/world projection
 -> bounded pickup feedback
 -> transactional redraw
```

Entrance corpus remains:

```text
114 pickups total
type3  = 84
type4  = 6
type5  = 3
type6  = 17
type16 = 4
consumed bitset = 43 B for 344 sprites
EntityDef metadata = 115 x 8 B = 920 B
```

Pickup presentation contract:

```text
message = top bar / 1200 ms
pickup flash = white RGB565 ffff / 500 ms
flash viewport = 0,20,160,80
border = 2 px / 944 pixels
snapshot = bounded
```

Final-boundary witnesses include an Armor Shard (`armor 5->9`) and Halon Can
(`ammo0 7->10`). Pickup sound playback and got-face presentation remain deferred.

## Movement hazard touch

Committed movement owns the type10/type11 subset of legacy
`Game_touchTile(..., true)` before monster-turn continuation.

```text
type10 = pain(1,2)
type11 = pain(10,10)
feedback = bounded dynamic top-bar damage text / 1200 ms
flash = red RGB565 b800 / 500 ms
viewport = 0,20,160,80
border = 2 px / 944 pixels
```

Earlier real-CYD type10 witnesses remain:

```text
tile613: hp 30->29, armor 8->6, message="3 damage!"
tile616: hp 30->29, armor 6->4, message="3 damage!"
```

PASS_TURN current-tile hazards, secondary burn text, pain face, shake and sound
remain separate families.

## Monster retaliation player-pain feedback

Enemy-turn path now has a bounded presentation layer after nonlethal retaliation
commit:

```text
MonsterTurn probe
 -> exact retaliation RNG replay
 -> shared PlayerState pain mutation
 -> existing ActionEngine DAMAGE feedback
 -> top-bar raw damage text
 -> red viewport-border flash
 -> native redraw/present
```

Legacy-visible amount is:

```text
totalDamage + totalArmorDamage
```

Final real-CYD witness:

```text
Hellhound sprite=179
roll: totalDamage=3 armorDamage=3 crit=0
player: HP 30->27, armor 8->5
message="6 damage!"
red flash=b800 / 500 ms
pass message=legacy-superseded
rollback=closed
```

The user physically observed the red border. Attack visual, pain face, shake,
sound and player death remain deferred/fail-closed.

## Dialog / feedback PAK ownership

Native dialogs intentionally keep the PAK open across typewriter rendering. The
ActionEngine top-bar painter needs a short exclusive PAK lease. Hardware exposed
a failure when a pickup message reached its 1200 ms expiry while a dialog was
active.

The permanent rule is now:

```text
feedback expiry due
 + PAK currently owned
 -> defer repaint
 -> do not fail gameplay
 -> preserve visible lease state
 -> retry after bounded owner closes PAK
```

Final real-CYD reproduction:

```text
"Got Halon Can" feedback active
white flash expires at 501 ms
DIALOG OPEN event=83 opcode=26 pack=open
# stays open beyond 1200 ms feedback lease
# no ACTIONFEEDBACK FAILED / ACTIONENGINE FAILED
DIALOG CLOSE ... packClosed=yes
DIALOGCHAIN RESUME state=1 mutation=1
DIALOG-RESUME redraw=yes
ACTIONFEEDBACK EXPIRE kind=5 elapsedMs=2911 restored=topbar-only
next opcode-8 dialog opens successfully
```

This confirms the feedback lease waits rather than crashing or disappearing.

## Adaptive plane renderer cache

The floor/ceiling path uses independent 2048 B texture leases. Six slots remain
the performance ceiling, but any successful prefix from 1..6 is valid.

```text
max slots = 6
slot bytes = 2048 B
minimum valid slots = 1
reduced capacity = more PAK misses/reads only
```

The final hardware run repeatedly used 5/6 in normal gameplay and one
DIALOG-RESUME frame used 4/6 with `PLANEPROFILE us=230457 ok=1`. Reduced cache
capacity remains a performance concern, not a correctness failure.

## Generic monster engine

Ordinary monster differences are data (`subtype`, `mType`, randomized stats),
not executor code.

Player-attack side:

```text
MonsterTrace
 -> CombatMath
 -> MonsterState + PlayerState
 -> MonsterCombat transaction
 -> visual/liveness projection
```

Enemy-turn side:

```text
committed movement / rotation / player attack / PASS_TURN
 -> MonsterTurn schedule
 -> candidate + cardinal LOS recovery
 -> exact rollback probe
 -> conservative activation delivery
 -> MonsterRetaliation transaction
 -> PlayerState + damage feedback + redraw
```

Movement side:

```text
MonsterState + topology
 -> MonsterPosition {sprite,tile,x,y}
 -> movement trigger/path planner probe
 -> exact gameplay-RNG replay
 -> MonsterPosition commit
 -> SpriteTopology relink
 -> moved-position sprite projection
 -> complete native redraw/present
```

Projection currently snaps to destination; interpolation/animation remains a
future family.

## Final combat regression witness

The locked-boundary run independently killed Hellhound sprite 179 with the Axe:

```text
weapon=0 distance=1 tile=718 sprite=179
roll: totalDamage=7 armorDamage=1 crit=0 rngCalls=4
attack pose logical=240 actual=603 frame=1
commit: hp 6->0, armor 2->1, xp=5, rollback=closed
settle-idle rendered successfully
```

The player then moved through the tile, picked up Armor Shard and Halon Can, and
completed/resumed the computer dialog chain. This is the current cross-family
renderer/combat/dialog survival witness.

## Representative RAM

Normal gameplay before lazy NOTE/dialog allocation:

```text
heap = 82516 B
heap8 = 16784 B
largest8 = 14324 B
SD/ZIP/VIDEO/CORE/LAYOUT/PRERENDER/RENDER/MAPPINGS/MENUBSP = ready
```

Computer interaction explicitly allocates:

```text
[NOTE] OWNER bytes=1416 allocation=lazy-gameplay
```

Post-dialog sample:

```text
heap = 81084 B
heap8 = 15352 B
largest8 = 13300 B
```

This run does not prove a leak. `shapeData == NULL` and `mediaTexels == NULL`
remain hard invariants and retained earlier witnesses.

## Performance observation

The user reported that the final firmware felt generally slower. Do not dismiss
that report, but do not attribute it to the correctness hotfix without evidence.
The serial sample shows:

```text
previous comparable SPRITEPROFILE ~= 22-25 ms
final comparable SPRITEPROFILE    ~= 46-49 ms
VIDEO present remains             ~= 34.4 ms
normal NATIVEPLANE remains broadly ~= 79-109 ms
one dialog-resume 4/6 plane frame = 230457 us
```

The PAK-lease fix itself only adds two `EspAssetPack_isOpen()` guards. Investigate
sprite/cache/SD timing separately after merge if it remains reproducible.

## Current intentionally deferred families

```text
PASS_TURN current-tile type10/type11 Entity_touched semantics
pickup sound playback / got-face presentation
movement-hazard secondary burn text / pain face / shake / sound
action XP migration: extinguisher +2, jammed door +1
materialized monster drops
corpse-pile trimming
monster movement interpolation/animation
multiple-monster activation/movement ordering
unsupported special calcPath plane corpus
special subtype-10 AI
player lethal/death transition
monster attack visual / player-pain animation and FX
monster attack sound / actual sound playback
status-warning presentation
chaingun/plasma multi-loop mechanics
rocket/BFG radius damage
familiar weapon attack semantics for slots 9..11
generic type-12 destructible combat
special death consequences for subtypes 7, 8, 12, 13
Kronos-specific semantics
password input
SAVEGAME / CHANGEMAP production route
```

These are mechanical family boundaries, not individual monster/item TODOs.

## CHANGEMAP recovery point

Entrance event 1 / tile 69 remains recovered but intentionally deferred:

```text
SAVEGAME -> /junction.bsp, targetMapId 9, savePos 992,1888 angle 64
CHANGEMAP -> /junction.bsp, targetMapId 9, showStats 1, spawnParam 0
OPENLINE -> third eligible command
```

## After this merge

Do not continue code on this locked branch.

When the merge is announced:

1. read the true GitHub `main` and exact SHA;
2. re-read `PORTING_STATUS.md`, this file and latest relevant milestone(s);
3. create a fresh coherent `agent/*` branch from that SHA;
4. choose the next bounded gameplay family from the actual merged frontier.

High-value candidates now include monster attack visual/player-pain animation,
generic type-12 destructible combat, player lethal/death transition,
multiple-monster activation/movement ordering, monster interpolation/animation,
PASS_TURN hazards, action XP migration, materialized monster drops and a bounded
performance investigation if the sprite-time slowdown remains reproducible.

Decide only after reading merged `main`.

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

After a merge announcement, re-read actual GitHub `main`, record its exact SHA,
and create the next `agent/*` branch from that SHA. Never merge `main` without an
explicit user request.