# ESP32 consolidation milestone — explicit intro/startup composition V13

Date: 2026-09-30

## Boundary

```text
repo = Kharn27/DoomRPG-RE
base main = 1d2a9d504b258936e58da1f0d1b88128466645d6
branch = agent/esp32-consolidation-intro-startup-composition-v13
hardware-tested code head = 0f733d1a6ac680b0ff3f7954f4e40bcf44f942f8
CI #1191 = SUCCESS
artifact id = 11117812139
artifact digest = sha256:0536352fded6c011068d126cee5e3a8b13c2c70915cbddf6d819f6a1d31499fc
firmware.bin = 782368 B
firmware sha256 = 048dc5175861fece86ee3c98fc1d75458c1692c392cd27474f2b106eb5251e29
firmware.elf sha256 = cce81719d6b6c11e39d18b2ec31cf63192b623c2bdae2af44ec080615b136179
static RAM = 45224 B
linked Flash = 782009 B
active linker wraps = 55
```

## Goal

Remove the remaining hidden linker composition between the resource-only intro
disposer and the already-generic native new-game bootstrap.

This milestone adds no feature, translation unit, map-specific logic or generic
router.

## Audit result

Merged main still carried:

```text
-Wl,--wrap=Esp32IntroDispose_reset
-Wl,--wrap=Esp32IntroDispose_service
```

The wrapper bodies lived in `esp_native_startup.c`. Their real live producer
was the intro clock: `Esp32IntroClock_arm()` reset the disposer, and the parked
clock serviced it after `intro-exit-ready`.

V12 had already made `Esp32IntroDispose_service()` resource-only. Keeping the
bootstrap hidden behind its linker interception was therefore a redundant
transitional boundary.

## Permanent composition

V13 replaces the linker interception with a narrow explicit owner API:

```text
Esp32IntroClock_arm
 -> Esp32IntroDispose_reset
 -> EspNativeStartup_reset

Esp32IntroClock_service
 -> [after intro-exit-ready]
 -> Esp32IntroDispose_service
 -> EspNativeStartup_service
```

The startup owner still contains the existing generic resident lifecycle,
initial-spawn and transition-presentation logic. The disposer still owns only
intro resources.

## Final ELF

Direct inspection of the CI artifact ELF:

```text
Esp32IntroDispose_reset                 PRESENT
Esp32IntroDispose_service               PRESENT
EspNativeStartup_reset                  PRESENT
EspNativeStartup_service                PRESENT

__wrap_Esp32IntroDispose_reset          ABSENT
__wrap_Esp32IntroDispose_service        ABSENT
```

Active final `__wrap_*` symbols:

```text
merged main = 57
V13         = 55
delta       = -2
```

The legacy symbols retired by V12 remain absent:

```text
Menu_startGame
DoomCanvas_loadState
DoomCanvas_changeStoryPage
DoomCanvas_disposeIntro
DoomCanvas_loadMap
```

## Resource delta

Against merged main `1d2a9d504b258936e58da1f0d1b88128466645d6`:

```text
                 main          V13           delta
static RAM       45224 B       45224 B        0 B
Flash           782009 B      782009 B       0 B
firmware.bin    782368 B      782368 B       0 B
```

## Real classic-CYD proof

Native START reaches the unchanged intro boundary and explicitly resets the
startup owner:

```text
[MAINSTART] READY native new-game -> Player_reset -> ST_INTRO
[NATIVEBOOT] RESET generic resident bootstrap armed
[INTROCLK] ARMED ... heap8=43104 largest8=12276
```

The exact automatic page transition remains live:

```text
[INTROIN] CONTINUE storyPage=0->1 ...
[INTROCLK] AUTO-PAGE 1->2 t=21850 textPage=0 epoch=21850
```

Final Continue parks before disposal:

```text
[INTROIN] FINAL-CONTINUE page=2 textPage=0 ... fullTextPresented=yes
[INTROCLK] PARK reason=intro-exit-ready ...
```

The bounded disposer then frees only intro resources and preserves the frame:

```text
[INTRODISP] READY state=9 page=3 textPage=0
frameFNV=6708d363->6708d363
heap8=43104->76876 recovered=33772
largest8=12276->55284
assets=NULL texts=NULL clip=off noMapLoad=yes
```

The explicit native startup owner immediately takes over:

```text
[NATIVEBOOT] LOADING-TAKEOVER map=1 source=intro-disposed owner=transition-presentation
[NATIVEBOOT] RESIDENT map=1 file=/intro.bsp ... arena=14095 ...
[NATIVEBOOT] READY map=1 ... shapeData=0x0 mediaTexels=0x0
```

Generic resident gameplay comes up with the memory invariants intact:

```text
[ENGINESESSION] READY map=1 angle=64 residentCache=yes largeCache=yes
touch=invisible-120ms TURN+MOVE=armed shapeData=0x0 mediaTexels=0x0
[ALIVE] ... heap=93076 heap8=27368 largest8=18420 ...
[ALIVE] ... heap=93076 heap8=27368 largest8=18420 ...
```

## Result

PASS on the real classic CYD.

The hardware-tested code boundary is
`0f733d1a6ac680b0ff3f7954f4e40bcf44f942f8`. All changes after that boundary
for milestone closure are documentation-only.
