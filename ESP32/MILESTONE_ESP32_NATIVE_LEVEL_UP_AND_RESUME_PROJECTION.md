# ESP32 native LEVEL UP and checkpoint resume projection milestone

Date: 2026-10-02

Branch: `agent/esp32-levelup-drop-consequences`

Hardware-tested code boundary:
`ccce5be96690086d56babc477c88b41faf289c72`

## Scope

This milestone closes two presentation consequences discovered while validating
native combat and checkpoint resume:

1. replace the transient legacy-style level-up dialog with a permanent native
   full-screen LEVEL UP owner;
2. reconstruct the presentation-only monster movement projection on checkpoint
   resume so restored positions are visible on the first world frame.

Neither change introduces a legacy Entity owner, a second framebuffer, map-wide
mutable sprite data, or a new SAVE field.

## Dedicated LEVEL UP owner

Legacy `Player_nextLevel()` remains the behavioral reference for progression.
The native PlayerState path already owned exact level increment, XP threshold,
stat RNG and health restoration. The missing consequence was presentation.

The dedicated owner stores only the level transition, six current stat values,
six gains, input/present flags and a framebuffer fingerprint. It paints directly
into the shared 160x120 RGB565 framebuffer.

Permanent behavior:

- exact PlayerState mutation happens before presentation;
- health is restored to the new max, armor current value is not refilled;
- the level-up screen has no timeout;
- the attack press cannot dismiss it because PlatformInput requires a stable
  release before another semantic tap is delivered;
- after that release, one fresh tap anywhere dismisses it;
- while active, monster-turn scheduling is skipped exactly as the former
  standalone level-up dialog path did;
- late death, GIBFX, HITFX and timed-feedback compositors cannot replace the
  visible LEVEL UP frame;
- close explicitly repaints HUD bands plus a complete world frame before
  releasing ownership;
- sound 5043 remains deferred.

The accepted layout uses the shared industrial/HUB palette:

```text
LEVEL UP
LEVEL 1 -> 2

MAX HP      MAX ARM
35 +5       23 +3

DEFENSE     STRENGTH
17 +1       14 +2

AGILITY     ACCURACY
15 +1       17 +1

HEALTH RESTORED
TAP TO CONTINUE
```

Values vary with the real legacy RNG rolls. Current values use the shared mini
3x5 face; gains are green. The missing `>` character was added to the shared
5x7 font so the transition is not truncated after `LEVEL 1`.

## Real-CYD LEVEL UP PASS

The final hardware run proves a complete level transition and explicit close:

```text
[MONSTERCOMBAT] COMMIT seq=1 ... xp=6-applied level=1->2 levelUps=1 ...
[LEVELUP] PRESENT seq=1 level=1->2 levelUps=1 gains=hp+5/armor+3/def+1/str+2/agi+1/acc+1 health=restored frame=5f4db24a input=fresh-release+fullscreen-tap stats=current+gain-mini owner=dedicated-fullscreen timer=none
[LEVELUP] ARM seq=1 level=1->2 levelUps=1 status=OK owner=dedicated-fullscreen continuation=explicit-tap monsterTurn=legacy-skip-while-levelup-active sound=5043-deferred
[MONSTERTURN] SKIP reason=PLAYER_ATTACK levelup=active legacySkipTurn=yes mutation=no
[RESIDENTGAMEPLAY] LEVELUP-TAP tap=2 logical=121,55 dismiss=requested source=fresh-fullscreen worldAction=no feedback=none
[GAMEPLAYHUD] REPAINT health=35/35 armor=11/23 weapon=0 ammo=18 angle=0 pixels=7452 reads=72 bytes=8072 ownerMutation=no dirtyConsume=no
[LEVELUP] CLOSE seq=1 level=1->2 input=tap worldRedraw=complete turnAdvance=no owner=released
[RESIDENTGAMEPLAY] LEVELUP-CLOSE hudRepaint=yes hudPixels=7452 worldRedraw=yes fullScreenOwner=released turnAdvance=no
```

The user explicitly accepted the final visual result.

## Checkpoint monster projection bug

V9/V10 checkpoint restore already staged and adopted exact native monster state,
topology, positions and activation. Collision therefore knew a moved monster's
restored tile immediately. However,
`EspNativeGameplayMonsterMovementPublish_reset()` correctly cleared its
presentation-only `projectedBits`, and the renderer then used immutable BSP
coordinates until the monster performed a live move.

That produced an apparent teleport after LOAD: the monster was visibly at its
original room position, while collision already blocked the saved position; on
the first monster move the projected bit became live and the sprite snapped to
the restored owner.

The fix derives presentation state before any checkpoint-resume cache witness or
visible frame. For each restored monster, its native position is compared with
the immutable BSP MapSprite coordinates. A mismatch sets the movement-projection
bit. No serialization is required.

## Real-CYD checkpoint projection PASS

The hardware log proves exact owner adoption:

```text
[MONSTERSTATE] RESTORE arena=c3882516 enemies=30 ... stateFNV=7cbae958 ...
[MONSTERPOS] RESTORE arena=c3882516 monsters=30 ownerBytes=240 stateFNV=149e39dd source=checkpoint-v9 topologyExact=yes
[MONSTERMOVELIVE] CHECKPOINT-PROJECTION arena=c3882516 monsters=30 projected=9 source=restored-position-v9 inference=raw-bsp-delta firstFrame=exact
[ENGINESESSION] RESUME checkpoint=restored ... preRender=yes freshFirstFrame=skipped ...
```

The user confirmed the zombie that had been adjacent when saved is drawn at the
correct restored position immediately after LOAD. It no longer appears at its
BSP spawn and teleports on the first movement.

## Build

GitHub Actions `ESP32 CYD Build` run #1532: SUCCESS.

```text
RAM:   13.9% (used 45392 bytes from 327680 bytes)
Flash: 59.4% (used 778489 bytes from 1310720 bytes)
```

No local PlatformIO build is claimed.

## Explicitly deferred

- level-up sound 5043;
- live dynamic-drop SAVE/LOAD persistence;
- secondary pickup-message presentation where multiple resources share a tile.

These are not required for this milestone PASS.
