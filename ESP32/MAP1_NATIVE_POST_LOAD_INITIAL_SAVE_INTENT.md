# ESP32 native Junction post-load initial save intent milestone

Branch: `agent/esp32-native-post-load-initial-save-intent`

Base merged `main`:

```text
PR   = #79 — post-load weapon self-select
main = 04e4e2269a6c70db3f3e4027717bdb36f286ce65
```

Status: **HARDWARE CANDIDATE — NOT YET CYD-PROVEN**.

## Objective

Recover only the exact conditional save call immediately after the hardware-
proven current-weapon self-selection:

```c
if ((doomCanvas->loadMapID != MAP_END_GAME) &&
    (game->isLoaded == false)) {
    Game_saveState(game,
                   doomCanvas->loadMapID,
                   doomCanvas->viewX,
                   doomCanvas->viewY,
                   doomCanvas->viewAngle,
                   false);
}
```

For the native fresh-Junction path the already hardware-proven `EspPlayerView`
is the authoritative equivalent of the legacy placement writes, so the caller
arguments are reconstructed as:

```text
mapId=9 / MAP_JUNCTION / /junction.bsp
viewX=992
viewY=1888
viewAngle=64
saveMode=false
```

The temporary probe samples only `Game.isLoaded` from the legacy object. It does
not use live legacy `DoomCanvas.view*` or `loadMapID` as native truth because the
native port deliberately never performed those legacy placement mutations.

## Why this milestone is an intent, not legacy save-file emulation

Current `Game_saveState()` is a desktop/J2ME orchestration boundary, not a small
state assignment. It performs all of the following:

```text
Saving... UI + framebuffer presentation
Game_saveConfig(game, z)
if !z: Game_savePlayerState("Player2", current map, x, y, angle)
Game_saveWorldState(game)
if !z: Game_savePlayerState("Player", pending SAVEGAME route or fallback)
```

Copying that implementation literally would violate the permanent ESP32
architecture:

- the save UI/presentation is not gameplay state;
- `Config`, `Player`, `Player2` and `World` are legacy persistence formats;
- `Game_saveWorldState()` traverses pointer-heavy legacy `Entity_t` topology;
- legacy `Game.entities`/`Game.monsters` remain zero on the native path;
- native player/world mutable ownership is not yet complete.

Therefore this milestone owns the **semantic request** and explicitly records
that persistence and presentation remain deferred. No legacy save file is
created.

## Exact legacy component policy for `z=false`

The current fresh-map call uses `z=false`, therefore the semantic component mask
is:

```text
0x01 CONFIG
0x02 PLAYER2 current checkpoint
0x04 WORLD
0x08 PLAYER route checkpoint
----
0x0f ALL
```

`Game_saveConfig(game, false)` does not increment `numSaves`.

## Historical EV_SAVEGAME route caveat

The earlier MAP_INTRO opcode-27 milestone hardware-proved parsing a compact
`EspMapSaveRouteState` including `/junction.bsp` and a future destination.
However, the temporary `native_map1_save_route_probe.c` copies the sample into
its probe-local `parkedRouteState` and then calls:

```c
EspMapSaveRoute_reset(&parkedRouteState);
```

before its final PARK. There is currently **no live cross-map owner instance**
carrying that route into Junction.

Consequently this milestone records `PLAYER_ROUTE` as a requested save component
but does not fabricate its payload. The probe reports the current legacy
`newMapName/newDest*` fields only as read-only scaffolding witnesses. A future
native persistence implementation must establish an explicit durable route
handoff rather than relying on the old `routeLifetimeCrossMap=yes` diagnostic
wording.

## Permanent owner

New files:

```text
ESP32/include/esp_post_load_initial_save_intent.h
ESP32/src/esp_post_load_initial_save_intent.c
```

Candidate ABI:

```text
EspPostLoadInitialSaveIntentState = 24 B
persistent heap = 0 B
```

Fields:

```text
int32 viewX
int32 viewY
int32 viewAngle
uint8 mapId
uint8 isLoadedBefore
uint8 saveMode
uint8 saveRequired
uint8 componentMask
uint8 persistenceDeferred
uint8 presentationDeferred
uint8 active
uint8 reserved[4]
```

Permanent API:

```c
EspPostLoadInitialSaveIntent_reset()
EspPostLoadInitialSaveIntent_isReady()
EspPostLoadInitialSaveIntent_view()
EspPostLoadInitialSaveIntent_prepare()
EspPostLoadInitialSaveIntent_route()
```

The permanent implementation has no `Game_t`, `DoomCanvas_t`, `Player_t`,
`Render_t`, `Hud_t`, `Entity_t`, filesystem or presentation dependency.

## Strict caller boundary

Preparation requires the semantic weapon self-select owner:

```text
active=1
targetMapId=9
gameplayLoadMapId=2
loadType=0
weaponBefore=requestedWeapon=weaponAfter
viewInvalidationRequested=0
```

and the fully-consumed native player/view owner:

```text
active=1
spawnApplied=1
targetMapId=9
gameplayLoadMapId=2
loadType=0
hudRefreshPending=0
facingRefreshPending=0
playerSetupPending=0
tileEnterPending=0
```

plus exact post-GIVEMAP Junction world identity:

```text
sourceBytes=21051
sourceCrc32=4a2c5800
runtimeFNV=bc432a0f
mapStateFNV=8dba0bb4
automapFNV=b699bd75
```

Only the current fresh path `isLoadedBefore=0` is accepted. `isLoadedBefore=1`
returns `LOADED_CONTEXT_DEFERRED` without parking state; restore semantics remain
outside this milestone.

## Temporary hardware probe

New files:

```text
ESP32/include/native_junction_post_load_initial_save_intent_probe.h
ESP32/src/native_junction_post_load_initial_save_intent_probe.c
```

The lifecycle bridge runs it after the weapon self-select probe. Because older
probes may use `done` as attempt-completed rather than success-completed, this
probe immediately revalidates every required preceding owner/FNV and the exact
resident snapshot instead of trusting `isDone()` alone.

This new probe itself uses `done=1` only after its successful final PARK.

Expected Serial block:

```text
=== Doom RPG ESP32-native Junction post-load initial save intent ===
[JUNCTIONSAVEINTENTPROBE] CONTRACT ...
[JUNCTIONSAVEINTENT] READY ...
[JUNCTIONSAVEINTENT] SEMANTIC ...
[JUNCTIONSAVEINTENT] INPUT ...
[JUNCTIONSAVEINTENT] FAILCLOSED ...
[JUNCTIONSAVEINTENT] RESIDENT ...
[JUNCTIONSAVEINTENT] RAM ...
[JUNCTIONSAVEINTENT] LEGACY ...
[JUNCTIONSAVEINTENT] DEFERRED ...
[JUNCTIONSAVEINTENT] PARK ...
```

## Hardware acceptance

The real normal `esp32-cyd` firmware must prove:

```text
stateBytes=24
mapId=9
view=992/1888
angle=64
isLoadedBefore=0
saveMode=0
saveRequired=1
componentMask=0f
persistenceDeferred=1
presentationDeferred=1
active=1
```

The state FNV is deliberately left for the real CYD to establish.

Semantic acceptance:

```text
mapNotEnd=yes
notLoaded=yes
config=yes
player2=yes
world=yes
playerRoute=yes
saveFileWrite=no
savingUi=no
presentation=no
```

Input owners must remain canonical:

```text
weaponSelfSelect FNV=699f3cf3
post-load GIVEMAP FNV=448e587d
post-load HUD-clear FNV=b7383e18
PlayerView FNV=afcdcf74
Facing FNV=95aa1108
```

Resident world must remain unchanged:

```text
snapshotFNV=bb714d80 -> bb714d80
runtimeFNV=bc432a0f
mapStateFNV=8dba0bb4
scriptFNV=bc9b18ff
lineFNV=3658710d
textureFNV=537319ad
automapFNV=b699bd75
topologyFNV=d6e8df7d
payload=10410
entities=30
enemies=0
destructibles=3
packClosed=yes
```

Fail-closed/deferred tests include:

```text
null weapon owner
null player view
null output
inactive weapon owner
non-self-select weapon owner
inactive player view
pending player-view follow-up
loaded context deferred
invalid isLoaded value
pure prepare atomicity
post-active prepare refusal
repeat-route atomicity
```

RAM / integrity acceptance:

```text
heap8 delta=0
largest8 delta=0
persistentHeapBytes=0
framebuffer unchanged
Game unchanged
Player unchanged
Hud unchanged
DoomCanvas unchanged
Render unchanged
legacy runtime remains clear
legacy Game_saveState not called
shapeData == NULL
mediaTexels == NULL
```

## Candidate PARK

```text
state=9 / ST_INTRO
page=3
targetMap=9
junctionResident=yes
nativeWeaponSelfSelect=yes
nativeInitialSaveIntent=yes
initialSaveDecisionPending=no
initialSavePersistencePending=yes
initialSavePending=yes   # persistence debt, not caller-decision debt
postLoadCleanupPending=yes
ST_PLAYING=no
entities=0
monsters=0
noGameplay=yes
```

The explicit persistence-deferred owner is sufficient to represent this
non-gameplay side effect in caller order. A successful milestone does **not**
require blocking later gameplay bring-up on reproducing J2ME save files.

## Next exact caller boundary after PASS + merge

Continue with the small post-load flag cleanup immediately after the save call:

```c
game->isLoaded = false;
game->isSaved = false;
game->activeLoadType = 0;
```

Keep event/particle cleanup, `isUpdateView`, `ST_PLAYING`, renderer and durable
native save storage outside that next milestone.
