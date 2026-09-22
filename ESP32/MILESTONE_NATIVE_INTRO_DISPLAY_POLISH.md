# Milestone — Native intro display polish

## Scope

This milestone validates the permanent display-fit policy for the native `ST_INTRO` presentation on the classic 320x240 CYD using the 160x120 RGB565 logical framebuffer.

It changes presentation geometry only. It does not add a second framebuffer, alter intro timing/state progression, or change gameplay ownership.

## Hardware-tuned fit

The original story composition uses a 128x128 coordinate space. The CYD logical framebuffer is 160x120, so one uniform scale policy gives poor results for some elements.

The validated split is:

```text
story virtual space  = 128x128
logical framebuffer  = 160x120
content viewport     = 120x120 @ x=20,y=0
background           = 160x120 @ x=0,y=0
animation scene      = 160x120 @ x=0,y=0
story text           = 156x120 @ x=2,y=0
extra frame bytes    = 0
```

### Ordinary story-page content

Small image/decorative geometry remains aspect-preserving in the centered 120x120 viewport. This avoids distorting UI ornaments such as the hand/prompt elements.

### Starfield and animated scene

The starfield and the dedicated animated scene use the full 160-pixel horizontal span. The animated path includes the background/layers/planet/spaceship and its mapped line geometry, keeping those elements spatially coherent under the same full-width transform.

### Narrative text

The earlier 120-pixel mapping looked too compressed on hardware. A later 160-pixel mapping looked slightly too wide. The accepted hardware-tuned compromise is a centered 156-pixel mapping with two logical pixels of margin on each side.

## Production witness

The renderer reports the fit contract explicitly:

```text
[INTROFIT] virtual=128x128 content=120x120@(20,0) background=160x120@(0,0) animation=160x120@(0,0) text=156x120@(2,0) fit=aspect-content+full-animation+soft-wide-text extraFrameBytes=0
```

## Hardware validation

Validated on real classic CYD from code head:

```text
0d21332bd2524bca73d5284f70e053ca8ba6430d
```

The visual verdict from the hardware test was PASS: the full-width animated image and slightly narrowed story text were accepted as correct.

## RAM / build boundary

CI for the tested code head:

```text
RAM:   [=         ]  13.7% (used 44832 bytes from 327680 bytes)
Flash: [======    ]  55.3% (used 724633 bytes from 1310720 bytes)
esp32-cyd SUCCESS
```

The canonical 44,832-byte static-RAM boundary remains unchanged.

## Result

**PASS on real classic CYD.**

The intro presentation now has a hardware-validated permanent fit policy without additional frame-sized memory.
