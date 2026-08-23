# ESP32 native Junction post-load weapon self-select milestone

Branch: `agent/esp32-native-post-load-weapon-self-select`

Base merged `main`:

```text
PR   = #78 — direct Junction post-load Game_givemap
main = 4737b016d02615b8435cf84909fe3c251b6d338b
```

Status: **HARDWARE CANDIDATE — NOT YET CYD-PROVEN**.

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

At this exact load caller, `i` is `player->weapon` itself. Therefore the recovered
semantics are strictly:

```text
requestedWeapon == weaponBefore
DoomCanvas_updateViewTrue branch is NOT taken
weaponAfter == weaponBefore
```

The assignment is an identity write. No ammo, inventory, disabled-weapon,
combat, sound or HUD behavior belongs to this call.

Legacy weapon iteration also confirms the valid weapon index range is `0..11`.

## Why this is not a weapon-gameplay owner

This milestone deliberately does **not** introduce native ammo/inventory/weapon
selection gameplay state merely because the legacy function is named
`Player_selectWeapon()`.

The exact caller is a semantic no-op. A future real selection where requested
weapon differs from current weapon will need its own native player/inventory/HUD
boundary because that path requests `DoomCanvas_updateViewTrue()`.

## Permanent caller-order owner

New files:

```text
ESP32/include/esp_post_load_weapon_select_state.h
ESP32/src/esp_post_load_weapon_select_state.c
```

Candidate ABI:

```text
EspPostLoadWeaponSelectState = 8 B
persistent heap = 0 B
```

Fields:

```text
weaponBefore
requestedWeapon
weaponAfter
viewInvalidationRequested
targetMapId
gameplayLoadMapId
loadType
active
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
scalar.

## Strict current-context gate

Pure preparation requires the already hardware-proven direct-GIVEMAP marker:

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

## Temporary hardware probe

New files:

```text
ESP32/include/native_junction_post_load_weapon_select_probe.h
ESP32/src/native_junction_post_load_weapon_select_probe.c
```

The lifecycle bridge runs it one Arduino loop after
`Esp32JunctionPostLoadGiveMapProbe_isDone()`.

The probe samples legacy `Player.weapon` **read-only** as the hardware input.
Neither the candidate owner nor the probe calls legacy `Player_selectWeapon()`.

Expected block:

```text
=== Doom RPG ESP32-native Junction post-load weapon self-select ===
[JUNCTIONWEAPONPROBE] CONTRACT ...
[JUNCTIONWEAPON] READY ...
[JUNCTIONWEAPON] SEMANTIC ...
[JUNCTIONWEAPON] INPUT ...
[JUNCTIONWEAPON] FAILCLOSED ...
[JUNCTIONWEAPON] RESIDENT ...
[JUNCTIONWEAPON] RAM ...
[JUNCTIONWEAPON] LEGACY ...
[JUNCTIONWEAPON] PARK ...
```

## Hardware values deliberately not predeclared

The real CYD must establish:

```text
legacy current weapon at this exact caller boundary
EspPostLoadWeaponSelectState FNV
same-build Game/Player/Hud/DoomCanvas/Render/frame witnesses
same-build heap8/largest8
```

Do not promote the legacy reset-time `weapon=2` default into a hardware canon
before this probe reports the actual value.

## Hardware acceptance

The real normal `esp32-cyd` firmware must prove:

```text
stateBytes=8
weaponBefore=requestedWeapon=weaponAfter
viewInvalidationRequested=0
selfSelect=yes
identityAssignment=yes
updateViewBranchTaken=no
legacy Player.weapon unchanged
legacy DoomCanvas.isUpdateView unchanged
```

Input owners must remain canonical:

```text
post-load GIVEMAP FNV=448e587d
post-load HUD-clear FNV=b7383e18
PlayerView FNV=afcdcf74
Facing FNV=95aa1108
```

Resident world must remain exactly unchanged:

```text
snapshotFNV=bb714d80 -> bb714d80
mapStateFNV=8dba0bb4
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

Fail-closed coverage includes:

```text
null preceding owner
null output
inactive preceding owner
wrong target map
wrong gameplay map
saved-load context
wrong direct-GIVEMAP counts
invalid weapon index
pure-prepare atomicity
post-active prepare refusal
repeat-route atomicity
```

RAM / integrity acceptance:

```text
heap8 delta=0
largest8 delta=0
persistentHeapBytes=0
framebuffer unchanged
Game unchanged
Player unchanged
Hud unchanged
DoomCanvas unchanged
Render unchanged
legacy runtime remains clear
legacy Player_selectWeapon not called
shapeData == NULL
mediaTexels == NULL
```

## Candidate PARK

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

## Promotion rule

Only a complete real-CYD `[JUNCTIONWEAPON]` PASS on normal `esp32-cyd` may
establish the current weapon and state FNV. After that tested firmware SHA, all
promotion commits must be documentation-only.

## Next exact caller boundary after PASS + merge

The next load caller operation is:

```c
if (!game->isLoaded) {
    Game_saveState(game, 1, 1, 1);
}
```

That is a larger save-state ownership boundary and must remain separate from this
identity weapon self-selection milestone.
