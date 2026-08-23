# ESP32 native Junction player spawn/load projection milestone

Branch: `agent/esp32-native-junction-spawn`

Base merged `main`:

```text
PR   = #68 — native committed Junction transition
main = 00268a100c6662cb883f9a02d979b4f29eecbf12
```

Firmware candidate:

```text
08a3a29c5e4e4a64000fa12a877299bbb1e772a0
```

Status: **IMPLEMENTED; REAL-CYD HARDWARE VALIDATION PENDING**.

## Objective

PR #68 hardware-proved a real native map swap and deliberately parked with Junction resident:

```text
sourceMap=1 / Entrance
targetMap=9 / Junction
mapSwapCommitted=yes
junctionResident=yes
spawnParam=0 retained
spawnApplied=no
ST_PLAYING=no
```

This milestone owns the next bounded semantic layer: the placement writes that recovered `Game_spawnPlayer()` would perform for a fresh normal map load. It projects those writes into one small pointer-free native state without touching legacy `Game`, `Player`, `DoomCanvas`, `Render`, HUD, gameplay or presentation.

It intentionally stops before:

```text
DoomCanvas_checkFacingEntity()
Player_setup()
initial Game_executeTile()
ST_PLAYING
native gameplay rendering
```

## Recovered legacy spawn contract

Current `Game_spawnPlayer()` resolves the player position from one of two sources.

### Header fallback

When:

```text
game->spawnParam == 0
```

legacy uses:

```text
x     = render->mapSpawnIndex % 32
y     = render->mapSpawnIndex / 32
angle = render->mapSpawnDir
```

### Packed override

When `spawnParam != 0`:

```text
x     = spawnParam & 31
y     = (spawnParam >> 5) & 31
angle = (spawnParam >> 10) & 255
game->spawnParam = 0
```

The common placement writes are:

```text
viewX = destX = x * 64 + 32
viewY = destY = y * 64 + 32
viewZ = 36
viewAngle = destAngle = angle
render->viewZOld = 4
```

Then legacy proceeds to:

```text
DoomCanvas_checkFacingEntity()

if (!game->isLoaded):
    Player_setup()
    Game_executeTile(...)
```

Those follow-up side effects remain explicitly pending in this milestone.

## Recovered loadType contract

`DoomCanvas_loadMap(mapID)` starts the ordinary map-load flow but does not assign a new nonzero `loadType`.

In the loading state:

```text
loadType == 0
 -> normal map/media load path

loadType != 0
 -> Game_loadState(loadType)
```

`DoomCanvas_loadState()` is the path that establishes a nonzero load type for save restoration.

Therefore the committed Entrance -> Junction CHANGEMAP path is modeled here only for:

```text
loadType    = 0
gameIsLoaded= 0
```

Saved-game restore semantics are intentionally fail-closed and remain future work.

## Permanent native spawn state

Files:

```text
ESP32/include/esp_player_spawn_state.h
ESP32/src/esp_player_spawn_state.c
```

API:

```text
EspPlayerSpawn_reset()
EspPlayerSpawn_prepareCommitted()
```

Pointer-free state:

```text
EspPlayerSpawnState = 24 B expected classic-ESP32 ABI
persistent heap = 0 B
```

Fields:

```text
sourceSpawnParam

tileIndex
worldX
worldY
tileX
tileY
angle
viewZ
viewZOld

spawnSource
loadType
overrideUsed

facingRefreshPending
playerSetupPending
tileEnterPending
active

targetMapId
gameplayLoadMapId
```

The state stores no legacy pointer and owns no map-sized data.

## Permanent validation boundary

`EspPlayerSpawn_prepareCommitted()` requires:

```text
transition phase = COMMITTED
transition committed = 1
pendingConsumed = 1
target map ID valid

loadType = 0
gameIsLoaded = 0

target inventory complete
target bytes/CRC/FNV/gameplayLoadMapId == committed transition
current native resident runtime == target inventory
resident lifecycle ready
```

The function performs:

```text
no PAK I/O
no allocation
no legacy mutation
```

Output is zeroed on every refused path where an output pointer exists.

## Real Junction projection

The committed transition retains:

```text
spawnParam = 0
```

Hardware-proven Junction BSP header:

```text
spawnIndex     = 943
spawnDirection = 64
```

Therefore the expected placement is:

```text
tileIndex = 943

tileX = 943 % 32 = 15
tileY = 943 / 32 = 29

worldX = 15 * 64 + 32 = 992
worldY = 29 * 64 + 32 = 1888

angle    = 64
viewZ    = 36
viewZOld = 4

spawnSource = HEADER
overrideUsed = 0
loadType = 0

targetMapId = 9
gameplayLoadMapId = 2
```

Pending followups:

```text
facingRefreshPending = 1
playerSetupPending    = 1
tileEnterPending      = 1
```

Static little-endian 24-byte FNV prediction:

```text
ba6af4a7
```

This is a candidate prediction until confirmed by the real CYD.

## Packed-override proof

The temporary probe also uses a synthetic committed-state copy with:

```text
spawnParam = 00030167
```

Recovered packed decode:

```text
x     = 7
y     = 11
angle = 192

tileIndex = 359
worldX    = 480
worldY    = 736
```

Expected semantics:

```text
spawnSource = OVERRIDE
overrideUsed = 1
BSP header spawn ignored
```

Static state FNV prediction:

```text
e0a5110b
```

The real committed transition remains unchanged; only a probe-local copy carries the synthetic override.

## Temporary hardware probe

Files:

```text
ESP32/include/native_junction_spawn_probe.h
ESP32/src/native_junction_spawn_probe.c
```

It arms only after the committed-transition probe has successfully left Junction resident.

Because the earlier temporary probe does not expose its local committed-transition object, this probe reconstructs the already hardware-proven scalar committed state:

```text
targetBytes=21051
targetCRC=4a2c5800
targetFNV=fefaf5ca
spawnParam=0
sourceMapId=1
targetMapId=9
targetGameplayLoadMapId=2
menuKind=LEVEL
phase=COMMITTED
pendingConsumed=1
statsAcknowledged=1
committed=1
```

It requires that reconstructed 24-byte state to hash exactly:

```text
2c595a62
```

This reconstruction is temporary probe scaffolding only. The permanent spawn API accepts the caller-owned real `EspMapCommittedTransitionState`.

## Fail-closed proof requested from hardware

The probe requires refusal with zero output for:

```text
null transition
null inventory
not committed
loadType != 0
gameIsLoaded != 0
target identity mismatch
resident runtime mismatch
BSP header spawnIndex >= 1024
```

Null output itself must return INVALID without mutation.

`EspPlayerSpawn_reset()` must produce a zero state.

## Resident / RAM integrity

Before and after all spawn projections, the complete resident Junction snapshot must remain byte-exact:

```text
snapshotFNV=bc9071e9 -> bc9071e9
payload=10410
entities=30
enemies=0
destructibles=3
```

Expected permanent heap cost of the spawn projection itself:

```text
0 B
```

The probe also requires same-build equality for:

```text
heap8 before == after
largest8 before == after
PAK closed
```

Absolute heap values are not predicted because code/BSS changes can shift the build baseline.

## Legacy integrity witnesses

The probe hashes before/after placement fields that legacy `Game_spawnPlayer()` would otherwise modify:

```text
Game.spawnParam
Game.isLoaded
Game.activeLoadType

DoomCanvas.viewX/viewY/viewZ/viewAngle
DoomCanvas.destX/destY/destAngle
DoomCanvas.loadMapID
DoomCanvas.loadType
DoomCanvas.state/storyPage

Render.viewZOld
Hud.isUpdate
```

A separate Player witness covers weapon/ammo/progression state.

The framebuffer is also hashed before/after.

All must remain exact because this milestone projects the spawn but does not apply it.

## Expected Serial family

```text
[JUNCTIONSPAWNPROBE] ARMED ...

=== Doom RPG ESP32-native Junction player spawn projection ===
[JUNCTIONSPAWNPROBE] CONTRACT ...

[BSPREAD] ... /junction.bsp ...

[JUNCTIONSPAWN] READY stateBytes=24 stateFNV=ba6af4a7 targetMap=9 gameplayLoadMapId=2 spawnParam=00000000 source=HEADER tileIndex=943 tile=15/29 world=992/1888 angle=64 viewZ=36 viewZOld=4 loadType=0 active=1

[JUNCTIONSPAWN] FOLLOWUPS facingRefresh=1 playerSetup=1 tileEnter=1 spawnApplied=no facingApplied=no playerSetupApplied=no tileEnterApplied=no

[JUNCTIONSPAWN] OVERRIDE param=00030167 tileIndex=359 tile=7/11 world=480/736 angle=192 source=OVERRIDE overrideUsed=1 stateFNV=e0a5110b headerIgnored=yes

[JUNCTIONSPAWN] LOADSEMANTIC loadType=0 gameIsLoaded=0 normalMapLoad=yes savedGameLoad=no activeLoadType=... loadTypeMutation=no

[JUNCTIONSPAWN] FAILCLOSED nullTransition=1 nullInventory=1 nullOutput=1 notCommitted=1 loadType=1 loadedWorld=1 targetMismatch=1 runtimeMismatch=1 badHeaderSpawn=1 reset=1 outputAtomic=yes

[JUNCTIONSPAWN] RESIDENT snapshotFNV=bc9071e9->bc9071e9 targetLeftResident=yes payload=10410 entities=30 enemies=0 destructibles=3 packClosed=yes

[JUNCTIONSPAWN] RAM heap8=...->... delta=0 largest8=...->... delta=0 persistentHeapBytes=0

[JUNCTIONSPAWN] LEGACY placementFNV=...->... playerFNV=...->... frameFNV=...->... legacyRuntimeClear=yes DoomCanvasMutation=no GameMutation=no PlayerMutation=no RenderMutation=no HudMutation=no

[JUNCTIONSPAWN] PARK state=9 page=3 committedTransition=yes mapSwapCommitted=yes targetMap=9 junctionResident=yes nativeSpawnState=yes spawnProjected=yes spawnApplied=no loadType=0 facingPending=yes playerSetupPending=yes tileEnterPending=yes ST_PLAYING=no entities=0 monsters=0 noGameplay=yes
```

## Strict PASS boundary

A PASS must prove:

```text
real Junction placement = 943 / 15,29 / 992,1888 / angle 64
real state FNV = ba6af4a7
packed override decode = 7,11 / angle 192
override state FNV = e0a5110b
fresh-map loadType semantic = 0
all fail-closed gates green
Junction resident snapshot unchanged
heap/largest unchanged
PAK closed
legacy placement/Player/framebuffer unchanged
spawnApplied=no
facingApplied=no
playerSetupApplied=no
tileEnterApplied=no
ST_PLAYING=no
legacy entities=0
legacy monsters=0
```

## Boundary after PASS

A hardware PASS would establish native ownership of the deterministic player placement required by the committed Junction transition while still separating placement computation from gameplay initialization.

The next milestone must be selected only after merge and repo recovery. Likely candidates are:

```text
native player/view owner applying the projected coordinates
native facing-entity query over compact topology
Player_setup-equivalent fresh-map initialization
initial tile-enter event
```

Those should not be collapsed blindly into one milestone.

Normal hardware environment:

```text
esp32-cyd
```

No local build or hardware PASS is claimed for this candidate.
