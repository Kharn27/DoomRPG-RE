# ESP32 MAP_INTRO native CHANGEMAP pending-transition milestone

Branch: `agent/esp32-map1-native-change-map-intent`

Base merged `main`:

```text
PR   = #60 — native EV_SAVEGAME route owner
main = 50ed329801fe99917ef2f848ee13e742ae7734ab
```

Hardware-tested firmware content:

```text
93e0be24558ebffcbc9f60ef0ced54f29274ab28
```

Status: **REAL-CYD HARDWARE PASS / MERGE-READY**.

## Objective

Own exact `2 / EV_CHANGEMAP` bytecode semantics without prematurely performing the later map transition.

Recovered legacy behavior separates two moments:

```text
EV_CHANGEMAP bytecode
 -> Game.changeMapParam = arg1
 -> handled=true

later, when an opened texture-7 transition door is processed
 -> play sound 5068
 -> Game_changeMap()
 -> decode spawn
 -> resolve target map
 -> update level stats
 -> either show stats menu or begin map load
 -> clear changeMapParam
```

This milestone owns only the first boundary: a compact pending transition intent.

No level stats, menu state, map load, sound, renderer state, entities or gameplay are changed.

## Why CHANGEMAP was the correct next family

After merged SAVEGAME, MAP_INTRO had only:

```text
2  EV_CHANGEMAP
7  EV_SHOW
18 EV_HIDE
```

`SHOW/HIDE` are entity-topology operations in the legacy engine. `CHANGEMAP` is naturally deferred: the opcode only stores a pending parameter, while the heavy transition consumer runs later. That permits a bounded permanent owner without opening gameplay or legacy world mutation.

## Exact legacy contract

`Game_executeEvent()`:

```c
case EV_CHANGEMAP:
    game->changeMapParam = arg1;
    break;
```

Outer `Game_runEvent()` may remove a handled command when source `arg2 & 0x200`; native code exposes this as `removeCommandIfHandled` but does not mutate `EspMapScriptState` here.

Deferred `Game_changeMap()` behavior:

```text
spawnParam = (changeMapParam << 1) >> 9
map name   = mapStrings[changeMapParam & 0xff]
map id     = Game_getResourceMapID(map name)

if bit31 SHOWSTATS:
    Player_addLevelStats(true)
    Menu.mapNameId = map id
    target=endgame ? MENU_MAP_STATS_OVERALL : MENU_MAP_STATS
else:
    Player_addLevelStats(false)
    DoomCanvas_loadMap(map id)

changeMapParam = 0
```

The consumer is triggered by the door update path when an open door has texture `7`:

```text
transition door open
 -> Sound 5068
 -> Game_changeMap()
```

Therefore `EV_CHANGEMAP` itself must not load a map.

## Raw parameter decoding

```text
bits  0..7  = map string index
bits  8..30 = spawn parameter payload
bit      31 = SHOWSTATS
```

Native spawn extraction uses:

```text
(rawParam << 1U) >> 9U
```

which preserves the intended bit extraction using explicit unsigned shifts.

## Permanent native owner

Files:

```text
ESP32/include/esp_map_change_map_state.h
ESP32/src/esp_map_change_map_state.c
```

State:

```c
typedef struct EspMapChangeMapState_s {
    uint32_t rawParam;
    EspMapStringRef mapName;
    uint16_t sourceEventIndex;
    uint16_t globalCommandIndex;
    uint8_t sourceCommandOffset;
    uint8_t active;
} EspMapChangeMapState;
```

Real classic ESP32 ABI:

```text
EspMapChangeMapState  = 16 B
EspMapChangeMapResult = 20 B
persistent heap       = 0 B
```

Unlike `EV_SAVEGAME`, the destination string is intentionally not copied inline. The later CHANGEMAP consumer resolves it while the source map is still resident, before teardown. A current-map immutable `EspMapStringRef` is therefore the correct lifetime owner.

The permanent source performs no PAK I/O and has no dependency on legacy Game/Render/Player/Menu/Sound/DoomCanvas types.

## Deferred-effect metadata

```text
ADD_LEVEL_STATS = 0x01
SHOW_STATS_MENU = 0x02
LOAD_MAP        = 0x04

pending=0:
  effects=0

pending + showStats=1:
  effects=0x03

pending + showStats=0:
  effects=0x05
```

These effects are metadata only. Sound `5068` belongs to the later transition-door trigger and is not emitted here.

## Real-CYD corpus proof

Normal optimized PlatformIO environment: `esp32-cyd`.

The real classic no-PSRAM CYD established:

```text
refs             = 1
pending          = 1
zeroParam        = 0
showStats        = 1
directLoad       = 0
removable        = 0
fallbackMap      = 0
ownerBytes       = 16
resultBytes      = 20
stateExecRefused = 1
mapNameBytes     = 13
maxMapName       = 13
ownerFNV         = f75eb7c7
resultFNV        = 2f40c9be
contentFNV       = f7a79d99
elapsed          = 38 ms
```

The corpus partitions exactly:

```text
pending + zeroParam = 1 + 0 = 1 ref
showStats + directLoad = 1 + 0 = 1 pending ref
```

## Canonical real command

```text
global command = 2
event          = 1
command offset = 1
arg1           = 80000000
arg2           = 00000100
mapString      = 0
mapName        = "/junction.bsp"
targetMap      = 9 / MAP_JUNCTION
spawnParam     = 0
showStats      = 1
effectFlags    = 03
pending        = 1
handled        = 1
removeIfHandled= 0
```

This proves the real MAP_INTRO exit command arms a deferred transition to Junction and requests the stats-menu path, while leaving the transition itself untouched.

## Owner mutation / rollback / closed-pack proof

Hardware fingerprints:

```text
initialOwnerFNV = 69691905
sampleOwnerFNV  = 4e4ebeac
rollback        = 1 / 1
reapplyExact    = 1
closedPackApply = 1
activeAtPark    = 0
```

The probe first opens `/DoomRPG-ESP32.pak` only to verify the real destination string. After closing the PAK, the exact same command is applied again and produces byte-identical owner/result state.

Therefore:

```text
executorPackIO = no
```

The permanent executor relies only on the already-resident native runtime/string span.

## Fail-closed proof

Real hardware proved:

```text
unsupported    = 1
badOffset      = 1
badDescriptor  = 1
nullDescriptor = 1
nullState      = 1
nullResult     = 1
reset          = 1
stateAtomic    = yes
```

Invalid non-zero map string indices also remain fail-closed through `STRING_NOT_FOUND` in permanent code.

## PAK / RAM proof

Bounded verification witness:

```text
entry               = /intro.bsp
size                = 21823
crc32               = 623f34e4
heapOpen            = 63800
transientHeapCost   = 4376 B
largestOpen         = 34804
packIO              = yes
verificationOnly    = yes
executorPackIO      = no
persistentHeapBytes = 0
```

Before/after the complete stage:

```text
heap8       = 68176 -> 68176
largest8    = 34804 -> 34804
frameFNV    = e36ac6fd -> e36ac6fd
arenaFNV    = c3882516 -> c3882516
mapStateFNV = cd99b98e -> cd99b98e
scriptFNV   = f9e3d9df -> f9e3d9df
automapFNV  = 669b1aa7 -> 669b1aa7
```

Persistent native heap remains exactly:

```text
immutable arena        14112 B
mutable tile state      1040 B
mutable script state     100 B
mutable line state       136 B
mutable texture state     76 B
mutable automap state    120 B
-----------------------------
total                  15584 B
```

CHANGEMAP adds no persistent heap allocation.

Absolute heap/frame values differ from earlier firmware builds; acceptance is same-build stability plus canonical native fingerprints.

## Legacy transition-integrity proof

The probe hashes fields that the later legacy consumer would otherwise mutate:

```text
Game.changeMapParam
Game.spawnParam
Menu.mapNameId
MenuSystem.menu
DoomCanvas.state
DoomCanvas.loadMapID
DoomCanvas.loadType
DoomCanvas.saveType
```

Real hardware witness:

```text
legacy transitionFNV = 79ab740c -> 79ab740c
```

Player fields touched by `Player_addLevelStats()` were separately protected:

```text
player statsFNV = 0b2ae445 -> 0b2ae445
```

Other protected witnesses stayed exact:

```text
notebookFNV       = 4d7705c5 -> 4d7705c5
keys              = 00000000 -> 00000000
hudFNV            = 505b1255 -> 505b1255
passwordCanvasFNV = 214171cf -> 214171cf
continuationFNV   = e2ba14a5 -> e2ba14a5
saveRouteFNV      = 9bcfe135 -> 9bcfe135
legacyRuntimeClear= yes
```

No legacy transition, level-stat mutation, menu transition, map load, renderer/entity mutation or framebuffer mutation occurred.

## Final PARK boundary

Hardware proved:

```text
nativeChangeMapIntent = yes
ownerBytes            = 16
resultBytes           = 20
persistentBytes       = 0
transitionArmedProven = yes
transitionTriggered   = no
statsMutation         = no
menuMutation          = no
mapLoad               = no
framebufferMutation   = no
entities              = 0
monsters              = 0
noGameplay            = yes
```

The Serial line printed `ownerBytes=16resultBytes=20` without an intervening space; this is formatting-only and both numeric values are unambiguous.

Stable post-PARK heartbeats:

```text
uptime=35181 ms heap=133940 heap8=68176 largest8=34804 all reported subsystems ready
uptime=40182 ms heap=133940 heap8=68176 largest8=34804 all reported subsystems ready
```

`ZIP=ready` is the existing subsystem status label and does not indicate runtime map access through ZIP. Runtime backing remains `/DoomRPG-ESP32.pak`.

## Hardware acceptance status

The real CYD proved the complete CHANGEMAP corpus, exact destination/span and show-stats decoding, exact owner/result fingerprints, rollback/reapply, closed-pack executor behavior, zero persistent heap, legacy transition/stat integrity and stable PARK heartbeats.

This milestone is **REAL-CYD HARDWARE PASS / MERGE-READY**.

Hardware-tested firmware content:

```text
93e0be24558ebffcbc9f60ef0ced54f29274ab28
```

All later commits must remain documentation-only unless another firmware is flashed.

## Remaining MAP_INTRO families

After CHANGEMAP, only these remain unowned:

```text
7  EV_SHOW
18 EV_HIDE
```

They are a single closely related entity/sprite-topology frontier in legacy behavior, but the exact next milestone must still be selected only after this branch is merged and the true new `main` is recovered.
