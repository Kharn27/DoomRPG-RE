# ESP32 native Junction post-load direct Game_givemap milestone

Branch: `agent/esp32-native-post-load-givemap`

Base merged `main`:

```text
PR   = #77 — native post-load HUD clear
main = 56c4211a91e6a95763dd4cc215ef40de6c10a98b
```

Status: **HARDWARE CANDIDATE — NOT YET CYD-PROVEN**.

## Objective

Own only the next exact caller operation after the hardware-proven post-load HUD
message reset:

```c
if (doomCanvas->loadMapID == MAP_JUNCTION) {
    Game_givemap(doomCanvas->game);
}
```

This milestone does **not** include:

```text
Player_selectWeapon(current weapon)
initial Game_saveState()
Game.isLoaded/isSaved/activeLoadType cleanup
queued-event / particle cleanup
isUpdateView=true
DoomCanvas_setState(ST_PLAYING)
idleTime=time+8000
```

## Exact recovered legacy semantics

`Game_givemap()` performs three bounded world-state mutations:

```text
for every line:
    if !(line.flags & 0x20):
        line.flags |= 0x80

for every map sprite:
    sprite.info |= 0x10000000

for every 32x32 mapFlags cell:
    if cell & BIT_AM_ENTRANCE:
        cell |= BIT_AM_VISITED
```

No entity topology, AI, combat, sound or presentation behavior belongs to this
operation.

## Existing permanent native ownership reused

The project already hardware-proved these semantics for real `9 / EV_GIVEMAP`
bytecode in `MAP1_NATIVE_GIVEMAP_STATE.md`.

Permanent owners already exist:

```text
EspMapAutomapState
  one reveal bit per immutable line
  one reveal bit per immutable map sprite

EspMapState
  canonical 1024 tile flags
  BIT_AM_VISITED mutation only through EspMapState_setVisited()
```

Junction resident input canon before this caller operation:

```text
runtimeFNV=bc432a0f
mapStateFNV=c5cdfc04
scriptFNV=bc9b18ff
lineFNV=3658710d
textureFNV=537319ad
automapFNV=0b2ae445
topologyFNV=d6e8df7d
snapshotFNV=bc9071e9
payload=10410
entities=30
enemies=0
destructibles=3
```

## Shared direct GIVEMAP primitive

`esp_map_automap_state` now exposes an event-independent result:

```c
EspMapGiveMapDirectResult = 12 B  // candidate ABI
```

Fields:

```text
lineTargetCount
spriteTargetCount
entranceTargetCount
linesMutated
spritesMutated
tilesMutated
```

New APIs:

```c
EspMapAutomapState_planGiveMapDirect()
EspMapAutomapState_applyGiveMapDirect()
```

`planGiveMapDirect()` is pure. It counts total targets and how many current
native-owner mutations would be required.

`applyGiveMapDirect()` performs the exact three recovered mutations, without
allocation and without Game/Render/Entity/Hud/Player/DoomCanvas dependencies.

The existing opcode API remains:

```c
EspMapAutomapState_applyGiveMapCommand(...)
EspMapGiveMapResult = 20 B
```

Its hardware-proven 20 B ABI is unchanged. The command wrapper still validates
canonical event descriptor, opcode 9, command offset/global index and
remove-if-handled metadata, then delegates only the world mutation to the same
direct primitive. Thus event-driven and caller-driven GIVEMAP now have one
permanent implementation of the world semantics.

The normal probe chain still runs the historical MAP_INTRO GIVEMAP probe before
the Junction transition, so the next real-CYD flash also regression-tests the
existing opcode-9 behavior.

## Post-load caller-order owner

New permanent files:

```text
ESP32/include/esp_post_load_givemap_state.h
ESP32/src/esp_post_load_givemap_state.c
```

Candidate ABI:

```text
EspPostLoadGiveMapState = 16 B
persistent heap = 0 B
```

It stores only:

```text
six direct target/mutation counts
map identity targetMapId/gameplayLoadMapId/loadType
active marker
```

The durable revealed state remains exclusively in `EspMapAutomapState` and
`EspMapState`; the 16 B owner is only an explicit caller-order marker for later
post-load progression.

### Strict current-context gate

The pure `EspPostLoadGiveMap_prepare()` requires:

```text
EspHudPostLoadClearState active + cleared
HUD-clear FNV boundary b7383e18
map identity 9 / gameplayLoadMapId 2 / loadType 0
Junction runtime sourceBytes=21051
Junction runtime sourceCrc32=4a2c5800
Junction runtime FNV=bc432a0f
pre-GIVEMAP mapStateFNV=c5cdfc04
pre-GIVEMAP automapFNV=0b2ae445
```

This deliberately fail-closes saved-world loads, other maps and reordered calls
until their own caller path is recovered.

`EspPostLoadGiveMap_route()` performs the pure plan, applies direct GIVEMAP once,
then parks the 16 B result. Repeat routing is refused as already active.

## Hardware probe

Temporary files:

```text
ESP32/include/native_junction_post_load_givemap_probe.h
ESP32/src/native_junction_post_load_givemap_probe.c
```

It runs one Arduino loop after the hardware-proven HUD-clear probe.

Expected block:

```text
=== Doom RPG ESP32-native Junction post-load Game_givemap ===
[JUNCTIONGIVEMAP] READY ...
[JUNCTIONGIVEMAP] INPUT ...
[JUNCTIONGIVEMAP] WORLD ...
[JUNCTIONGIVEMAP] FAILCLOSED ...
[JUNCTIONGIVEMAP] RESIDENT ...
[JUNCTIONGIVEMAP] RAM ...
[JUNCTIONGIVEMAP] LEGACY ...
[JUNCTIONGIVEMAP] PARK ...
```

The probe does **not** predeclare the Junction target counts, post-world FNVs or
new state FNV. The real CYD establishes them.

Acceptance requires:

```text
EspPostLoadGiveMapState=16 B
EspMapGiveMapDirectResult=12 B
pure prepare with no mutation
all eligible lines revealed after route
all map sprites revealed after route
all entrance tiles visited after route
second pure direct plan reports zero mutations
runtime/script/line/texture/topology FNVs unchanged
map-state FNV changes from c5cdfc04
automap FNV changes from 0b2ae445
payload/entity counts unchanged
HUD-clear b7383e18 unchanged
PlayerView afcdcf74 unchanged
Facing 95aa1108 unchanged
PAK closed
heap/largest delta=0
legacy Game/Player/Hud/DoomCanvas/Render/framebuffer unchanged
legacy Game_givemap not called
ST_PLAYING=no
entities=0
monsters=0
```

Fail-closed coverage includes null input/output, inactive/uncleared HUD owner,
wrong target map, wrong gameplay map, saved-load context, null direct planner,
pure-prepare atomicity, post-active prepare refusal and repeat-route atomicity.

## Mandatory invariants

```text
shapeData == NULL
mediaTexels == NULL
runtime ZIP access forbidden
legacy Game.entities = 0
legacy Game.monsters = 0
ST_PLAYING not reached
```

## Promotion rule

Do not promote this milestone until normal `esp32-cyd` Serial proves the complete
`[JUNCTIONGIVEMAP]` block and the earlier `[MAPGIVEMAPPROBE]` still passes in the
same firmware.

After PASS, only documentation commits may follow the flashed firmware SHA.

## Next caller boundary after PASS

The next exact operation is:

```c
Player_selectWeapon(player, player->weapon);
```

Do not implement it until this direct Junction GIVEMAP boundary is hardware-
proven and merged. Weapon ownership/save/load cleanup/`ST_PLAYING` remain
separate milestones.
