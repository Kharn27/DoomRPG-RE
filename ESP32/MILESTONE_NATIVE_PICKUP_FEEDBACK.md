# Milestone — Native pickup feedback and renderer pressure recovery

## Git / hardware boundary

```text
base main = 782bee100bad1169b85cc738c3a860e367e81553
branch = agent/esp32-native-pickup-feedback
hardware-tested code boundary = c9df4d452eae2610701fde839b7aa73cdceda0ac
status = REAL-CYD PASS
branch policy = LOCKED; docs-only tail only
```

`c9df4d45...` is the exact branch code exercised on the real classic CYD.
Commits after this boundary must remain documentation-only until merge.

Normal GitHub Actions `esp32-cyd` build also completed successfully for this SHA.
CI is only the compile/link gate; the serial witnesses below are the runtime
authority.

## Goal

Complete the already-generic native player-resource route with compact live pickup
feedback while preserving the shared PlayerState and consumed-sprite ownership.
The same hardware run also exposed and closed a renderer-memory-pressure failure
that occurred after a dialog continuation allocated its topology rollback
snapshot.

Permanent flow remains:

```text
EntityDef {tile,type,subtype,parm}
 -> generic PlayerResources classifier
 -> shared 52 B PlayerState
 -> consumed-sprite bitset
 -> bounded top-bar pickup message + bounded white viewport flash
 -> native full redraw / rollback
```

No item-specific owner or map-wide media allocation was introduced.

## Live pickup feedback — hardware PASS

The pickup path now publishes the resolved item/resource message through the
existing bounded action-feedback owner and adds the recovered white pickup flash:

```text
message owner = existing top-bar action feedback
message duration = 1200 ms
flash = white RGB565 ffff viewport border
flash viewport = 0,20,160,80
flash thickness = 2 px
flash pixels = 944
flash duration = 500 ms
snapshot = bounded
presentation = caller-owned
```

Real-CYD witnesses from Entrance include two separate armor shards, ammo, a
weapon and credits through the same generic resource engine:

```text
Armor shard #1:
  tile=839 sprite=99 type=3 subtype=21 parm=4
  armor=0->4
  message="Got Armor Shard"

Armor shard #2:
  tile=838 sprite=86 type=3 subtype=21 parm=4
  armor=4->8
  message="Got Armor Shard"

Bullet Clip:
  tile=675 sprite=51 type=6 subtype=1 parm=4
  ammo1=8->12
  message="Got Bullet Clip"

Fire Ext:
  tile=643 sprite=50 type=5 subtype=1 parm=10
  selected weapon=1, weapons=0006
  message="Got Fire Ext"

Credit:
  tile=783 sprite=191 type=3 subtype=22 parm=1
  credits=0->1
  message="Got 1 credit"
```

Representative feedback chain:

```text
[ACTIONFEEDBACK] PAINT kind=5 text="Got 1 credit" ... durationMs=1200
[PICKUPFLASH] PAINT color=white565/ffff viewport=0,20,160,80 thickness=2 pixels=944 durationMs=500 snapshot=bounded
[PLAYERRES] COMMIT ... credits=1 ... message=pickup-live flash=white-500ms ... rollback=closed
[PICKUPFLASH] EXPIRE elapsedMs=500 targetMs=500 restored=viewport-border-only
[ACTIONFEEDBACK] EXPIRE kind=5 elapsedMs=1202 targetMs=1200 restored=topbar-only
```

The user physically observed the pickups disappear and reported continued stable
gameplay after collecting multiple resources.

## Dialog-continuation memory-pressure failure

Before the final boundary, closing the soldier dialog reached a real native
continuation and allocated the compact topology rollback snapshot:

```text
[DIALOGCHAIN] TOPOLOGY-SNAPSHOT ownerBytes=2408 allocation=lazy-gameplay
[DIALOGCHAIN] RESUME event=60 start=1 handled=3 show=1 hide=0 unlock=0 state=2 removed=1 mutation=1 topologySnapshot=2408B
```

The subsequent full redraw then failed only in floor/ceiling reconstruction:

```text
[NATIVEPLANE] FAILED textured floor/ceiling reconstruction
[PLANEPROFILE] ... ok=0
[RESIDENTGAMEPLAY] FAILED reason=dialog-resume-render-rollback
```

The native plane renderer used six independent 2048 B texture-cache leases, but
its old initialization policy treated all six as mandatory. A single failed
lease discarded the already-acquired cache and failed the whole frame.

## Adaptive plane cache — hardware PASS

The cache remains bounded at six slots, but correctness now requires only one
2048 B slot. Under pressure it accepts the successfully acquired prefix rather
than failing the frame. Cache capacity therefore becomes a performance variable,
not a correctness dependency:

```text
maximum slots = 6
slot bytes = 2048 B
minimum usable slots = 1
pixel semantics = unchanged
reduced slots => additional PAK misses/reads only
```

The real CYD immediately exercised the fallback rather than merely taking the
six-slot fast path:

```text
[NATIVEPLANE] CACHE-FALLBACK slots=5/6 leaseBytes=2048 totalLeaseBytes=10240
[NATIVEPLANE] rows=80 pixels=12800 textures=12 cache=12789H/11M/6E reads=22528B
[PLANEPROFILE] us=79933 ok=1
```

The same `5/6` fallback remained healthy through jammed-door destruction, weapon
cycling, combat, movement and later pickups. This is the decisive proof that the
renderer now survives the post-gameplay allocation pressure instead of depending
on all six transient leases.

The player reported no recurrence of the soldier-dialog crash on the final
firmware.

## Regression chain after the fix

The final hardware run continued well beyond the original failure point.

### Jammed door / Axe

```text
[ACTIONENGINE] TRACE seq=84 weapon=0 ... route=JAMMED_DOOR_CLEARED
[DESTRUCTIBLE] HIT seq=84 ... open=0->1 rngConsumed=1
[NATIVEPLANE] CACHE-FALLBACK slots=5/6 ...
[PLANEPROFILE] ... ok=1
[WEAPON] DRAW weapon=0 ... frame=1 pose=attack
[DESTRUCTIBLE] COMMIT seq=84 ... open=0->1 message="Door cleared!" ... rollback=closed
[ACTIONENGINE] ATTACK seq=84 weapon=0 frame=1->0 generic=yes worldCommitted=yes
```

### PREV weapon control now independently hardware-exercised

The previous weapon-control milestone had only an independent NEXT witness. This
run adds real PREV execution through the same circular selector:

```text
[WEAPONCONTROL] COMMIT seq=86 action=PREV_WEAPON
  weapon=1->0 weapons=0007 ... redraw=yes turn=no rollback=closed
[WEAPONCONTROL] COMMIT seq=87 action=PREV_WEAPON
  weapon=0->2 weapons=0007 ammoType=1 ammo=12 ... redraw=yes turn=no rollback=closed
```

### Pistol combat after RNG refill and cache fallback

```text
[ACTIONENGINE] TRACE seq=88 weapon=2 distance=3 tile=750 target=sprite index=179 ...
[RNGGUARD] REFILL refill=1 ... hiddenGenerator=advanced-once rollbackReplay=armed
[MONSTERCOMBAT] ROLL seq=88 sprite=179 weapon=2 distance=3
  firstRandHit=245 firstRandDamage=79 totalDamage=5 armorDamage=3
  crit=0 rngCalls=4
[NATIVEPLANE] CACHE-FALLBACK slots=5/6 ...
[PLANEPROFILE] ... ok=1
[WEAPON] DRAW weapon=2 logical=242 actual=611 frame=1 pose=attack
[MONSTERCOMBAT] COMMIT seq=88 sprite=179
  hp=6->0 armor=2->0 alive=1->0 ammo=12->11
  xp=5-applied rollback=closed
```

The player then moved successfully and later collected a credit through the
resource/feedback route, proving the session remained live after the kill.

## Final RAM witness

Representative `ALIVE` lines from the final real-CYD run:

```text
heap = 82516 B
heap8 = 16784 B
largest8 = 14324 B
SD = ready
ZIP = ready
VIDEO = ready
CORE = ready
LAYOUT = ready
PRERENDER = ready
RENDER = ready
MAPPINGS = ready
MENUBSP = ready
```

The permanent project invariants remain `shapeData == NULL`, `mediaTexels == NULL`
and no PSRAM. This log excerpt does not independently reprint those pointers, so
it is not used as a new pointer-value witness.

## Deferred boundaries

This milestone does not broaden into unrelated gameplay families. Still separate:

```text
pickup sound playback and got-face presentation
combat MISS/HIT/CRIT textual feedback
fire +2 XP and jammed-door +1 XP migration into PlayerState
materialized monster drops
corpse-pile trimming
monster movement interpolation/animation
multiple-monster activation/movement ordering
special subtype-10 AI
player lethal/death transition
monster attack/player-pain presentation
chaingun/plasma multi-loop mechanics
rocket/BFG radial damage
familiar slots 9..11 attack semantics
special death consequences / Kronos-specific semantics
SAVEGAME / CHANGEMAP production route
```

## Merge rule

The code boundary is locked at
`c9df4d452eae2610701fde839b7aa73cdceda0ac`. Only documentation may follow on
this branch. After merge, recover the exact new GitHub `main` SHA before creating
the next `agent/*` branch.
