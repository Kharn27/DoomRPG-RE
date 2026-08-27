# ESP32 native Entrance startup route

Status: **REAL-CYD HARDWARE PASS / MERGE-READY**

```text
branch = agent/esp32-native-entrance-startup-route
base main = be4a9a666245663da7866a8aa0aa40b98339d076
base PR = #100 — native gameplay SELECT front-tile
hardware-tested implementation SHA = 87d7923b42eda0f36a1c7daded33ca0c4f5a4958
```

## Goal

Correct the native boot route before adding more Junction gameplay.

The recovered legacy startup semantics are:

```text
cinematic intro
 -> startupMap = 1
 -> MAP_INTRO = 1
 -> /intro.bsp
 -> BSP map name = Entrance
```

`/intro.bsp` is therefore **not the cinematic resource**. It is the first post-cinematic gameplay BSP, named Entrance.

`/level01.bsp` is a later resource-map entry (`MAP_SECTOR01 = 2`) and must not be substituted merely because its filename appears to mean "level 1".

## Bug recovered

The historical ESP32 milestone chain intentionally validated the complete Entrance event/transition path. During normal boot it then kept servicing already-proven probes:

```text
Entrance resident
 -> transition preflight
 -> resident handoff
 -> committed transition
 -> Junction resident
 -> Junction spawn/render/gameplay
```

That was useful as laboratory scaffolding, but once left in the production boot path it effectively auto-played the end of Entrance and skipped the first playable level on every startup.

The bug was **not** an incorrect `startupMap` value and **not** a bad `/intro.bsp` catalog mapping. The bug was continuing the historical validation pipeline beyond the source-only preflight boundary.

## Corrected production boundary

The new startup route stops after the already hardware-proven source-only transition preflight:

```text
cinematic intro
 -> build canonical Entrance resident owners
 -> optional read-only /junction.bsp target preflight from PAK
 -> preserve Entrance exactly
 -> PARK
```

Forbidden during startup:

```text
Esp32ResidentHandoffProbe_service()
Esp32CommittedTransitionProbe_service()
Junction resident build
Junction spawn
Junction render/gameplay
```

The old probe implementations remain in the repository as historical executable evidence, but their destructive/committing services are no longer part of the normal boot chain.

Both old probe states are reset during lifecycle reset so a logical restart cannot inherit stale completion state.

## Strict Entrance startup witness

New temporary probe:

```text
ESP32/include/native_entrance_startup_route_probe.h
ESP32/src/native_entrance_startup_route_probe.c
```

It executes only after `Esp32TransitionPreflightFinalProbe_isDone()` and requires:

```text
DoomCanvas state = ST_INTRO
storyPage = 3
startupMap = MAP_INTRO = 1
startup file = /intro.bsp
resident lifecycle ready
canonical Entrance snapshot
canonical Entrance runtime source/CRC/FNV
legacy runtime clear
legacy Game.entities = 0
legacy Game.monsters = 0
PAK closed
ResidentHandoff probe not completed
CommittedTransition probe not completed
shapeData = NULL
mediaTexels = NULL
```

It is an observer only. Framebuffer hash, free 8-bit heap and largest 8-bit block must be identical before/after it.

## Hardware-proven Entrance identity

Real classic CYD:

```text
resourceMapId = 1
file = /intro.bsp
name = Entrance
startupMap = 1
sourceBytes = 21823
crc32 = 623f34e4
runtime arena = 14095 B
runtimeFNV = c3882516
snapshotBytes = 96
snapshotFNV = b3811f3d
total payload = 17891 B
spawn header tile = 904
spawn direction = 64
```

Canonical resident owners:

```text
mapStateFNV = cd99b98e
scriptFNV = f9e3d9df
lineFNV = e5e74861
textureFNV = f1fc1875
automapFNV = 669b1aa7
topologyFNV = 3f321e43

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

These topology counts are compact native map semantics. Legacy `Game.entities` and `Game.monsters` remain zero.

## Real-CYD startup route proof

Hardware log:

```text
[NATIVEBOOT] ENTRANCE source validation complete silent passes=25; production startup stops before ResidentHandoff/CommittedTransition
```

Strict route witness:

```text
[ENTRANCEBOOT] ROUTE
cinematicIntro=done
entranceResident=yes
targetPreflightOnly=yes
residentHandoff=no
committedTransition=no
junctionResident=no
junctionGameplay=no
spawnDeferred=yes
firstFrameDeferred=yes
packClosed=yes
legacyRuntimeClear=yes
shapeData=0x0
mediaTexels=0x0
entities=0
monsters=0
```

Therefore the corrected runtime boot no longer transitions to Junction automatically.

## Hardware memory/frame proof

Real CYD:

```text
frameFNV = faa62417 -> faa62417 exact=yes
heap8 = 64464 -> 64464
largest8 = 34804 -> 34804
allocation = none
```

Stable later heartbeats:

```text
heap = 130172
heap8 = 64464
largest8 = 34804
```

No Guru Meditation, reboot or probe failure was reported.

## Architectural meaning

This milestone intentionally regresses the visible boot endpoint from playable Junction back to a pre-render Entrance PARK.

That is a **routing correction**, not a loss of proven Junction functionality. The Junction spawn/render/HUD/input/MOVE/TURN/SELECT implementations remain valid historical hardware-proven components and can be generalized/reused later.

The production sequence is now aligned with the original game progression:

```text
cinematic
 -> Entrance first
 -> later player-driven end-of-level transition
 -> Junction
```

The actual Entrance end-of-level `EV_CHANGEMAP` remains part of the real BSP behavior, but it must only be committed after gameplay reaches and triggers it; it must never be auto-executed by boot scaffolding.

## Boundary after PASS

Current production PARK:

```text
Entrance resident = yes
Entrance player spawned = no
Entrance first frame = no
native ST_PLAYING for Entrance = no
Junction resident = no
Junction gameplay = no
shapeData = NULL
mediaTexels = NULL
legacy entities = 0
legacy monsters = 0
```

Next bounded milestone:

```text
canonical Entrance resident
 -> consume BSP header spawn tile 904
 -> direction 64
 -> prepare native player/view placement
 -> render first native Entrance frame
 -> preserve native compact ownership
 -> no automatic CHANGEMAP
```

Only after that frame is hardware-proven should HUD/input/MOVE/TURN/SELECT be reattached to Entrance. That will provide the correct corpus for real openable doors, computers/password entry and other first-level gameplay semantics.

## Validation history

```text
87d7923b42eda0f36a1c7daded33ca0c4f5a4958
 -> REAL-CYD HARDWARE PASS
```

No local PlatformIO build is claimed for this milestone; the real CYD Serial log is the final hardware source of truth.

All commits after the hardware-tested implementation SHA are documentation-only.

## Merge recommendation

```text
MERGE agent/esp32-native-entrance-startup-route
```
