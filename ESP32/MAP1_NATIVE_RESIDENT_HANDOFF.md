# ESP32 native reversible resident handoff milestone

Branch: `agent/esp32-native-resident-handoff`

Base merged `main`:

```text
PR   = #66 — native transition preflight
main = 9f981f490282200f216aef66d22608d2244beb00
```

Hardware-tested firmware:

```text
090d7dac5c255fc42a3d12fb3441053fdefe681b
```

Status: **REAL-CYD HARDWARE PASS / MERGE-READY**.

## Objective and result

This milestone proves that the current ESP32-native resident-map owner set can be torn down and rebuilt in a dependency-safe, RAM-safe order without falling back to legacy `DoomCanvas_loadMap()`.

The real classic CYD executed the complete reversible transaction:

```text
Entrance resident
 -> explicit resetAll()
 -> EMPTY1
 -> build full Junction resident runtime + mutable owners
 -> capture Junction
 -> explicit resetAll()
 -> EMPTY2
 -> rebuild Entrance
 -> require exact source restoration
```

The hardware PASS proves for the first time that `/junction.bsp` can exist as the complete current native resident owner set. Junction is still temporary in this milestone: PARK leaves Entrance restored and no committed gameplay map swap occurs.

No legacy `Game/Menu/Player/Render` mutation, no `ST_PLAYING`, no gameplay entity/monster population and no legacy map loader are introduced.

## Permanent resident lifecycle

Files:

```text
ESP32/include/esp_map_resident_lifecycle.h
ESP32/src/esp_map_resident_lifecycle.c
```

Permanent API:

```text
EspMapResidentLifecycle_resetAll()
EspMapResidentLifecycle_isEmpty()
EspMapResidentLifecycle_isReady()
EspMapResidentLifecycle_capture()
EspMapResidentLifecycle_loadFromEmpty()
```

### Explicit teardown rule

`loadFromEmpty()` never hides a destructive source teardown. A live resident set returns `ESP_MAP_RESIDENT_NOT_EMPTY` before PAK I/O.

The only destructive primitive is explicit:

```text
EspMapResidentLifecycle_resetAll()
```

Dependency-safe release order:

```text
sprite topology
 -> automap state
 -> line texture state
 -> line state
 -> script state
 -> map/tile state
 -> immutable runtime arena
```

Empty-only build order:

```text
EspMapRuntime
 -> EspMapState
 -> EspMapScriptState
 -> EspMapLineState
 -> EspMapLineTextureState
 -> EspMapAutomapState
 -> EspMapSpriteTopology + /entities.db
```

The lifecycle owns one temporary PAK session, closes it before return and resets all partially built owners on failure. `PACK_BUSY` preserves caller ownership of an already-open PAK.

## Pointer-free resident snapshot

```text
EspMapResidentSnapshot = 96 B
```

The snapshot contains only logical payload sizes, FNVs and cardinalities. No pointer or legacy object is retained.

Important semantic distinction recovered during hardware validation:

```text
runtimeArenaBytes = logical payload
heap delta        = actual allocator cost
```

The original runtime milestone already established Entrance as:

```text
runtime payload = 14095 B
runtime heap    = 14112 B
runtime overhead= 17 B
```

The first handoff candidate incorrectly used `14112` as snapshot payload. Diagnostic v2 isolated that single mismatch before any teardown. Corrected v3 uses the permanent snapshot contract correctly.

## Hardware-proven Entrance source snapshot

Real-CYD source capture:

```text
snapshotBytes = 96
snapshotFNV   = b3811f3d

runtime arena = 14095 B
map state     = 1024 B
script state  = 81 B
line state    = 120 B
texture state = 60 B
automap state = 103 B
topology      = 2408 B
----------------------
payload total = 17891 B
```

Actual resident heap cost remains the inherited hardware canon:

```text
18008 B
```

Total allocator overhead across the seven owners is therefore:

```text
18008 - 17891 = 117 B
```

Exact source fingerprints/cardinalities:

```text
arenaFNV      = c3882516
mapStateFNV   = cd99b98e
scriptFNV     = f9e3d9df
lineFNV       = e5e74861
textureFNV    = f1fc1875
automapFNV    = 669b1aa7
topologyFNV   = 3f321e43

nodes=223
lines=480
sprites=344
events=93
byteCodes=265
strings=94
entities=220
enemies=30
destructibles=13
```

## Source release / fail-closed gates

Before destruction the hardware proved:

```text
NOT_EMPTY gate     = 1
invalid resource   = 1
capture(NULL)      = 1
PACK_BUSY          = 1
busy result zero   = 1
caller owns PAK    = 1
empty atomic       = yes
```

After explicit `resetAll()`:

```text
SOURCE heap8 = 65592
EMPTY1 heap8 = 83600
released     = 18008 B
sourcePayload= 17891 B
allocatorOverhead=117 B
allOwnersEmpty=yes
largest8     = 34804 -> 34804
```

This proves the complete current native resident set is released exactly and does not consume the largest free 8-bit block.

## Hardware-proven Junction resident owner set

Source BSP remains the preflight canon:

```text
resource=/junction.bsp
resourceMapId=9
gameplayLoadMapId=2
bytes=21051
crc32=4a2c5800
sourceFNV=fefaf5ca
nodes=77 lines=207 sprites=48 events=66 byteCodes=319 strings=126
```

Full native resident build:

```text
snapshotBytes = 96
snapshotFNV   = bc9071e9
buildElapsed  = 121 ms

runtime       = 8867 B
map state     = 1024 B
script state  = 73 B
line state    = 52 B
texture state = 26 B
automap state = 32 B
topology      = 336 B
---------------------
payload       = 10410 B
actual heap   = 10540 B
allocator ovh = 130 B
```

Heap/largest while Junction is resident:

```text
heap8    = 73060
largest8 = 34804
```

Permanent builders emitted:

```text
[MAPRT] ARENA bytes=8867
[MAPRT] READY populateReadCalls=44 arenaFNV=bc432a0f
[MAPSTATE] READY bytes=1024 fnv=c5cdfc04
[MAPLINESTATE] READY storageBytes=52 stateFNV=3658710d
[MAPLINETEX] READY storageBytes=26 stateFNV=537319ad
[MAPAUTOMAP] READY storageBytes=32 stateFNV=0b2ae445
```

Hardware-proven Junction resident fingerprints:

```text
runtimeFNV  = bc432a0f
mapStateFNV = c5cdfc04
scriptFNV   = bc9b18ff
lineFNV     = 3658710d
textureFNV  = 537319ad
automapFNV  = 0b2ae445
topologyFNV = d6e8df7d
snapshotFNV = bc9071e9
```

Hardware-proven Junction compact topology:

```text
sprites       = 48
entities      = 30
enemies       = 0
destructibles = 3
```

These are compact native topology semantics only; legacy `Game.entities` and `Game.monsters` remain zero.

## Target release / fragmentation proof

After releasing Junction:

```text
EMPTY2 heap8    = 83600
EMPTY2 largest8 = 34804
targetReleased  = 10540 B
emptyExact      = 1
fragmentationDelta=0
```

Therefore:

```text
EMPTY2 heap/largest == EMPTY1 heap/largest
```

The target round-trip introduces no final allocator drift and no largest-block degradation.

## Exact Entrance restoration

The source was rebuilt through the same permanent `loadFromEmpty()` API.

Hardware:

```text
restored snapshotFNV = b3811f3d
exact                 = 1
heap8                  = 65592 -> 65592
largest8               = 34804 -> 34804
sourceCost             = 18008 B
payload                = 17891 B
```

Whole RAM sequence:

```text
SOURCE   heap8=65592 largest8=34804
EMPTY1   heap8=83600 largest8=34804
JUNCTION heap8=73060 largest8=34804
EMPTY2   heap8=83600 largest8=34804
RESTORED heap8=65592 largest8=34804

sourceCost   = 18008 B
junctionCost = 10540 B
finalDelta   = 0
```

This is the first hardware proof that a complete target native resident set can replace the source allocation and that the source can then be reconstructed exactly on the no-PSRAM classic CYD.

## Legacy/frame integrity

Same-build witnesses remained unchanged across the destructive native round-trip:

```text
playerFNV     0b2ae445 -> 0b2ae445
transitionFNV f450c49f -> f450c49f
frameFNV      ee9d9dbc -> ee9d9dbc

legacyRuntimeClear=yes
sourceTeardownNativeOnly=yes
DoomCanvas_loadMapCalled=no
menuMutation=no
legacyPlayerMutation=no
```

Final PARK:

```text
state=9
page=3
nativeResidentLifecycle=yes
reversibleHandoff=yes
junctionResidentProven=yes
sourceRestored=yes
targetLeftResident=no
packClosed=yes
persistentBytes=18008
mapSwapCommitted=no
entities=0
monsters=0
noGameplay=yes
```

Stable post-PASS heartbeat:

```text
heap=131356
heap8=65592
largest8=34804
```

## Pre-Start Game heartbeat context

The same firmware also showed stable pre-Start-Game heartbeats before the native Entrance lifecycle was mounted:

```text
heap=93948
heap8=28184
largest8=18420
```

These values are useful same-run lifecycle context but are not resident-map allocation canons. The resident handoff proof begins only after the normal Start Game chain has built and proven Entrance.

## Validation history

```text
v1 f71520281254ff9d0b2d5e4be1b3611e29ca87c4
   -> safe FAIL at combined source boundary; no teardown

v2 13f7f3787bf6de08595167081af1ea628ae30946
   -> diagnostic safe FAIL; isolated 14095 payload vs 14112 heap-cost confusion

v3 090d7dac5c255fc42a3d12fb3441053fdefe681b
   -> REAL-CYD HARDWARE PASS
```

No local PlatformIO build or CI result is claimed for this milestone; the real CYD Serial log is the hardware source of truth.

## Boundary after PASS

Hardware ownership now includes:

```text
generic explicit resident lifecycle
complete temporary Junction native residency
explicit Entrance teardown
explicit Junction teardown
exact Entrance reconstruction
no-fragmentation resident round-trip
```

Still outside:

```text
committing Junction as the active map after the stats-menu pause
native transition state machine / point-of-no-return ownership
spawn placement and loadType handoff
actual stats-menu presentation/input
full native entity/monster gameplay beyond compact topology
ST_PLAYING progression
native gameplay renderer
sound playback
```

The next bounded milestone should use this proven lifecycle to own a **committed native transition state machine** rather than reimplementing teardown/build ordering.

## Merge recommendation

```text
MERGE agent/esp32-native-resident-handoff
```

Hardware-tested firmware is `090d7dac5c255fc42a3d12fb3441053fdefe681b`. Every later commit must remain documentation-only until merge.
