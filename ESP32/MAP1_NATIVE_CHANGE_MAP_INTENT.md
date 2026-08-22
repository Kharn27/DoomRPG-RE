# ESP32 MAP_INTRO native CHANGEMAP pending-transition milestone

Branch: `agent/esp32-map1-native-change-map-intent`

Base merged `main`:

```text
PR   = #60 — native EV_SAVEGAME route owner
main = 50ed329801fe99917ef2f848ee13e742ae7734ab
```

Firmware candidate content:

```text
93e0be24558ebffcbc9f60ef0ced54f29274ab28
```

Status: **IMPLEMENTED; REAL-CYD HARDWARE VALIDATION PENDING**.

## Objective

Own exact `2 / EV_CHANGEMAP` bytecode semantics without prematurely performing the later map transition.

Recovered legacy behavior cleanly separates two moments:

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

This milestone owns only the first boundary: the pending transition intent.

No level stats, menu state, map load, sound, renderer state, entities or gameplay are changed.

## Why CHANGEMAP before SHOW/HIDE

After SAVEGAME, MAP_INTRO has only:

```text
2  EV_CHANGEMAP
7  EV_SHOW
18 EV_HIDE
```

`SHOW/HIDE` are not visibility-only operations. Recovered legacy behavior couples them to entity death/link/unlink and tile entity chains, so they need an explicit compact native entity/topology boundary.

`CHANGEMAP`, by contrast, is already naturally deferred in the original engine. The opcode merely stores a pending scalar and the transition consumer runs later. That makes it a small permanent ownership boundary now.

## Recovered legacy dispatch

`Game_executeEvent()`:

```c
case EV_CHANGEMAP:
    game->changeMapParam = arg1;
    break;
```

Normal execution then returns handled=true. As with other handled commands, outer `Game_runEvent()` may remove the command when source `arg2 & 0x200`.

Native code returns this as `removeCommandIfHandled`; it does not mutate `EspMapScriptState` yet.

## Recovered deferred consumer

`Game_changeMap()` only acts when `changeMapParam != 0`:

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

The consumer itself is triggered by the door animation/update path when an open door has texture `7`:

```text
transition door open
 -> Sound 5068
 -> Game_changeMap()
```

This proves that `EV_CHANGEMAP` must not directly load a map.

## Raw parameter decoding

Recovered format:

```text
bits  0..7  = map string index
bits  8..30 = spawn parameter payload
bit      31 = SHOWSTATS
```

Legacy spawn expression:

```text
(changeMapParam << 1) >> 9
```

Native code uses explicit unsigned shifts:

```text
(rawParam << 1U) >> 9U
```

so the low-byte map string id and bit31 SHOWSTATS flag are removed without depending on signed-shift undefined behavior.

## Permanent native owner

New files:

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

Expected classic ESP32 ABI:

```text
EspMapChangeMapState = 16 B
persistent heap      = 0 B
```

Unlike `EV_SAVEGAME`, the destination name is deliberately **not copied inline**. `Game_changeMap()` resolves the target while the source map is still resident, before initiating teardown. Therefore a current-map immutable `EspMapStringRef` is the correct lifetime owner here.

SAVEGAME required an inline name because its later save consumer can run after source-map teardown; the two owners intentionally model different lifetimes.

Permanent calls:

```text
EspMapChangeMap_reset()
EspMapChangeMap_isActive()
EspMapChangeMap_apply()
```

For a non-zero raw parameter, `apply()` resolves/validates the low-byte string index through the immutable native runtime and stores the pending state.

For raw parameter zero, the owner becomes clear/inactive but the command still returns handled=true, matching the legacy assignment plus later no-op consumer.

The permanent source performs no PAK I/O and has no dependency on legacy Game/Render/Player/Menu/Sound/DoomCanvas types.

## Result / deferred effects

```c
typedef struct EspMapChangeMapResult_s {
    uint32_t rawParam;
    uint32_t spawnParam;
    uint16_t sourceEventIndex;
    uint16_t globalCommandIndex;
    uint16_t mapStringIndex;
    uint8_t sourceCommandOffset;
    uint8_t showStats;
    uint8_t pending;
    uint8_t legacyReturnValue;
    uint8_t removeCommandIfHandled;
    uint8_t effectFlags;
} EspMapChangeMapResult;
```

Expected ABI:

```text
EspMapChangeMapResult = 20 B
```

Deferred effects are metadata only:

```text
ADD_LEVEL_STATS    = 0x01
SHOW_STATS_MENU    = 0x02
LOAD_MAP           = 0x04

pending=0:
  effects=0

pending + showStats=1:
  effects=ADD_LEVEL_STATS | SHOW_STATS_MENU = 0x03

pending + showStats=0:
  effects=ADD_LEVEL_STATS | LOAD_MAP = 0x05
```

Sound `5068` belongs to the later texture-7 transition-door trigger, not to `EV_CHANGEMAP` itself, and is intentionally not emitted here.

## Temporary real-CYD probe

New files:

```text
ESP32/include/native_map1_change_map_probe.h
ESP32/src/native_map1_change_map_probe.c
```

The stage runs only after hardware-proven SAVEGAME route validation.

Inherited required fingerprints:

```text
arenaFNV            = c3882516
mapStateFNV         = cd99b98e
scriptFNV           = f9e3d9df
lineStateFNV        = e5e74861
lineTextureStateFNV = f1fc1875
automapStateFNV     = 669b1aa7
```

Other preconditions remain:

```text
/intro.bsp size=21823 crc32=623f34e4
events=93 bytecodes=265 lines=480 mapSprites=344
ST_INTRO storyPage=3
legacy Render runtime clear
pack closed
entities=0 monsters=0
```

### Complete real corpus

The probe scans all 93 events / 265 commands and discovers every real opcode-2 command.

For each ref it requires:

```text
state-only executor -> UNSUPPORTED
canonical descriptor/command provenance
native CHANGEMAP apply -> OK
raw parameter exact
spawn decoding exact
showStats exact
pending exact
removeCommandIfHandled exact
deferred effect flags exact
owner reset to exact initial fingerprint
```

Hardware will establish rather than predeclare:

```text
refs
pending refs
zero-param refs
showStats refs
direct-load refs
removable refs
fallback-map refs
map-name total bytes / max length
ownerFNV
resultFNV
contentFNV
initial owner FNV
sample owner FNV
first real command / target / spawn / flags
```

Acceptance requires:

```text
refs > 0
pending > 0
pending + zeroParam = refs
showStats + directLoad = pending
stateExecRefused = refs
rollback = refs/refs
ownerBytes = 16
resultBytes = 20
reapplyExact = 1
closedPackApply = 1
activeAtPark = 0
```

### Destination verification

The executor itself performs no I/O. The probe temporarily opens `/DoomRPG-ESP32.pak` only to read the real destination strings and compare them to the recovered legacy map-file list:

```text
/intro.bsp
/level01.bsp ... /level07.bsp
/junction.bsp
/junction_destroyed.bsp
/items.bsp
/reactor.bsp
/endgame.bsp
```

Unknown names are counted as `fallbackMapRefs`, matching legacy `Game_getResourceMapID()` fallback-to-MAP_INTRO behavior. Hardware decides the actual corpus.

After verification the PAK closes. The same sample CHANGEMAP is then applied again with the pack closed and must produce byte-identical owner/result:

```text
closedPackApply = 1
executorPackIO  = no
```

## Fail-closed / atomicity

Expected:

```text
unsupported=1
badOffset=1
badDescriptor=1
nullDescriptor=1
nullState=1
nullResult=1
reset=1
stateAtomic=yes
```

Invalid non-zero map string indices also fail closed in permanent code through `STRING_NOT_FOUND`.

## Legacy transition-integrity witness

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

It separately hashes player fields touched by `Player_addLevelStats()`:

```text
Player.time
Player.totalTime
Player.moves
Player.totalMoves
Player.completedLevels
Player.killedMonstersLevels
Player.foundSecretsLevels
Player.xpGained
```

Both fingerprints must remain unchanged. The already-proven legacy SAVE route witness also remains unchanged.

Thus any accidental call into `Game_changeMap()`, level-stats mutation, menu transition or map load causes the probe to fail.

## RAM / integrity boundary

Hardware-proven persistent native heap entering this milestone:

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

Candidate state/result are caller-owned values:

```text
ownerBytes       = 16 expected
resultBytes      = 20 expected
persistent heap  = 0 expected
```

The temporary PAK open used for string verification will have a transient heap cost. After close:

```text
heap8 after == heap8 before
largest8 after == largest8 before
persistentHeapBytes = 0
```

All inherited state must remain exact:

```text
framebuffer unchanged
arena/map/script/line/texture/automap fingerprints unchanged
legacy notebook/keys/Hud/password/continuation unchanged
legacy SAVE route unchanged
legacy transition witness unchanged
player stats witness unchanged
legacy Render runtime clear
entities=0 monsters=0
no gameplay / no ST_PLAYING
```

## Expected Serial family

```text
[MAPCHANGEMAPPROBE] ARMED ...

=== Doom RPG ESP32-native MAP_INTRO CHANGEMAP pending transition ===
[MAPCHANGEMAPPROBE] CONTRACT ...
[MAPCHANGEMAP] READY refs=... pending=... zeroParam=... showStats=... directLoad=... removable=... fallbackMap=... ownerBytes=16 resultBytes=20 stateExecRefused=... mapNameBytes=... maxMapName=... ownerFNV=... resultFNV=... contentFNV=... elapsed=...ms
[MAPCHANGEMAP] SAMPLE cmd=... event=... off=... arg1=... arg2=... mapString=... name="..." targetMap=... spawnParam=... showStats=... effects=... pending=... handled=1 removeIfHandled=...
[MAPCHANGEMAP] STATE initialOwnerFNV=... sampleOwnerFNV=... rollback=.../... reapplyExact=1 closedPackApply=1 activeAtPark=0
[MAPCHANGEMAP] FAILCLOSED unsupported=1 badOffset=1 badDescriptor=1 nullDescriptor=1 nullState=1 nullResult=1 reset=1 stateAtomic=yes
[MAPCHANGEMAP] IO entry=/intro.bsp size=21823 crc32=623f34e4 heapOpen=... transientHeapCost=... largestOpen=... packIO=yes verificationOnly=yes executorPackIO=no persistentHeapBytes=0
[MAPCHANGEMAPPROBE] RAM heap8=...->... delta=0 largest8=...->... delta=0 ...
[MAPCHANGEMAPPROBE] LEGACY ... transitionFNV=...->... statsFNV=...->... legacyRuntimeClear=yes
[MAPCHANGEMAPPROBE] PARK ... nativeChangeMapIntent=yes ownerBytes=16 resultBytes=20 persistentBytes=0 transitionArmedProven=yes transitionTriggered=no statsMutation=no menuMutation=no mapLoad=no framebufferMutation=no entities=0 monsters=0 noGameplay=yes
[ALIVE] ...
```

Use normal optimized PlatformIO environment:

```text
esp32-cyd
```

No CI status is published for firmware candidate `93e0be24558ebffcbc9f60ef0ced54f29274ab28`. No local build or hardware PASS is claimed.

## Remaining families after PASS

If CHANGEMAP passes, MAP_INTRO will have only:

```text
7  EV_SHOW
18 EV_HIDE
```

Those remain deferred to an explicit native sprite/entity-topology owner. Do not pre-authorize that final boundary before CHANGEMAP PASS + merge recovery.
