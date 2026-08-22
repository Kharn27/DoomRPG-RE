# ESP32 MAP_INTRO native SAVEGAME route-owner milestone

Branch: `agent/esp32-map1-native-save-route`

Base merged `main`:

```text
PR   = #59 — native EV_GIVEMAP automap state
main = 9891a25d700f9ffe1be044ac4a7629c3487604ec
```

Firmware candidate content:

```text
42497b80c6158300ec3fa7b8eb8af6cee643f59e
```

Status: **IMPLEMENTED; REAL-CYD HARDWARE VALIDATION PENDING**.

## Objective

Own exact `27 / EV_SAVEGAME` command semantics without pretending that this opcode itself serializes a save file.

Recovered behavior shows that opcode 27 only captures a future save destination:

```text
map name
x tile -> 32 + (x << 6)
y tile -> 32 + (y << 6)
angle byte
```

The actual filesystem save is performed later by `Game_saveState()`.

Native target:

```text
real EV_SAVEGAME bytecode
 -> resolve one bounded map string from /DoomRPG-ESP32.pak
 -> copy only that <=31-byte destination name into caller-owned route state
 -> store destination x/y + angle + provenance
 -> return handled/remove-if-handled metadata
 -> no save-file write
 -> no legacy Game mutation
```

Only opcode 27 is supported here. Actual save serialization, filesystem ownership, map transition, legacy `Game_saveState()`, entity/world/render mutation, gameplay and `ST_PLAYING` remain forbidden.

## Recovered legacy behavior

`Game_executeEvent()` handles opcode 27 as:

```c
int i11 = arg1 >> 8;
game->newDestX = i11 & 255;
game->newDestX = 32 + (game->newDestX << 6);
game->newDestY = (i11 >> 8) & 255;
game->newDestY = 32 + (game->newDestY << 6);
game->newAngle = (i11 >> 16) & 255;
strncpy(game->newMapName,
        game->doomRpg->render->mapStringsIDs[arg1 & 255],
        sizeof(game->newMapName));
```

It then reaches the normal `Game_executeEvent()` handled return.

So `arg1` is packed as:

```text
bits  0.. 7 = map string ID
bits  8..15 = x tile
bits 16..23 = y tile
bits 24..31 = angle
```

The outer `Game_runEvent()` still removes a handled command when source `arg2 & 0x200`; native SAVE route exposes `removeCommandIfHandled` but does not mutate `EspMapScriptState` yet.

## Why the destination name is copied

The first implementation attempt stored only `EspMapStringRef`. Static recovery found that this is not a permanent-enough owner.

For a normal map transition, `DoomCanvas_loadMedia()` executes:

```text
DoomCanvas_unloadMedia()
Render_freeRuntime()
Game_unloadMapData()
...
```

The later `Game_saveState()` consumer can therefore run after current-map runtime teardown has started. Legacy deliberately stores `newMapName[32]` inside `Game_t`, making the destination name independent of the source map string table.

The native permanent owner follows that lifetime semantics instead of keeping a dangling map-local ref:

```text
char mapName[32]
```

Only one small string is copied; there is still no map-wide text materialization.

Pre-hardware correction history:

```text
8d6e7405d59aba7ba7699a3289de4094cf2a3341
  initial zero-copy route candidate

42497b80c6158300ec3fa7b8eb8af6cee643f59e
  corrected durable inline route-name ownership
```

Only `42497b80...` is the firmware candidate for hardware validation.

## Permanent native API

New files:

```text
ESP32/include/esp_map_save_route.h
ESP32/src/esp_map_save_route.c
```

State:

```c
typedef struct EspMapSaveRouteState_s {
    char mapName[32];
    uint16_t destinationX;
    uint16_t destinationY;
    uint16_t sourceEventIndex;
    uint16_t globalCommandIndex;
    uint8_t sourceCommandOffset;
    uint8_t angle;
    uint8_t rawX;
    uint8_t rawY;
    uint8_t mapNameLength;
    uint8_t active;
} EspMapSaveRouteState;
```

Expected classic ESP32 ABI:

```text
ownerBytes = 46
```

Result:

```c
typedef struct EspMapSaveRouteResult_s {
    uint16_t sourceEventIndex;
    uint16_t globalCommandIndex;
    uint16_t mapStringIndex;
    uint16_t destinationX;
    uint16_t destinationY;
    uint8_t sourceCommandOffset;
    uint8_t rawX;
    uint8_t rawY;
    uint8_t angle;
    uint8_t legacyReturnValue;
    uint8_t removeCommandIfHandled;
} EspMapSaveRouteResult;
```

Expected ABI:

```text
resultBytes = 16
```

Permanent calls:

```text
EspMapSaveRoute_reset()
EspMapSaveRoute_isActive()
EspMapSaveRoute_apply()
```

The permanent source depends only on native pack/runtime/string/event APIs. It has no `Game`, `Render`, `Entity`, Hud, Player, Sound or DoomCanvas dependency.

## Bounded string / I/O rule

A valid SAVEGAME command resolves `arg1 & 0xff` through `EspMapStrings_getRef()` and requires:

```text
mapNameLength < 32
```

The executor then performs one `EspMapStrings_read()` into the 32-byte owner.

This is deliberate bounded native-pack I/O:

```text
/DoomRPG-ESP32.pak only
one source string only
no allocation
no ZIP
no decompression
no map-wide string table copy
```

If the pack is closed or the read fails, the executor returns `ESP_MAP_SAVE_ROUTE_IO_ERROR`, zeroes the result and preserves the previous owner exactly.

## Lifetime contract

After a successful apply, the owner contains all data required by a future save consumer even if the source map runtime is later destroyed:

```text
mapName bytes are inline
destinationX/Y are inline
angle is inline
no future source-map string lookup required
```

The temporary probe explicitly closes the PAK while a sample owner remains populated and verifies that the owner content/fingerprint stays unchanged. A new apply with pack closed must fail closed because it cannot capture a new route, but the already-captured route must survive.

## Temporary real-CYD probe

New files:

```text
ESP32/include/native_map1_save_route_probe.h
ESP32/src/native_map1_save_route_probe.c
```

The stage runs only after the hardware-proven GIVEMAP stage and requires all inherited owners to remain canonical:

```text
arenaFNV            = c3882516
mapStateFNV         = cd99b98e
scriptFNV           = f9e3d9df
lineStateFNV        = e5e74861
lineTextureStateFNV = f1fc1875
automapStateFNV     = 669b1aa7
```

### Complete real SAVEGAME corpus

The probe scans all 93 events / 265 bytecodes and discovers every opcode-27 command.

For every real ref it requires:

```text
state-only opcode executor -> UNSUPPORTED
canonical descriptor + command provenance
map string ID resolves
map string payload < 32 bytes
bounded PAK read succeeds
raw x/y/angle exactly match packed arg1
destination = 32 + (tile << 6)
legacyReturnValue = 1
removeCommandIfHandled exactly matches arg2 & 0x200
```

Every route apply is reset immediately and must return to the exact initial owner fingerprint.

Hardware will establish rather than predeclare:

```text
SAVEGAME ref count
removable ref count
map-name total bytes
max map-name length
ownerFNV
resultFNV
contentFNV
initial owner FNV
first sample owner FNV
first real command/sample route
```

### Reapply proof

The first real command is applied twice while the pack is open:

```text
first apply -> OK / handled=1
second apply -> OK / handled=1
owner bytes exactly identical
result bytes exactly identical
```

Then the owner is reset.

### Cross-map lifetime / closed-pack proof

After the complete audit:

```text
sample owner copied to parked test state
PAK closed
sample owner must remain byte-exact and active
new apply with PAK closed -> IO_ERROR
previous owner remains exact
result remains zero
owner reset before PARK
```

Expected proof fields:

```text
ownerSurvivesPackClose=1
closedPack=1
activeAtPark=0
```

## Fail closed / atomicity

Expected hardware proof:

```text
unsupported=1
badOffset=1
badDescriptor=1
nullDescriptor=1
nullEntry=1
nullState=1
nullResult=1
closedPack=1
reset=1
stateAtomic=yes
```

Strings >=32 bytes also fail closed by permanent API contract, even if MAP_INTRO has no such real SAVEGAME ref.

## RAM boundary

Persistent native heap entering this milestone is hardware-proven at:

```text
immutable arena         14112 B
mutable tile state       1040 B
mutable script state      100 B
mutable line state        136 B
mutable texture state      76 B
mutable automap state     120 B
------------------------------
total                   15584 B
```

SAVE route is caller-owned/static value state:

```text
ownerBytes       = 46 expected
resultBytes      = 16 expected
persistent heap  = 0 expected
```

The probe opens the native pack temporarily for bounded string reads, so hardware will measure a transient pack-open heap cost. After close:

```text
heap8 after == heap8 before
largest8 after == largest8 before
persistentHeapBytes=0
```

No save file is created or modified by this milestone.

## Legacy integrity witness

In addition to all inherited witnesses, this milestone hashes exactly the legacy fields opcode 27 would otherwise mutate:

```text
Game.newMapName[32]
Game.newDestX
Game.newDestY
Game.newAngle
```

The before/after `legacy saveRouteFNV` must remain identical.

Other protected state:

```text
framebuffer
arena / map / script / line / texture / automap owners
legacy notebook / keys / Hud / password / continuation
legacy Render runtime clear
pack closed at PARK
entities=0
monsters=0
ST_PLAYING not reached
```

## Expected Serial family

```text
[MAPSAVEROUTEPROBE] ARMED ...

=== Doom RPG ESP32-native MAP_INTRO SAVEGAME route owner ===
[MAPSAVEROUTEPROBE] CONTRACT ...
[MAPSAVEROUTE] READY refs=... removable=... ownerBytes=46 resultBytes=16 stateExecRefused=... mapNameBytes=... maxMapName=... ownerFNV=... resultFNV=... contentFNV=... elapsed=...ms
[MAPSAVEROUTE] SAMPLE cmd=... event=... off=... arg1=... arg2=... mapString=... name="..." nameLen=... tile=...,... dest=...,... angle=... handled=1 removeIfHandled=...
[MAPSAVEROUTE] STATE initialOwnerFNV=... sampleOwnerFNV=... rollback=.../... reapplyExact=1 ownerSurvivesPackClose=1 activeAtPark=0
[MAPSAVEROUTE] FAILCLOSED unsupported=1 badOffset=1 badDescriptor=1 nullDescriptor=1 nullEntry=1 nullState=1 nullResult=1 closedPack=1 reset=1 stateAtomic=yes
[MAPSAVEROUTE] IO entry=/intro.bsp size=21823 crc32=623f34e4 heapOpen=... transientHeapCost=... largestOpen=... packIO=yes boundedNameRead=yes persistentHeapBytes=0 saveFileWrite=no
[MAPSAVEROUTEPROBE] RAM heap8=...->... delta=0 largest8=...->... delta=0 ...
[MAPSAVEROUTEPROBE] LEGACY ... saveRouteFNV=...->... legacyRuntimeClear=yes
[MAPSAVEROUTEPROBE] PARK ... nativeSaveRoute=yes ownerBytes=46 resultBytes=16 persistentBytes=0 routeLifetimeCrossMap=yes legacySaveRouteMutation=no saveFileWrite=no worldMutation=no framebufferMutation=no entities=0 monsters=0 noGameplay=yes
[ALIVE] ...
```

Use normal optimized PlatformIO environment:

```text
esp32-cyd
```

No CI status is currently published for firmware candidate `42497b80c6158300ec3fa7b8eb8af6cee643f59e`. No local build or hardware PASS is claimed.

## Remaining MAP_INTRO families after PASS

If SAVEGAME passes, still unowned:

```text
2  EV_CHANGEMAP
7  EV_SHOW
18 EV_HIDE
```

Do not pre-authorize the next family before PASS + merge recovery.
