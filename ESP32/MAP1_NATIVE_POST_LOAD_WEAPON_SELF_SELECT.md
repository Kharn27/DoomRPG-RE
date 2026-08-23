# ESP32 native Junction post-load weapon self-select milestone

Branch: `agent/esp32-native-post-load-weapon-self-select`

Base merged `main`:

```text
PR   = #78 — direct Junction post-load Game_givemap
main = 4737b016d02615b8435cf84909fe3c251b6d338b
```

Hardware-tested firmware:

```text
24fb8fbf914820500d2e16815e22beb0439c9ba0
```

Status: **REAL-CYD HARDWARE PASS / MERGE-READY**.

## Objective

Own only the exact caller operation immediately after the hardware-proven direct
Junction `Game_givemap()`:

```c
Player_selectWeapon(player, player->weapon);
```

Do **not** include the later caller operations:

```text
initial Game_saveState() when !game->isLoaded
Game.isLoaded=false
Game.isSaved=false
Game.activeLoadType=0
queued-event / particle cleanup
isUpdateView=true
DoomCanvas_setState(ST_PLAYING)
idleTime=time+8000
```

## Exact recovered legacy behavior

Current `src/Player.c` implements:

```c
void Player_selectWeapon(Player_t* player, int i)
{
    if (player->weapon != i) {
        DoomCanvas_updateViewTrue(player->doomRpg->doomCanvas);
    }
    player->weapon = i;
}
```

At this exact load caller, `i` is `player->weapon` itself. Therefore:

```text
requestedWeapon == weaponBefore
DoomCanvas_updateViewTrue branch is NOT taken
weaponAfter == weaponBefore
```

The assignment is an identity write. No ammo, inventory, disabled-weapon,
combat, sound or HUD behavior belongs to this call. Legacy weapon iteration
confirms the valid weapon index range is `0..11`.

## Permanent caller-order owner

Files:

```text
ESP32/include/esp_post_load_weapon_select_state.h
ESP32/src/esp_post_load_weapon_select_state.c
```

Hardware-proven ABI:

```text
EspPostLoadWeaponSelectState = 8 B
persistent heap = 0 B
stateFNV = 699f3cf3
```

Hardware-proven state:

```text
weaponBefore=2
requestedWeapon=2
weaponAfter=2
viewInvalidationRequested=0
targetMapId=9
gameplayLoadMapId=2
loadType=0
active=1
```

Permanent APIs:

```c
EspPostLoadWeaponSelect_reset()
EspPostLoadWeaponSelect_isReady()
EspPostLoadWeaponSelect_view()
EspPostLoadWeaponSelect_prepare()
EspPostLoadWeaponSelect_route()
```

The permanent implementation has no `Player_t`, `DoomCanvas_t`, `Hud_t`,
`Game_t` or `Render_t` dependency. The caller supplies only the current weapon
scalar. A future real weapon change remains a separate native player/inventory/
HUD boundary.

## Strict current-context gate

Preparation requires the post-GIVEMAP caller marker:

```text
EspPostLoadGiveMapState = 16 B
stateFNV = 448e587d
lineTargets=198
spriteTargets=48
entranceTargets=15
linesMutated=198
spritesMutated=48
tilesMutated=15
targetMap=9
gameplayLoadMapId=2
loadType=0
active=1
```

It also requires the exact post-GIVEMAP Junction world:

```text
sourceBytes=21051
sourceCrc32=4a2c5800
runtimeFNV=bc432a0f
mapStateFNV=8dba0bb4
automapFNV=b699bd75
```

Only current weapon indices `0..11` are accepted. Other maps, saved-load
contexts, reordered calls, malformed preceding owners and invalid weapon values
remain fail-closed.

## Real-CYD evidence

The supplied normal `esp32-cyd` boot emitted the complete successful block.

Semantic result:

```text
stateBytes=8
stateFNV=699f3cf3
weaponBefore=2
requestedWeapon=2
weaponAfter=2
viewInvalidationRequested=0
selfSelect=yes
identityAssignment=yes
updateViewBranchTaken=no
legacyWeapon=2->2
legacyIsUpdateView=1->1
```

Input-owner integrity:

```text
giveMapFNV=448e587d
hudClearFNV=b7383e18
viewFNV=afcdcf74
facingFNV=95aa1108
unchanged=yes
callerOrder=yes
```

Fail-closed proof:

```text
nullGiveMap=1
nullOutput=1
inactiveGiveMap=1
targetMap=1
gameplayMap=1
loadType=1
count=1
invalidWeapon=1
prepareAtomic=yes
postActivePrepare=1
repeat=1
repeatAtomic=yes
```

Resident-world integrity:

```text
snapshotFNV=bb714d80->bb714d80
unchanged=yes
mapFNV=8dba0bb4
automapFNV=b699bd75
runtimeFNV=bc432a0f
scriptFNV=bc9b18ff
lineFNV=3658710d
textureFNV=537319ad
topologyFNV=d6e8df7d
payload=10410
entities=30
enemies=0
destructibles=3
packClosed=yes
```

Normal-env RAM proof:

```text
heap8=72684->72684
delta=0
largest8=34804->34804
delta=0
persistentHeapBytes=0
```

Same-build legacy/frame equality witnesses:

```text
gameFNV=d073b2d5->d073b2d5
playerFNV=c64e7862->c64e7862
hudFNV=b18611d2->b18611d2
canvasFNV=d6d1b92a->d6d1b92a
renderFNV=f9344dec->f9344dec
frameFNV=ee9d9dbc->ee9d9dbc
legacyRuntimeClear=yes
GameMutation=no
PlayerMutation=no
HudMutation=no
DoomCanvasMutation=no
RenderMutation=no
legacyPlayer_selectWeaponCalled=no
```

Stable heartbeat after PARK:

```text
heap=138448
heap8=72684
largest8=34804
SD=ready
VIDEO=ready
CORE=ready
```

`ZIP=ready` in the platform heartbeat does not change the runtime asset invariant:
this milestone performed no ZIP map access and the native backing store remains
`/DoomRPG-ESP32.pak`.

## Hardware PARK

```text
state=9 / ST_INTRO
page=3
targetMap=9
junctionResident=yes
nativeHudClear=yes
nativePostLoadGiveMap=yes
nativeWeaponSelfSelect=yes
weaponReselectPending=no
initialSavePending=yes
postLoadCleanupPending=yes
ST_PLAYING=no
entities=0
monsters=0
noGameplay=yes
```

## Probe `done` recovery note

Repository audit after the hardware PASS confirmed that current temporary probes
use `done` as a terminal-attempt marker: some failure branches also set
`probeState.done=1`. Therefore `*_isDone()` alone must **not** be documented as a
proof that the preceding probe passed.

This milestone is hardware-proven by its complete successful `[JUNCTIONWEAPON]`
block and by its own strict revalidation of the preceding owners/world. Future
stages must continue to revalidate their input boundary instead of treating an
`isDone()` gate by itself as a PASS certificate.

## Merge rule

The firmware actually tested on the real CYD is exactly:

```text
24fb8fbf914820500d2e16815e22beb0439c9ba0
```

All commits after that SHA on this branch must be documentation-only.

## Next exact caller boundary after merge

The next load caller operation is:

```c
if (!game->isLoaded) {
    Game_saveState(game, 1, 1, 1);
}
```

That is a larger save-state ownership boundary and must remain separate from this
identity weapon self-selection milestone.
