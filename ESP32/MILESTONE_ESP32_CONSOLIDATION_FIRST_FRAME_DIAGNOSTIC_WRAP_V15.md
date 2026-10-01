# ESP32 consolidation milestone — first-frame diagnostic wrapper V15

Date: 2026-10-01

## Boundary

```text
repo = Kharn27/DoomRPG-RE
base main = fb3b4ee310ccc54d7301dcdfbba9d4648e17c38a
branch = agent/esp32-consolidation-first-frame-diagnostic-wrap-v15
hardware-tested code head = 5460c689b708468e3bdd618d0000753159a24109
CI #1202 = SUCCESS
artifact id = 11121161150
artifact digest = sha256:89e6527638a4471f37b1019589b10624796814bcfecce6a236304c4b96ca68ff
firmware.bin = 772528 B
firmware sha256 = 644d668d8209a8d7ccd3aa6680a5b1fae92f0e90b8a50c799bde3f011ea64f85
firmware.elf = 12249092 B
firmware.elf sha256 = de4b561ca07c4200c28f7ffcf924cf169494ff8ef86ff032c79592640e87fb12
static RAM = 45120 B
linked Flash = 772161 B
active linker wraps = 51
```

## Goal

Remove one diagnostic-only linker interception from the normal firmware without
changing the permanent native first-frame renderer or gameplay-session contract.

## Pre-V15 production shape

The normal environment still linked:

```text
EspNativeGameplaySession
 -> EspNativeFirstFrame_route
 -> linker redirect
 -> __wrap_EspNativeFirstFrame_route
    -> __real_EspNativeFirstFrame_route
    -> measureViewport()
    -> pendingDumpRender = render
```

The wrapper's only observable runtime effect after a successful frame was the
read-only log:

```text
[JUNCTIONFRAME] COLORSTATS ...
```

and retention of a pointer for an optional BMP export diagnostic.

The real route already owned all functional behavior:

- readiness and pack-busy guards;
- native wall/plane render;
- exact framebuffer FNV;
- first-frame state publication;
- physical present;
- fail-closed status return.

## V15 composition

Normal `esp32-cyd` now calls the real function directly:

```text
EspNativeGameplaySession
 -> EspNativeFirstFrame_route
 -> render + FNV + present + publish
 -> EspNativeFirstFrame_view
```

The compatibility wrap is moved to `esp32-cyd-bringup` only, where historical
COLORSTATS/BMP inspection remains available.

No C source file changed in the hardware-tested V15 code commit; only
`ESP32/platformio.ini` changed the production-vs-bringup linker scope.

## CI / ELF result

Against merged V14 main:

```text
                         V14 main      V15          delta
static RAM               45128 B      45120 B        -8 B
linked Flash            773089 B     772161 B      -928 B
firmware.bin            773456 B     772528 B      -928 B
active __wrap_*              52           51           -1
```

Final ELF keeps:

```text
400e30e4 T EspNativeFirstFrame_reset
400e3114 T EspNativeFirstFrame_isReady
400e3138 T EspNativeFirstFrame_view
400e3148 T EspNativeFirstFrame_route
400e3200 T EspNativeFirstFrame_renderGameplayViewport
400fe1e0 T __wrap_Esp32PlatformVideo_present
```

Final ELF contains none of:

```text
__wrap_EspNativeFirstFrame_route
Esp32FirstFrameDiagnostic_reset
Esp32FirstFrameDiagnostic_exportBmp
pendingDumpRender
```

The active `__wrap_*` count is exactly 51.

## Real classic-CYD proof

The pre-game and intro heap each expose the exact +8 B recovered static RAM:

```text
[ALIVE] ... heap8=35664 largest8=23540 ... MENU=ready
[MAINSTART] Begin ... heap8=35664 ...
[INTRO1] Begin ... heap8=43208 largest8=12276
```

V14 had 35656 / 43200 at the corresponding points.

The bounded intro still completes with exact 33772 B resource recovery:

```text
[INTRODISP] READY ... heap8=43208->76980 recovered=33772 ...
assets=NULL texts=NULL clip=off noMapLoad=yes
```

Entrance builds normally, then the direct first-frame route publishes the same
known world frame as before:

```text
[NATIVEFRAME] BSP map=1 resource=/intro.bsp nodes=23 leaves=6 ...
[NATIVEFRAME] WALL requests=8 draws=8 spans=160 pixels=4430 ...
[ENGINESESSION] FIRST_FRAME map=1 angle=64 frame=71ca7465 walls=8 pixels=4430 presented=1
```

There is no `[JUNCTIONFRAME] COLORSTATS` output in the production log.

The remaining session stages, cache priming and gameplay arm complete:

```text
[ENGINECACHE] PRIMED map=1 angle=64 ...
[RESIDENTGAMEPLAY] READY map=current ...
[ENGINESESSION] READY map=1 angle=64 residentCache=yes largeCache=yes
touch=invisible-120ms TURN+MOVE=armed shapeData=0x0 mediaTexels=0x0
```

Final hardware memory is stable across consecutive ALIVE samples:

```text
[ALIVE] ... heap=93396 heap8=27472 largest8=18420 ...
[ALIVE] ... heap=93396 heap8=27472 largest8=18420 ...
```

V14's corresponding settled heap8 was 27464, exactly 8 B lower.

## Result

PASS on the real classic CYD.

The hardware-tested code boundary is
`5460c689b708468e3bdd618d0000753159a24109`. Any V15 closure commits after
that boundary are documentation-only.

The permanent first-frame renderer remains the direct production owner.
Historical first-frame color/BMP fidelity instrumentation remains available only
where it belongs: `esp32-cyd-bringup`.
