# ESP32 MAP_INTRO native SAVEGAME route-owner milestone

Branch: `agent/esp32-map1-native-save-route`

Base merged `main`:

```text
PR   = #59 — native EV_GIVEMAP automap state
main = 9891a25d700f9ffe1be044ac4a7629c3487604ec
```

Hardware-tested firmware content:

```text
42497b80c6158300ec3fa7b8eb8af6cee643f59e
```

Status: **REAL-CYD HARDWARE PASS / MERGE-READY**.

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

Native behavior:

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

So `arg1` is packed as:

```text
bits  0.. 7 = map string ID
bits  8..15 = x tile
bits 16..23 = y tile
bits 24..31 = angle
```

The outer `Game_runEvent()` may remove a handled command when source `arg2 & 0x200`; native SAVE route exposes `removeCommandIfHandled` but does not mutate `EspMapScriptState` yet.

## Durable cross-map ownership

The first implementation attempt stored only `EspMapStringRef`. Static recovery found that normal map loading can tear down the source map runtime before the later save consumer uses the route. The firmware was corrected before hardware validation.

Pre-hardware history:

```text
8d6e7405d59aba7ba7699a3289de4094cf2a3341
  initial zero-copy route candidate

42497b80c6158300ec3fa7b8eb8af6cee643f59e
  corrected durable inline route-name ownership
```

Only `42497b80...` is hardware-tested.

The permanent owner therefore keeps:

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

Hardware-proven classic ESP32 ABI:

```text
EspMapSaveRouteState  = 46 B
EspMapSaveRouteResult = 16 B
persistent heap       = 0 B
```

The permanent source depends only on native pack/runtime/string/event APIs. It has no `Game`, `Render`, `Entity`, Hud, Player, Sound or DoomCanvas dependency.

## Bounded string / I/O rule

A valid SAVEGAME command resolves `arg1 & 0xff` through `EspMapStrings_getRef()` and requires:

```text
mapNameLength < 32
```

The executor performs exactly one `EspMapStrings_read()` into the 32-byte owner.

```text
/DoomRPG-ESP32.pak only
one source string only
no allocation
no ZIP
no decompression
no map-wide string table copy
no save-file write
```

If the pack is closed or the read fails, the executor returns `ESP_MAP_SAVE_ROUTE_IO_ERROR`, zeroes the result and preserves the previous owner exactly.

## Real-CYD corpus proof

Normal optimized environment: `esp32-cyd`.

The real classic no-PSRAM CYD established:

```text
refs             = 1
removable        = 0
ownerBytes       = 46
resultBytes      = 16
stateExecRefused = 1
mapNameBytes     = 13
maxMapName       = 13
ownerFNV         = 06ea6ea8
resultFNV        = c2ecb064
contentFNV       = 725845aa
elapsed          = 37 ms
```

Canonical real command:

```text
global command = 1
event          = 1
command offset = 0
arg1           = 401d0f00
arg2           = 00000100
mapString      = 0
mapName        = "/junction.bsp"
mapNameLength  = 13
raw tile       = 15,29
destination    = 992,1888
angle          = 64
handled        = 1
removeIfHandled= 0
```

## Owner mutation / rollback / lifetime proof

Hardware fingerprints:

```text
initialOwnerFNV = 9a00a0bd
sampleOwnerFNV  = 7e69bd59
rollback        = 1 / 1
reapplyExact    = 1
```

The sample owner was captured while the PAK was open. The PAK was then closed and the already-captured owner remained byte-exact and active:

```text
ownerSurvivesPackClose = 1
```

A new apply with the pack closed failed closed without changing the existing route:

```text
closedPack = 1
stateAtomic = yes
```

The owner was reset before PARK:

```text
activeAtPark = 0
```

This proves the route lifetime crosses source-map/pack lifetime correctly.

## Fail-closed proof

Real hardware proved:

```text
unsupported    = 1
badOffset      = 1
badDescriptor  = 1
nullDescriptor = 1
nullEntry      = 1
nullState      = 1
nullResult     = 1
closedPack     = 1
reset          = 1
stateAtomic    = yes
```

## Native-pack / RAM proof

Real bounded PAK witness:

```text
entry              = /intro.bsp
size               = 21823
crc32              = 623f34e4
heapOpen           = 63832
transientHeapCost  = 4376 B
largestOpen        = 34804
packIO             = yes
boundedNameRead    = yes
persistentHeapBytes= 0
saveFileWrite      = no
```

Before/after the complete stage:

```text
heap8      = 68208 -> 68208
largest8   = 34804 -> 34804
frameFNV   = 99102464 -> 99102464
arenaFNV   = c3882516 -> c3882516
mapStateFNV= cd99b98e -> cd99b98e
scriptFNV  = f9e3d9df -> f9e3d9df
automapFNV = 669b1aa7 -> 669b1aa7
```

Persistent native heap therefore remains exactly:

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

SAVE route adds no persistent heap allocation.

## Legacy integrity proof

The probe hashes the exact legacy fields opcode 27 would otherwise mutate:

```text
Game.newMapName[32]
Game.newDestX
Game.newDestY
Game.newAngle
```

Real hardware witness:

```text
legacy saveRouteFNV = 9bcfe135 -> 9bcfe135
```

Other protected witnesses also stayed exact:

```text
notebookFNV       = 4d7705c5 -> 4d7705c5
keys              = 00000000 -> 00000000
hudFNV            = 505b1255 -> 505b1255
passwordCanvasFNV = 214171cf -> 214171cf
continuationFNV   = e2ba14a5 -> e2ba14a5
legacyRuntimeClear= yes
```

No legacy route/world/render/entity mutation occurred.

## Final PARK boundary

Hardware proved:

```text
nativeSaveRoute=yes
ownerBytes=46
resultBytes=16
persistentBytes=0
routeLifetimeCrossMap=yes
legacySaveRouteMutation=no
saveFileWrite=no
worldMutation=no
framebufferMutation=no
entities=0
monsters=0
noGameplay=yes
```

Stable post-PARK heartbeats:

```text
uptime=135324 ms heap=133972 heap8=68208 largest8=34804 all reported subsystems ready
uptime=140327 ms heap=133972 heap8=68208 largest8=34804 all reported subsystems ready
```

Absolute heap/frame values can differ across builds; acceptance uses same-build stability plus canonical fingerprints.

## Hardware acceptance status

The real CYD proved the complete SAVEGAME corpus, exact map-name/x/y/angle decoding, durable cross-map route ownership, exact rollback/reapply, closed-pack fail-closed behavior, zero persistent heap, no save-file write, legacy integrity and stable PARK heartbeats.

This milestone is **REAL-CYD HARDWARE PASS / MERGE-READY**.

Hardware-tested firmware content:

```text
42497b80c6158300ec3fa7b8eb8af6cee643f59e
```

All later commits must remain documentation-only unless another firmware is flashed.

## Remaining MAP_INTRO families

After SAVEGAME, still unowned:

```text
2  EV_CHANGEMAP
7  EV_SHOW
18 EV_HIDE
```

Do not preselect the next family before merge recovery.