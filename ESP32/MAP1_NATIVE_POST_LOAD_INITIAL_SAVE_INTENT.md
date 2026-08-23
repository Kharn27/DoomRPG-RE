# ESP32 native Junction post-load initial save intent milestone

Branch: `agent/esp32-native-post-load-initial-save-intent`

Base merged `main`:

```text
PR   = #79 — post-load weapon self-select
main = 04e4e2269a6c70db3f3e4027717bdb36f286ce65
```

Hardware-tested firmware:

```text
0da9526775b706606338045babeb89e0d6c72729
```

Status: **REAL-CYD HARDWARE PASS / MERGE-READY**.

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

For the native fresh-Junction path the hardware-proven `EspPlayerView` is the
authoritative equivalent of the legacy placement writes. The real CYD proved the
caller arguments:

```text
mapId=9 / MAP_JUNCTION / /junction.bsp
viewX=992
viewY=1888
viewAngle=64
isLoadedBefore=0
saveMode=false
```

The temporary probe samples only `Game.isLoaded` from the legacy object. Live
legacy `DoomCanvas.view*` and `loadMapID` are not native source-of-truth because
the native port deliberately never applied those legacy placement mutations.

## Why this milestone is an intent, not legacy save-file emulation

Current `Game_saveState()` is a desktop/J2ME orchestration boundary. It bundles:

```text
Saving... UI + framebuffer presentation
Game_saveConfig(game, z)
if !z: Game_savePlayerState("Player2", current map, x, y, angle)
Game_saveWorldState(game)
if !z: Game_savePlayerState("Player", pending SAVEGAME route or fallback)
```

Copying that implementation literally would violate the permanent ESP32
architecture: `World` traverses pointer-heavy legacy `Entity_t` state, while the
native path still keeps legacy `Game.entities=0` and `Game.monsters=0`.

This milestone therefore owns the semantic save request and explicitly records
that persistence and presentation remain deferred. It performs no legacy file
write and no presentation.

## Exact component policy for `z=false`

```text
0x01 CONFIG
0x02 PLAYER2 current checkpoint
0x04 WORLD
0x08 PLAYER route checkpoint
----
0x0f ALL
```

## Historical EV_SAVEGAME route caveat

Opcode 27 route parsing remains hardware-proven. However, the old MAP_INTRO
probe resets its probe-local sampled `EspMapSaveRouteState` before PARK. There is
currently no live cross-map route owner instance reaching Junction.

Consequently this milestone records `PLAYER_ROUTE` as a requested persistence
component but does not fabricate route payload. The real-CYD log confirmed the
legacy route scaffold is currently empty:

```text
legacyNewMapPresent=no
legacyNewDest=0/0
legacyNewAngle=0
routePayloadOwned=no
```

A future native persistence implementation must establish an explicit durable
route handoff.

## Permanent owner

```text
ESP32/include/esp_post_load_initial_save_intent.h
ESP32/src/esp_post_load_initial_save_intent.c
```

Hardware-proven ABI:

```text
EspPostLoadInitialSaveIntentState = 24 B
persistent heap = 0 B
stateFNV = 0bf1a911
```

Real-CYD state:

```text
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

The owner requires the exact post-weapon Junction boundary plus fully-consumed
native player/view state and post-GIVEMAP world identity:

```text
weaponFNV=699f3cf3
giveMapFNV=448e587d
hudClearFNV=b7383e18
viewFNV=afcdcf74
facingFNV=95aa1108
runtimeFNV=bc432a0f
mapFNV=8dba0bb4
automapFNV=b699bd75
snapshotFNV=bb714d80
```

Only fresh-map `isLoadedBefore=0` is supported. `isLoadedBefore=1` returns
`LOADED_CONTEXT_DEFERRED`; restore semantics remain outside this milestone.

## Real-CYD hardware evidence

The normal `esp32-cyd` firmware printed:

```text
[JUNCTIONSAVEINTENT] READY stateBytes=24 stateFNV=0bf1a911 mapId=9 view=992/1888 angle=64 isLoadedBefore=0 saveMode=0 saveRequired=1 componentMask=0f persistenceDeferred=1 presentationDeferred=1 active=1
[JUNCTIONSAVEINTENT] SEMANTIC mapNotEnd=yes notLoaded=yes config=yes player2=yes world=yes playerRoute=yes saveFileWrite=no savingUi=no presentation=no
[JUNCTIONSAVEINTENT] INPUT weaponFNV=699f3cf3 giveMapFNV=448e587d hudClearFNV=b7383e18 viewFNV=afcdcf74 facingFNV=95aa1108 unchanged=yes callerOrder=yes callMap=9 callView=992/1888 angle=64 source=nativePlayerView legacyIsLoaded=0
[JUNCTIONSAVEINTENT] FAILCLOSED nullWeapon=1 nullView=1 nullOutput=1 inactiveWeapon=1 weaponMismatch=1 inactiveView=1 viewPending=1 loadedContextDeferred=1 invalidLoaded=1 prepareAtomic=yes postActivePrepare=1 repeat=1 repeatAtomic=yes
[JUNCTIONSAVEINTENT] RESIDENT snapshotFNV=bb714d80->bb714d80 unchanged=yes mapFNV=8dba0bb4 automapFNV=b699bd75 runtimeFNV=bc432a0f scriptFNV=bc9b18ff lineFNV=3658710d textureFNV=537319ad topologyFNV=d6e8df7d payload=10410 entities=30 enemies=0 destructibles=3 packClosed=yes
[JUNCTIONSAVEINTENT] RAM heap8=72644->72644 delta=0 largest8=34804->34804 delta=0 persistentHeapBytes=0
[JUNCTIONSAVEINTENT] DEFERRED legacyNewMapPresent=no legacyNewDest=0/0 legacyNewAngle=0 routePayloadOwned=no playerPersistence=no worldPersistence=no configPersistence=no
[JUNCTIONSAVEINTENT] PARK state=9 page=3 targetMap=9 junctionResident=yes nativeWeaponSelfSelect=yes nativeInitialSaveIntent=yes initialSaveDecisionPending=no initialSavePersistencePending=yes initialSavePending=yes postLoadCleanupPending=yes ST_PLAYING=no entities=0 monsters=0 noGameplay=yes
```

Same-build equality witnesses:

```text
gameFNV=6960d5bb->6960d5bb
playerFNV=c64e7862->c64e7862
hudFNV=b18611d2->b18611d2
canvasFNV=592d59c9->592d59c9
renderFNV=f9344dec->f9344dec
frameFNV=6a0726c1->6a0726c1
legacyRuntimeClear=yes
GameMutation=no
PlayerMutation=no
HudMutation=no
DoomCanvasMutation=no
RenderMutation=no
legacyGame_saveStateCalled=no
```

Stable post-PARK normal-env heartbeat:

```text
heap=138408
heap8=72644
largest8=34804
SD=ready
ZIP=ready
VIDEO=ready
CORE=ready
LAYOUT=ready
PRERENDER=ready
RENDER=ready
MAPPINGS=ready
MENUBSP=ready
```

## SD false alarm during hardware test

An earlier boot of the same tested firmware stopped before the menu with
`SD=unavailable`. Investigation showed the new save-intent code cannot execute
before SD initialization, and the branch did not modify `main.cpp`,
`platformio.ini`, `board_config.h` or the SD path. The user identified the
microSD card as the cause; after restoring the card the exact same firmware
reached the complete successful block above.

Therefore no SD workaround or unrelated code change was introduced.

## Probe completion semantics

Historical temporary probes may use `done` as terminal-attempt rather than PASS.
This probe revalidates all predecessor owners and world FNVs, and itself sets
`done=1` only after its successful final PARK.

## Hardware-proven PARK

```text
state=9 / ST_INTRO
page=3
targetMap=9
junctionResident=yes
nativeWeaponSelfSelect=yes
nativeInitialSaveIntent=yes
initialSaveDecisionPending=no
initialSavePersistencePending=yes
initialSavePending=yes   # persistence debt only
postLoadCleanupPending=yes
ST_PLAYING=no
entities=0
monsters=0
noGameplay=yes
```

The explicit persistence-deferred owner is sufficient to represent this
non-gameplay side effect in caller order. Native gameplay bring-up does not need
to reproduce the old J2ME save files before continuing.

## Next exact caller boundary after merge

Own only the small post-load flag cleanup:

```c
game->isLoaded = false;
game->isSaved = false;
game->activeLoadType = 0;
```

Keep queued-event/particle cleanup, `isUpdateView`, `ST_PLAYING`, renderer and
durable native save storage outside the next milestone.
