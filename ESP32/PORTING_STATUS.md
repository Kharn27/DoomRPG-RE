# Doom RPG ESP32 CYD porting status

Authoritative recovery/status file for the classic ESP32-2432S028R port.
Repository state wins over chat history. Serial logs from the real classic CYD
are the final runtime authority.

## Git boundary — LOCKED milestone

```text
main at branch creation = bc9e0435f6ee6ea23a68cb4bdbb18324440cf360
branch = agent/esp32-native-gameplay-hub-touch-ui
base main = bc9e0435f6ee6ea23a68cb4bdbb18324440cf360
hardware-tested code boundary = 515bb4b0122c55348251eee726110ec946a4aeb6
status = REAL-CYD GAMEPLAY HUB TOUCH UI + HAND MENU BUTTON PASS
branch policy = LOCKED; docs-only tail only
```

`515bb4b0...` is the exact code boundary exercised on the real classic CYD.
Commits after that SHA must remain documentation-only until merge.

Normal GitHub Actions `esp32-cyd` run `34091004475` / run #177 completed
successfully on this exact SHA and produced the firmware artifact. CI is
compile/link evidence only; hardware serial logs remain authoritative.

Latest detailed record:

- [`MILESTONE_NATIVE_GAMEPLAY_HUB_TOUCH_UI.md`](MILESTONE_NATIVE_GAMEPLAY_HUB_TOUCH_UI.md)

After merge, read the real GitHub `main` SHA again before creating the next
`agent/*` branch.

## Permanent architecture and hard invariants

```text
Doom RPG original data/behavior
 -> ESP32-native parsers/catalogs
 -> compact immutable EspMapRuntime
 -> small explicit mutable owners
 -> native event/script engine
 -> native gameplay
 -> native renderer
```

Hardware / memory invariants:

```text
board       = ESP32-2432S028R classic CYD
MCU         = ESP32-D0WD-V3 dual core 240 MHz
flash       = 4 MB
PSRAM       = none
framebuffer = 160x120 RGB565 = 38400 B
shapeData   = NULL
mediaTexels = NULL
```

Do not reintroduce map-wide legacy texels, desktop pointer-heavy world ownership,
or runtime ZIP dependence for migrated gameplay/map data. `/DoomRPG.zip` remains
transitional startup/reference debt only; native gameplay/map access must not
regress to it.

## Native asset backing — hardware PASS

Active gameplay storage path:

```text
/DoomRPG-ESP32.pak on microSD
 -> generic requested-map raw internal-flash slot
 -> 19 KiB resident RAM cache
 -> native gameplay / renderer
```

Permanent preparation API:

```text
EspAssetPack_mapFlashPrepare(targetMapId)
```

Classic CYD raw partition layout:

```text
nvs       0x009000  size 0x005000
otadata   0x00e000  size 0x002000
app0      0x010000  size 0x140000  = 1310720 B
spiffs    0x150000  size 0x2A0000  = 2752512 B raw slot
coredump  0x3F0000  size 0x010000
```

The partition named `spiffs` is intentionally not mounted as a filesystem and is
managed through `esp_partition_*`.

Entrance raw-slot witness:

```text
pack = 2457398 B
entries = 241
index = 4820 B
metadata = 12288 B
excluded non-current BSPs = 12 / 203811 B
staged payload = 2248743 B
partition = 2752512 B
headroom = 491481 B
[MAPFLASH] COPY indexFNV=3a51cc4d payloadFNV=9ec04e22 verified=yes
```

Requested-map reuse witness:

```text
[MAPFLASH] REUSE HIT requestedMap=1 current=/intro.bsp cachedMap=1
           sourceIndexFNV=3a51cc4d payloadFNV=9ec04e22
           verifyUs=361875 rebuild=no
```

Active gameplay never silently falls back to SD.

## Entrance canonical witness

```text
resourceMapId = 1
resource = /intro.bsp
name = Entrance
sourceBytes = 21823
crc32 = 623f34e4
sourceFNV = d5cc751f
runtime arena = 14095 B
runtimeFNV = c3882516
resident payload = 17891 B
spawn tile = 904
spawn direction = 64
spawn position = 544,1824
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

Retained owner fingerprints:

```text
mapStateFNV = cd99b98e
scriptFNV   = f9e3d9df
lineFNV     = e5e74861
textureFNV  = f1fc1875
automapFNV  = 669b1aa7
topologyFNV = 3f321e43
```

## Resident cache baseline retained

```text
owner = 23592 B
payload = 19456 B (19 KiB)
range records = 288
range record = 12 B
resident entry slots = 24
large exact range = 2048 B
```

`PlatformVideo_present()` remains around 34.4 ms and is not the current
optimization target.

## Current hardware-owned gameplay frontier

Validated behavior includes movement/turn/strafe, native collision/topology,
SELECT event-first routing, state/event opcodes already migrated, regular door
animation, mutable line textures, native weapon presentation/combat, shared
52 B PlayerState, pickups/resources, hazard touch, feedback flashes, compact
MonsterState/MonsterPosition, live monster movement/topology relink, active-list
sequencing, raw-flash gameplay backing and requested-map reuse.

The latest permanent UI frontier adds:

```text
native gameplay HUB owner = 28 B
shared PlayerState owner = 52 B
read-only Inventory + Status pages
visible native INV / STATUS touch tabs
visible Inventory touch cards
HUB viewport touch ownership isolated from world 3x3 controls
unsupported HUB actions absorbed / fail closed
visible MENU close affordance uses original Doom RPG p.bmp hand
bounded MENU underlay = 32x20 RGB565 = 1280 B
all other HUD pixels protected by fingerprint
MENU close restores underlay bit-exact
normal world rerender restores exact pre-HUB world frame
```

Detailed recent records:

- [`MILESTONE_NATIVE_MONSTER_ACTIVE_SEQUENCE.md`](MILESTONE_NATIVE_MONSTER_ACTIVE_SEQUENCE.md)
- [`MILESTONE_NATIVE_GAMEPLAY_HUB_V2.md`](MILESTONE_NATIVE_GAMEPLAY_HUB_V2.md)
- [`MILESTONE_NATIVE_GAMEPLAY_HUB_TOUCH_UI.md`](MILESTONE_NATIVE_GAMEPLAY_HUB_TOUCH_UI.md)

## Latest real-CYD HUB touch UI witness

The exact hardware-tested code boundary is `515bb4b0...`.

Deterministic page/menu witnesses:

```text
Inventory frame = 79bbc30d
Status frame    = eea0759d
Inventory again = 79bbc30d
hudProtectedFNV = bd7588ae
menuZoneFNV     = 109b46aa
playerFNV       = e745fce9
```

Representative Inventory paint:

```text
[HUB] FRAME paint=3 page=inventory row=0 frame=79bbc30d
      viewport=160x80/y20..99
      hudProtected=bd7588ae preserved=yes
      menuButton=hand asset=p.bmp frame=0
      menuZone=109b46aa underlayBytes=1280
      playerFNV=e745fce9 exact=yes
      packClosed=yes presented=1 mutation=no turn=no
```

Status reproduced the same protected/menu fingerprints and exact player state.
Returning to Inventory reproduced `79bbc30d` exactly.

A PASS_TURN touch while the HUB was active was absorbed:

```text
[HUB] IGNORE action=14 page=inventory row=0
      worldDispatch=blocked mutation=no turn=no
[RESIDENTGAMEPLAY] HUB-INPUT ... action=PASS_TURN status=IGNORED
      worldDispatch=blocked turnAdvance=no
```

Closing through the visible hand restored both the bounded MENU underlay and the
complete HUD exactly:

```text
[HUB] CLOSE n=3 page=inventory
      playerFNV=e745fce9->e745fce9 exact=yes
      menuUnderlayRestore=exact
      hudBands=6c2aa46f expected=6c2aa46f exact=yes
      packClosed=yes
```

The resident gameplay compositor then rebuilt the exact world frame:

```text
pre-HUB world frame  = 22397b55
post-HUB world frame = 22397b55
```

No player/world mutation or turn advance occurred.

## RAM witness at current boundary

The supplied real-CYD session remained flat before, during and after page
switching, ignored HUB input and return to gameplay:

```text
heap = 87988
heap8 = 22256
largest8 = 14324
hub owner = 28 B
player owner = 52 B
MENU underlay = 1280 B
full framebuffer snapshot = 0 B
```

The previous HUB-v2 witness was `89280 / 23548 / 20468`; the current free heap is
1292 B lower after adding the bounded 1280 B MENU underlay and bookkeeping. The
largest 8-bit block is now 14324 B and must be treated as the current canonical
hardware witness before future RAM-heavy milestones.

Audio remains deferred. Do not enable it without its own RAM milestone.

## Intentionally deferred families

```text
production SAVEGAME / CHANGEMAP transition ownership
pre-arm first-frame/HUD SD startup path
L1 range-record eviction/recycle redesign
pickup sound playback / got-face presentation
movement/PASS_TURN hazard secondary burn text / pain face / shake / sound
complete mixed movement-tile resource/hazard ordering
action XP migration
materialized monster drops
corpse-pile trimming
monster movement interpolation/animation
simultaneous attack-ready multi-monster ordering
ranged >=217 multi-active expansion beyond current single-candidate boundary
unsupported special calcPath plane corpus
special subtype-10 AI
player lethal/death transition
three-shot / multi-loop monster attack presentation
monster projectile visuals
monster attack message / sound
player-pain face / shake / sound
status-warning presentation
chaingun/plasma multi-loop player mechanics
rocket/BFG radius damage
familiar weapon slots / hazard redirection
generic type-12 destructible combat
special death consequences
Kronos-specific semantics
password input
EV_GIVEMAP production route
EV_CHECK_KEY production route
HUB real weapon/ammo/item names and richer content layout
HUB weapon selection
HUB consumable confirmation/use + turn consumption
HUB Notebook / Automap / Save / Load / Options / store
```

Different mechanical families stay fail-closed rather than being enabled through
legacy desktop/J2ME ownership.

## CHANGEMAP remains deferred

Entrance event 1 / tile 69 remains recovered but intentionally not live:

```text
SAVEGAME -> /junction.bsp, targetMapId 9, savePos 992,1888 angle 64
CHANGEMAP -> /junction.bsp, targetMapId 9, showStats 1, spawnParam 0
OPENLINE -> third eligible command
```

The storage layer can prepare map 9, but the actual production transition still
needs bounded teardown/load/spawn/state-transfer ownership.

## Next direction after merge

Do **not** continue code on this locked branch.

After the user announces the merge:

1. read actual GitHub `main` and exact SHA;
2. re-read this file, `DOCUMENTATION.md` and
   `MILESTONE_NATIVE_GAMEPLAY_HUB_TOUCH_UI.md`;
3. create a fresh `agent/*` from that exact main SHA;
4. recover the next bounded legacy/UI family before coding.

Strong next HUB milestone: improve **content readability**, not input plumbing.
Use the now-proven visible touch layer to expose real Doom RPG weapon/ammo/item
labels through bounded native catalog lookups while preserving read-only
semantics, the 28 B HUB owner, bounded MENU underlay and no world/turn mutation.
Item use and weapon mutation remain separate milestones.

## Development workflow

```text
recover true main + docs
 -> choose one bounded behavior FAMILY
 -> recover exact legacy behavior
 -> design small permanent native API/owner
 -> keep genuinely different families fail-closed
 -> commit/push agent/*
 -> test normal esp32-cyd on real CYD
 -> Serial is truth
 -> fix failures directly
 -> after PASS, docs-only tail
 -> verify tested SHA + docs-only commits
 -> merge-ready
```

Never merge into `main` without explicit user request.
