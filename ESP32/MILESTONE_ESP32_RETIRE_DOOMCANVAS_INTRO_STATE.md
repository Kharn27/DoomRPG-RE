# Milestone — DoomCanvas intro owner to 44-byte compact shell

Status: **REAL-CYD PASS**

Hardware-tested code boundary:
`d75451c56151458d8b0370c0c183531aa74fcd52`

Branch:
`agent/esp32-intro-state-owner`

## Goal

Continue shrinking the permanent ESP32 `DoomCanvas_t` compatibility object
without reviving desktop/J2ME ownership. Every cut must have a current native
owner, a fixed platform contract, or a source-closed proof that the field is
inert.

The branch starts from the hardware-proven 368-byte Canvas shell and ends at
44 bytes.

## Ownership cuts

### 1. ST_INTRO state -> transient native owner: 96 B

`EspNativeIntroState_t` owns the four prologue images, three copied strings,
story timers and story page/text state. It exists only for a fresh prologue and
is released by the bounded intro disposer before MAP_INTRO loading.

`DoomCanvas_loadPrologueText` leaves the ESP32 source ABI.

### 2. Dead shell: 56 B

Removed permanent fields with no ESP32 behavior: desktop allocation metric,
legacy automap cursor image, legal-screen image, retired config mirrors and the
write-only softkey restore flag. Config bytes remain consumed for file-format
compatibility.

The old `b.bmp` automap cursor is no longer loaded; native automap already
draws its compact vector cursor.

### 3. Inert animation / softkey / control shell: 72 B

The retained bridge no longer owns desktop animation-frame bookkeeping,
softkey storage/drawing, stale-view invalidation or other control bits whose
actual behavior is native or absent. Config still consumes the retired
animation-frame integer.

This retires three more bridge exports:

```text
DoomCanvas_drawSoftKeys
DoomCanvas_setAnimFrames
DoomCanvas_invalidateRectAndUpdateView
```

Source ABI: 10 -> 7.

### 4. Large font: 16 B plus runtime texture

The ESP32 bridge uses only the compact 9x12 font. All remaining compact-font
callers use `DoomCanvas_drawString1`, so `DoomCanvas_drawFont` and the
`imgLargerFont` field are retired.

`larger_font.bmp` is no longer decoded or retained during startup.

Source ABI: 7 -> 6.

Hardware on the 128-byte boundary
`b0362edcca38880069888ec039f8b5f6eab00ca9` showed the combined 88-byte
object reduction from the 216-byte boundary and the much larger post-layout
heap gain from avoiding the large-font texture allocation.

### 5. Fixed CYD geometry mirrors: 56 B

Canvas no longer stores:

```text
SCR_CX / SCR_CY
clipRect
displayRect
screenRect
```

The current target contract is permanent and explicit:

```text
logical canvas  160x120 @ 0,0
world viewport  160x80  @ 0,20
canvas centre   80,60
physical output 320x240, exact 2x
```

Bridge/menu/story/render startup callers derive those values from the ESP32
platform constants. Generated Render and DoomRPG sources are patched with
strict source-shape checks so an inherited geometry dependency cannot silently
return.

The 72-byte boundary
`74a4669a67269069700afc52dc10333e294f1935` passed CI #1669 and real CYD.

### 6. Inert view/shake/render aliases: 28 B

The final source-closure audit removes:

```text
Canvas.viewX/viewY/viewZ/viewAngle   16 B
Canvas.shakeX/shakeY                 8 B
Canvas.render                        4 B
                                    ----
                                     28 B
```

The four Canvas view values have no ESP32 consumer; current camera state lives
in native player/view owners and `Render_t`.

`shakeX/shakeY` have no ESP32 writer. The inherited Render source had exactly
eight reads of these always-zero fields. The generated ESP32 Render copy now
substitutes literal zero and the build script requires the exact 4+4 source
shape before patching.

`Canvas.render` was only a duplicate of `doomRpg->render`; startup now calls
`Render_setup(doomRpg->render, ...)` directly.

## Final layout

```text
DoomCanvas_t: 368 -> 272 -> 216 -> 144 -> 128 -> 72 -> 44 B

branch permanent reduction:                  324 B
desktop DoomCanvas reclaimed:       3696 / 3740 B
desktop layout retired:                       ~98.8%
source ABI exports:                         11 -> 6
```

The remaining 44-byte kernel is deliberately live:

```text
imgFont                      16 B
time                          4 B
state                         4 B
startupMap                    2 B
alignment                     2 B
skipIntro                     4 B
fontColor                     4 B
renderFloorCeilingTextures    4 B
doomRpg*                      4 B
                            -----
                              44 B
```

This is a useful architectural boundary, not an arbitrary target to force to
zero. `renderFloorCeilingTextures` still controls two generated Render paths;
`time/state/startupMap/skipIntro` are current transition/intro state;
`imgFont/fontColor` are the compact text compatibility path; and `doomRpg`
is the surviving object-graph root.

The bridge pins the 44-byte layout with a `_Static_assert`.

## CI

Final normal `esp32-cyd` CI #1670: **SUCCESS**.

```text
[ESP32] Desktop DoomCanvas.c retired; esp_legacy_doomcanvas_bridge.c owns 6 source ABI exports
[ESP32] Render generated with 1 legacy Game/Player monster activation block retired + 1 legacy Game mapFiles lookup redirected + 1 Canvas geometry mirror retired + 8 Canvas shake reads fixed-zero
RAM:   45056 B
Flash: 772405 B
esp32-cyd SUCCESS
```

Artifact:
`doom-rpg-esp32-cyd-d75451c56151458d8b0370c0c183531aa74fcd52`,
ID `11513961743`.

## Real-CYD final boot

```text
Engine structs: Render=5040 Game=4 Canvas=44 Total=5848 bytes
[DOOMCANVASBRIDGE] INIT exports=6 desktopTU=no bytes=44 ... retiredFixedGeometry=56 retiredViewShakeAlias=28 clip=160x120
[CORE] DoomCanvas     used=60 heap=187196 largest=110580
[CORE] READY objects=5 heap used=6028 remaining=181608 largest=110580 clip=160x120
```

Startup geometry remains exactly:

```text
[DOOMCANVASBRIDGE] STARTUP ... display=160x120 screen=160x80@0,20 ... geometry=fixed-cyd
[LAYOUT] clip    x=0 y=0 w=160 h=120
[LAYOUT] display x=0 y=0 w=160 h=120
[LAYOUT] screen  x=0 y=20 w=160 h=80
```

## Exact final memory delta

Relative to the immediately preceding 72-byte hardware boundary:

```text
checkpoint               72-B boundary   44-B boundary   gain
CORE READY                    181580          181608      +28 B
LAYOUT READY                  174968          175000      +32 B
mappings resident             155808          155840      +32 B
fresh gameplay ALIVE          114756          114788      +32 B
Exit->Menu heap8              160652          160684      +32 B
```

The object-level gain is exactly 28 B. The later allocator checkpoints expose
32 B because of heap block alignment.

## Geometry-sensitive menu acceptance

The final 44-byte firmware explicitly exercises both child screens that were
migrated off Canvas geometry mirrors.

HELP:
- parses 83 lines from PAK;
- page-down/page-up repaint hashes are stable;
- native Back returns exact MENU_MAIN FNV `522dc605`;
- heap stays `155840 / largest 110580`.

OPTIONS:
- paints native options FNV `162d3999`;
- armed Back FNV is `73303639`;
- fast native Back returns exact `522dc605`;
- `shapeData=0x0 mediaTexels=0x0`;
- no legacy menu renderer/map reload is invoked.

## Intro / MAP_INTRO acceptance

The compact text path and fixed centre still produce the canonical first intro
frame:

```text
[INTRO1] READY one deterministic ST_INTRO frame presented once FNV=ade0195d
```

Final Continue parks, then bounded disposal releases the hand and transient
intro owner while preserving the framebuffer and forbidding early map load:

```text
[INTROSTATE] RELEASE owner=native-transient state=NULL
[INTRODISP] READY ... heap8=130224->164280 recovered=34056 ... owner=NULL assets=NULL texts=NULL clip=off noMapLoad=yes
```

Native MAP_INTRO is unchanged:

```text
/intro.bsp
name=Entrance
CRC32=623f34e4
arenaBytes=14095
arenaFNV=c3882516
nodes=223 lines=480 sprites=344 events=93 byteCodes=265 strings=94
```

The canonical world frame remains exact:

```text
[ENGINESESSION] FIRST_FRAME map=1 angle=64 frame=71ca7465 walls=8 pixels=4430 presented=1
[ENGINESESSION] READY map=1 angle=64 ... shapeData=0x0 mediaTexels=0x0
```

## Gameplay / teardown acceptance

The final 44-byte run covers:
- movement and rotation;
- crate combat/transform;
- resource pickups;
- regular door animation;
- HUB inventory, weapon and status pages;
- PASS_TURN;
- known compact-renderer guard recovery;
- confirmed Exit To Menu.

Representative teardown:

```text
[RESIDENTRESET] heap8=142676->160684 released=18008 ... after=0/0/0/0/0/0/0 empty=1
[MAINMENU] Runtime cleanup legals=retired heap8=160684->160684 gained=0 ... shapeData=0x0 mediaTexels=0x0
[SYSEXIT] MENU-READY frame=522dc605 session=off resident=empty saveWrite=no checkpoint=unchanged
```

The established `LEGACY_GUARD -> RETRY -> RECOVERED` renderer path fires once
and recovers normally. It is unrelated to Canvas compaction.

The immediately preceding 72-byte hardware build also performed a version-11
checkpoint LOAD, exact native world/session restore, further movement/pickups,
monster activation, ordered attack visualization and committed retaliation.
That test remains a regression witness for the same branch before the final
source-closed 28-byte alias cut.

## Closure

Hardware-tested runtime code ends at
`d75451c56151458d8b0370c0c183531aa74fcd52`.

Any following closure commit must be documentation-only.
