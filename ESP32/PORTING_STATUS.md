# Doom RPG ESP32 CYD porting status

Authoritative recovery point for the classic ESP32-2432S028R port.

## Latest merged hardware baseline

```text
PR   = #61 — native EV_CHANGEMAP pending transition intent
main = fc39ac60757e0d992e3729a5044a9d83e9994971
hardware-tested firmware = 93e0be24558ebffcbc9f60ef0ced54f29274ab28
```

Merged evidence: [`MAP1_NATIVE_CHANGE_MAP_INTENT.md`](MAP1_NATIVE_CHANGE_MAP_INTENT.md).

## Current merge-ready milestone

```text
branch = agent/esp32-map1-native-show-hide-topology
base   = fc39ac60757e0d992e3729a5044a9d83e9994971
hardware-tested firmware = f881ccdad20d950462dd781456c340e792f59ec3
status = REAL-CYD HARDWARE PASS / MERGE-READY
```

Active evidence: [`MAP1_NATIVE_SHOW_HIDE_TOPOLOGY.md`](MAP1_NATIVE_SHOW_HIDE_TOPOLOGY.md).

This milestone owns the final two real MAP_INTRO opcode IDs:

```text
7  EV_SHOW
18 EV_HIDE
```

**All 16 real MAP_INTRO opcode IDs now have explicit native ownership/execution boundaries.**

## Permanent invariants

```text
board       = ESP32-2432S028R classic CYD
MCU         = ESP32-D0WD-V3 dual core 240 MHz
flash       = 4 MB
PSRAM       = none
framebuffer = 160x120 RGB565 = 38400 B
shapeData   = NULL
mediaTexels = NULL
runtime ZIP = forbidden
backing     = /DoomRPG-ESP32.pak
```

## MAP_INTRO identity

```text
/intro.bsp / Entrance
bytes=21823 crc32=623f34e4
nodes=223 lines=480 mapSprites=344 events=93 byteCodes=265
strings=94 stringData=7779 maxString=313
```

Real opcode IDs — all owned:

```text
2, 7, 8, 9, 10, 11, 13, 15, 16, 18, 19, 24, 26, 27, 40, 41
```

## Hardware-proven fingerprints

Inherited canons:

```text
arenaFNV               = c3882516
mapStateFNV            = cd99b98e
scriptFNV              = f9e3d9df
lineStateFNV           = e5e74861
lineTextureStateFNV    = f1fc1875
automapStateFNV        = 669b1aa7
lineDoorFNV            = b1c9d297
unlockFNV              = 261d756a
giveMapFNV             = 98c7ac59
giveMapMutatedAutoFNV  = 9d03ca2d
giveMapMutatedMapFNV   = e21edbce
saveRouteOwnerFNV      = 06ea6ea8
saveRouteResultFNV     = c2ecb064
saveRouteContentFNV    = 725845aa
legacySaveRouteFNV     = 9bcfe135
changeMapOwnerFNV      = f75eb7c7
changeMapResultFNV     = 2f40c9be
changeMapContentFNV    = f7a79d99
changeMapInitialFNV    = 69691905
changeMapSampleFNV     = 4e4ebeac
legacyTransitionFNV    = 79ab740c
playerStatsFNV         = 0b2ae445
```

New SHOW/HIDE canons:

```text
spriteTopologyFNV      = 3f321e43
showResultFNV          = 6029eb3c
hideResultFNV          = d24f5bae
showStateFNV           = b6a45f47
hideStateFNV           = bec68187
contextAfterShowFNV    = 2de723aa
contextAfterHideFNV    = bb1d78a4
legacyEntityTopologyFNV= f8f9b485
```

## Hardware-proven persistent native RAM

```text
immutable arena          14112 B
mutable tile state        1040 B
mutable script state       100 B
mutable line state         136 B
mutable texture state       76 B
mutable automap state      120 B
mutable sprite topology   2424 B
-------------------------------
total                    18008 B
```

Topology storage:

```text
344 sprites
7 B / sprite
payload            = 2408 B
actual heap cost   = 2424 B
allocator overhead = 16 B
largest8           = 34804 B unchanged
```

SAVE route and CHANGEMAP intent remain caller-owned value states and add no persistent heap.

## Native sprite topology hardware canon

```text
entityDefCount = 115
entities       = 220
hasDef         = 213
fallback       = 7
linked         = 209
hiddenSprites  = 11
hiddenEntities = 11
enemies        = 30
destructibles  = 13
nextLinkOrder  = 209
stateFNV       = 3f321e43
```

Permanent result ABI:

```text
EspMapShowResult = 26 B
EspMapHideResult = 18 B
```

No legacy `Entity_t*`, sprite pointers, monster objects or 1024-entry pointer entity database are retained by the native owner.

## SHOW/HIDE real corpus

```text
refs                = 12
showRefs            = 11
hideRefs            = 1
removableRefs       = 12
stateExecRefused    = 12
showMutated         = 11
hideMutatedIsolated = 0
hideNoMutation      = 1
showTargetEnt       = 11
showTargetNoEnt     = 0
blockersFound       = 2
blockersRemoved     = 2
blockerNoops        = 0
deferredDeaths      = 2
hideEntitiesIsolated= 0
```

Canonical SHOW:

```text
cmd=111 event=43 off=0
sprite=1 tile=613
blockers=0 removed=0
effects=0005
handled=1 removeIfHandled=1
```

Canonical isolated HIDE:

```text
cmd=173 event=60 off=9
tile=2,22 index=706
hidden=0 effects=00
handled=1 removeIfHandled=1
```

Canonical contextual HIDE proof:

```text
same event 60
SHOW cmd=165 off=1 sprite=0 tile=706
HIDE cmd=173 off=9 tile=706

afterShowFNV = 2de723aa
afterHideFNV = bb1d78a4
hidden       = 1
first/last   = 0 / 0
effects      = 03
secondHidden = 0
contextProven=1
idempotent   =1
```

## SHOW/HIDE rollback and guards

```text
rollback        = 12/12
showRepeatGuard = 1
hideContext     = 1
hideIdempotent  = 1
reset           = 1
worldRestored   = yes
```

Fail closed:

```text
unsupported     = 1
badOffset       = 1
badDescriptor   = 1
nullDescriptor  = 1
nullResult      = 1
randomCrateReal = 0
targetRelink    = guarded
stateAtomic     = yes
```

Full legacy `Entity_died()` gameplay remains outside this direct topology owner; two real SHOW references report deferred blocker-gameplay effects. No legacy death/link/unlink routine is called by this milestone.

## PAK / RAM / integrity evidence

```text
/entities.db size = 2762 B
crc32             = 4f2be32d
heapOpen          = 63704
transientPackCost = 4376 B
largestOpen       = 34804
packIO            = build only
executorPackIO    = no
```

Hardware integrity:

```text
heap8      68080 -> 65656
largest8   34804 -> 34804
frameFNV   b3b98db4 -> b3b98db4
arenaFNV   c3882516 -> c3882516
mapStateFNV cd99b98e -> cd99b98e
scriptFNV   f9e3d9df -> f9e3d9df
automapFNV  669b1aa7 -> 669b1aa7

notebookFNV       4d7705c5 -> 4d7705c5
keys              00000000 -> 00000000
hudFNV            505b1255 -> 505b1255
passwordCanvasFNV 214171cf -> 214171cf
continuationFNV   e2ba14a5 -> e2ba14a5
entityTopologyFNV f8f9b485 -> f8f9b485
```

Final PARK:

```text
state=9 page=3
nativeSpriteTopology=yes
nativeShowHideExec=yes
allMapIntroOpcodeFamiliesOwned=yes
worldMutationProven=yes
worldRestored=yes
legacyEntityMutation=no
framebufferMutation=no
entities=0 monsters=0
noGameplay=yes
```

Stable real-CYD heartbeats:

```text
70345 ms heap=131420 heap8=65656 largest8=34804
75346 ms heap=131420 heap8=65656 largest8=34804
80347 ms heap=131420 heap8=65656 largest8=34804
85348 ms heap=131420 heap8=65656 largest8=34804
90349 ms heap=131420 heap8=65656 largest8=34804
```

## Current architecture boundary

Hardware-proven native ownership now includes:

```text
compact immutable map runtime
allocation-free semantic access
tile mutable state
script mutable state
event lookup/descriptors/filtering
state-only opcode executor
UI/string/status/dialog/notebook/key/password owners
OPEN/CLOSE line state
UNLOCK texture state
GIVEMAP automap state
SAVEGAME durable future-save route
CHANGEMAP pending transition intent
SHOW/HIDE compact sprite/entity topology
```

Still intentionally outside the boundary:

```text
full native entity/monster gameplay
consumption of deferred blocker death/gameplay effects
actual CHANGEMAP transition consumer
legacy-world-free gameplay loop
native gameplay renderer integration
ST_PLAYING progression
actual sound playback
```

`entities=0`, `monsters=0`, `ST_PLAYING` is not reached, and `shapeData/mediaTexels` remain NULL.

## Merge recommendation

```text
MERGE agent/esp32-map1-native-show-hide-topology
```

Hardware-tested firmware is `f881ccdad20d950462dd781456c340e792f59ec3`. All commits after that firmware must remain documentation-only until merge; no further flash is required if that condition holds.
