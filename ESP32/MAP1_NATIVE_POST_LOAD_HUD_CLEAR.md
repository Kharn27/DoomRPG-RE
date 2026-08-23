# ESP32 native Junction post-load HUD clear milestone

Branch: `agent/esp32-native-post-load-hud-clear`

Base merged `main`:

```text
PR   = #76 — native durable facing
main = 3ab143110a1f44ebb44bc130d12d1844f3ae73ca
```

Status: **HARDWARE CANDIDATE — NOT YET CYD-PROVEN**.

## Objective

Own only the first caller-side side effect immediately after the now
hardware-proven `DoomCanvas_finishRotation()`:

```c
DoomCanvas_finishRotation(doomCanvas);

doomCanvas->hud->msgCount = 0;
doomCanvas->hud->statBarMessage = NULL;
doomCanvas->hud->logMessage[0] = '\0';
```

Do not execute the following caller operations in this milestone:

```text
Junction Game_givemap()
Player_selectWeapon()
initial Game_saveState()
Game.isLoaded/isSaved/activeLoadType cleanup
numEvents / particle cleanup
isUpdateView=true
DoomCanvas_setState(ST_PLAYING)
idleTime update
```

## Recovered caller order

The exact fresh-map caller sequence after `finishRotation()` is:

```text
1. clear HUD message channels                 [THIS MILESTONE]
2. Junction Game_givemap()                    [deferred]
   else DoomCanvas_uncoverAutomap()
3. Player_selectWeapon(current weapon)         [deferred]
4. initial Game_saveState when !isLoaded       [deferred]
5. clear isLoaded/isSaved/activeLoadType       [deferred]
6. clear queued events / particles             [deferred]
7. isUpdateView=true                           [deferred]
8. DoomCanvas_setState(ST_PLAYING)             [deferred]
9. idleTime=time+8000                          [deferred]
```

This boundary is intentionally separate from the existing post-spawn
`EspHudRefreshState`: that earlier owner represents `Hud.isUpdate=true`, while
this new owner represents the later message-channel reset.

## Hardware-proven input boundary

The candidate runs only after PR #76 durable facing is complete:

```text
PlayerView FNV=afcdcf74
hudRefreshPending=0
facingRefreshPending=0
playerSetupPending=0
tileEnterPending=0

EspPlayerFacingState=32 B
facing FNV=95aa1108
kind=none
finishRotationComplete=yes

Junction resident snapshot FNV=bc9071e9
automap FNV=0b2ae445
ST_PLAYING=no
legacy Game.entities=0
legacy Game.monsters=0
```

## Permanent owner

```text
ESP32/include/esp_hud_post_load_clear_state.h
ESP32/src/esp_hud_post_load_clear_state.c
```

Candidate ABI:

```text
EspHudPostLoadClearState = 8 B
persistent heap = 0 B
```

Fields encode only the semantic postcondition:

```text
targetMapId
gameplayLoadMapId
loadType
messageCount=0
statBarMessagePresent=0
logMessageLength=0
cleared=1
active=1
```

API:

```text
EspHudPostLoadClear_reset()
EspHudPostLoadClear_isReady()
EspHudPostLoadClear_view()
EspHudPostLoadClear_prepare()
EspHudPostLoadClear_route()
```

`prepare()` is pure. It requires an active fresh-map PlayerView whose prior
spawn/setup/tile/facing responsibilities are all complete and a matching durable
facing owner. Saved-world load remains fail-closed until separately recovered.

`route()` parks the owner once. It does not add or consume another PlayerView
pending bit, because durable facing already closes the hardware-proven spawn
chain and the new owner itself becomes the explicit caller-order marker.

No legacy `Hud_t*` pointer is retained or written.

## Hardware probe

Temporary files:

```text
ESP32/include/native_junction_post_load_hud_clear_probe.h
ESP32/src/native_junction_post_load_hud_clear_probe.c
```

The normal `esp32-cyd` chain arms it only after the durable-facing probe has
completed, then executes it on the next Arduino loop.

Expected marker:

```text
=== Doom RPG ESP32-native Junction post-load HUD clear ===
[JUNCTIONHUDCLEAR] READY ...
```

The probe requires:

```text
EspHudPostLoadClearState=8 B
messageCount=0
statBarMessagePresent=0
logMessageLength=0
cleared=1
active=1
PlayerView afcdcf74 unchanged
Facing 95aa1108 unchanged
resident snapshot bc9071e9 unchanged
automap 0b2ae445 unchanged
PAK closed
heap/largest delta=0
legacy Game/Player/Hud/DoomCanvas/Render/framebuffer unchanged
ST_PLAYING=no
entities=0
monsters=0
```

The full legacy `Hud_t` is hashed before/after, and the three exact legacy fields
are also logged before/after. This proves semantic ownership without silently
mutating the legacy HUD.

Fail-closed coverage includes null view/facing/output, inactive PlayerView,
inactive or mismatched facing, unsupported saved-load context, wrong caller
order and repeat routing.

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

Do not promote this milestone until normal `esp32-cyd` Serial proves a complete
`[JUNCTIONHUDCLEAR] READY` block, unchanged PlayerView/facing/resident/automap,
closed PAK, zero same-build heap delta and no legacy HUD or framebuffer mutation.

After PASS, only documentation commits may follow the flashed firmware SHA.

## Next boundary after PASS

The next exact caller operation on Junction is:

```c
Game_givemap(doomCanvas->game);
```

Its semantics are already recovered and map cleanly onto native automap state:

```text
reveal non-hidden lines
reveal every map sprite
mark BIT_AM_ENTRANCE tiles visited
```

That direct caller-side reveal must remain a separate milestone from this HUD
clear and from later weapon/save/ST_PLAYING progression.
