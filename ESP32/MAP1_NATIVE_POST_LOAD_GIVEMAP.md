# ESP32 native Junction post-load direct Game_givemap milestone

Branch: `agent/esp32-native-post-load-givemap`

Base merged `main`:

```text
PR   = #77 — native post-load HUD clear
main = 56c4211a91e6a95763dd4cc215ef40de6c10a98b
```

Hardware-tested firmware:

```text
511156120bd877367d13ffa4b98ed6815005bc3c
```

Status: **REAL-CYD HARDWARE PASS / historical MAP_INTRO regression witness not included in supplied excerpt**.

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

Permanent owners:

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

`esp_map_automap_state` exposes an event-independent result:

```text
EspMapGiveMapDirectResult = 12 B
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

Permanent APIs:

```text
EspMapAutomapState_planGiveMapDirect()
EspMapAutomapState_applyGiveMapDirect()
```

`planGiveMapDirect()` is pure. `applyGiveMapDirect()` performs the exact three
recovered mutations without allocation and without Game/Render/Entity/Hud/
Player/DoomCanvas dependencies.

The existing opcode API remains:

```text
EspMapAutomapState_applyGiveMapCommand(...)
EspMapGiveMapResult = 20 B
```

Its previously hardware-proven 20 B ABI and descriptor/opcode/remove semantics
are unchanged. The command wrapper delegates only the world mutation to the same
direct primitive, so event-driven and caller-driven GIVEMAP share one permanent
implementation.

## Post-load caller-order owner

Permanent files:

```text
ESP32/include/esp_post_load_givemap_state.h
ESP32/src/esp_post_load_givemap_state.c
```

Hardware-proven ABI:

```text
EspPostLoadGiveMapState = 16 B
persistent heap = 0 B
stateFNV = 448e587d
```

Real-CYD state:

```text
lineTargetCount=198
spriteTargetCount=48
entranceTargetCount=15
linesMutated=198
spritesMutated=48
tilesMutated=15
targetMapId=9
gameplayLoadMapId=2
loadType=0
active=1
```

The FNV `448e587d` independently matches the 16-byte little-endian ABI payload
for those exact values.

The durable revealed state remains exclusively in `EspMapAutomapState` and
`EspMapState`; the 16 B owner is only an explicit caller-order marker.

## Strict current-context gate

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

Saved-world loads, other maps and reordered calls remain fail-closed.

## Real-CYD world proof

The tested normal `esp32-cyd` firmware produced:

```text
[JUNCTIONGIVEMAP] READY stateBytes=16 directResultBytes=12 stateFNV=448e587d lineTargets=198 spriteTargets=48 entranceTargets=15 linesMutated=198 spritesMutated=48 tilesMutated=15 active=1 targetMap=9 gameplayLoadMapId=2 loadType=0
```

Both target owners mutated exactly as required:

```text
mapStateFNV  c5cdfc04 -> 8dba0bb4
automapFNV   0b2ae445 -> b699bd75
```

Every current target required mutation on this fresh Junction boundary:

```text
198 / 198 eligible lines revealed
48 / 48 map sprites revealed
15 / 15 entrance tiles visited
```

The semantic scan and second pure plan both passed:

```text
allTargetsRevealed=yes
idempotentPlan=yes
second-plan mutations=0/0/0
```

Non-target resident owners stayed canonical:

```text
runtimeFNV=bc432a0f
scriptFNV=bc9b18ff
lineFNV=3658710d
textureFNV=537319ad
topologyFNV=d6e8df7d
nonTargetOwnersUnchanged=yes
```

Because map/automap are intentionally mutable, the resident snapshot FNV changed:

```text
snapshotFNV bc9071e9 -> bb714d80
payload=10410
entities=30
enemies=0
destructibles=3
packClosed=yes
```

## Input-owner integrity

Hardware kept the preceding owners unchanged:

```text
HUD-clear FNV=b7383e18
PlayerView FNV=afcdcf74
Facing FNV=95aa1108
callerOrder=yes
```

## Fail-closed / atomicity proof

Real hardware proved:

```text
nullHud=1
nullOutput=1
inactiveHud=1
uncleared=1
targetMap=1
gameplayMap=1
loadType=1
plannerNull=1
prepareAtomic=yes
postActivePrepare=1
repeat=1
repeatAtomic=yes
```

## RAM proof

Normal `esp32-cyd`:

```text
heap8=72700 -> 72700
delta=0
largest8=34804 -> 34804
delta=0
persistentHeapBytes=0
```

Stable post-PARK heartbeat:

```text
heap=138464
heap8=72700
largest8=34804
SD=ready
VIDEO=ready
CORE=ready
```

## Legacy / framebuffer integrity

Same-build equality witnesses:

```text
gameFNV=d073b2d5 -> d073b2d5
playerFNV=c64e7862 -> c64e7862
hudFNV=b18611d2 -> b18611d2
canvasFNV=702a1a9d -> 702a1a9d
renderFNV=f9344dec -> f9344dec
frameFNV=2cb60336 -> 2cb60336
```

And explicitly:

```text
legacyRuntimeClear=yes
GameMutation=no
PlayerMutation=no
HudMutation=no
DoomCanvasMutation=no
RenderMutation=no
legacyGame_givemapCalled=no
```

These equality hashes are same-build witnesses, not cross-build canons.

## Hardware PARK

```text
state=9 / ST_INTRO
page=3
targetMap=9
junctionResident=yes
nativeHudClear=yes
nativePostLoadGiveMap=yes
Game_givemapPending=no
weaponReselectPending=yes
initialSavePending=yes
postLoadCleanupPending=yes
ST_PLAYING=no
entities=0
monsters=0
noGameplay=yes
```

## Mandatory invariants

```text
shapeData == NULL
mediaTexels == NULL
runtime ZIP access forbidden
legacy Game.entities = 0
legacy Game.monsters = 0
ST_PLAYING not reached
```

## Regression-witness note

The candidate contract also requested the historical MAP_INTRO
`[MAPGIVEMAPPROBE]` PASS from the same firmware, because this branch factors the
shared GIVEMAP world primitive used by opcode 9. The supplied Serial excerpt
contains the complete Junction `[JUNCTIONGIVEMAP]` PASS block but does not
contain the earlier MAP_INTRO regression block.

Therefore:

```text
direct Junction boundary = REAL-CYD HARDWARE PASS
historical opcode-9 regression witness = not present in supplied excerpt
```

Do not invent that missing witness. If it is supplied from the same flashed
firmware, the branch can be declared fully merge-ready with documentation-only
follow-up.

## Next caller boundary after merge

The next exact operation remains:

```c
Player_selectWeapon(player, player->weapon);
```

Weapon ownership, initial save, load cleanup and `ST_PLAYING` remain separate
milestones.