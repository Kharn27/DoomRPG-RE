# Milestone: Render planeTextures retirement — REAL-CYD PASS

Branch: `agent/esp32-render-mapflags-retirement`
Hardware-tested code SHA: `d98b6e07cc21b3bff91cd7f0f2c07b00607b01ed`
Base main SHA: `02baa3bddd3b52c2399e33725aa4147a3840ad6e`

## Boundary

The second native Render memory cut removes `planeTextures[1024*2]`
from `Render_t` on normal `esp32-cyd` firmware. This historic
2048-byte plane-tile mirror belongs to the retired desktop BSP
loader and renderer. The original layout/code remains in desktop
and explicit `DOOMRPG_ESP32_BRINGUP_PROBES` builds.

Production `Render_render()` and `Render_renderFloorAndCeilingBG()`
are guarded fail-closed; the ESP32 generator checks all four
historical accesses to `planeTextures`. Native geometry remains
owned by compact `EspMapRuntime`, and native world/plane rendering
never requires this mirror. Production layout is statically asserted
at `sizeof(Render_t)==1968`.

## CI

GitHub Actions #1678, normal `esp32-cyd`, exact code SHA:
SUCCESS, static RAM 45056 B, flash 772389 B.

## Hardware acceptance

Compared with the previous mapFlags milestone:
| Stage | Before heap8 | After heap8 | Delta |
| --- | ---: | ---: | ---: |
| CORE | 182632 | 184680 | +2048 |
| LAYOUT | 176024 | 178072 | +2048 |
| mappings resident | 156864 | 158912 | +2048 |
| gameplay | 115812 | 117860 | +2048 |
| exit menu | 161708 | 163756 | +2048 |

The two cuts together reduce `Render_t` from 5040 to 1968 bytes,
a cumulative 3072 bytes. `Game_t=4` and `DoomCanvas_t=44`
are unchanged.

Hardware witnesses:

```text
[MAINBOOT] READY ... frame=522dc605
[INTRO1] READY ... FNV=ade0195d
[MAPRT] READY arenaBytes=14095 ... arenaFNV=c3882516
[ENGINESESSION] FIRST_FRAME map=1 angle=64 frame=71ca7465 walls=8 pixels=4430 presented=1
[ENGINESESSION] READY ... shapeData=0x0 mediaTexels=0x0
[NATIVEFRAME] LEGACY_GUARD ...
[NATIVEFRAME] RETRY ...
[NATIVEFRAME] RECOVERED ...
[DOORANIM] COMPLETE transitions=1 frames=4 state=stable transaction=committed
[RESIDENTRESET] heap8=145748->163756 released=18008 ... after=0/0/0/0/0/0/0 empty=1
[SYSEXIT] MENU-READY frame=522dc605 session=off resident=empty saveWrite=no checkpoint=unchanged
```

Fresh game and intro, movements, rotations, crate attack,
transformed pickup, Armor Shards, an animated door, HUD/SYS,
and confirmed exit were tested. No checkpoint was modified.
The known native compact guard recovered as before. Checkpoint
LOAD was not retested at this exact SHA.

Hardware-tested code is frozen for this boundary. All subsequent
changes, if any, form new independently tested milestones.
