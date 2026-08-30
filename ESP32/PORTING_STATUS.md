# Doom RPG ESP32 CYD porting status

Authoritative recovery/status file for the classic ESP32-2432S028R port.
Architecture belongs in [`ARCHITECTURE.md`](ARCHITECTURE.md); this file keeps
the current Git boundary, hardware facts, canonical resident witnesses and the
explicit fail-closed frontier.

Repository state wins over chat history. Serial logs from the real classic CYD
are the final runtime authority.

## Git boundary

Current merged baseline:

```text
main = 45d634449faa511dd02ab25ac5bb980fa4ef86b1
```

Current development branch:

```text
branch = agent/esp32-native-action-engine
base main = 45d634449faa511dd02ab25ac5bb980fa4ef86b1
hardware-tested final code HEAD = 1f88ca6488d78be181fc97b500beefbe0eb9a751
status = REAL-CYD HARDWARE PASS
merge-ready = YES, after documentation-only tail verification
```

The branch adds a bounded native Action/Combat presentation frontier without
broad-enabling legacy combat/world mutation. The permanent direction remains a
generic action/combat engine; crates, fire and monsters must not become separate
special-case engines.

## Permanent rule

```text
A NEW BSP IS NOT A NEW ENGINE.
```

Production map path:

```text
/DoomRPG-ESP32.pak
 -> EspBspReader inventory
 -> EspMapResidentLifecycle
 -> compact immutable EspMapRuntime
 -> explicit compact mutable owners
 -> EspPlayerView
 -> EspNativeGameplaySession
 -> generic renderer / HUD / input / events / dialog / action
```

No future level should create another `native_mapN_*` ladder or level-specific
renderer.

## Hardware / memory invariants

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

Migrated world data must remain compact/offset-based. Do not reintroduce map-wide
shape/media texel pools or a desktop Entity pointer graph.

## Entrance canonical resident witness

Entrance remains the hardware corpus for initial new-game startup, not a
specialized engine path.

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
spawn position = 544,1824,36
oldZ = 4
```

Owner fingerprints:

```text
mapStateFNV  = cd99b98e
scriptFNV    = f9e3d9df
lineFNV      = e5e74861
textureFNV   = f1fc1875
automapFNV   = 669b1aa7
topologyFNV  = 3f321e43
```

Cardinalities:

```text
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

These values are regression witnesses only. Production behavior must never
select an implementation based on them.

## Generic session baseline

Current initial session canon:

```text
targetMapId = 1
gameplayLoadMapId = 1
angle = 64
graphics textures = 33
graphics sprites = 45 -> 46 after dependency closure
catalog storage = 3120 B
catalog FNV = 29ffc14a
initial world frame = 71ca7465
initial walls = 8 / 4430 pixels
HUD hp = 30/30
HUD armor = 0/20
HUD weapon = 2
HUD ammo = 8
```

Resident render-cache stable owner/capacity canon:

```text
owner = 21160 B
payload = 16384 B
large entries after learn = 2
```

## Native Action engine hardware boundary

The real CYD validated the bounded Action engine on Entrance.

### Empty Action / feedback lifetime

An Action into empty space routes to the native top-bar message without world
mutation or full redraw:

```text
[ACTIONENGINE] ROUTE seq=1 weapon=2 target=none distance=0 route=NOTHING_TO_USE feedback=screen turnAdvance=deferred
[ACTIONFEEDBACK] PAINT kind=1 text="Nothing to use" chars=14 reads=36 bytes=10792 present=caller durationMs=1200
[ACTIONENGINE] PRESENT seq=1 route=NOTHING_TO_USE worldMutation=no fullRedraw=no feedback=yes
[ACTIONFEEDBACK] CLEAR mode=topbar-only reads=22 bytes=358 present=caller
[ACTIONFEEDBACK] EXPIRE kind=1 elapsedMs=1203 targetMs=1200 restored=topbar-only
```

A second real-CYD witness expired at 1204 ms. The feedback lease therefore
matches the recovered legacy `MSG_DISPLAY_TIME = 1200 ms` contract.

A prior implementation cancelled the lease on any unrelated `Present()`, which
could strand painted text indefinitely. That is fixed: external presents do not
implicitly cancel a visible Action message.

### Touch-feedback arbitration

A real-CYD run exposed a second race: if Action feedback expired while the
120 ms touch-control flash still owned its exact framebuffer snapshot, the
message clear changed the frame underneath that snapshot and the strict restore
correctly failed with:

```text
[RESIDENTGAMEPLAY] FAILED reason=touch-feedback-restore
```

Final code HEAD `1f88ca6488d78be181fc97b500beefbe0eb9a751`
defers Action-message expiry while `EspNativeGameplayControls_isActive()`.
The user subsequently reported the interaction working on hardware. The strict
control snapshot invariant remains intact; it was not weakened.

### Destructible targeting

Crates/destructibles are found generically by the recovered Action trace, but the
combat consequence is intentionally fail-closed because native entity health,
weapon-mask consequence ownership and generic destruction are not yet owned:

```text
[ACTIONENGINE] TRACE ... weapon=2 distance=1 ... type=12 subtype=2 route=DESTRUCTIBLE_COMBAT_DEFERRED
[ACTIONENGINE] BACKEND-DEFER ... family=destructible-combat reason=entity-parm-weapon-mask+subtype-consequence-not-owned mutation=no
```

No crate-specific implementation is permitted. The later generic combat backend
must own animation, hit/damage, destruction, ammo, sound and turn consequences.

### Fire / extinguisher action

Adjacent fire with weapon 1 is now a real native mutation plus generic weapon
presentation. Hardware witness:

```text
[ACTIONENGINE] TRACE seq=102 weapon=1 distance=1 tile=613 target=sprite index=74 line=65535 type=10 subtype=0 route=FIRE_CLEARED
[ACTIONENGINE] COMMIT seq=102 sprite=74 effect=fire-remove overlayBytes=128 xp=2-deferred ammoUsage=1-deferred sound=5045-deferred attackFrame=pending redraw=pending rollback=yes
[WEAPON] DRAW weapon=1 logical=241 actual=607 frame=1 pose=attack ... cache=miss reads=10
[ACTIONENGINE] FRAME seq=102 route=FIRE_CLEARED phase=attack ... presented=1
[WEAPON] DRAW weapon=1 logical=241 actual=606 frame=0 pose=idle ... cache=miss reads=10
[ACTIONENGINE] FRAME seq=102 route=FIRE_CLEARED phase=settle-idle ... presented=1
[ACTIONENGINE] ATTACK seq=102 weapon=1 frame=1->0 generic=yes worldCommitted=yes
[ACTIONFEEDBACK] EXPIRE kind=2 elapsedMs=1204 targetMs=1200 restored=topbar-only
```

This materially validates the permanent generic weapon-frame route:

```text
logical weapon sprite = 240 + weapon
frame 0 = idle
frame 1 = attack
cache key = weapon + animation frame
```

The fire removal remains a compact 128 B consumed/removed overlay with rollback
on the first render failure. XP, ammo and sound remain deferred.

### Extinguisher range: current safe divergence

The legacy desktop `SELECT` trace can see targets up to eight tiles away and will
still call `Player_fireWeapon()` for a fire when the extinguisher is selected.
The extinguisher has `rangeMin = 0`; `CombatEntity_calcHit()` therefore misses
when squared world distance exceeds 4096 (more than one cardinal tile). The
legacy combat path then presents the attack and reports `No effect!`.

The current ESP32 action frontier does not yet own generic miss/ammo/turn combat.
Therefore final code HEAD intentionally fails closed for fire at `distance > 1`:

```text
route = FIRE_RANGE_DEFERRED
mutation = no
attack presentation = deferred
```

This is a known temporary behavior difference, not the final Doom RPG contract.
When generic combat/miss ownership lands, remote extinguisher attempts should
animate and resolve to the recovered no-effect behavior rather than removing the
fire.

## Dynamic door texture hardware boundary

The mutable line-texture variant now reaches the renderer without mutating the
immutable runtime. The first implementation recursively re-entered
`EspMapRuntime_getLine()` through its linker wrapper and overflowed the loopTask
stack; the final implementation resolves the 9/10 texture bit directly from the
already-owned mutable line-texture view.

Real-CYD door animation after unlock:

```text
[DOORANIM] FRAME 1/4 ... textureVariants=2 ... render=ok
[DYNAMICLINES] FRAME ... textureVariants=2 render=ok immutableRuntime=yes
...
[DOORANIM] FRAME 4/4 ... textureVariants=2 ... render=ok
[DOORANIM] COMPLETE transitions=1 frames=4 state=stable transaction=committed
```

The user confirmed the previously red lock indicator becomes green after the
soldier unlocks the door.

Permanent wrapper rule:

```text
A linker wrapper must not call a higher-level API that can indirectly re-enter
that wrapped symbol. Use __real_*, an already materialized owner/view, or an
explicitly non-reentrant helper.
```

## Renderer performance evidence

The current correctness frontier is hardware-valid, but the loaded first-fire
room is visibly slow. Serial timings identify the dominant cost; do not guess.

Adjacent-fire attack frame:

```text
worldUs   = 178264
spriteUs  = 544735
hudUs     = 1304
presentUs = 46783
totalUs   = 843825
spriteReads = 100
```

Settle-to-idle frame:

```text
worldUs   = 177817
spriteUs  = 544602
hudUs     = 1276
presentUs = 46678
totalUs   = 842463
spriteReads = 100
```

A move in the same loaded area reached:

```text
[RESIDENTGAMEPLAY] FRAME ... totalUs=832308 presented=1
```

Door animation in the same region reached roughly 1.10 s for the final
`SELECT-DOOR` frame sequence.

Conclusion:

```text
primary hotspot = sprite rendering / sprite asset reads
secondary cost  = world render
small cost       = 160x120 -> 320x240 presentation (~34-47 ms)
```

Do **not** optimize `PlatformVideo_present()` first. A dedicated bounded sprite
renderer/cache milestone should profile why a loaded scene still incurs about
100 sprite pack reads and ~545 ms sprite time per full frame.

## Production gameplay boundary

Hardware-proven native gameplay includes:

```text
TURN_LEFT / TURN_RIGHT
FORWARD / BACK / STRAFE
native topology/entity/line collision
dynamic line/door collision
SELECT front-tile provenance
EV_OPENLINE / EV_CLOSELINE
MOVE source EXIT + destination ENTER bounded event routes
regular-door bounded visual interpolation
mutable line texture variants visible in native renderer
EV_DIALOG / EV_DIALOGNOBACK presentation
progressive dialog / paging / fast-forward / close
saved dialog continuation transaction
EV_SHOW / EV_HIDE / EV_UNLOCK continuation
state ops 11 / 19 / 20
EV_FORCEMESSAGE top-bar owner/painter
EV_NOTE bounded prefix before dialog
native idle first-person weapon painting
generic native weapon attack frame 1 -> idle frame 0 presentation
eType=5 weapon consumed-bit/ownership/auto-select overlay
empty/human Action feedback with bounded 1200 ms top-bar lifetime
adjacent extinguisher fire removal with rollback-safe presentation
resident opcode/pickup/action diagnostics
```

The engine still does **not** broad-enable legacy `Game_executeEvent()` or
legacy `Combat_performAttack()` as a permanent ESP32 backend.

## Event frontier

Known Entrance opcode IDs:

```text
2, 7, 8, 9, 10, 11, 13, 15, 16, 18, 19, 24, 26, 27, 40, 41
```

Currently production-bounded:

```text
7  EV_SHOW
8  EV_DIALOG
11 EV_CHANGESTATE
13 EV_UNLOCK
15 EV_OPENLINE
16 EV_CLOSELINE
18 EV_HIDE
19 EV_NEXTSTATE
20 EV_PREVSTATE
24 EV_FORCEMESSAGE
26 EV_DIALOGNOBACK
40 EV_NOTE
```

Still intentionally deferred/fail-closed:

```text
2  EV_CHANGEMAP  -> live transition consumer pending
9  EV_GIVEMAP    -> automap production route pending
10 EV_PASSWORD   -> password input UI pending
27 EV_SAVEGAME   -> save consumer pending
41 EV_CHECK_KEY  -> native player-key owner pending
```

`EspMapOpcodeExecutor` itself remains intentionally limited to 11/19/20; other
owned families use their dedicated native semantic modules.

## Pickup frontier

Current production pickup owner:

```text
eType=5 weapon
world remove = consumed-sprite bit overlay
ownership = native uint16 weapon mask
new weapon select = native HUD overlay
scope = current map/runtime arena
rollback = exact on redraw failure
```

Real-CYD extinguisher pickup witness:

```text
[PICKUP] WEAPON tile=643 sprite=50 subtype=1 new=yes weapons=0004->0006 selected=2->1 worldRemove=overlay ammoOwner=deferred legacyDialog=deferred
[WEAPON] DRAW weapon=1 logical=241 actual=606 frame=0 pose=idle ... cache=miss reads=10
[PICKUP] FRAME reason=WEAPON-PICKUP ... presented=1
```

Deferred:

```text
weapon ammo increment / acquisition feedback / sound
eType=3 world/player-stat item (armor/health-style pickups)
eType=4 inventory item
eType=6 ammo
eType=16 alternate ammo
```

The user physically walked over two armor helmets and they were not consumed.
That is expected at this frontier: player-stat pickup ownership has not been
implemented yet and must not be patched as a helmet-specific special case.

## Build/include guardrails

The recovered legacy headers contain the historical C declaration:

```c
typedef enum { false, true } boolean;
```

Never solve an ESP-IDF include problem by shadowing a framework header under
`ESP32/include`. A temporary `ESP32/include/esp_timer.h` shim once intercepted
Arduino/FreeRTOS C++ include chains and broke unrelated translation units.

Prefer existing project clocks such as `DoomRPG_GetUpTimeMS()` when appropriate,
or add a narrowly project-named adapter. Do not globally intercept framework
headers.

## Next bounded direction

Correctness work on this branch is hardware-pass. The strongest next performance
candidate is now evidence-driven:

```text
sprite renderer / resident sprite-asset cache audit
```

Goal for that milestone should be bounded and measurable: explain and reduce the
~100 sprite pack reads / ~545 ms sprite phase seen in the loaded fire room while
preserving the compact resident owner, no-PSRAM target and exact visual output.

Separately, gameplay correctness still needs generic owners for:

```text
player-stat pickups
ammo/inventory pickups
generic monster/destructible combat
combat miss / no-effect / ammo / sound / turn consequences
```

After this branch merges, recover the new exact GitHub `main` SHA before starting
another `agent/*` branch.
