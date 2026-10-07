## Render planeTextures 2048-byte retirement — REAL-CYD PASS (2026-10-08)

Hardware-tested code SHA: `d98b6e07cc21b3bff91cd7f0f2c07b00607b01ed`.
Normal `esp32-cyd` CI #1678: SUCCESS (RAM 45056 B, flash 772389 B).
The second bounded Render cut removes 2048 B of legacy plane tile
mirror storage and fail-closes desktop world/plane render entries in
production; native `EspMapRuntime` and renderer are untouched.
`Render_t`: 5040 -> 4016 -> **1968 bytes**; cumulative -3072 B.
The real CYD reproduced exactly +2048 B heap8 at CORE (184680),
layout (178072), mappings (158912), gameplay (117860), and clean
Exit->Menu (163756), versus the preceding hardware cut.
MAP_INTRO first frame `71ca7465`, runtime arena FNV `c3882516`,
and MENU_MAIN return `522dc605` are unchanged. Confirmed native
movement, rotation, crate transformation, loot/pickups, door 275,
HUB/SYS/EXIT; `shapeData==NULL`, `mediaTexels==NULL`,
`[RESIDENTRESET] empty=1`, no checkpoint write. The known compact
renderer guard recovered. LOAD was not rechecked on this exact SHA.

Closure is documentation-only after the hardware-tested code commit.
Further branch cuts require separate CI and real-CYD logs.
See [MILESTONE_ESP32_RETIRE_RENDER_PLANETEXTURES.md](MILESTONE_ESP32_RETIRE_RENDER_PLANETEXTURES.md).

## Legacy Render mapFlags retired — REAL-CYD PASS (2026-10-08)

Hardware-tested runtime SHA:
\`2d58cdd52bf74242c35b4a196d388ab802ff573c\`.

Branch:
\`agent/esp32-render-mapflags-retirement\`.

The first bounded Render_t ownership cut drops its desktop BSP-only
\`mapFlags[1024]\` mirror in the normal \`esp32-cyd\` firmware. The
native \`EspMapState\` and \`EspMapAutomapState\` already own the
block-map/event/visited and reveal-state semantics. No substitute
map-wide allocation is introduced.

\`\`\`text
Render_t             5040 -> 4016 B   (-1024 B)
Game_t                         4 B
DoomCanvas_t                   44 B
Engine structs       5848 -> 4824 B   (-1024 B)
\`\`\`

The generated ESP32 Render source compiles fail-closed production
\`Render_beginLoadMap()\` and \`Render_beginLoadMapData()\` entries rather
than preserving the historical map-wide BSP parser. Their full original
bodies and \`mapFlags\` remain available only to desktop or explicit
\`DOOMRPG_ESP32_BRINGUP_PROBES\` builds. The generator insists on exactly
13 historical accesses in the loader region, rejecting future unreviewed
source consumers. The permanent Render startup bridge asserts the
4016-byte normal-firmware layout.

The normal \`esp32-cyd\` GitHub Actions CI #1675 completed SUCCESS on
the exact runtime SHA:

\`\`\`text
[ESP32] Legacy Render BSP loader fail-closed in production; Render.mapFlags 1024-byte mirror retired (bringup/desktop unchanged)
RAM:   45056 B
Flash: 772441 B
esp32-cyd SUCCESS
\`\`\`

The real classic CYD confirmed precise heap savings against the
hardware-validated 44-byte DoomCanvas boundary:

\`\`\`text
checkpoint                   before      after      gain
CORE READY heap8             181608     182632     +1024 B
LAYOUT READY heap8           175000     176024     +1024 B
mappings resident heap8      155840     156864     +1024 B
fresh gameplay ALIVE heap8   114788     115812     +1024 B
Exit->Menu heap8             160684     161708     +1024 B
\`\`\`

Hardware tested native Options and Help/About paths, both returning
exact main-menu FNV \`522dc605\` with steady heap8. Fresh START
completes the fitted intro, bounded intro disposal, native
\`/intro.bsp\` reconstruction, and canonical first world frame:

\`\`\`text
[INTRO1] READY ... FNV=ade0195d
[MAPRT] READY arenaBytes=14095 ... arenaFNV=c3882516
[ENGINESESSION] FIRST_FRAME map=1 angle=64 frame=71ca7465 walls=8 pixels=4430 presented=1
[ENGINESESSION] READY ... shapeData=0x0 mediaTexels=0x0
\`\`\`

The same tested firmware commits movement/rotation, automap discovery,
crate attack/transform, Armor Shard pickups, regular door 275
animation, HUB SYS and double-select EXIT. The known compact
\`LEGACY_GUARD -> RETRY -> RECOVERED\` renderer path also recovered
while gameplay continued. Final teardown is exact:

\`\`\`text
[RESIDENTRESET] heap8=143700->161708 released=18008 ... after=0/0/0/0/0/0/0 empty=1
[MAINMENU] Runtime cleanup ... heap8=161708->161708 ... shapeData=0x0 mediaTexels=0x0
[SYSEXIT] MENU-READY frame=522dc605 session=off resident=empty saveWrite=no checkpoint=unchanged
\`\`\`

No version-11 checkpoint LOAD was executed in this particular hardware
test; the preceding DoomCanvas milestone had already validated LOAD.
All subsequent code cuts on this branch are separate validation
boundaries and must not be described as hardware-proven by this record.

See [MILESTONE_ESP32_RETIRE_RENDER_MAPFLAGS.md](MILESTONE_ESP32_RETIRE_RENDER_MAPFLAGS.md).

# Doom RPG ESP32 CYD porting status

## DoomCanvas compact native shell — REAL-CYD PASS (2026-10-07)

Hardware-tested code boundary:
`d75451c56151458d8b0370c0c183531aa74fcd52`.

Branch:
`agent/esp32-intro-state-owner`.

This branch continued the DoomCanvas ownership cut from the previous 368-byte
hardware boundary and now leaves only a 44-byte ESP32 compatibility kernel.

The permanent cuts on this branch are:

```text
368 -> 272 B   transient ST_INTRO state moved native            -96 B
272 -> 216 B   dead shell + retired cursor/legal/config fields  -56 B
216 -> 144 B   inert animation/softkey/control shell            -72 B
144 -> 128 B   unused large-font image                          -16 B
128 ->  72 B   fixed CYD geometry mirrors                       -56 B
 72 ->  44 B   inert view/shake/render aliases                  -28 B
                                                                  -----
branch-total permanent reduction                                  324 B
desktop layout reclaimed                         3696 / 3740 B (~98.8%)
source ABI exports                                      11 -> 6
```

The 96-byte `EspNativeIntroState_t` remains transient and is still released
before MAP_INTRO loading. The dead-shell cut also stopped loading the obsolete
`b.bmp` automap cursor. The large-font cut removes `larger_font.bmp` from
startup entirely. Fixed CYD geometry is now derived from the permanent
160x120 / 160x80@0,20 platform contract instead of mirrored in Canvas.

The final 28-byte cut removes four unused Canvas view integers, two shake
integers with no ESP32 writer, and the redundant `Canvas->render` alias.
Generated `Render.c` replaces exactly eight inherited `shakeX/shakeY` reads
with fixed zero; the generator checks that exact source shape and fails closed
if it changes.

The 44 bytes that remain are intentionally live:

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

In particular, `renderFloorCeilingTextures` is still read by generated
`Render.c`; `time/state/startupMap/skipIntro` are active transition/intro
state; `imgFont/fontColor` remain the compact text path; and `doomRpg` is the
remaining object-graph root.

Normal `esp32-cyd` CI #1670 is green on the exact hardware-tested SHA:

```text
[ESP32] Desktop DoomCanvas.c retired; esp_legacy_doomcanvas_bridge.c owns 6 source ABI exports
[ESP32] Render generated with ... 1 Canvas geometry mirror retired + 8 Canvas shake reads fixed-zero
RAM:   45056 B
Flash: 772405 B
esp32-cyd SUCCESS
```

Artifact: `doom-rpg-esp32-cyd-d75451c56151458d8b0370c0c183531aa74fcd52`
(ID `11513961743`).

Real-CYD boot proves the final layout and allocator boundary:

```text
Engine structs: Render=5040 Game=4 Canvas=44 Total=5848 bytes
[DOOMCANVASBRIDGE] INIT exports=6 desktopTU=no bytes=44 ... retiredFixedGeometry=56 retiredViewShakeAlias=28 clip=160x120
[CORE] DoomCanvas     used=60 heap=187196 largest=110580
[CORE] READY objects=5 heap used=6028 remaining=181608 largest=110580 clip=160x120
```

The last 72 -> 44 B cut produces the exact +28 B CORE gain. Heap alignment
rounds that to +32 B at later stable checkpoints:

```text
checkpoint               72-B boundary   44-B boundary   gain
CORE READY                    181580          181608      +28 B
LAYOUT READY                  174968          175000      +32 B
mappings resident             155808          155840      +32 B
fresh gameplay ALIVE          114756          114788      +32 B
Exit->Menu heap8              160652          160684      +32 B
```

The final hardware acceptance explicitly covers both geometry-sensitive
main-menu children. HELP pages down/up and returns through native Back to exact
MENU_MAIN FNV `522dc605`; OPTIONS -> Back returns to the same FNV with
unchanged `heap8=155840`, and both paths keep
`shapeData=0x0 mediaTexels=0x0`.

Fresh START then completes the full prologue and bounded disposal. MAP_INTRO
remains `/intro.bsp`, arena FNV `c3882516`, and the canonical first world
frame is unchanged:

```text
[INTRO1] READY one deterministic ST_INTRO frame presented once FNV=ade0195d
[INTRODISP] READY ... recovered=34056 ... noMapLoad=yes
[MAPRT] READY arenaBytes=14095 ... arenaFNV=c3882516 ...
[ENGINESESSION] FIRST_FRAME map=1 angle=64 frame=71ca7465 walls=8 pixels=4430 presented=1
[ENGINESESSION] READY ... shapeData=0x0 mediaTexels=0x0
```

The same final build commits movement/turning, crate transform, pickups, regular
door animation, HUB inventory/weapons/status, PASS_TURN and confirmed
Exit To Menu. Resident cleanup returns exact MENU_MAIN FNV `522dc605` with
`shapeData/mediaTexels` still NULL. The known compact-renderer
`LEGACY_GUARD -> RETRY -> RECOVERED` path also fires and recovers normally.

A prior 72-byte hardware boundary on
`74a4669a67269069700afc52dc10333e294f1935` additionally exercised version-11
LOAD, restored native world/session state and monster retaliation. No regression
was observed before the final 28-byte source-closed alias cut.

Detailed record:
[MILESTONE_ESP32_RETIRE_DOOMCANVAS_INTRO_STATE.md](MILESTONE_ESP32_RETIRE_DOOMCANVAS_INTRO_STATE.md)

## DoomCanvas object-graph mirrors retired — REAL-CYD PASS (2026-10-07)

Hardware-tested code boundary:
`032c4d6eb8f1b9e3d5b4a3712499832cda96d4bc`.

Branch:
`agent/esp32-doomcanvas-bridge`.

The ESP32 DoomCanvas compatibility object no longer mirrors seven pointers that
were copied from `DoomRPG_t` during startup but never consumed through
`DoomCanvas_t` by the current ESP32 closure:

```text
player
game
entityDef
combat
hud
menuSystem
particleSystem
```

The bridge retains only the object references it actually consumes:
`doomRpg` and `render`.

This removes seven 32-bit pointers with no replacement owner and no runtime
allocation:

```text
DoomCanvas_t: 396 B -> 368 B
reclaimed this step:    28 B
reclaimed from desktop 3740 B layout: 3372 B (~90.2%)
source ABI exports: 11, unchanged
```

Normal `esp32-cyd` CI #1654 is SUCCESS:

```text
[ESP32] Desktop DoomCanvas.c retired; esp_legacy_doomcanvas_bridge.c owns 11 source ABI exports
RAM:   45432 B
Flash: 772925 B
esp32-cyd SUCCESS
```

ELF inspection confirms `DoomCanvas_s` is exactly 368 bytes and the linked
DoomCanvas ABI remains exactly 11 symbols.

Real-CYD boot proves the layout and allocator boundary:

```text
Engine structs: Render=5040 Game=4 Canvas=368 Total=6172 bytes
[DOOMCANVASBRIDGE] INIT exports=11 desktopTU=no bytes=368 ... retiredGraphMirrors=28 clip=160x120
[CORE] DoomCanvas     used=384 heap=186496 largest=110580
[CORE] READY objects=5 heap used=6352 remaining=180908 largest=110580 clip=160x120
```

Relative to the prior 396-byte hardware boundary, the deterministic startup
checkpoints recover the expected 28 bytes exactly:

```text
CORE READY   180880 -> 180908
LAYOUT       163296 -> 163324
mappings     144136 -> 144164
```

The acceptance run then exercises fresh START, complete intro and disposal,
native MAP_INTRO load, canonical first frame, PASS_TURN, movement, crate
combat/transform, pickup, regular door animation, HUB/System and confirmed Exit
To Menu. The same firmware performs a version-11 checkpoint LOAD, exact native
world/session restore, resumed movement and pickups, monster activation through
door visibility, ordered attack visualization and committed retaliation.

Canonical witnesses remain intact:

```text
[ENGINESESSION] FIRST_FRAME map=1 angle=64 frame=71ca7465 walls=8 pixels=4430 presented=1
[ENGINESESSION] READY ... shapeData=0x0 mediaTexels=0x0
[SYSEXIT] MENU-READY frame=522dc605 session=off resident=empty
[NATIVESAVE] LOAD ... version=11 ... world=...restored-exact session=reprime-pending
[MONSTERATKVIS] COMPLETE probe=1 ... resolution=unblocked-after-animation
[MONSTERRETAL] COMMIT probe=1 ... playerHP=34->32 armor=19->17 ... rollback=closed
```

The known compact renderer `LEGACY_GUARD -> RETRY -> RECOVERED` path appears
and recovers normally; it remains unrelated to this ownership cut.

The INIT witness still lacks a space between
`retiredDormantText=428` and `retiredStateLayout=116`. This is a cosmetic
logging defect only and does not affect the tested boundary.

No runtime/code change follows the hardware-tested commit in this closure.

Detailed record:
[MILESTONE_ESP32_RETIRE_DOOMCANVAS_GRAPH_MIRRORS.md](MILESTONE_ESP32_RETIRE_DOOMCANVAS_GRAPH_MIRRORS.md)


## DoomCanvas state bridge narrowed — REAL-CYD PASS (2026-10-07)

Hardware-tested code boundary:
`63a684d9c30bb20a9962a842c3551dc88aaa64c0`.

Branch:
`agent/esp32-doomcanvas-bridge`.

The permanent ESP32 DoomCanvas state compatibility hook is now explicitly
limited to the only three states still used by the current firmware:

```text
ST_MENU
ST_PLAYING
ST_INTRO
```

Inherited COMBAT / DIALOG / DYING / EPILOGUE / CREDITS / LOADING / AUTOMAP /
CAST branches are no longer accepted by the bridge. Their behaviors already
have native owners. Unsupported future calls fail closed with a
`STATE-REJECT` witness instead of silently widening desktop ownership.

Retiring those branches removes 113 bytes of obsolete field payload and 116
bytes of actual ESP32 layout after alignment. Three helpers reachable only
through the retired state fan-out also disappear from the ESP32 ABI:

```text
DoomCanvas_initCredits
DoomCanvas_loadEpilogueText
DoomCanvas_renderScene
```

The compact object is therefore:

```text
DoomCanvas_t: 512 B -> 396 B
reclaimed this step:   116 B
reclaimed from desktop 3740 B layout: 3344 B (~89.4%)
source ABI exports: 14 -> 11
```

Normal `esp32-cyd` CI #1650 is SUCCESS:

```text
[ESP32] Desktop DoomCanvas.c retired; esp_legacy_doomcanvas_bridge.c owns 11 source ABI exports
RAM:   45432 B
Flash: 772949 B
esp32-cyd SUCCESS
```

Compared with the prior 512-byte hardware boundary, CI also releases 32 bytes
of static RAM and 6232 bytes of flash by dropping the legacy state branches and
their loading strings.

Real-CYD boot proves the exact object and allocator boundary:

```text
[DOOMCANVASBRIDGE] INIT exports=11 desktopTU=no bytes=396 ... retiredStateLayout=116 clip=160x120
[CORE] DoomCanvas     used=412 heap=186468 largest=110580
[CORE] READY objects=5 heap used=6380 remaining=180880 largest=110580 clip=160x120
```

The stable hardware checkpoints improve by 148 bytes relative to the prior
512-byte boundary: 116 bytes from the heap-allocated DoomCanvas object plus the
32-byte static-RAM CI reduction.

```text
CORE READY       180732 -> 180880
LAYOUT           163148 -> 163296
mappings         143988 -> 144136
fresh LAZY_POST  103748 -> 103896
fresh ALIVE      102936 -> 103084
```

The acceptance run covers complete fresh START/intro/disposal, native MAP_INTRO
load, canonical first frame, crate actions, pickups, regular and deferred door
animation, a real opcode-26 dialog plus opcode-19 chained resume, HUB/System and
clean Exit To Menu. The same firmware then performs version-11 checkpoint LOAD,
restores the complete native world/session, resumes movement and pickups,
activates a monster through door visibility, completes its ordered attack
visual and commits retaliation damage, then returns through Options/Back with
no legacy menu renderer.

Canonical witnesses remain exact:

```text
[ENGINESESSION] FIRST_FRAME map=1 angle=64 frame=71ca7465 walls=8 pixels=4430 presented=1
[DIALOG] OPEN event=79 cmd=0 resume=1 opcode=26 string=3 ...
[DIALOGCHAIN] RESUME event=79 start=1 handled=1 ... state=1 ... mutation=1
[ENGINESESSION] READY ... shapeData=0x0 mediaTexels=0x0
[MONSTERATKVIS] COMPLETE probe=1 ... resolution=unblocked-after-animation
[MONSTERRETAL] COMMIT probe=1 ... playerHP=34->31 armor=15->12 ... rollback=closed
[SYSEXIT] MENU-READY frame=522dc605 session=off resident=empty
[OPTIONBACK] READY native semantic Back + opaque bounded repaint; no MenuSystem_back ...
```

The known compact renderer `LEGACY_GUARD -> RETRY -> RECOVERED` path still
recovers normally and remains unrelated.

The boot witness currently concatenates
`retiredDormantText=428` and `retiredStateLayout=116` without a separating
space. This remains a cosmetic log-format defect only; the values and runtime
behavior are unambiguous and hardware-proven.

No runtime/code change follows the hardware-tested commit in this closure.

Detailed record:
[MILESTONE_ESP32_NARROW_DOOMCANVAS_STATE_BRIDGE.md](MILESTONE_ESP32_NARROW_DOOMCANVAS_STATE_BRIDGE.md)


## DoomCanvas dormant text payloads + dead overall export retired — REAL-CYD PASS (2026-10-07)

Hardware-tested code boundary:
`ccad65e7f56a05a7f5d2e0c6e4768ce0a364d454`.

Branch:
`agent/esp32-doomcanvas-bridge`.

The ESP32 compatibility object no longer carries two desktop-only permanent
text payloads:

```text
epilogueText[2][150] = 300 B
printMsg[128]         = 128 B
retired this step     = 428 B
```

The desktop epilogue run-loop that consumed `epilogueText` is already absent
from the ESP32 firmware. The retained compatibility hook preserves only its
remaining lifecycle side effects; any future visible epilogue belongs to the
native UI/string path. No compiled ESP32 owner wrote `printMsg`, so the
loading/saving compatibility branch observably always used its existing
`"Processing..."` fallback. The 428-byte payload is removed rather than moved
to another permanent arena/BSS owner.

This makes the compact object:

```text
DoomCanvas_t: 940 B -> 512 B
reclaimed this step:   428 B
reclaimed from desktop 3740 B layout: 3228 B (~86.3%)
```

Removing the epilogue payload also made `DoomCanvas_getOverall()` unreachable.
The linked ELF already dropped it; the source ABI whitelist and ESP32 bridge
were then narrowed from 15 to 14 explicit exports so source and binary
boundaries agree.

Normal `esp32-cyd` CI #1646 is SUCCESS:

```text
[ESP32] Desktop DoomCanvas.c retired; esp_legacy_doomcanvas_bridge.c owns 14 source ABI exports
RAM:   45464 B
Flash: 779181 B
esp32-cyd SUCCESS
```

Real-CYD boot proves the exact object and allocator reduction:

```text
Engine structs: Render=5040 Game=4 Canvas=512 Total=6316 bytes
[DOOMCANVASBRIDGE] INIT exports=14 desktopTU=no bytes=512 retiredDialogStores=2560 retiredZeroRefLayout=240 retiredDormantText=428...
[CORE] DoomCanvas     used=528 heap=186320 largest=110580
[CORE] READY objects=5 heap used=6496 remaining=180732 largest=110580 clip=160x120
```

Compared with the hardware-proven 940-byte boundary, stable startup checkpoints
recover exactly 428 bytes: `CORE READY 180304 -> 180732`,
`LAYOUT 162720 -> 163148`, mappings `143560 -> 143988`, and fresh-session
lazy gameplay `103320 -> 103748`.

The acceptance run exercises substantially more than the retired payload:
fresh START and complete intro, exact intro disposal, native MAP_INTRO load,
the canonical first frame, crate combat/removal, pickups, regular and deferred
door animation, a real opcode-26 dialog with chained resume, standalone weapon
help dialog, HUB/System, confirmed Exit To Menu, version-11 checkpoint LOAD,
world/session restore, resumed movement, pickup, monster activation, ordered
attack visualization and committed retaliation.

Canonical witnesses remain exact:

```text
[ENGINESESSION] FIRST_FRAME map=1 angle=64 frame=71ca7465 walls=8 pixels=4430 presented=1
[DIALOG] OPEN event=79 cmd=0 resume=1 opcode=26 string=3 ...
[DIALOGCHAIN] RESUME event=79 start=1 handled=1 ... state=1 ... mutation=1
[DIALOG] OPEN-STANDALONE ... continuation=none ...
[ENGINESESSION] READY ... shapeData=0x0 mediaTexels=0x0
[MONSTERATKVIS] COMPLETE probe=1 ... resolution=unblocked-after-animation
[MONSTERRETAL] COMMIT probe=1 ... playerHP=34->32 armor=19->17 ... rollback=closed
[RESIDENTRESET] ... released=18008 ... after=0/0/0/0/0/0/0 empty=1
[SYSEXIT] MENU-READY frame=522dc605 session=off resident=empty
```

The known compact renderer `LEGACY_GUARD -> RETRY -> RECOVERED` path appears
and recovers normally. It remains unrelated to DoomCanvas compaction.

The boot witness currently concatenates `retiredDormantText=428` and
`clip=160x120` without a separating space. This is a log-format-only defect;
the values and runtime behavior are unambiguous and hardware-proven. It can be
fixed in the next code milestone without changing this tested boundary.

No runtime/code change follows the hardware-tested commit in this closure.

Detailed record:
[MILESTONE_ESP32_COMPACT_DOOMCANVAS_DORMANT_TEXT.md](MILESTONE_ESP32_COMPACT_DOOMCANVAS_DORMANT_TEXT.md)


## DoomCanvas zero-reference fields retired — REAL-CYD PASS (2026-10-07)

Hardware-tested code boundary:
`596a62b667a2a41dd29789abc249068fca4265fa`.

Branch:
`agent/esp32-doomcanvas-bridge`.

After the 2560-byte dialog-store retirement, a second complete source-closure
audit identified 52 inherited DoomCanvas fields with no access from any
translation unit compiled into the normal ESP32 firmware. Their explicit field
payload totals 239 bytes; removing them also eliminates one byte of alignment
padding, for an exact 240-byte layout reduction.

The ESP32 compatibility object is therefore:

```text
DoomCanvas_t: 1180 B -> 940 B
reclaimed this step:    240 B
reclaimed from desktop: 2800 B total
```

The desktop struct remains unchanged. On ESP32, the retired fields are absent,
so accidental future reuse fails at compile time. The bridge pins the resulting
layout with a `_Static_assert`.

Normal `esp32-cyd` CI #1640 is SUCCESS:

```text
RAM:   45464 B
Flash: 780097 B
esp32-cyd SUCCESS
```

Real-CYD boot proves the exact object and heap reduction:

```text
Engine structs: Render=5040 Game=4 Canvas=940 Total=6744 bytes
[DOOMCANVASBRIDGE] INIT exports=15 desktopTU=no bytes=940 retiredDialogStores=2560 retiredZeroRefLayout=240 clip=160x120
[CORE] DoomCanvas     used=956 heap=185892 largest=110580
```

The hardware acceptance run covers the full fresh-session route: START,
prologue, exact intro disposal, native MAP_INTRO load, the canonical first
frame, movement/rotation, crate transform, resource pickups, door animation,
HUB/System and a clean confirmed Exit To Menu.

The exact first-frame witness is unchanged:

```text
[ENGINESESSION] FIRST_FRAME map=1 angle=64 frame=71ca7465 walls=8 pixels=4430 presented=1
[ENGINESESSION] READY map=1 angle=64 ... shapeData=0x0 mediaTexels=0x0
```

The same firmware then performs MENU_MAIN -> LOAD on an existing version-11
checkpoint, restores the complete native world/session state, accepts further
movement and pickups, opens another door, activates a monster, delivers the
ordered native attack visualization and commits retaliation damage, then exits
cleanly again.

Representative resumed-session witnesses:

```text
[NATIVESAVE] LOAD ... version=11 ... world=...monster-drops-restored-exact session=reprime-pending
[ENGINESESSION] READY map=1 angle=0 ... shapeData=0x0 mediaTexels=0x0
[MONSTERATKVIS] COMPLETE probe=1 ... gameplayMutation=no resolution=unblocked-after-animation
[MONSTERRETAL] COMMIT probe=1 ... playerHP=34->32 armor=19->17 ... rollback=closed
[RESIDENTRESET] ... released=18008 ... after=0/0/0/0/0/0/0 empty=1
[SYSEXIT] MENU-READY frame=522dc605 session=off resident=empty saveWrite=no checkpoint=unchanged
```

The known compact renderer `LEGACY_GUARD -> RETRY -> RECOVERED` path appears
and recovers normally in both fresh and resumed gameplay; it remains unrelated
to DoomCanvas compaction.

No runtime/code change follows the hardware-tested commit in this closure.

Detailed record:
[MILESTONE_ESP32_COMPACT_DOOMCANVAS_ZEROREF_FIELDS.md](MILESTONE_ESP32_COMPACT_DOOMCANVAS_ZEROREF_FIELDS.md)


## DoomCanvas dialog stores retired — REAL-CYD PASS (2026-10-07)

Hardware-tested code boundary:
`2907e966f01c22cdecda048d7df0f2afdfef2549`.

Branch:
`agent/esp32-doomcanvas-bridge`.

The ESP32 definition of `DoomCanvas_t` no longer contains the inherited
desktop dialog payload stores:

```text
dialogIndexes[1024] = 2048 B
dialogBuffer[512]    =  512 B
total retired        = 2560 B
```

Those buffers had no reader or writer in the permanent 15-export DoomCanvas
bridge, the ESP32-native sources, or the remaining compiled compatibility TUs.
Dialogs are already owned by the native dialog/runtime path. The desktop
definition is unchanged; on ESP32 the fields do not exist, so any future direct
reuse fails at compile time.

The bridge pins the new layout with a compile-time guard:

```text
DoomCanvas_t: 3740 B -> 1180 B
reclaimed:                2560 B
```

Normal `esp32-cyd` CI #1636 is SUCCESS:

```text
RAM:   45464 B
Flash: 780161 B
esp32-cyd SUCCESS
```

Real-CYD boot proves the exact heap-object reduction:

```text
Engine structs: Render=5040 Game=4 Canvas=1180 Total=6984 bytes
[DOOMCANVASBRIDGE] INIT exports=15 desktopTU=no bytes=1180 retiredDialogStores=2560 clip=160x120
[CORE] DoomCanvas     used=1196
```

The hardware run then validates the full production path: fresh START, fitted
intro and exact disposal, native MAP_INTRO load, exact first-frame witness,
movement/rotation, crate transform and pickups, door animation, HUB/System,
confirmed Exit To Menu with exact resident teardown, MENU_MAIN -> LOAD of a
version-11 checkpoint, complete native world restore, and resumed committed
gameplay.

The canonical first-frame and memory invariants remain unchanged:

```text
[ENGINESESSION] FIRST_FRAME map=1 angle=64 frame=71ca7465 walls=8 pixels=4430 presented=1
[ENGINESESSION] READY ... shapeData=0x0 mediaTexels=0x0
[RESIDENTRESET] ... released=18008 ... after=0/0/0/0/0/0/0 empty=1
[SYSEXIT] MENU-READY frame=522dc605 session=off resident=empty
```

Checkpoint resume also reaches `ENGINESESSION READY` with
`shapeData=0x0 mediaTexels=0x0` and accepts further movement before a second
clean Exit To Menu.

The known compact renderer `LEGACY_GUARD -> RETRY -> RECOVERED` path appears
during both fresh and resumed gameplay and recovers normally; it is unrelated
to the DoomCanvas compaction.

No runtime/code change follows the hardware-tested commit in this closure.

Detailed record:
[MILESTONE_ESP32_COMPACT_DOOMCANVAS_DIALOG_STORES.md](MILESTONE_ESP32_COMPACT_DOOMCANVAS_DIALOG_STORES.md)


## Desktop DoomCanvas translation unit retired — REAL-CYD PASS (2026-10-07)

Hardware-tested code boundary:
`50dd03cdf9bf8a5531f6e4d9261a7e2fcce8709b`.

Branch:
`agent/esp32-doomcanvas-bridge`.

The normal ESP32 build no longer generates or compiles desktop
`src/DoomCanvas.c`. Its previously hardware-proven 15-source-function ABI is
implemented permanently by
`ESP32/src/esp_legacy_doomcanvas_bridge.c`.

The generator now treats that bridge as an explicit fail-closed boundary: any
future dependency on another inherited `DoomCanvas_*` function must fail at
link time instead of silently reviving the desktop state machine. It also
rejects accidental reintroduction of `DoomCanvas_run()` or
`DoomCanvas_loadMap()` into the bridge.

Normal `esp32-cyd` CI #1632 is SUCCESS:

```text
[ESP32] Desktop DoomCanvas.c retired; esp_legacy_doomcanvas_bridge.c owns 15 source ABI exports
RAM:   45464 B
Flash: 780389 B
esp32-cyd SUCCESS
```

The CI artifact ELF exposes exactly the intended 15 `DoomCanvas_*` symbols and
its source/DWARF records reference `esp_legacy_doomcanvas_bridge.c`, not
`DoomCanvas.c`.

Real-CYD boot proves the bridge owns construction and fixed native layout:

```text
[DOOMCANVASBRIDGE] INIT exports=15 desktopTU=no bytes=3740 clip=160x120
[CORE] DoomCanvas     used=3756 heap=183092 largest=110580
...
[DOOMCANVASBRIDGE] STARTUP desktopTU=no display=160x120 screen=160x80@0,20 startupMap=1 hud=native
[LAYOUT] READY real engine layout fits inside 160x120
```

The same hardware run completes a fresh START through the full intro/disposal
handoff and native MAP_INTRO load. The exact first-frame witness remains
unchanged:

```text
[ENGINESESSION] FIRST_FRAME map=1 angle=64 frame=71ca7465 walls=8 pixels=4430 presented=1
[ENGINESESSION] READY map=1 angle=64 ... shapeData=0x0 mediaTexels=0x0
[ALIVE] ... heap=165632 heap8=99708 largest8=86004 ...
```

Fresh gameplay then exercises native SELECT feedback, movement/rotation, crate
combat/transform, HUB/System and confirmed Exit To Menu. Resident teardown is
exact:

```text
[RESIDENTRESET] ... released=18008 ... after=0/0/0/0/0/0/0 empty=1
[SYSEXIT] MENU-READY frame=522dc605 session=off resident=empty saveWrite=no checkpoint=unchanged
```

The same firmware then performs MENU_MAIN -> LOAD on an existing version-11
checkpoint, restores the complete native world/session state, resumes gameplay
and continues committed movement. The resumed session settles at the same
`heap8=99708/largest8=86004` as the fresh session in this run:

```text
[NATIVESAVE] LOAD ... version=11 ... world=...monster-drops-restored-exact session=reprime-pending
[ENGINESESSION] RESUME checkpoint=restored ...
[ENGINESESSION] READY map=1 angle=0 ... shapeData=0x0 mediaTexels=0x0
[ALIVE] ... heap=165632 heap8=99708 largest8=86004 ...
```

A compact renderer `LEGACY_GUARD` was encountered during checkpoint resume and
closed through the already-accepted `RETRY -> RECOVERED` path before normal
gameplay continued. It is unrelated to the DoomCanvas TU retirement.

The current `DoomCanvas_t` compatibility object remains 3740 B. This milestone
retires the desktop translation unit only; structural compaction of that object
is the next separate boundary.

Several dormant compatibility exports (for example epilogue/credits helpers)
remain intentionally present because the linked-ABI audit requires them, but
they were not newly exercised by this hardware transcript. No claim is made
beyond the current runtime paths and the previously accepted ABI closure.

No runtime/code change follows the hardware-tested commit in this closure.

Detailed record:
[MILESTONE_ESP32_RETIRE_DESKTOP_DOOMCANVAS_TU.md](MILESTONE_ESP32_RETIRE_DESKTOP_DOOMCANVAS_TU.md)


## DoomCanvas linked-ABI whitelist — REAL-CYD PASS (2026-10-07)

Hardware-tested code boundary:
`d34cf007663bbb213f5c4e3f3eea7ae694339217`.

Branch:
`agent/esp32-compact-game-entity-storage`.

The generated ESP32 `DoomCanvas.c` now retains only the source ABI proven to
survive link-time GC. The desktop source contains 76 public `DoomCanvas_*`
definitions; the ESP32 generator removes **61** before compilation and retains
**15 source roots**.

The resulting ELF preserves the same linked DoomCanvas surface as the previous
accepted build: 15 source functions plus GCC's internal split
`DoomCanvas_drawFont$part$0`, for 16 binary symbols total.

Normal `esp32-cyd` CI for the tested commit is SUCCESS:

```text
[ESP32] DoomCanvas generated ... 61 dead public function(s) pruned + 15 source ABI root(s) retained ...
RAM:   45464 B
Flash: 780193 B
esp32-cyd SUCCESS
```

The real-CYD acceptance run exercises substantially more than boot. It covers:

- fresh START and complete intro handoff;
- exact MAP_INTRO first-frame witness `71ca7465`;
- movement, rotation, collision, automap publication and native renderer guard recovery;
- crate combat/transform and resource pickups;
- opcode-26 enter-dialog plus resumed opcode-19 state transitions;
- standalone weapon-help dialog;
- door open/close animation including deferred post-monster close;
- native monster activation, attack visualization, retaliation and committed player damage;
- HUB/System, confirmed Exit To Menu and exact resident teardown;
- version-11 LOAD and checkpoint restore of resources, scripts, line state/texture,
  action removals, crate state, automap, monsters, topology, positions, activation
  order and monster drops;
- resumed gameplay with the restored 12-monster active set.

Critical invariants remain intact:

```text
[ENGINESESSION] FIRST_FRAME map=1 angle=64 frame=71ca7465 ...
[ENGINESESSION] READY ... shapeData=0x0 mediaTexels=0x0
[RESIDENTRESET] ... empty=1
[SYSEXIT] MENU-READY ... session=off resident=empty
[NATIVESAVE] LOAD ... version=11 ... restored-exact
[ENGINESESSION] RESUME checkpoint=restored ...
```

The observed renderer compact-span `LEGACY_GUARD` path recovers normally and
is unchanged by this milestone.

No runtime/code change follows the hardware-tested commit in this closure.

Detailed record:
[MILESTONE_ESP32_DOOMCANVAS_LINKED_ABI_WHITELIST.md](MILESTONE_ESP32_DOOMCANVAS_LINKED_ABI_WHITELIST.md)

## Minimal ESP32 Game compatibility shell — REAL-CYD PASS (2026-10-07)

Hardware-tested code boundary:
`44ecee201cbfec6de72150999d94060c231585e5`.

Branch:
`agent/esp32-compact-game-entity-storage`.

The ESP32 `Game_t` compatibility shell is now exactly one pointer:

```c
struct DoomRPG_s* doomRpg;
```

Hardware confirms:

```text
Engine structs: Render=5040 Game=4 Canvas=3740 Total=9544 bytes
[CORE] Game           used=20 heap=177504 largest=110580
[CORE] Legacy Game shell minimal gameBytes=4 desktopBytes=36468 totalReclaimed=36464 fields=doomRpg-only worldOwner=native
```

This removes 36,464 of the 36,468 desktop `Game_t` bytes from the ESP32
layout (~99.99%). The remaining 4-byte backpointer exists only for the compact
config/teardown compatibility bridge.

The normal `esp32-cyd` build no longer depends on inherited Game world state.
To make the 4-byte layout compile, the generated ESP32 sources now explicitly
retire the last compile-only edges:

- 14 dead DoomCanvas functions that referenced retired Game fields;
- 2 retained DoomCanvas reads of `monstersTurn` / `activeSprites`;
- the stale Render monster-activation block that dereferenced retired
  Player/Game fields;
- the inherited Render map-file lookup, redirected to `EspMapCatalog`;
- inherited DoomRPG Game-store cleanup and Game allocation metric writes.

CI #1622 is SUCCESS:

```text
RAM:   45464 B
Flash: 780189 B
esp32-cyd SUCCESS
```

The real-CYD acceptance run validates the complete behavioral boundary, not
just boot. It covers fresh START, intro disposal, exact MAP_INTRO first frame,
crate transform + pickup, repeated native dialog chains with opcode continuation,
door animation/close deferral, HUB/SYS Exit To Menu, resident reset, dedicated
LOAD, exact version-11 spatial/checkpoint restore, active-monster resumed
gameplay, and another crate transform/pickup after restore.

Fresh-session memory settles at:

```text
[ALIVE] ... heap=165632 heap8=99708 largest8=86004 ...
```

The dialog-chain allocation later lowers that session to
`heap8=98672/largest8=86004`, and the checkpoint-resume session settles at the
same `98672/86004`, as expected for the owners active in that path.

Critical witnesses remain intact:

```text
[ENGINESESSION] FIRST_FRAME map=1 angle=64 frame=71ca7465 ...
[ENGINESESSION] READY ... shapeData=0x0 mediaTexels=0x0
[RESIDENTRESET] ... empty=1
[SYSEXIT] MENU-READY ... session=off resident=empty
[NATIVESAVE] LOAD ... version=11 ... world=...-restored-exact
[ENGINESESSION] RESUME ... checkpoint=restored ...
```

The existing native renderer compact-span safety guard also recovers normally
during resumed gameplay and does not affect the milestone result.

No post-test runtime/code change is included in this closure commit.

Detailed record:
[MILESTONE_ESP32_MINIMAL_GAME_SHELL.md](MILESTONE_ESP32_MINIMAL_GAME_SHELL.md)

## Desktop Game translation unit retired — REAL-CYD PASS (2026-10-07)

Hardware-tested code boundary:
`e497199152be23b86d98054ca458761f04109b9d`.

Branch:
`agent/esp32-compact-game-entity-storage`.

The normal ESP32 build no longer generates or compiles desktop `src/Game.c`.
Its only retained ABI roots are now permanently implemented by
`ESP32/src/esp_legacy_game_bridge.c`:

```text
Game_init
Game_loadConfig
Game_unloadMapData
Game_activate
```

Any future dependency on another inherited `Game_*` function now fails at
link time instead of silently reviving desktop gameplay code.

Normal `esp32-cyd` CI #1615 is SUCCESS:

```text
[ESP32] Desktop Game.c retired; esp_legacy_game_bridge.c owns Game_init/Game_loadConfig/Game_unloadMapData/Game_activate
RAM:   45464 B
Flash: 780881 B
```

Real-CYD acceptance exercises the bridge responsibilities end-to-end. The
shortened boot log starts after the core-object banner, but proves the bridge
`Game_loadConfig()` path reaches config/mappings startup with unchanged heap:

```text
[CONFIG] -> Game_loadConfig()
loadConfig
loadConfig: (unable to open file)
[CONFIG] DONE heap delta=0 heap8=148768 largest8=110580
[CONFIGMAP] READY config path exercised and mappings resident
```

A fresh START traverses intro disposal, catalog-backed MAP_INTRO loading and the
exact first-frame witness:

```text
[NATIVEBOOT] RESIDENT map=1 file=/intro.bsp ...
[ENGINESESSION] FIRST_FRAME map=1 angle=64 frame=71ca7465 walls=8 pixels=4430 presented=1
[ENGINESESSION] READY ... shapeData=0x0 mediaTexels=0x0
[ALIVE] ... heap=165200 heap8=99276 largest8=86004 ...
```

The same hardware run then exercises native crate transform, pickup, dialog
open/page/close plus resumed opcode execution, HUB/SYS Exit To Menu, and the
bridge teardown path:

```text
[SYS] EXIT-CONFIRMED queued=yes saveWrite=no boundary=after-session
[RESIDENTRESET] ... empty=1
[SYSEXIT] MENU-READY frame=522dc605 session=off resident=empty ...
```

Finally the main-menu LOAD path restores a version-11 checkpoint, including
resources, script, line state, automap, monster state/topology/positions/
activation and drops, then re-enters resident gameplay successfully:

```text
[NATIVESAVE] LOAD ... version=11 ... world=resources+script+lines+action-removals+crate-transforms+automap+monster-state+topology+position+activation+monster-drops-restored-exact
[ENGINESESSION] RESUME ... checkpoint=restored ...
[ENGINESESSION] READY map=1 angle=0 ... shapeData=0x0 mediaTexels=0x0
```

The checkpoint-resume session settles at `heap8=98240/largest8=86004`, lower
than fresh-start gameplay only because the dialog-chain owner and restored
session owners differ from the fresh path; no regression is observed.

This milestone deliberately keeps the 440-byte `Game_t` compatibility shell
unchanged. The next architectural blocker is the still-generated full desktop
`DoomCanvas.c`, whose dead functions force legacy `Game_t` fields to remain
compilable even though the linked DoomCanvas surface observes only a tiny subset.

No post-test runtime/code change is part of the closure commit.

Detailed record:
[MILESTONE_ESP32_RETIRE_DESKTOP_GAME_TU.md](MILESTONE_ESP32_RETIRE_DESKTOP_GAME_TU.md)

## Dormant Game transient stores compacted — REAL-CYD PASS (2026-10-07)

Hardware-tested code boundary:
`233904e1b27c03554166f0514bbf34465234dcb4`.

Branch:
`agent/esp32-compact-game-entity-storage`.

The normal ESP32 ELF no longer retains legacy trace or GameSprite producers:
`Game_trace()`, `Game_gsprite_alloc*()` and `Game_gsprite_update()` are
absent. Native collision/action tracing and gameplay FX own those responsibilities.
Only cleanup compatibility remained in the linked `Game_unloadMapData()`.

For ESP32 only, `Game_t::traceEntities[8]` and
`Game_t::gsprites[MAX_CUSTOM_SPRITES]` are therefore reduced to one-element
compile-time sentinels. The compatibility scalars still observed by retained
DoomCanvas state code (`activeSprites`, `f684l`, `monstersTurn`) are kept.
Desktop/J2ME capacities are unchanged.

Generated `Game_gsprite_clear()` clears only the sentinel and compatibility
scalars; generated `Game_unloadMapData()` clears the trace sentinel and
`numTraceEntities` fail-closed.

Normal `esp32-cyd` CI #1609 is SUCCESS:

```text
static RAM   = 45464 B
linked Flash = 780817 B
```

Real-CYD boot proves the exact compact layout:

```text
Engine structs: Render=5040 Game=440 Canvas=3740 Total=9980 bytes
[CORE] Game           used=456 heap=177068 largest=110580
[CORE] Legacy Game transient stores retired trace/gsprites=sentinel capacities=1/1 gameBytes=440 desktopBytes=36468 totalReclaimed=36028 owner=native-collision+gameplay-fx
[CORE] READY objects=5 heap used=10160 remaining=177068 largest=110580 clip=160x120
```

Relative to the preceding hardware-tested 768-byte `Game_t`, this recovers
another 328 B exactly. Settled resident gameplay rises from `heap8=98948` to
`heap8=99276`.

The same run exercises START, full intro disposal, native MAP_INTRO load,
movement/turning, pickups, door interaction, HUB/SYS, Exit To Menu, a second
fresh START, then additional movement, a native crate attack/removal and
strafe/turn actions. Both resident sessions settle at:

```text
[ALIVE] ... heap=165200 heap8=99276 largest8=86004 ...
```

The second START resets the mutated player fingerprint `363261d1` back to
the canonical fresh `e745fce9`. The exact first-frame witness remains
`71ca7465`; `shapeData=0x0` and `mediaTexels=0x0` remain invariant.

The existing renderer compact-guard recovery was exercised and recovered
normally before committed gameplay continued.

No post-test runtime/code change is part of the closure commit.

Detailed record:
[MILESTONE_ESP32_COMPACT_GAME_TRANSIENT_STORES.md](MILESTONE_ESP32_COMPACT_GAME_TRANSIENT_STORES.md)

## Legacy Game map string tables retired — REAL-CYD PASS (2026-10-07)

Hardware-tested code boundary:
`46b463a6c240f5e7fadd02d3aeb3dc83d65a620e`.

Branch:
`agent/esp32-compact-game-entity-storage`.

The ESP32 runtime no longer keeps the inherited mutable `Game_t::mapNames[]`
and `Game_t::mapFiles[]` tables. Map identity/resource names are owned by the
immutable native `EspMapCatalog`; the two legacy arrays are reduced to one-slot
compile-only sentinels on ESP32 while desktop/J2ME capacities remain unchanged.

Generated `Game.c` now routes the legacy map-resource lookup and save-state
resource-name edge through `EspMapCatalog`, retires the desktop map-table
initialization, and fails closed if any direct `game->mapNames[]` or
`game->mapFiles[]` access survives generation. Native START/bootstrap also
resolves `startupMap` directly through `EspMapCatalog`.

Normal `esp32-cyd` CI #1605 is SUCCESS:

```text
static RAM   = 45464 B
linked Flash = 780721 B
```

Real-CYD boot proves the compact layout and heap effect:

```text
Engine structs: Render=5040 Game=768 Canvas=3740 Total=10308 bytes
[CORE] Game           used=784 heap=176740 largest=110580
[CORE] Legacy Game map tables retired stores=sentinel capacities=1/1 gameBytes=768 desktopBytes=36468 totalReclaimed=35700 owner=EspMapCatalog
[CORE] READY objects=5 heap used=10488 remaining=176740 largest=110580 clip=160x120
```

Relative to the preceding hardware-tested `Game_t=1296` boundary, this removes
another 528 B from the structure. The menu/runtime witnesses rise accordingly
(`heap8=140000` at MENU_MAIN and `heap8=98948` in settled gameplay, subject
to allocator effects).

The same run completes START, full intro disposal, native `/intro.bsp` load,
the exact first-frame witness, movement, pickups, a door interaction, HUB/SYS,
Exit To Menu, then a second fresh START. Both gameplay sessions settle at the
same memory state:

```text
[ENGINESESSION] FIRST_FRAME map=1 angle=64 frame=71ca7465 walls=8 pixels=4430 presented=1
[ENGINESESSION] READY map=1 angle=64 residentCache=yes largeCache=yes touch=invisible-120ms TURN+MOVE=armed shapeData=0x0 mediaTexels=0x0
[ALIVE] ... heap8=98948 largest8=86004 ...
...
[SYSEXIT] MENU-READY frame=522dc605 session=off resident=empty saveWrite=no checkpoint=unchanged
...
[ALIVE] ... heap8=98948 largest8=86004 ...
```

The second START sees the mutated player fingerprint `363261d1` and resets it
to the canonical fresh `e745fce9`. Native map loading reports
`file=/intro.bsp`, confirming the catalog-backed startup path on hardware.

The existing renderer compact-guard recovery was exercised once and recovered
normally before gameplay continued. It remains unrelated to this map-catalog
ownership milestone.

No post-test runtime/code change is part of the closure commit.

Detailed record:
[MILESTONE_ESP32_RETIRE_GAME_MAP_TABLES.md](MILESTONE_ESP32_RETIRE_GAME_MAP_TABLES.md)

## Dormant Game entity storage compacted — REAL-CYD PASS (2026-10-07)

Hardware-tested code boundary:
`449e9e8425e6a4d125804728052c2b4735e31b97`.

Branch:
`agent/esp32-compact-game-entity-storage`.

A live-ELF audit of the merged main showed that the normal classic-CYD firmware
retains only five `Game_*` symbols: `Game_init`, `Game_gsprite_clear`,
`Game_loadConfig`, `Game_unloadMapData`, and the already fail-closed
`Game_activate` ABI stub. The inherited `Game_t` nevertheless still embedded
the retired desktop entity runtime:

```text
Entity_t entities[400]              25600 B
Entity_t *entityDb[1024]             4096 B
EntityMonster_t entityMonsters[100]  5600 B
```

On ESP32 only, those three stores are now compile-only sentinel arrays of one
element each. Desktop/J2ME capacities remain unchanged. Generated
`Game_unloadMapData()` also retires the obsolete 1024-entry `entityDb` clear.
Compile-time guards require the sentinel capacities and
`sizeof(Game_t) == 1296`.

Normal `esp32-cyd` CI #1602 is SUCCESS:

```text
static RAM   = 45464 B
linked Flash = 781141 B
```

Real-CYD boot proves the heap allocation collapsed exactly as intended:

```text
[CORE] Game           used=1312 heap=176212 largest=110580
[CORE] Legacy entity runtime retired stores=sentinel capacities=1/1/1 gameBytes=1296 desktopBytes=36468 reclaimed=35172 entities=0 monsters=0 owner=native-resident-map
[CORE] READY objects=5 heap used=11016 remaining=176212 largest=110580 clip=160x120
```

Compared with the preceding hardware witness (`Game used=36484`,
`remaining=141040`), the core heap recovers exactly 35172 B.

The same hardware run completes MENU_MAIN -> START, full intro disposal,
MAP_INTRO resident load, exact first frame `71ca7465`, native pickups/HUD,
SYS Exit To Menu, then a second fresh START. Both resident sessions settle at
the same memory state:

```text
[ALIVE] ... heap8=98416 largest8=86004 ...
...
[SYSEXIT] MENU-READY frame=522dc605 session=off resident=empty saveWrite=no checkpoint=unchanged
...
[ALIVE] ... heap8=98416 largest8=86004 ...
```

The second START also resets the mutated player fingerprint back to the canonical
fresh `e745fce9`. Throughout both sessions,
`shapeData=0x0` and `mediaTexels=0x0`.

A renderer compact-guard recovery was exercised once during a turn and recovered
normally before gameplay continued; it is not a failure of this storage
milestone.

No post-test runtime/code change is part of the closure commit.

Detailed record:
[MILESTONE_ESP32_COMPACT_GAME_ENTITY_STORAGE.md](MILESTONE_ESP32_COMPACT_GAME_ENTITY_STORAGE.md)

## Retired Player/Sound config dereferences closed — REVIEW FIX + REAL-CYD BOOT PASS (2026-10-07)

Current tested code boundary:
`084b0c0345cd38ed2093d6c08faf5db65d9a60e9`.

Branch:
`fix/mainMenu`.

Post-review audit found one remaining legacy `Player_t` dereference in
`Game_loadConfig()`: a compatible existing Config file would still read
`totalDeaths` into `doomRpg->player->totalDeaths`, even though
`doomRpg->player == NULL`. The symmetric `Game_saveConfig()` path also still
dereferenced retired Player/Sound owners.

The ESP32 generator now preserves Config stream/layout compatibility while
removing those owners:

- legacy Sound volume is consumed on load and not stored in `Sound_t`;
- legacy Player `totalDeaths` is consumed on load and not stored in `Player_t`;
- both retired fields are written as zero on legacy Config save paths;
- generation fails closed if either
  `doomRpg->sound->volume` or
  `doomRpg->player->totalDeaths` survives in generated `Game.c`.

Real-CYD boot on the corrected boundary reaches the config/mappings startup
without regression:

```text
[CONFIG] Config file present=no (missing is valid on first boot)
[CONFIG] -> Game_loadConfig()
loadConfig: (unable to open file)
[CONFIG] DONE heap delta=0 heap8=112740 largest8=73716
[MAPPINGS] Header texelOffsets=592 bitShapeOffsets=1300 textures=152 sprites=252
[MAPPINGS] Plan payload=8376B largestAlloc=5200B whileData heap8=112740 largest8=73716
```

This hardware run exercises the corrected firmware and the missing-Config
branch. A pre-existing compatible Config file was not present, so the
`version == CONFIG_VERSION` field-consumption branch is not claimed as a
hardware witness. Its retired-owner dereferences are nevertheless prevented
structurally by exact generator replacement plus a post-generation fail-closed
guard.

This review fix does not restore `Player_t` or `Sound_t`, does not alter the
native 52-byte PlayerState, and does not change the Config field ordering.

## Desktop Entity / EntityMonster translation units retired — REAL-CYD PASS (2026-10-06)

Hardware-tested code boundary:
`1442bda7f7f19d578afac81d151edfe2fc85e58a`.

Branch:
`fix/mainMenu`.

The normal `esp32-cyd` build no longer compiles `src/Entity.c` or
`src/EntityMonster.c`. The inherited `Game_t` shell remains temporarily for
config/teardown ABI, but its embedded `Entity_t[400]` and
`EntityMonster_t[100]` arrays are runtime-dormant: no legacy back-pointers are
seeded, `numEntities == 0`, `numMonsters == 0`, and active/inactive/combat/
spawn monster list heads remain NULL. Live map entities and monsters continue
to be owned by compact native resident-map/gameplay state.

The first retirement attempt at `c332ed12c7078b6a555ab1ebe876e302acc12ce4`
correctly removed both translation units but exposed one residual linker edge:
desktop `Game_activate()` still referenced `EntityMonster_getSoundID()`.
The final boundary keeps the legacy `Game_activate` ABI symbol only as a
fail-closed no-op; production activation remains
`EspNativeGameplayMonsterActivation`.

Normal local `esp32-cyd` build on the final boundary:

```text
static RAM   = 45464 B
linked Flash = 781517 B
```

Real-CYD boot proves the legacy entity runtime stays dormant:

```text
[CORE] Game           used=36484 heap=141040 largest=73716
[CORE] Legacy entity runtime retired arrays=dormant entities=0 monsters=0 owner=native-resident-map
[CORE] Player retired object=NULL owner=native-gameplay-player-state bytes=52
[CORE] READY objects=5 heap used=46188 remaining=141040 largest=73716 clip=160x120
```

START then traverses the full bounded intro/disposal and resident MAP_INTRO load,
reaching the exact first-frame witness and native gameplay session:

```text
[ENGINESESSION] FIRST_FRAME map=1 angle=64 frame=71ca7465 walls=8 pixels=4430 presented=1
[ENGINESESSION] READY map=1 angle=64 residentCache=yes largeCache=yes touch=invisible-120ms TURN+MOVE=armed shapeData=0x0 mediaTexels=0x0
[MONSTERSTATE] READY ... noLegacyEntity=yes ...
[MONSTERCOMBAT] READY ... legacyEntity=no
[MONSTERACT] READY ... source=bsp-render-visible persistence=map-session ...
```

The resident session stabilizes at:

```text
[ALIVE] ... heap=129164 heap8=63240 largest8=51188 ... CORE=ready ... MENU=ready
```

No post-test gameplay code change is part of this closure commit.

Detailed record:
[MILESTONE_ESP32_RETIRE_DESKTOP_ENTITY_TUS.md](MILESTONE_ESP32_RETIRE_DESKTOP_ENTITY_TUS.md)

## Legacy Player object retired — REAL-CYD PASS (2026-10-06)

Hardware-tested code boundary:
`9b47b4c141232bc75646d25d04d8b7adf6ecffc2`.

Branch:
`fix/mainMenu`.

The classic-CYD runtime no longer constructs the inherited `Player_t`.
`doomRpg->player` remains `NULL`; the 52-byte
`EspNativeGameplayPlayerState` is the authoritative player owner.
The normal ESP32 build also excludes `Player.c` and `CombatEntity.c`.
START now calls `EspNativeGameplayPlayerState_resetFresh()` directly instead
of resetting a desktop Player object.

The real CYD proves the fresh-game contract with the retired legacy pointer:

```text
[MAINSTART] Native player before stateFNV=00000000 legacyPlayer=0x0 owner=native-gameplay-player-state
[PLAYERSTATE] READY bytes=52 level=1 xp=0/80 hp=30/30 armor=0/20 def=16 str=12 agi=14 acc=16 ammo1=8 weapon=2 weapons=0004 stateFNV=e745fce9 legacyPlayer=no
[MAINSTART] Player after ... hp=30/30 armor=0/20 ... stateFNV=e745fce9 legacyPlayer=0x0
[MAINSTART] READY native new-game -> PlayerState_resetFresh -> ST_INTRO legacyPlayer=NULL
```

The same run exercises full intro/disposal, MAP_INTRO resident load, dialogs,
doors, fire clearing, pickups, secret XP, native monster activation,
retaliation, live movement, player kill/gib handling and HUB pages with no
legacy Player object.

Most importantly, the test mutates the native player to
`stateFNV=4745097e` (HP 22/30, Axe selected/owned, changed resources), performs
SYS `EXIT TO MENU`, then selects START again. The second START sees the old
native fingerprint but resets it exactly back to the canonical fresh state:

```text
[SYSEXIT] MENU-READY frame=522dc605 session=off resident=empty saveWrite=no checkpoint=unchanged
[MAINSTART] Native player before stateFNV=4745097e legacyPlayer=0x0 owner=native-gameplay-player-state
[PLAYERSTATE] READY ... hp=30/30 armor=0/20 ... weapon=2 weapons=0004 stateFNV=e745fce9 legacyPlayer=no
[MAINSTART] READY native new-game -> PlayerState_resetFresh -> ST_INTRO legacyPlayer=NULL
```

This closes the stale dual-owner ambiguity: a new game can no longer reset only
the desktop Player while leaving the actual native gameplay state dirty.

Memory remains healthy and returns cleanly across the Exit-to-Menu teardown:

```text
[RESIDENTRESET] heap8=87668->105684 released=18016 ... empty=1
[MAINOPAQUE] ... heap8=105684 largest8=73716
...
[ALIVE] ... heap=125704 heap8=59780 largest8=36852 ... CORE=ready ... MENU=ready
```

The second resident session has the same total `heap8=59780` as the preceding
gameplay state, so no leak is indicated. Its largest free block is lower
(`51188 -> 36852`) after the repeated Exit/START cycle; this remains above the
16384-byte reserve target and is recorded as fragmentation to watch on future
multi-cycle tests, not as a failure.

No post-test gameplay code changes are part of this closure commit.

Detailed record:
[MILESTONE_ESP32_RETIRE_LEGACY_PLAYER_OBJECT.md](MILESTONE_ESP32_RETIRE_LEGACY_PLAYER_OBJECT.md)

## Legacy Hud object retired — REAL-CYD PASS (2026-10-06)

Hardware-tested code boundary:
`8d0cd7443d2524829128f6ee26b1c19244bd6dbd`.

Branch:
`agent/esp32-retire-legacy-combat`.

The remaining inherited `Hud_t` compatibility object is now retired from the
normal classic-CYD runtime. `doomRpg->hud` stays `NULL` by construction;
visible HUD composition, top-bar/status feedback, dialogs and view flashes are
owned by the native gameplay UI path. A final stale Hud dependency in the
native main-menu model was also removed before the tested boundary.

The real CYD validates MENU_MAIN -> START, full intro/disposal, exact Entrance
first frame `71ca7465`, resident gameplay, chained dialogs including opcode
19/26, automap open/close with HUD repaint, doors, fire-clear, note/dialog
resume, pickups and weapon-help, secret feedback, active monster turns,
committed monster retaliation, live monster movement, player kill/gib/drop and
dynamic drop pickup.

Representative combat witnesses:

```text
[MONSTERRETAL] COMMIT ... playerHP=30->27 armor=8->5 ... message="6 damage!" ...
[MONSTERRETAL] COMMIT ... playerHP=27->24 armor=5->2 ... message="6 damage!" ...
[MONSTERCOMBAT] COMMIT seq=102 ... hp=5->0 ... alive=1->0 ... xp=5-applied ...
[MONSTERDROP] COMMIT ... type=3 subtype=21 def=92 tile=658 ...
[PLAYERRES] COMMIT tile=658 ... armor=6/20 ... rollback=closed
```

Stable post-lazy-allocation ALIVE:

```text
heap=123612 heap8=57688 largest8=51188
```

The normal local PlatformIO build reports 45464 B static RAM and 781017 B
linked Flash. No completed GitHub Actions result is attached to this exact
hardware-tested head, so no CI result is claimed for it.

Detailed record:
[MILESTONE_ESP32_RETIRE_LEGACY_HUD_OBJECT.md](MILESTONE_ESP32_RETIRE_LEGACY_HUD_OBJECT.md)

## Desktop Hud translation unit retired — REAL-CYD PASS (2026-10-04)

Hardware-tested code boundary:
`9efa082d1aacbc137e1e7fcd726f857c0dbae2d6`.

Branch:
`agent/esp32-retire-legacy-combat`.

The normal `esp32-cyd` build no longer compiles `src/Hud.c`. A bounded
ESP32 compatibility shim temporarily preserves only the scalar/message ABI
still referenced by inherited helpers. All legacy HUD raster functions in that
shim are no-ops; visible HUD composition is owned by
`EspNativeGameplayHud` and its PAK-backed renderer.

Real-CYD validation covers cold boot, MENU_MAIN, START, the complete bounded
intro, resident MAP_INTRO load, hub inventory/weapons/status pages, PASS_TURN,
automap open/close, native dialogs, standalone weapon-help dialog, player
movement/pickups, monster activation, monster hit/miss retaliation, player
attack/kill, drop materialization and top-bar feedback.

Representative evidence:

```text
[HUDCOMPAT] INIT bytes=600 images=NULL renderer=native-pak-stream
[LAYOUT] HUD legacy bitmaps status=0x0 large=0x0 faces=0x0 icons=0x0 attack=0x0 arrow=0x0 owner=native-pak-stream
...
[GAMEPLAYHUD] REPAINT ... ownerMutation=no ...
...
[MONSTERRETAL] COMMIT ... message="5 damage!" ...
[MONSTERRETAL] MISS-COMMIT ... message="Dodged!" ...
[MONSTERCOMBAT] COMMIT ... alive=1->0 ...
```

The stable resident witness remains:

```text
heap=127880 heap8=61956 largest8=51188
```

Normal `esp32-cyd` CI #1584 is SUCCESS:

```text
static RAM   = 45472 B
linked Flash = 781385 B
artifact id  = 11303743052
artifact sha256 = d90cb7dc7e4f18db92c0d3317be493ab2ea85ba6af6920d61da913693148886a
```

This is 1384 B less linked Flash than the preceding HUD-bitmap build.
No local PlatformIO build is claimed.

Detailed record:
[MILESTONE_ESP32_RETIRE_DESKTOP_HUD_TU.md](MILESTONE_ESP32_RETIRE_DESKTOP_HUD_TU.md)

## Legacy HUD bitmap residency retired — REAL-CYD PASS (2026-10-04)

Hardware-tested code boundary:
`96813c8333ec07dcfc7343517e6c69764b70f082`.

Branch:
`agent/esp32-retire-legacy-combat`.

The normal ESP32 layout no longer calls legacy `Hud_startup()` to make six
desktop HUD bitmaps resident. The compact `Hud_t` compatibility object remains
temporarily for scalar geometry/message fields, while the production
`EspNativeGameplayHud` continues to read HUD assets directly from the native
PAK as bounded on-demand data.

Retired resident images:

```text
imgStatusBar
imgStatusBarLarge
imgHudFaces
imgIconSheet
imgAttArrow
imgStatusArrow
```

Real-CYD gameplay remains healthy through PASS_TURN, movement, automap-derived
redraws, repeated armor pickups, collision, regular door open/close and
top-bar/action feedback. A stable resident witness is:

```text
[ALIVE] ... heap=127880 heap8=61956 largest8=51188 ...
```

The previous legal-strip run at a comparable resident boundary reported
`heap=123632 / heap8=57708`. The observed +4248 B is consistent with removing
the six packed HUD image allocations plus allocator/image metadata overhead.

Normal `esp32-cyd` CI #1582 is SUCCESS:

```text
static RAM   = 45472 B
linked Flash = 782769 B
artifact id  = 11304071480
artifact sha256 = ca85db10f8a0595a743f68c27ea02f5ccc8b30bd165ea03ffbd58091a90b243e
```

No local PlatformIO build is claimed.

Detailed record:
[MILESTONE_ESP32_RETIRE_LEGACY_HUD_BITMAPS.md](MILESTONE_ESP32_RETIRE_LEGACY_HUD_BITMAPS.md)

## Legacy legal-strip residency retired — REAL-CYD PASS (2026-10-04)

Hardware-tested code boundary:
`35c310026484f095448ef926b2bba944c8dbb359`.

Branch:
`agent/esp32-retire-legacy-combat`.

The normal classic-CYD boot no longer allocates the inherited `g.bmp`
128x512 legal-screen strip. ESP32 production boot already bypasses
`ST_LEGALS` and paints the native opaque MENU_MAIN dashboard directly, so
retaining 32768 packed pixel bytes until START was dead residency.

Real-CYD evidence:

```text
[LAYOUT] heap8 used=21828 remaining=117932 largest=73716
...
[MAINOPAQUE] ... heap8=98772 largest8=73716
[ALIVE] ... heap=164696 heap8=98772 largest8=73716 ...
...
[MAINMENU] Runtime cleanup legals=already-free heap8=98772->107212 gained=8440 ...
```

The previous Sound-retirement menu witness was `heap8=65888`; the new menu
witness is `98772`, a +32884 B increase consistent with avoiding the 32768 B
packed legal strip plus allocator effects. START, intro disposal, MAP_INTRO
load, native HUD, resident gameplay, pickups and doors remain functional.

Normal `esp32-cyd` CI #1580 is SUCCESS:

```text
static RAM   = 45472 B
linked Flash = 782817 B
artifact id  = 11302983372
artifact sha256 = 5b53223c103def2db06d252853751e3d512f7e41c3fb00359eb245fee515d25e
```

No local PlatformIO build is claimed.

Detailed record:
[MILESTONE_ESP32_RETIRE_LEGACY_LEGALS_STRIP.md](MILESTONE_ESP32_RETIRE_LEGACY_LEGALS_STRIP.md)

## Legacy Sound object retired — REAL-CYD PASS (2026-10-04)

Hardware-tested code boundary:
`fffe6f6d780ae1d7444c49cb08df747fe5f4ca0c`.

Branch:
`agent/esp32-retire-legacy-combat`.

The classic-CYD runtime no longer constructs the inherited `Sound_t`.
Audio playback remains intentionally deferred; gameplay/menu code publishes
bounded `EspNativeAudioIntent` records while compatibility
`Sound_playSound/stopSounds/freeSounds` calls are NULL-safe no-ops.

The ESP32 generated sources also retire every remaining direct field dereference
that would require a resident Sound object:

```text
Game_loadConfig()      -> legacy volume field consumed but not stored in Sound_t
DoomCanvas_castState() -> legacy soundEnabled branch fixed to playback-disabled behavior
DoomCanvas_run()       -> legacy nextplay reset removed
DoomRPG_free()         -> Sound_free ownership removed
```

Real-CYD boot proves the object is absent:

```text
[CORE] Sound retired object=NULL owner=native-audio-intent playback=deferred
[CORE] READY objects=7 heap used=47460 remaining=139760 largest=73716 clip=160x120
```

START, the full bounded intro, resident MAP_INTRO construction, first frame,
HUD, caches and resident gameplay all complete successfully with
`doomRpg->sound == NULL`. The session reaches:

```text
[ENGINESESSION] READY ... shapeData=0x0 mediaTexels=0x0
[ALIVE] ... heap=123640 heap8=57716 largest8=51188 ... CORE=ready ...
```

The previous Combat-retirement witness at the same broad gameplay stage was
`heap=123436`; the observed +204 B is consistent with removing the ~220 B
Sound allocation, subject to normal allocator/layout variation.

Normal `esp32-cyd` CI #1578 is SUCCESS:

```text
static RAM   = 45472 B
linked Flash = 782837 B
artifact id  = 11302676326
artifact sha256 = 502243e5bdd8abc9c9b2764428939098440619e44e5f7b3871442925028501e9
```

No local PlatformIO build is claimed.

Detailed record:
[MILESTONE_ESP32_RETIRE_LEGACY_SOUND.md](MILESTONE_ESP32_RETIRE_LEGACY_SOUND.md)

## Legacy Combat object retired — REAL-CYD PASS (2026-10-04)

Hardware-tested code boundary:
`766d1e0cf2280d784f2c073f552e3f57889dc541`.

Branch:
`agent/esp32-retire-legacy-combat`.

The normal ESP32 runtime no longer constructs the inherited `Combat_t`.
`Combat.c` and the now-unreferenced desktop `Weapon.c` are excluded from
the `esp32-cyd` build, `doomRpg->combat` is required to remain `NULL`,
and the remaining production responsibilities are owned by the native combat
math, monster combat/retaliation, monster-turn sequencer and native first-person
weapon renderer.

The pre-retirement ELF retained only three desktop Combat roots:
`Combat_init`, `Combat_free` and `Combat_drawWeapon`. The native weapon
renderer already owns the legacy idle/attack offsets and bounded PAK-backed
decode/cache path, so the ESP32-generated `DoomCanvas.c` no longer calls the
legacy weapon draw. Generated `DoomRPG.c` likewise no longer retains
`Combat_free`.

The first hardware attempt exposed one real cleanup dependency rather than a
combat dependency. START crashed with `StoreProhibited`,
`EXCVADDR=0x00000004`; symbolization placed the fault in
`Game_unloadMapData()` during `DoomRPG_esp32ReleaseMainMenuMemory()`.
That cleanup still wrote `combat->curTarget/curAttacker = NULL`. The final
ESP32-generated `Game.c` retires only those obsolete writes while preserving
the rest of menu/map teardown. No `Combat_t` allocation or stub was restored.

Real-CYD acceptance on the final boundary proves START reaches resident gameplay
and a native player attack remains functional:

```text
[ACTIONENGINE] TRACE seq=3 weapon=2 distance=1 tile=873 target=sprite index=127 ... route=CRATE_SUBTYPE2
[CRATE] CONSEQUENCE seq=3 ... outcome=TRANSFORM ... attackDamage=6 attackArmorDamage=4 ...
[MONSTERTURN] ATTACK-REQUEST seq=3 source=explicit-native-player-attack ...
[CRATE] COMMIT seq=3 ... ammo=8->7 ... turnAdvance=PLAYER_ATTACK-requested rollback=closed
[ACTIONENGINE] ATTACK seq=3 weapon=2 frame=1->0 generic=yes worldCommitted=yes
[MONSTERTURN] ORDERED-DISPATCH reason=PLAYER_ATTACK turnToken=2 activeCount=0 ...
```

This covers the native weapon attack pose, ammo mutation, generic combat math,
crate consequence RNG/world mutation and semantic turn publication with
`doomRpg->combat == NULL`.

The same run remains stable in resident gameplay:

```text
[ALIVE] ... heap=123436 heap8=57512 largest8=51188 ... CORE=ready ... MENU=ready
```

A previous comparable early resident-gameplay witness before Combat retirement
was `heap=122376`; the observed +1060 B is consistent with removing the
1036-byte `Combat_t` allocation plus allocator overhead. Treat this as a
hardware consistency witness, not an exact allocator accounting proof.

Normal `esp32-cyd` CI #1575 is SUCCESS:

```text
static RAM   = 45480 B
linked Flash = 782773 B
artifact id  = 11299309172
artifact sha256 = f0d066fdfa7535ae875e1a9ff06269656103ff5b3952ae6a05ee85378a1d18ab
```

Versus merged main before the milestone: static RAM is 8 B lower and linked
Flash is 1800 B lower. More importantly, the 1036-byte desktop Combat object is
no longer allocated at runtime.

The final ELF contains no `Combat_*` or desktop `Weapon_*` symbols.
`CombatEntity_*` remains intentionally linked because `Player_t` still uses
that small stats container; retiring it belongs to a later Player cleanup.

No local PlatformIO build is claimed.

Detailed record:
[MILESTONE_ESP32_RETIRE_LEGACY_COMBAT.md](MILESTONE_ESP32_RETIRE_LEGACY_COMBAT.md)

## Legacy EntityDef manager retired — REAL-CYD PASS (2026-10-04)

Hardware-tested code boundary:
`9fa46cf86a1e8b4bbb25e20d80cfbf9664ef22db`.

Branch:
`agent/esp32-retire-legacy-entitydef`.

The ESP32 normal runtime no longer allocates or starts the inherited
`EntityDefManager_t`. `src/EntityDef.c` is excluded from the normal
`esp32-cyd` build, `doomRpg->entityDef` is required to remain `NULL`, and
resident maps resolve all entity metadata through the compact immutable
`EspEntityDefTypeCatalog` built from `/entities.db` in the native PAK.

This removes the duplicated desktop representation: the retired manager owned a
heap table of full 24-byte `EntityDef_t` records including persistent 16-byte
names, while the native catalog retains only compact
`{tileIndex,type,subtype,parm}` metadata and reads names from the PAK on
demand.

The final ELF/build graph contains no compiled `EntityDef.c` object and no
`EntityDef_init/startup/find/lookup/free` symbol. The native
`EspEntityDefTypeCatalog_*` owner remains linked.

The real CYD validates the consumers that matter rather than only startup:

```text
[PLAYERRES] PREPARE ... defTile=92 type=3 subtype=21 parm=4 action=armor ...
[PLAYERRES] FEEDBACK ... message="Got Armor Shard" sourceDefTile=92 ...
[PLAYERRES] PREPARE ... defTile=1 type=5 subtype=0 parm=0 action=weapon ...
[PLAYERRES] FEEDBACK ... message="Got Axe" sourceDefTile=1 ...
```

Those paths require catalog metadata and on-demand definition names. Combat then
materializes a dynamic monster drop by native type/subtype lookup, and the same
catalog validates and consumes it:

```text
[MONSTERDROP] COMMIT ... type=3 subtype=20 def=91 tile=658 ... persistence=map-session-live/save-deferred
[PLAYERRES] PREPARE tile=658 sprite=65535 defTile=91 type=3 subtype=20 parm=4 action=health ...
[PLAYERRES] FEEDBACK tile=658 message="Got Health Vial" sourceDefTile=91 ...
```

The same run also exercises doors, dialogs, secret activation, ordered monster
combat and post-kill movement without any legacy EntityDef owner.

Observed stable ALIVE samples include:

```text
heap=122376 heap8=56452 largest8=51188
heap=118916 heap8=52992 largest8=49140
```

The first figure is before later lazy gameplay allocations; the second is after
dialog/combat resources have become resident. They are runtime witnesses, not a
claim that every byte difference versus an older build belongs to EntityDef.

Normal `esp32-cyd` CI #1568 is SUCCESS:

```text
static RAM   = 45488 B
linked Flash = 784573 B
artifact id  = 11280285689
artifact sha256 = 3bff6f3b27453ac42a988f918eac3d0d83c1adc6813e091f54b131d855d38785
```

Static RAM is 8 B lower than the merged pre-milestone boundary. The larger
memory win is runtime heap: the desktop manager object and its separately
allocated full definition table are no longer constructed.

No local PlatformIO build is claimed.

Detailed record:
[MILESTONE_ESP32_RETIRE_LEGACY_ENTITYDEF.md](MILESTONE_ESP32_RETIRE_LEGACY_ENTITYDEF.md)

## Native extinguisher turn parity — REAL-CYD PASS (2026-10-03)

Hardware-tested code boundary:
`a36edddd5b74b1737991f8fcfecb0ac43da727b8`.

Branch:
`agent/esp32-player-action-turn-parity`.

Legacy `Player_fireWeapon()` routes a successful player-owned utility attack
through `ST_COMBAT`; when that player combat finishes,
`DoomCanvas_combatState()` calls `Game_advanceTurn()` for
`curAttacker == NULL`. The native extinguisher path previously committed
`FIRE_CLEARED` with `turnAdvance=deferred`, allowing gameplay to continue
without explicitly publishing the player turn.

The native fire-clear commit now requests the already-proven
`PLAYER_ATTACK` monster-turn producer after the world/player transaction is
prepared. If the transactional world render fails, that pending turn is cancelled
before the fire removal and PlayerState/ammo rollback are restored. No new turn
owner, RNG path, heap allocation or legacy entity ownership is introduced.

The scope was deliberately reduced after real-CYD feedback: jammed/destructible
doors already behaved correctly on hardware, including a monster behind the
destroyed door taking its turn. That route was therefore left unchanged rather
than risking a duplicate semantic advance.

Hardware acceptance was initially functional gameplay acceptance. A later
real-CYD run on the merged code supplied a direct serial witness for the
fire-clear turn producer and dispatch:

```text
[MONSTERTURN] ATTACK-REQUEST seq=64 source=explicit-native-player-attack rollback=available-until-cancel
[ACTIONENGINE] FIRE-COMMIT seq=64 ... turnAdvance=PLAYER_ATTACK-requested rollback=closed
[MONSTERTURN] ORDERED-DISPATCH reason=PLAYER_ATTACK turnToken=19 activeCount=0 ...
[MONSTERACTIVESEQ] BEGIN turn=19 reason=3 activeCount=0 ...
```

A second fire in the same session repeats the exact sequence at seq=68 /
turnToken=23. The witness proves the successful extinguisher action now consumes
the semantic player turn and dispatches `PLAYER_ATTACK`. Both observed fires
had `activeCount=0`, so this still does not claim a captured “fire plus
already-active enemy attacks immediately” scenario.

Normal `esp32-cyd` CI #1563 is SUCCESS:

```text
static RAM   = 45496 B
linked Flash = 784877 B
artifact id  = 11268746232
artifact sha256 = a5ba9d656fcd0c468a7c4b489abdc4f6314b900301d41281b45f379fc3d53ed2
```

No local PlatformIO build is claimed.

Detailed record:
[MILESTONE_ESP32_NATIVE_EXTINGUISHER_TURN_PARITY.md](MILESTONE_ESP32_NATIVE_EXTINGUISHER_TURN_PARITY.md)

## Native door / monster-turn parity — REAL-CYD PASS (2026-10-03)

Hardware-tested code boundary:
`69ee31a17f2e4900c825cc73ba1fee9400fc8ac5`.

Branch:
`agent/esp32-monster-drop-checkpoint-v11`.

This closes the remaining native door-turn parity gap against the legacy
`DoomCanvas SELECT -> Game_executeTile -> Game_advanceTurn` behavior.

Three bounded corrections now compose without reviving legacy world ownership:

- a monster standing on a door line that closes may leave its source tile through
  the recovered legacy type-0 source-axis exception; destination collision still
  sees the closed line;
- MOVE-triggered door closure commits `EspMapLineState=closed` immediately but
  holds the regular-door visual open through the monster turn, then animates the
  close after the ordered sequence completes;
- a successful ordinary SELECT door event explicitly schedules
  `SELECT_DOOR`, so monsters revealed by the opening render receive the same
  semantic turn as legacy. CHANGEMAP transition doors remain transition-owned.

The real CYD proves an ordinary opened door with no active monsters is harmless:

```text
[ACTION] DOOR-BATCH event=65 count=1 status=OK ... open=0->1 ...
[MONSTERTURN] DOOR-REQUEST seq=83 ... legacyAdvance=yes ...
[RESIDENTGAMEPLAY] SELECT ... turnAdvance=SELECT_DOOR-requested
[MONSTERTURN] ORDERED-DISPATCH reason=SELECT_DOOR turnToken=25 activeCount=0 ...
```

It then opens a two-line secret door whose render activates two monsters. The
same SELECT immediately owns their ordered turn: sprite 220 attacks, the
sequencer waits for retaliation to resolve, then sprite 264 moves:

```text
[MONSTERACT] ACTIVE sprite=220 ... activeCount=1 activationOrder=0 ...
[MONSTERACT] ACTIVE sprite=264 ... activeCount=2 activationOrder=1 ...
[MONSTERTURN] DOOR-REQUEST seq=92 ...
[MONSTERTURN] ORDERED-DISPATCH reason=SELECT_DOOR turnToken=31 activeCount=2 ...
[MONSTERTURN] MEMBER-ATTACK-PROBE reason=SELECT_DOOR sprite=220 ...
[MONSTERACTIVESEQ] PAUSE ... probe=1 reason=attack-in-flight ...
[MONSTERRETAL] COMMIT probe=1 ... playerHP=30->27 armor=8->6 ...
[MONSTERACTIVESEQ] RESUME ... nextOrdinal=2/2 ...
[MONSTERMOVELIVE] COMMIT ... sprite=264 tile=694->693 ...
[MONSTERACTIVESEQ] COMPLETE turn=31 reason=5 activeCount=2 delivered=2 ...
```

The same hardware session also proves the new close-presentation ordering keeps
gameplay collision closed while visual closure waits for the monster turn:

```text
[DOORANIM] HOLD line=234 logical=closed visual=open ... collision=closed-now
[DOORANIM] HELD-FRAME ... logical=closed visual=open ...
[MONSTERTURN] ORDERED-DISPATCH reason=MOVE turnToken=28 ...
[DOORANIM] RELEASE deferredClose=1 phase=after-monster-turn logical=closed ...
[DOORANIM] COMPLETE transitions=1 frames=4 state=stable transaction=committed
[DOORANIM] POST-MONSTER-COMPLETE ... logical=closed visual=closed
```

A preceding real-CYD run had already confirmed the recovered source-line escape
prevents a monster caught by a closing door from remaining permanently trapped.
The visual hold was then added so that escape/movement occurs before the player
sees the door close.

Normal `esp32-cyd` CI #1556 is SUCCESS:

```text
static RAM   = 45496 B
linked Flash = 784621 B
artifact id  = 11266949372
artifact sha256 = 59d5e1ea7848495f6a936e8c03364f545b8129b116be696160d48fe48302c85c
```

No local PlatformIO build is claimed.

Detailed record:
[MILESTONE_ESP32_NATIVE_DOOR_MONSTER_TURN_PARITY.md](MILESTONE_ESP32_NATIVE_DOOR_MONSTER_TURN_PARITY.md)

## Native monster-drop checkpoint persistence V11 — REAL-CYD PASS (2026-10-02)

Hardware-tested code boundary:
`41665b6676f31372ef67a41257fd8baabc74d8ed`.

Branch:
`agent/esp32-monster-drop-checkpoint-v11`.

The compact 8-slot `EspNativeGameplayMonsterDrop` owner is now part of the
native checkpoint. Legacy Doom RPG is the behavioral reference: its World save
serialized the eight rotating drop entities plus `dropIndex`, and LOAD restored
that already-materialized state directly rather than calling
`Entity_spawnDropItem()` or consuming RNG again.

V11 is an append-only checkpoint extension:

```text
V10 bytes                    = 5460
monster-drop snapshot        = 144
V11 bytes                    = 5604
rotating slots               = 8
record size                  = 16 B
CRC coverage                 = full V11 record
legacy V1..V10 read support  = retained
```

The snapshot keeps the exact current pool rather than its history: eight compact
records plus arena identity, `spawnSerial`, `nextSlot` and a semantic FNV.
`visibleCount` is derived on restore. Records carry only native identity,
tile/world position and active/taken state; no `Entity_t`, mutable BSP sprite,
map-wide decompression or ZIP runtime state is introduced.

Session replacement now explicitly resets the drop owner before checkpoint
restore. This matters for LOAD on the same map, where the rebuilt immutable arena
has the same FNV and could otherwise retain post-SAVE live state accidentally.
V1..V10 checkpoints therefore restore an honestly empty dynamic-drop pool.

The real CYD first proves backward compatibility by loading the pre-existing V10
checkpoint:

```text
[NATIVESAVE] READABLE-SPATIAL ... bytes=5460 ... result=valid
[NATIVESAVE] LEGACY-MONSTER-DROP-GAP version=10 dynamicDrops=fresh-empty ... rng=untouched
[NATIVESAVE] LOAD ... version=10 bytes=5460 ... monsterDrops=legacy-empty/0/00000000/serial0/next0 ...
```

A lethal zombie attack then materialized Shell Clips in slot 0. The live drop was
left on the floor and saved:

```text
[MONSTERDROP] COMMIT roll=b61a3cc5 slot=0 ... type=16 subtype=2 def=86 tile=178 pos=1184,352 visible=1 next=1 ...
[MONSTERDROP] SAVE version=11 arena=c3882516 serial=1 next=1 visible=1 stateFNV=16550b12 snapshotBytes=144 rng=untouched
[NATIVESAVE] SAVE ... version=11 bytes=5604 ... recordCrc=3a147996 ...
```

The player then picked that live drop up after the SAVE. A same-map LOAD of the
saved checkpoint restored the earlier pool exactly:

```text
[NATIVESAVE] READABLE-SPATIAL ... bytes=5604 ... result=valid
[MONSTERDROP] RESTORE version=11 arena=c3882516 serial=1 next=1 visible=1 stateFNV=16550b12 rng=untouched materialize=replay-no
[NATIVESAVE] LOAD ... version=11 bytes=5604 ... monsterDrops=restored/1/16550b12/serial1/next1 ... monster-drops-restored-exact
```

The user confirmed the restored presentation is visually correct. Entering the
saved drop tile then consumed the restored dynamic drop through the normal
transactional pickup path, together with the co-located static ammo pickup:

```text
[MONSTERDROP] RENDER-CULL slot=0 tile=178 reason=player-tile pickup=pending-after-commit
[PLAYERRES] PREPARE ... defTile=86 type=16 subtype=2 ... action=ammo value=25->35 ... worldRemove=dynamic-drop-slot rollback=armed
[PLAYERRES] PREPARE ... defTile=83 type=6 subtype=1 ... action=ammo value=6->10 ... worldRemove=hidden-overlay rollback=armed
[PLAYERRES] COMMIT tile=178 candidates=2 consumed=2 ...
```

This proves the live materialized state survives SAVE/LOAD without RNG replay or
legacy entity reconstruction, and that same-map session replacement does not leak
the post-SAVE consumed state into the restored checkpoint.

A second real-CYD run independently closes the opposite checkpoint state:
the drop was already consumed before SAVE. The saved PlayerResources overlay
contained 36 consumed resources, and V11 restored the same rotating slot as
taken:

```text
[PLAYERRES] RESTORE ... consumed=36 bytes=43 ...
[MONSTERDROP] RESTORE version=11 arena=c3882516 serial=1 next=1 visible=0 stateFNV=8d1747e5 rng=untouched materialize=replay-no
[NATIVESAVE] LOAD ... version=11 bytes=5604 ... monsterDrops=restored/0/8d1747e5/serial1/next1 ... monster-drops-restored-exact
```

The drop did not reappear visually after LOAD. Hardware therefore proves both
V11 states for the same slot: an untaken saved drop restores visible and
pickable; a taken saved drop restores invisible and stays consumed.

Normal `esp32-cyd` CI #1540 is SUCCESS:

```text
static RAM   = 45392 B
linked Flash = 781433 B
artifact id  = 11248194036
```

No local PlatformIO build is claimed.

Detailed record:
[MILESTONE_ESP32_NATIVE_MONSTER_DROP_CHECKPOINT_V11.md](MILESTONE_ESP32_NATIVE_MONSTER_DROP_CHECKPOINT_V11.md)

## SYS Exit To Menu — REAL-CYD PASS, merge accepted (2026-10-06)

The fixed `CHECKPOINT 1` save-slot caption is replaced by three stacked
`SAVE` / `LOAD` / `EXIT TO MENU` cards. Exit uses two-step confirmation with
`UNSAVED CHANGES LOST`, never autosaves, and leaves the existing SD checkpoint
untouched. The existing native main dashboard/model/touch route is reused.
Session/map teardown is deferred until all gameplay service wrappers return;
intro startup servicing is parked before returning to `MENU_MAIN`/`ST_MENU`.
No new production source file or legacy menu dependency is added.

Validation: `pio run -e esp32-cyd` passes, 45392 B static RAM / 779333 B flash.
The committed `test_sys_touch.c` passes all content pixels, gaps/margins, all
nine cursor/target routes and fail-closed preselection. Level-progress regression
passes. Temporary host fixtures pass SYS painting/framebuffer guards and mocked
exit lifecycle/order/repeat/recovery checks, plus parked intro continuation
cancellation. Hardware restart/load behavior is
not claimed: the acceptance checklist is in
[`DOCUMENTATION.md`](DOCUMENTATION.md#sys-exit-to-menu--development-candidate-2026-10-02).

Rebased on `origin/main` `72ec1b2` on 2026-10-04. V11 checkpoint persistence,
monster utility/door turn parity and legacy EntityDef retirement are retained.
Post-rebase local CYD build: PASS, 45488 B static RAM / 785225 B flash. SYS touch
and level-progress host tests: PASS. Exit hardware acceptance remains pending.

Rebased again on `origin/main` `f01be0b` on 2026-10-06. The upstream retirement
of legacy Combat/Sound/Hud and presentation resources is preserved. Removed
Exit's obsolete non-NULL Hud/Combat admission guards; no legacy owner is restored.
Local CYD build: PASS, 45464 B static RAM / 781645 B flash. SYS touch and
level-progress tests: PASS. A mocked host lifecycle fixture also passes Exit
with both legacy pointers NULL, teardown ordering, repeat and core/pack refusal.

Real-CYD lifecycle acceptance now passes on exact code boundary
`986a703b967e747219334e5f2d04d0bafe0bab6e`. Double-select EXIT returns from
resident gameplay to the exact main dashboard with `saveWrite=no` and the
checkpoint unchanged. The teardown reports:

```text
[RESIDENTRESET] heap8=90480->108488 released=18008 before=1/1/1/1/1/1/1 after=0/0/0/0/0/0/0 empty=1
[SYSEXIT] MENU-READY frame=522dc605 session=off resident=empty saveWrite=no checkpoint=unchanged
```

The user repeated `MENU_MAIN -> LOAD -> gameplay -> SYS -> EXIT` twice. Both
loads stabilize at `heap8=62580 largest8=51188`; both exits return exactly to
`heap8=108488 largest8=73716`. This is the hardware no-leak witness for the
exercised destructive lifecycle. `shapeData` and `mediaTexels` remain NULL.

Additional real-CYD acceptance proves Exit-confirmation cancellation by
leaving SYS: after arming Exit, switching pages and later closing the HUB does
not queue a delayed teardown; normal gameplay resumes and commits movement and
pickup state. A later in-game LOAD after unsaved movement plus an Armor Shard
pickup restores the checkpoint exactly from `pos=1248,352 / armor=15/23 /
playerFNV=eeda091c` back to `pos=1120,352 / armor=11/23 /
playerFNV=52c15778`.

After another Exit, Help/About opens, pages and returns through the native Back
route to exact main FNV `522dc605` with menu memory unchanged at
`heap8=108488 largest8=73716`. Options opens with zero heap delta and its Back
card arms correctly; the supplied transcript stops before the second Back tap,
so Options -> Back is not yet claimed.

The later legacy-Player retirement run additionally proves a fresh
`EXIT TO MENU -> START` cycle resets the authoritative native PlayerState and
re-enters full intro/gameplay successfully. The branch is accepted for merge.
Two edge cases remain explicitly unclaimed rather than blocking: the second tap
of Options -> Back after Exit, and the no-checkpoint / main-menu `No Save`
path.

## Native LEVEL UP screen + checkpoint monster projection — REAL-CYD PASS (2026-10-02)

Hardware-tested code boundary:
`ccce5be96690086d56babc477c88b41faf289c72`.

Branch:
`agent/esp32-levelup-drop-consequences`.

The legacy level-up dialog has been replaced by a dedicated native full-screen
presentation owner. The real CYD proves the complete lethal-combat transition:

```text
[MONSTERCOMBAT] COMMIT ... xp=6-applied level=1->2 levelUps=1 ...
[LEVELUP] PRESENT seq=1 level=1->2 levelUps=1 gains=hp+5/armor+3/def+1/str+2/agi+1/acc+1 health=restored ... input=fresh-release+fullscreen-tap stats=current+gain-mini owner=dedicated-fullscreen timer=none
[LEVELUP] ARM seq=1 level=1->2 levelUps=1 status=OK ...
[MONSTERTURN] SKIP reason=PLAYER_ATTACK levelup=active legacySkipTurn=yes mutation=no
[RESIDENTGAMEPLAY] LEVELUP-TAP tap=2 ... dismiss=requested source=fresh-fullscreen ...
[GAMEPLAYHUD] REPAINT health=35/35 armor=11/23 ...
[LEVELUP] CLOSE seq=1 level=1->2 input=tap worldRedraw=complete turnAdvance=no owner=released
[RESIDENTGAMEPLAY] LEVELUP-CLOSE hudRepaint=yes ... worldRedraw=yes fullScreenOwner=released turnAdvance=no
```

Hardware acceptance confirms the screen remains visible until an explicit fresh
tap, cannot be overwritten by late combat/HITFX/feedback presents, restores both
HUD bands immediately on close, and resumes gameplay without advancing a
monster turn. The title visibly reports the real transition as
`LEVEL 1 -> 2`; each stat card shows the current post-level value plus its
compact gain instead of a gain in isolation. Health restoration follows the
legacy Player_nextLevel behavior. Level-up sound 5043 remains intentionally
deferred.

The same branch also fixes checkpoint-resume monster presentation. V9/V10
MonsterPosition and collision/topology were already restored exactly, but the
presentation-only movement projection bitset was previously cleared on session
reset. That made a moved monster render at its immutable BSP spawn until its
first live movement. Resume now reconstructs projection before any visible
world frame by comparing restored native positions with immutable BSP positions:

```text
[MONSTERPOS] RESTORE ... stateFNV=149e39dd source=checkpoint-v9 topologyExact=yes
[MONSTERMOVELIVE] CHECKPOINT-PROJECTION arena=c3882516 monsters=30 projected=9 source=restored-position-v9 inference=raw-bsp-delta firstFrame=exact
```

The real CYD confirms the previously displaced zombie is visually at the saved
position immediately after LOAD, before any player or monster movement. No SAVE
format field was added; the projection mask is derived presentation state.

Normal `esp32-cyd` CI #1532 is SUCCESS:

```text
static RAM   = 45392 B
linked Flash = 778489 B
```

Detailed record:
[MILESTONE_ESP32_NATIVE_LEVEL_UP_AND_RESUME_PROJECTION.md](MILESTONE_ESP32_NATIVE_LEVEL_UP_AND_RESUME_PROJECTION.md)

Authoritative recovery/status file for the classic ESP32-2432S028R port. Repository state wins over chat history. Serial logs from the real classic CYD are the final runtime authority.

## Native monster dynamic drops — REAL-CYD PASS (2026-10-02)

Hardware-tested code boundary:
`61b156415829a4f4bc70f76d1ba751effedc29eb`.

Branch:
`agent/esp32-levelup-drop-consequences`.

The native lethal-monster path now owns the original Doom RPG drop resolution
without mutating immutable BSP sprite/topology state. It keeps the legacy
8-entry rotating pool as a compact 144 B map-session owner, resolves the exact
legacy type/subtype selection from the already-consumed drop RNG word, renders
live drops from that owner, and feeds them through the existing shared
`EspNativeGameplayPlayerState` pickup path.

A first real-CYD test exposed a renderer boundary bug: entering the drop tile
projected the dynamic billboard through the camera origin, producing
`TURNFRAME DIAG fail=SPRITES` and a MOVE rollback. Commit
`61b156415829a4f4bc70f76d1ba751effedc29eb` fixes that boundary by culling
a live drop only while it occupies the player's committed tile; pickup service
then consumes it immediately after the MOVE frame.

The real CYD now proves the complete path with a zombie drop:

```text
[MONSTERDROP] COMMIT roll=d8fb13bb slot=0 overwriteVisible=0 type=3 subtype=21 def=92 tile=268 pos=800,544 visible=1 next=1 ownerBytes=144 persistence=map-session-live/save-deferred
[MONSTERCOMBAT] COMMIT ... dropRoll=value/d8fb13bb dropMaterialize=live ...
[MONSTERDROP] RENDER-CULL slot=0 tile=268 reason=player-tile pickup=pending-after-commit
[RESIDENTGAMEPLAY] MOVE ... tile=267->268 ... committed=yes
[PLAYERRES] PREPARE tile=268 sprite=65535 defTile=92 type=3 subtype=21 parm=4 action=armor value=1->5 ... worldRemove=dynamic-drop-slot rollback=armed
[PLAYERRES] PREPARE tile=268 sprite=148 defTile=91 type=3 subtype=20 parm=4 action=health value=26->30 ... worldRemove=hidden-overlay rollback=armed
[PLAYERRES] COMMIT tile=268 candidates=2 consumed=2 ... hp=30/30 armor=5/20 ...
[PLAYERRES] FEEDBACK tile=268 message="Got Armor Shard" sourceDefTile=92 ... additionalMessages=1-deferred
```

This also validates a dynamic monster drop and an existing static pickup sharing
one tile in the same transaction. Earlier hardware witnesses on the same branch
also proved legitimate `dropMaterialize=none` results for legacy no-drop RNG
values. Dynamic-drop checkpoint persistence remains intentionally deferred and
is not claimed by this milestone.

Normal `esp32-cyd` CI #1524 is SUCCESS at the tested boundary:

```text
static RAM   = 45352 B
linked Flash = 776233 B
```

A later real-CYD milestone on the same branch hardware-validates the dedicated
LEVEL UP screen and checkpoint monster projection; see the section above.

Detailed record:
[MILESTONE_ESP32_NATIVE_MONSTER_DYNAMIC_DROP.md](MILESTONE_ESP32_NATIVE_MONSTER_DYNAMIC_DROP.md)

## Mission report — USER ACCEPTED; V10 round-trip remains candidate (2026-10-02)

Working branch `fix/mainMenu`, rebased onto `origin/main` at `0f1cdb0` without
conflicts or changes to the mission-report implementation. The user accepted
the report on the rebased CYD branch at `e0ae824`. No detailed serial evidence
was provided; this acceptance does not claim full V10 SAVE/LOAD validation.
The previous hardware/CI witness below remains unchanged.

End-of-stage presentation now shares the HUB palette/crisp 5x7 face and shows
Secrets/Monsters with real progress bars plus elapsed time, original-style Moves
(player turns, including attacks/PASS_TURN) and XP earned during the map visit.
The report adds no asset read, heap allocation, framebuffer or production source.
Monotonic elapsed time needs no RTC; loading/offline time is excluded, in-level
menus/dialogs are included. Counters reset at new-map entry.

V10 writes 5460 bytes: the V9 world payload plus a CRC-covered 16-byte level
progress suffix. V1–V9 reads remain compatible. Earlier saves cannot reconstruct
historical counters and display `SINCE LOAD`; that partial flag persists across
V10 SAVE/LOAD. Old firmware cannot read V10.

Rebased normal CYD build PASS: static RAM 45208 B; linked flash 771897 B.
The host progress regression also passes on the rebased tree.
Host counter regressions, production report rendering and in-memory SD writer/
CRC/byte-verification checks PASS. The report is user-accepted on CYD; targeted
map-handoff/counter checks, V10 reboot resume and V9 partial-history migration
are not individually confirmed. Details and the checklist are
in [`DOCUMENTATION.md`](DOCUMENTATION.md#mission-report-and-v10-progress--development-candidate-2026-10-02).

## Previous main hardware/CI witness (before this branch)

```text
current main = afb7c7e8034ecb1084c0ed066bd1d5a03aa18ed1
branch = agent/esp32-native-menu-state-root
hardware-tested code boundary = ad8fe2f6e1ed8186a1cdd871d241b72623d905c6
CI = esp32-cyd #1509 SUCCESS
static RAM = 45176 B
linked Flash = 770193 B
artifact id = 11225823078
artifact digest = sha256:f37f9fc35cc06b9731ba50f4a41c6d27ccc20a00e61fb1c26743cd683c6cb6fd
hardware = targeted story teardown probe PASS + normal Start Game intro -> Entrance -> ENGINESESSION READY PASS
status = DoomCanvas_free now unconditionally releases native story hand; real CYD proves owner cleared and heap8 exact after teardown; MenuSystem_t remains 496 B
```

## Native divider formatter replaces Player MenuSystem buffer — REAL-CYD PASS (2026-10-02)

Hardware-tested code boundary:
`ad8fe2f6e1ed8186a1cdd871d241b72623d905c6`.

ESP32 `Player.c` no longer depends on `MenuSystem_buildDivider()` or the
legacy `MenuSystem_t::stringBuffer` scratch. The replacement is the
caller-owned, allocation-free `EspNativeText_buildDivider()`.

The first probe exposed a latent legacy overflow pattern: the original
`strncpy(..., 32)` zero-padding could write beyond the 32-byte destination
when reproduced with a local buffer. The native formatter now copies only the
visible legacy text bytes and writes the suffix/NUL explicitly.

A guarded hardware probe validates all four Player divider strings byte-for-byte
while checking heap stability and post-buffer guard bytes:

```text
[DIVIDERPROBE] PASS cases=4 heap8=84048 exact=yes allocation=no owner=caller
```

The diagnostic firmware then continued normally through prerender, mappings and
native MAIN with no stack-protector failure.

Normal `esp32-cyd` CI #1509 is SUCCESS at the same code boundary.

## Native menu runtime decoupled from MenuSystem.h — REAL-CYD PASS (2026-10-02)

Hardware-tested code boundary:
`35b6986686fa015a4c1ce554b45f1c6d1c865f7d`.

The permanent ESP32 menu runtime modules no longer include the legacy
`MenuSystem.h` header for state layout or capacity. They include
`esp_native_menu_state.h` directly and use
`ESP_NATIVE_MENU_MAX_ITEMS`.

The CI initially exposed two residual `MAX_MENUITEMS` uses in the native menu
model; these were replaced with the native capacity constant without restoring
the legacy include.

Real-CYD validation covered the full path:

```text
boot
[MENUSTORAGE] INIT bytes=496
OPTIONS -> Back
HELP page up/down -> Back
Start Game
INTRO1 FNV=ade0195d deltaHeap=0
intro disposal recovered=33944
Entrance load
ENGINESESSION READY
HUB open -> weapons -> status -> system -> close
shapeData=NULL
mediaTexels=NULL
```

Normal `esp32-cyd` CI #1489 is SUCCESS:

```text
static RAM   = 45176 B
linked Flash = 770193 B
artifact id  = 11225823078
digest       = sha256:f37f9fc35cc06b9731ba50f4a41c6d27ccc20a00e61fb1c26743cd683c6cb6fd
```

This establishes that the native menu runtime no longer depends on the legacy
menu header for its own state representation.

## Native menu state header extracted — REAL-CYD PASS (2026-10-02)

Hardware-tested code boundary:
`d67caf8fe889f39802ca8a4a8499ecac53fb9e3d`.

The permanent ESP32 menu state definition now lives in
`ESP32/src/esp_native_menu_state.h`.
`src/MenuSystem.h` no longer owns the native struct layout; on ESP32 it is only
a compatibility facade that imports the native header and aliases the legacy
name.

The struct remains layout-identical:

```text
[MENUSTORAGE] INIT bytes=496 items=8 owner=esp-native compatibilityLayout=EspNativeMenuState_t
[CORE] MenuSystem used=512
```

Real-CYD validation covered:

```text
boot
OPTIONS -> Back
HELP page up/down -> Back
MAIN FNV=522dc605
Start Game
INTRO1 FNV=ade0195d deltaHeap=0
intro disposal recovered=33944
Entrance load
ENGINESESSION READY
shapeData=NULL
mediaTexels=NULL
```

Normal `esp32-cyd` CI #1466 is SUCCESS:

```text
static RAM   = 45176 B
linked Flash = 770193 B
artifact id  = 11223917653
digest       = sha256:ecd9433708f4f6c3b7dbeb25770faa0f4b87cdd2b7c8733b84c21fe6ddb44021
```

This proves the native type can live independently of the legacy header before
native modules drop their own `MenuSystem.h` includes.

## Native menu runtime APIs typed directly — REAL-CYD PASS (2026-10-02)

Hardware-tested code boundary:
`7a14a0c8c092f7a24195251e669f020982206755`.

All permanent ESP32 native menu runtime modules now use
`EspNativeMenuState_t*` directly rather than the compatibility
`MenuSystem_t*` alias, including storage, MAIN model/touch/actions,
OPTIONS/Back, Start action and the core-size accounting probe.

Real-CYD proof:

```text
[MENUSTORAGE] INIT bytes=496 items=8 owner=esp-native compatibilityLayout=EspNativeMenuState_t
MAIN FNV=522dc605
HELP page-up 24->16->8->0 exact FNVs=9213df95/d0788359/5f22cf6b
HELP -> MAIN FNV=522dc605
heap8=62112
largest8=32756
p.bmp preflight=present
```

Normal `esp32-cyd` CI #1458 is SUCCESS:

```text
static RAM   = 45176 B
linked Flash = 770193 B
artifact id  = 11224131637
digest       = sha256:77ad414bf3e5f0487444260776c527cc239fbaaf1a99f4af4f583c2deb4466bb
```

This validates direct native typing before moving the native state definition
out of the legacy `MenuSystem.h` header.

## Shared p.bmp preflight restored — REAL-CYD PASS (2026-10-02)

Hardware-tested code boundary:
`862d2f7ca482d74c1fbbd2f7730d0e3479d3c22c`.

A review correctly identified that `p.bmp` remained a mandatory shared runtime
dependency after ownership moved out of the legacy menu shell: native story
presentation and the gameplay HUB both still require it. Removing it from the
startup preflight allowed a malformed PAK to pass startup and fail later during
Start.

The resource remains **not owned by MenuSystem**. Only its presence validation
is restored:

```text
[PRERENDER] Resource preflight (3 files)
[PRERENDER] j.bmp          bytes=4264 backing=pak
[PRERENDER] p.bmp          bytes=156 backing=pak
[PRERENDER] entities.db    bytes=2762 backing=pak
[PRERENDER] Resource preflight OK
```

The same real-CYD boot reaches the unchanged native MAIN:

```text
[MENUSTORAGE] INIT bytes=496
MAIN FNV=522dc605
heap8=62112
largest8=32756
```

Normal `esp32-cyd` CI #1448 is SUCCESS:

```text
static RAM   = 45176 B
linked Flash = 770193 B
artifact id  = 11223741289
digest       = sha256:09daf578d0c2f854ee27fdfefbf2a7d49be0a45527e976ba03293bb247b11971
```

## Native menu state root identity — REAL-CYD PASS (2026-10-02)

Hardware-tested code boundary:
`f76e3ed415c5d2012fe61cc38b7cca11631ee796`.

This first root-migration step introduces `EspNativeMenuState_t` as the
canonical ESP32 menu-state identity while preserving the existing 496-byte
layout exactly.

On ESP32:
- `DoomRPG_t::menuSystem` now points to `EspNativeMenuState_s*`;
- `DoomCanvas_t::menuSystem` now points to `EspNativeMenuState_s*`;
- `MenuSystem_t` remains only as a compatibility alias for surviving legacy
  signatures;
- desktop/J2ME layout remains unchanged.

Real-CYD regression:

```text
[MENUSTORAGE] INIT bytes=496
MAIN FNV=522dc605
HELP0 FNV=5f22cf6b
HELP8 FNV=d0788359
HELP16 FNV=9213df95
HELP24 FNV=b0191189
HELP32 FNV=4c944ee5
HELP40 FNV=943f0b77
OPTIONS FNV=162d3999
MAIN heap8=62112
largest8=32756
shapeData=NULL
mediaTexels=NULL
```

Normal `esp32-cyd` CI #1434 is SUCCESS:

```text
static RAM   = 45176 B
linked Flash = 770197 B
artifact id  = 11222798316
digest       = sha256:6b61f9188b2a079e11d527d0cfd357e9062e67927826b500e35e1d597bc4e503
```

No runtime behavior or layout change was observed. This validates the native
type identity before removing the compatibility alias from native APIs.

## Story-hand teardown safety — REAL-CYD PASS (2026-10-02)

Production fix boundary:
`f398807df63c88d3d453a71eaf675cd1be2c1dbd`.

A reviewer correctly identified that the native story-hand owner was only
released on normal intro disposal. If the engine tore down first, the
module-global `storyHandOwner` could outlive the `DoomRPG_t` it referenced.

The permanent fix adds an unconditional ESP32 release in
`DoomCanvas_free()`:

```text
DoomCanvas_free()
 -> Esp32StoryFit_release(doomCanvas)
 -> clear storyHand + storyHandOwner
 -> continue ordinary canvas teardown
```

The normal `esp32-cyd` build at this boundary is CI #1420 SUCCESS:

```text
static RAM   = 45176 B
linked Flash = 770197 B
artifact id  = 11222143129
digest       = sha256:2f99a975d67cf895c54a5d50fbc0714b6829fc91c7ee65215ddb5fe041683911
```

A dedicated diagnostic env,
`esp32-cyd-story-teardown-probe`, was then hardware-tested on the real CYD.
It creates a synthetic canvas, prepares the native story hand, invokes the real
`DoomCanvas_free(..., false)`, and requires both owner release and exact heap
restoration.

Real-CYD proof:

```text
[STORYTEARDOWN] BEGIN heap8=84048 owner=0
[INTROFIT] HAND-READY asset=p.bmp bytes=bounded owner=native-story
[INTROFIT] PREPARE hand=13x10 asset=p.bmp owner=native-story ...
[INTROFIT] HAND-RELEASE asset=p.bmp owner=native-story
[STORYTEARDOWN] PASS prepare->DoomCanvas_free->released heap8=84048 exact=yes
```

The same diagnostic firmware then continued through the normal Start Game intro
and reached native gameplay, confirming that the targeted teardown probe does not
damage the ordinary path.

## Final ESP32 MenuSystem compatibility shell — REAL-CYD PASS (2026-10-02)

Hardware-tested production boundary:
`d8d0622eb92d7f32b2e37033cb557b6ba35deb5e`.

The final simple trim for this milestone removes the ESP32-only
`MenuSystem_t::doomRpg` backpointer and the write-only `paintMenu` flag.
`EspNativeMenuStorage_startup/free` now receive the `DoomRPG_t*` owner
explicitly.

The real CYD reports the final compact shell as:

```text
[MENUSTORAGE] INIT bytes=496 items=8 owner=esp-native compatibilityLayout=MenuSystem_t desktopTU=no
[CORE] MenuSystem used=512
```

The measured struct size is therefore **496 B**, not the conservative 504 B
estimate. Relative to the original 604 B compact compatibility shell, this
milestone removes 108 B from the retained ESP32 layout.

Full real-hardware regression on the exact code boundary covered:

```text
cold boot
MAIN exact FNV=522dc605
OPTIONS -> Back
HELP page 0 -> 8 -> 16 -> 8 -> Back
Start Game
full semantic intro
bounded first ST_INTRO draw with deltaHeap=0
native story-hand release gain=176 B
bounded intro disposal heap8=69492 -> 103436 recovered=33944
/intro.bsp Entrance
ENGINESESSION READY
one committed FORWARD move
native HUB open
```

The menu presentation fingerprints remain exact:
`OPTIONS=162d3999`, `HELP0=5f22cf6b`,
`HELP8=d0788359`, `HELP16=9213df95`.

The story-owner contract remains symmetric and allocation-safe:

```text
[INTROFIT] PREPARE ... drawAllocation=no
[INTRO1] Drawn ... deltaHeap=0
[INTROFIT] HAND-RELEASE asset=p.bmp owner=native-story
[INTRODISP] FREE image=p.bmp/storyHand ... gain=176
```

The same run reaches native gameplay with:

```text
shapeData   = NULL
mediaTexels = NULL
```

Normal `esp32-cyd` CI #1413 is SUCCESS:

```text
static RAM   = 45176 B
linked Flash = 770189 B
artifact id  = 11221787233
digest       = sha256:aabcf3c22267d1b0866f4b642eee70fa2d81b14236cfed1a7b891bfed4dc0603
```

This closes the bounded field-pruning phase. The remaining ESP32
`MenuSystem_t` fields are live native-menu state; replacing the type/root is
a separate architectural milestone rather than another dead-field cleanup.

## 512-byte MenuSystem shell + native story hand owner — REAL-CYD PASS (2026-10-02)

Hardware-tested code boundary:
`0a2bcc39656d8b855d777edae4273a368f44307c`.

The ESP32-only compatibility shell now removes an additional 40 bytes of legacy
layout residue:

```text
memory
imgHand
imgArrowUpDown
field_0xc58
f749g
setBind
```

This brings `sizeof(MenuSystem_t)` from 552 B to 512 B on ESP32.

A real remaining consumer of the old `imgHand` was found in
`native_story_fit.c`. That dependency is now owned natively:
`p.bmp` is acquired before the bounded first ST_INTRO frame, the draw path is
allocation-free, and the same owner is released by the bounded native intro
disposer.

Real-CYD proof:

```text
[INTROFIT] HAND-READY asset=p.bmp owner=native-story
[INTROFIT] PREPARE hand=13x10 asset=p.bmp owner=native-story ... drawAllocation=no
[INTRO1] Drawn ... heap8=69484->69484 deltaHeap=0
[INTROFIT] HAND-RELEASE asset=p.bmp owner=native-story
[INTRODISP] FREE image=p.bmp/storyHand heap8=69484->69660 gain=176
[INTRODISP] READY ... heap8=69484->103428 recovered=33944
```

The final disposal heap exactly returns to the pre-intro allocation level,
proving no retained story-hand leak.

The same hardware run continues through:
- full semantic intro navigation;
- bounded intro disposal;
- native transition loading;
- `/intro.bsp` Entrance runtime creation;
- fresh generic gameplay session;
- `ENGINESESSION READY`.

Critical invariants remain:

```text
shapeData   = NULL
mediaTexels = NULL
```

Normal `esp32-cyd` CI #1389 is SUCCESS:

```text
static RAM   = 45176 B
linked Flash = 770205 B
artifact id  = 11221054541
digest       = sha256:1a8e220c45c25194375ad2d8a9bfbb8d02c3e5dc8adc6097c9cb08c4b8e025be
```

## MenuSystem compatibility-shell trim — REAL-CYD PASS (2026-10-02)

Hardware-tested code boundary:
`9d42d669d274c14bf39fde5a4d8af75df3cbe04b`.

The ESP32-only `MenuSystem_t` layout no longer carries six fields that had no
surviving reader in either compiled desktop translation units or native ESP32
code:

```text
stringBuffer[32]
bindIndx
nextMsgTime
nextMsg
cheatCombo
digitCount
```

The two native writes to `cheatCombo` / `digitCount` were dead resets and
were removed with the fields. Desktop/J2ME layout remains unchanged outside the
ESP32 build.

Real-CYD structural witness:

```text
[MENUSTORAGE] INIT bytes=552 items=8 ...
[CORE] MenuSystem used=568
```

Previous hardware-proven values were 604 B / 620 B, so the shell shrank by
exactly 52 bytes.

Runtime memory on the same hardware:

```text
MAIN heap8:     62016 -> 62060  (+44 B)
gameplay heap8: 55416 -> 55456  (+40 B)
largest8:       38900 -> 38900  (unchanged)
```

Regression coverage:
- cold boot and native MAIN exact;
- OPTIONS -> Back;
- HELP page down/up -> Back;
- V9 LOAD -> Sector 1 -> ENGINESESSION READY;
- native gameplay HUB opened and all four pages rendered.

All relevant menu fingerprints remain exact and
`shapeData == NULL` / `mediaTexels == NULL` remain true.

Important ownership clarification discovered during the hardware run:
the native gameplay HUB still intentionally uses `p.bmp` through its own
`HUB_FACE_NAME` path in `esp_native_gameplay_hub.c`. The earlier
`p.bmp/q.bmp` retirement applies only to `EspNativeMenuStorage_startup()`
and the old `MenuSystem_t::imgHand/imgArrowUpDown` ownership; it does not mean
that `p.bmp` is globally unused.

Normal `esp32-cyd` CI #1360 is SUCCESS:

```text
static RAM   = 45160 B
linked Flash = 769573 B
artifact id  = 11219735467
digest       = sha256:ec07fc36ff3aff92080e34444a2066fbb5a221924df0536c5a3eb78c8314f407
```

## Legacy menu accessory assets retirement — REAL-CYD PASS (2026-10-02)

Hardware-tested code boundary:
`31caa46af67a88550adcf2cd01ccbf4124bcf40b`.

The ESP32 runtime no longer preflights or loads the legacy menu accessory BMPs
`p.bmp` / `imgHand` and `q.bmp` / `imgArrowUpDown`. Direct final-ELF
inspection of the previous hardware-proven image showed that their remaining
source references lived only in DoomCanvas Story/Epilogue/scrollbar functions
that did not survive the link. The native menu keeps only `j.bmp`, which is
actually presented as the MAIN/OPTIONS logo.

Real-CYD cold boot proves the reduced startup contract:

```text
[PRERENDER] Resource preflight (2 files)
[PRERENDER] j.bmp          bytes=4264 backing=pak
[PRERENDER] entities.db    bytes=2762 backing=pak
[MENUSTORAGE] STARTUP READY assets=j owner=esp-native
              legacyAccessoryAssets=p/q-retired
```

Measured runtime improvement versus the previous hardware-proven boundary:

```text
EspNativeMenuStorage_startup: 4500 -> 4156 B  (-344 B)
MAIN heap8:                   61672 -> 62016   (+344 B)
gameplay heap8:               55024 -> 55416   (+392 B)
gameplay largest8:            38900 -> 38900   (unchanged)
```

MENU regression coverage on the real CYD:

```text
MAIN -> OPTIONS -> Back
MAIN -> HELP
HELP page 0 -> 8 -> 16 -> 24 -> 32 -> 24 -> 16 -> 8 -> 0
HELP -> Back -> MAIN
MAIN -> LOAD V9 -> Sector 1 -> ENGINESESSION READY
```

All relevant presentation fingerprints remain exact:

```text
MAIN        = 522dc605
OPTIONS     = 162d3999
HELP page0  = 5f22cf6b
HELP page8  = d0788359
HELP page16 = 9213df95
HELP page24 = b0191189
HELP page32 = 4c944ee5
```

Critical invariants remain `shapeData == NULL` and `mediaTexels == NULL`.

Normal `esp32-cyd` CI #1354 is SUCCESS:

```text
static RAM   = 45160 B
linked Flash = 769585 B
artifact id  = 11218479535
digest       = sha256:8997a4b99c89bc57e149d7dcf8d51e56c88ef9eb6d0f6765829d6659a13bee0b
```

The retained `imgHand` / `imgArrowUpDown` struct fields are now layout-only
compatibility residue; no runtime asset is owned behind them.

## Desktop MenuItem helper translation unit retirement — REAL-CYD PASS (2026-10-02)

Hardware-tested code boundary:
`4c4a48230303cef7eeedc9722198fbe2c7506517`.

The remaining ESP32 callers of desktop `MenuItem_Set()` / `MenuItem_Set2()`
were only the already-native fixed main-menu builders. Those trivial writes are
now local native bounded copies, and `src/MenuItem.c` is excluded from the
ESP32 compile graph.

Real-CYD validation covers the exact affected surface:

```text
MAIN stable for >95 s
 -> OPTIONS -> Back
 -> HELP page up/down through multiple ranges -> Back
 -> OPTIONS -> Back again
```

All previously validated framebuffer/model fingerprints remain exact:

```text
MAIN        = 522dc605
OPTIONS     = 162d3999
HELP page0  = 5f22cf6b
HELP page8  = d0788359
HELP page16 = 9213df95
HELP page24 = b0191189
```

The menu remains allocation-stable outside the intentionally temporary HELP
buffer:

```text
MAIN/OPTIONS heap8=61672 largest8=32756
HELP active  heap8=60248 largest8=32756
HELP Back    heap8=61672 largest8=32756
```

Normal `esp32-cyd` CI #1341 is SUCCESS:

```text
static RAM   = 45160 B
linked Flash = 769613 B
artifact id  = 11218132618
digest       = sha256:ecf7fb1385a00d7577c85624805d658dadede308802ae0ce7894c9e8fb269448
```

Direct final-ELF inspection confirms:

```text
MenuItem_*   = 0
MenuSystem_* = 0
```

The ESP32 compile graph now excludes `Menu.c`, `MenuItem.c`,
`MenuSystem.c`, and `ParticleSystem.c`.

Remaining menu-related cleanup is no longer equivalent dead-helper retirement:
`imgHand` and `imgArrowUpDown` are still referenced by linked DoomCanvas
Story/Epilogue/scrollbar paths, while full `MenuSystem_t` root replacement
crosses multiple compatibility contracts. Those belong to later bounded
milestones rather than this retirement step.

## Compact ESP32 menu storage — REAL-CYD PASS (2026-10-02)

Hardware-tested code boundary:
`bb04faa839169c559e161ff0dea59b5e8f1e6dbc`.

The retained compatibility `MenuSystem_t` no longer reserves the desktop
`MenuItem_t items[96]` array on ESP32. Its ESP32 item capacity is bounded to 8,
which is sufficient for the fixed native MAIN/OPTIONS/CONTINUE models (4/4/3).
HELP keeps the original `help.txt` payload compact and uses a bounded native
line-offset table instead of inflating 83 lines into desktop `MenuItem_t`
records.

Real-CYD HELP witness:

```text
[MAINMODEL] HELP-PARSE bytes=1405 ... declared=83 parsed=83
            compactBytes=1116 menuItemSlots=8 lineChars<=31 result=valid
[MAINHELP] PAGE-DOWN 0->8 ->16 ->24
[MAINHELP] PAGE-UP   24->16 ->8 ->0
[MAINBACK] READY source=help ... frame=522dc605
```

The fixed OPTIONS path also remains exact and allocation-free:

```text
MAIN heap8=61624 largest8=32756
OPTIONS heap8=61624 largest8=32756
Back -> MAIN frame=522dc605
```

The V9 LOAD path then restores Sector 1, reaches resident gameplay, executes
player movement and ordered four-monster movement/three-goal behavior, including
publication of a three-loop monster attack probe.

Hardware memory improvement versus the previous MenuSystem-retirement boundary:

```text
MAIN menu: 56908 -> 61624 heap8  (+4716 B)
gameplay:  50300 -> 55024 heap8  (+4724 B)
largest8:  38900 -> 38900        (unchanged in gameplay)
```

HELP cleanup returns exactly to the pre-HELP MAIN value `heap8=61624`, proving
the compact HELP allocation is not leaked.

Critical invariants remain:

```text
shapeData == NULL
mediaTexels == NULL
```

Normal `esp32-cyd` CI #1333 is SUCCESS:

```text
static RAM   = 45208 B
linked Flash = 769665 B
artifact id  = 11202101528
digest       = sha256:308632475a69248c35d2bd57ceac73e12c7cd432ebbf77884cf83117e9b9cf87
```

The remaining `MenuSystem_t` is now a small compatibility shell rather than a
5.5 KiB desktop menu container. Further retirement must audit each remaining
field and linked consumer before replacing the root type/pointer entirely.

## Desktop MenuSystem translation unit retirement — REAL-CYD PASS (2026-10-02)

Hardware-tested code boundary:
`2e0193b4e4f82d78f0b361c84e0d2bcd4f8f1cba`.

The ESP32 build no longer compiles `src/MenuSystem.c`. The final ELF contains zero
`MenuSystem_*` symbols. The still-transitional `MenuSystem_t` allocation is now
owned by `EspNativeMenuStorage`, which performs only the bounded storage/image
responsibilities still consumed by the native MAIN/OPTIONS/HELP models.

The former menu audio calls are preserved as semantic native intents rather than
desktop Sound/MenuSystem behavior:

```text
5046 = select/enter
5042 = back
5067 = in-game menu entry companion cue
```

The backend remains deliberately silent. The real CYD proves call-order publication
through:

```text
[AUDIOINTENT] seq=1 resource=5046
[AUDIOINTENT] seq=2 resource=5042
[AUDIOINTENT] seq=3 resource=5046
[AUDIOINTENT] seq=4 resource=5042
```

Hardware path:

```text
cold boot
 -> EspNativeMenuStorage INIT 5532 B / 96 items
 -> p/q/j asset startup
 -> MAIN
 -> OPTIONS -> Back
 -> HELP -> page down/up -> Back
 -> V9 LOAD Sector 1
 -> resident gameplay
 -> player MOVE
 -> ordered four-monster movement
```

CI #1324 succeeds in normal `esp32-cyd`:

```text
static RAM   = 45000 B
linked Flash = 769861 B
artifact id  = 11200848779
digest       = sha256:82fdd69dd60c3a8fa22fe15ce82c296bc291a854495807d7ec7d76c8682961dd
```

The +8 B static RAM is the native audio-intent state. Hardware memory moves by the
same exact amount relative to the previous milestone and fragmentation is unchanged:

```text
MAIN:     heap=122832 heap8=56908 largest8=32756
gameplay: heap=116224 heap8=50300 largest8=38900
```

Critical invariants remain `shapeData == NULL` and `mediaTexels == NULL`.

The next structural question is not `MenuSystem.c` anymore; it is the 5532-byte
compatibility layout itself. Any compaction must first prove that no linked
DoomCanvas/DoomRPG path still dereferences fields outside the native model contract.

## Dead desktop Menu / ParticleSystem translation units — REAL-CYD PASS (2026-10-02)

Hardware-tested code boundary:
`bc65cc337000d8c7ef54b5f0451e51d958cf7cef`.

The ESP32 engine build previously used `+<*.c>` for the desktop source directory and
therefore still compiled `src/Menu.c` and `src/ParticleSystem.c` even though earlier
hardware milestones had already retired both runtime owners and direct final-ELF
inspection showed zero `Menu_*` and zero `ParticleSystem_*` symbols.

This milestone changes only `ESP32/scripts/build_engine.py` so those two translation
units are excluded before compilation. It does not change inherited headers, type
layouts, `DoomRPG_t` fields, runtime ownership, gameplay, renderer, RNG, input,
save/load, audio, or death-menu behavior.

Normal `esp32-cyd` CI #1298 succeeds. The compile log no longer contains
`Menu.c.o` or `ParticleSystem.c.o`. Final metrics remain exactly:

```text
static RAM   = 44992 B
linked Flash = 769357 B
```

The unchanged linked image size is expected: both translation units were already
fully garbage-collected at link time. This milestone removes build-graph debt rather
than firmware bytes.

The real classic CYD validates a meaningful runtime path after the compile-graph
change:

```text
MAIN -> Load Game
 -> readable V9 checkpoint
 -> Sector 1 native session restore
 -> shapeData=0x0 mediaTexels=0x0
 -> resident gameplay READY
 -> MOVE
 -> ordered four-monster sequence
 -> subtype-4 three-goal continuation
 -> three-loop attack animation
 -> native retaliation commit
```

Stable hardware witness:

```text
heap=116232
heap8=50308
largest8=38900
```

A separate presentation gap remains intentionally outside this milestone:
player MOVE and TURN currently commit directly between settled camera poses rather
than showing legacy-style interpolation. That is a native presentation milestone,
not a reason to retain or restore desktop gameplay ownership.

Detailed milestone:
[MILESTONE_ESP32_RETIRE_DEAD_MENU_PARTICLE_TUS.md](MILESTONE_ESP32_RETIRE_DEAD_MENU_PARTICLE_TUS.md)

## Ordered monster turn + lethal monster death — REAL-CYD PASS (final review closure 2026-10-02)

Hardware-tested code boundary:
`717e7bd980ff7110c227055d940d01c980a5683d`.

This closes two intentionally deferred boundaries from the earlier active-sequence
and player-death milestones.

The monster turn is now serialized in first-activation order instead of choosing
one global immediate attacker. Each active member executes its own legacy-style
`Entity_aiThink` slice. If that member publishes an attack probe, the sequence
pauses, lets AttackVisual + Retaliation resolve it, then resumes at the next
ordinal in the same semantic monster turn.

Real-CYD witness:

```text
[MONSTERACTIVESEQ] BEGIN turn=3 ... activeCount=4 ...
[MONSTERACTIVESEQ] MEMBER ... ordinal=2/4 ... attackProbe=1->2 ...
[MONSTERACTIVESEQ] PAUSE ... probe=2 ... nextOrdinal=3 ...
[MONSTERRETAL] MISS-COMMIT probe=2 ... gameplayRngCommitted=yes ...
[MONSTERACTIVESEQ] RESUME ... resolvedProbe=2 ...
[MONSTERACTIVESEQ] MEMBER ... ordinal=3/4 ...
[MONSTERACTIVESEQ] MEMBER ... ordinal=4/4 ... attackProbe=2->3 ...
[MONSTERACTIVESEQ] COMPLETE ... ordered=yes publication=serialized-per-member multiAttack=one-probe-at-a-time
```

The same session proves repeated pause/resume across several attackers and keeps
three-goal subtype-4 movement/shortcut behavior live. A dead active monster is
skipped on later turns without disturbing active-list order.

Monster retaliation now owns lethal player damage. On probe 10, the real CYD
commits the attack RNG and authoritative HP=0, arms the already-native
PlayerDeath owner, consumes exactly one additional legacy death RNG byte, and
terminates the remaining active-list suffix:

```text
[MONSTERTURN] MEMBER-ATTACK-PROBE ... probe=10 ... playerHP=4->0 ... lethal=deferred-player-death ...
[PLAYERDEATH] ARM seq=10 tile=506 ... hp=0 ... rngByte=87 deathSound=5058-deferred ...
[MONSTERRETAL] LETHAL-COMMIT probe=10 ... playerHP=4->0 ... rng=41a9a848->f4415a47 attackRngCommitted=yes deathRngCommitted=yes ... deathOwner=armed ... turn=terminal
[MONSTERACTIVESEQ] TERMINAL turn=7 ordinal=2/4 probe=10 cause=player-death remaining=discarded ...
[PLAYERDEATH] PHASE ... fall=complete ...
[PLAYERDEATH] READY ... elapsedMs=3012 ... input=death-menu load=available ...
```

Death-menu touch routing remains bounded and explicit. LOAD is the only live
session-replacement route. JUNCTION, RETRY and MAIN classify correctly and
remain fail-closed with no session mutation. The active LOAD row is visually
distinguished from the deliberately disabled routes. Earlier hardware on the
same branch already proved LOAD replaces the dead session with the V9
checkpoint; this final hardware run revalidates the retained menu routing and
presentation.

The final code-review P1 closes a one-session-tick input race after a
post-move attack publication. Session composition services
`MonsterActivation_serviceTurn()` before `MovementProbe/ActiveSequence`, so a
post-move member can increment the raw `MonsterTurn.attackProbes` after the
filtered activation view for that tick has already been copied. Previously,
`AttackVisual_isBusy()` observed only the filtered view; a very fast world tap
could therefore enter before the next activation delivery and a new
`runProbe()` could clear `lastAttackerSpriteIndex`.

Commit `717e7bd980ff7110c227055d940d01c980a5683d` makes the input gate also
observe the raw MonsterTurn producer. Any producer
`attackProbes > AttackVisual.observedAttackProbes` is combat-owned
immediately, while the activation-filtered view remains the normal delivery
contract on the following service tick. This is intentionally fail-closed for
an unexpected probe gap and changes no movement, RNG, retaliation or animation
transaction.

The real CYD regression run kept the four-active-monster ordered movement path
stable and the user explicitly confirmed rapid taps no longer stack through the
combat boundary. Representative stable sample remains
`heap=116232 heap8=50308 largest8=38900`.

CI #1290 succeeds in normal `esp32-cyd` at 44992 B static RAM and 769357 B
linked Flash. Artifact 11196908002 has digest
`sha256:c250af278aa1d18add1bbd87071e9d7f96d0b575199fb644d269ccc1cc3af5f5`.

Repeated live samples remain stable:

```text
heap=116232
heap8=50308
largest8=38900
```

CI #1290 succeeds in the normal `esp32-cyd` environment at 44992 B static RAM
and 769357 B linked Flash. Artifact 11196908002 has digest
`sha256:c250af278aa1d18add1bbd87071e9d7f96d0b575199fb644d269ccc1cc3af5f5`.

The old "simultaneous attack-ready = fail-closed" and "monster lethal =
fail-closed" statements remain historically true for their earlier milestones,
but are superseded by this boundary.

Detailed closure:
[MILESTONE_NATIVE_MONSTER_ORDER_AND_PLAYER_DEATH.md](MILESTONE_NATIVE_MONSTER_ORDER_AND_PLAYER_DEATH.md)


### Native player death core — REAL-CYD PASS (2026-10-01)

Hardware-tested head `d6811e23db4580795887c97c3bdf5e6493224268`
owns the first bounded native `ST_DYING` path. The producer is deliberately
limited to a lethal type-10/11 hazard reached through `PASS_TURN`; MOVE
hazards, monster retaliation and barrel/crate radius damage keep their existing
fail-closed boundaries until dedicated producer milestones connect them.

The lethal transaction now commits the authoritative PlayerState to health zero,
keeps exact hazard armor semantics, then arms one 24-byte PlayerDeath owner.
Arming clears the legacy death-state weapon visibility fields
(`weapons=0`, `weapon=0`), consumes the single legacy
`Player_died()` random byte used for death-sound selection, blocks resident
gameplay input, and explicitly suppresses MonsterTurn after death.

The native presentation reproduces the recovered legacy timing without touching
desktop render storage: the existing `EspPlayerView` camera falls from
`viewZ=36` during the first 750 ms, then a bounded RGB565 clamp fades only the
160x80 world viewport until the 3000 ms death-menu boundary. No new framebuffer,
`shapeData`, or `mediaTexels` owner is introduced.

Real-CYD witness:

```text
[HAZARDPASS] LETHAL-COMMIT ... hp=1->0 ... deathOwner=pending rollback=armed monsterTurn=no
[PLAYERDEATH] ARM ... hp=0 weapons=000f->0000 ... rngByte=65 ... viewZ=36 fallMs=750 fadeMs=750..3000 ... input=blocked ownerBytes=24
[PASSTURN] DEATH ... deathOwner=armed monsterTurn=no input=blocked ...
[PLAYERDEATH] PHASE ... elapsedMs=845 fall=complete viewZ=6 fade=begin input=blocked
[PLAYERDEATH] READY ... elapsedMs=3004 phase=death-menu-ready viewZ=6 fade=0 frames=49 input=blocked menuOwner=deferred
```

The user confirmed the camera physically falls to the floor and the world then
fades fully black. The intentionally unowned death menu leaves the post-fade HUD
non-interactive; that is the next presentation boundary, not a fallback to
legacy input. Repeated live samples remain stable at
`heap=116264 heap8=50340 largest8=38900`.

CI #1274 succeeds at 44960 B static RAM / 765497 B linked Flash. Artifact
11193296410 has digest
`sha256:5770b336e4c4686fb6ec189a864f234d0017347a87adac83ab5eb0f67d4a24b5`.

Detailed contract:
[MILESTONE_NATIVE_PLAYER_DEATH_CORE.md](MILESTONE_NATIVE_PLAYER_DEATH_CORE.md)







### Hazard PASS_TURN HUD refresh — REAL-CYD PASS (2026-10-01)

Commit `13bb05ed09aa217a2263a4f3fb8a521348c96258` closes the presentation gap
where repeated PASS_TURN damage on a current-tile type-10/11 hazard mutated the
authoritative PlayerState but left the retained bottom HUD digits stale until a
later world redraw.

The fix adds no new HUD/gameplay owner. It reuses the existing wrapped
`EspNativeGameplayHud_view()` overlay, whose health/armor/ammo/weapon values are
derived from the current native PlayerState, and repaints only the two retained
HUD bands before the already-existing immediate feedback present.

Real-CYD witness:

```text
[HAZARDPASS] COMMIT ... hp=17->16 armor=6->4 ...
[GAMEPLAYHUD] REPAINT health=16/38 armor=4/28 ...
[PASSTURN] HUD-REPAINT ... phase=hazard-commit health=16/38 armor=4/28 ... source=current-player-overlay ...
[PASSTURN] REQUEST ... monsterTurn=requested ... feedbackPresent=immediate

[HAZARDPASS] COMMIT ... hp=16->15 armor=4->2 ...
[GAMEPLAYHUD] REPAINT health=15/38 armor=2/28 ...
[PASSTURN] HUD-REPAINT ... phase=hazard-commit health=15/38 armor=2/28 ...
```

The user confirmed the HUD changes physically on every PASS_TURN. Repeated
`ALIVE` samples remain stable at
`heap=116288 heap8=50364 largest8=38900`.

CI #1257 succeeds at 44936 B static RAM / 762861 B linked Flash. Artifact
11191801698 has digest
`sha256:22e4479376aac2daed8ef3e6300a633b3f13ef9fa133369586619374f58e2c25`.

The same hardware session reaches the next explicit gameplay boundary through a
nearby barrel explosion:
`[BARRELRADIUS] PLAYER-DEFER ... mutation=no/lethal-deferred`, followed by exact
monster-turn/RNG/player/world rollback. Native player death remains intentionally
unowned.

Detailed historical hazard contract:
[MILESTONE_NATIVE_PASS_TURN_HAZARD_TOUCH.md](MILESTONE_NATIVE_PASS_TURN_HAZARD_TOUCH.md)

### Three-goal subtype 4/13 multi-loop attack — REAL-CYD PASS (2026-10-01)

Commits `f550580369bfdca07631ad33e6b31f2c63dc1f14` and
`5943974dcf1b5bd1c142e4665340f51fb6fbb19b` connect the already-native
three-goal movement owner to the existing MonsterTurn -> Activation ->
AttackVisual -> Retaliation pipeline.

The permanent bounded contract is:

- subtype 4/13 keeps the exact legacy goal count of three;
- reaching cardinal distance 64 before goal 3 applies the legacy shortcut and
  skips the remaining movement goals;
- one rollback-exact three-loop attack probe is published to MonsterTurn;
- the existing visual owner renders three attack/idle phases before resolution;
- the retaliation owner replays and commits the exact roll/RNG once;
- a second attack-ready monster before delivery remains deliberately fail-closed
  until ordered multi-attacker publication has a dedicated milestone.

CI #1253 succeeds at 44936 B static RAM / 761949 B linked Flash /
762320 B firmware.bin. Artifact 11191212285 has digest
`sha256:42b26ff060341d02dbc37954006df9825dc7a12463e7168ed9f09cc68de02966`.

The real classic CYD exercises the shortcut branch directly:

```text
[MONSTER3GOAL] COMMIT sprite=1 goal=2/3 tile=538->506 ...
[MONSTER3ATTACK] ATTACK-PROBE reason=MOVE sprite=1 subtype=4 ... weapon=13 ... loops=3 goalStep=2/3 ... hitLoops=2 ... totalDamage=2 armorDamage=2 ... rngCalls=6 ... rngRollback=yes playerExact=yes ...
[MONSTER3GOAL] ATTACK-PROBE ... frameTime=2->3 goal=2/3 ... shortcut=yes remainingGoals=skipped loops=3 ...
```

The same probe is then delivered and animated as three real phases:

```text
[MONSTERACT] DELIVER actualProbe=1 deliveredProbe=1 sprite=1 reason=1 activated=yes
[MONSTERATKVIS] ARM ... loops=3 shot=1/3 ...
[MONSTERATKVIS] STEP ... shot=2/3 phase=attack ...
[MONSTERATKVIS] STEP ... shot=3/3 phase=attack ...
[MONSTERATKVIS] COMPLETE ... loops=3 ...
```

Resolution commits the exact prospective result:

```text
[MONSTERRETAL] COMMIT ... subtype=4 ... weapon=13 ... loops=3 hitLoops=2 ... totalDamage=2 armorDamage=2 ... playerHP=33->31 armor=23->21 ... rngCalls=6 ... rng=39420bce->1a4b8634 ... rollback=closed
```

Repeated ALIVE samples remain stable at
`heap=116288 heap8=50364 largest8=38900`. Session readiness also keeps
`shapeData=0x0 mediaTexels=0x0`.

The same hardware session exposes a separate presentation bug: repeated
PASS_TURN while standing on a type-10/11 hazard mutates PlayerState and repeats
damage feedback correctly, but the bottom HUD health/armor digits remain stale
until a later world movement redraw. That presentation-only bug is the next
candidate fix and does not invalidate this multi-loop gameplay PASS.

Detailed record:
[MILESTONE_ESP32_NATIVE_MONSTER_THREE_GOAL_MULTI_LOOP_ATTACK.md](MILESTONE_ESP32_NATIVE_MONSTER_THREE_GOAL_MULTI_LOOP_ATTACK.md)

### Fire Ext monster combat semantics — REAL-CYD PASS (2026-10-01)

Commit `ef8dc9b5f06dd93c34c5179f6b95935dd0af13d2` fixes the native
player-combat gate for weapon 1. Legacy Doom RPG treats Fire Ext as a normal
direct weapon when the target is an enemy (`eType == 1`); its special entity
rule applies only outside that monster path.

The permanent native distinction is presentation-only after a successful hit:

- Phantom subtype 4: gray `RGB565 ce79`, exactly 15 local-visual particles;
- other monster subtypes: normal combat/damage, but no blood/HITFX spray;
- a true Fire Ext miss uses legacy text `No effect!`;
- visual FX do not consume gameplay RNG.

CI #1247 succeeds at 44936 B static RAM / 757869 B linked Flash /
758240 B firmware.bin. The artifact is 11189341147 with digest
`sha256:42e47397c8792f82f0129c8c85340bd1535374ad42787ec38c0c0a5757febdc0`.

The real classic CYD validates both branches in one loaded Sector 1 session.
Two subtype-4 Phantoms are hit and killed with weapon 1. Both arm the exact
gray impact owner:

```text
[MONSTERCOMBAT] ARM ... subtype=4 ... weapon=1 ...
[HITFX] ARM ... subtype=4 weapon=1 mode=extinguisher-gray ... color565=ce79 particles=15 ...
[MONSTERHITFEEDBACK] ... impact=extinguisher-gray-armed ...
[MONSTERCOMBAT] COMMIT ... alive=1->0 ... ammo=14->13
```

A following subtype-5 monster is also attacked successfully with Fire Ext:

```text
[MONSTERCOMBAT] ARM ... subtype=5 ... weapon=1 ...
[MONSTERCOMBAT] ROLL ... totalDamage=0 armorDamage=1 ...
[MONSTERHITFEEDBACK] ... impact=none-extinguisher ...
[MONSTERCOMBAT] COMMIT ... hp=14->14 armor=6->5 ... ammo=12->11
```

No `reason=weapon-entity-rule-family` appears. Native GIBFX also remains live
after the Phantom deaths with `legacyParticleSystem=no`. Repeated ALIVE
samples after movement, two Fire Ext kills, a subtype-5 hit and retaliation
remain stable at `heap=116288 heap8=50364 largest8=38900`.

The same hardware log exposes the next gameplay boundary directly:
subtype-4 three-goal movement reaches adjacent-cardinal attack gates but still
reports `multi-loop-attack-family-deferred` for its three-shot attack family.

Detailed record:
[MILESTONE_ESP32_NATIVE_FIRE_EXT_MONSTER_COMBAT.md](MILESTONE_ESP32_NATIVE_FIRE_EXT_MONSTER_COMBAT.md)

### Legacy Menu root retirement V24 — REAL-CYD PASS (2026-10-01)

V24 removes the dead desktop `Menu_t` root from the ESP32 core graph. The
first candidate, `675b4a554498014c666af55205d3355fcbb39ad1`, correctly
removed the object but the real CYD stopped at:

```text
[MAINOPAQUE] FAILED dashboard presentation contract menu=1 selected=0
[MAINBOOT] FAILED native MENU_MAIN model/presentation
```

The failure was not a hidden consumer of `Menu_t`. The native graphics safety
gate still contained one historical precondition,
`doomRpg->menu != NULL`, while using only Render, DoomCanvas, MenuSystem,
framebuffer and native-cache state. Commit
`21ee2c95afd351af5c20ba38d6ef897bd81d1d05` removes only that obsolete
guard.

CI #1239 succeeds at 44936 B static RAM / 757585 B linked Flash /
757952 B firmware.bin with 49 active linker wraps. Direct final-ELF inspection
shows zero `Menu_*` symbols. The four intentionally retained legacy
`MenuSystem_*` symbols are `init/startup/playSound/free`.

The real CYD now boots with:

```text
[CORE] ParticleSystem retired object=NULL owner=native-gibfx
[CORE] Menu root retired object=NULL owner=native-menu-models
[CORE] READY objects=10 heap used=53804 ...
```

Compared with V23, core usage drops exactly 76 B
(`53880 -> 53804`), matching the retired `Menu_t` allocation. Native MAIN
is stable at `heap=122896 heap8=56972 largest8=32756`, +120 B free heap8
versus the V23 menu boundary.

The hardware run validates OPTIONS entry, all disabled OPTIONS cards, native
Back, HELP parsing/paging in both directions, and native HELP Back. Both child
routes repaint the exact main framebuffer FNV `522dc605`, re-arm touch, keep
`shapeData == NULL` / `mediaTexels == NULL`, and leave heap8/largest8
unchanged throughout menu interaction.

The corrected code boundary is merge-ready after documentation-only tail.

Detailed milestone:
[MILESTONE_ESP32_RETIRE_LEGACY_MENU_ROOT_V24.md](MILESTONE_ESP32_RETIRE_LEGACY_MENU_ROOT_V24.md)

### Legacy ParticleSystem core-object retirement V23 — REAL-CYD PASS (2026-10-01)

Commits `15efaaeef2bfb39964e5724dc7dfdbd1f6484c32` and
`d7eed080766016fdb0870bade94e2b03a98c6990` complete the ESP32
ParticleSystem retirement started in V22. The core graph no longer allocates
`ParticleSystem_t`; the retired field is required to remain NULL; and the
generated ESP32 DoomRPG cleanup no longer retains `ParticleSystem_free()`.

CI #1236 succeeds at 44944 B static RAM / 757525 B linked Flash /
757888 B firmware.bin. Direct ELF inspection finds **zero**
`ParticleSystem_*` symbols. Active linker wraps remain 49.

The real classic CYD proves the exact core-object reduction:

```text
V22 core used = 56160 B
V23 core used = 53880 B
delta          = -2280 B
```

The boot witness is now:

```text
[CORE] ParticleSystem retired object=NULL owner=native-gibfx
[CORE] READY objects=11 ...
```

Native MAIN is stable at
`heap=122776 heap8=56852 largest8=32756`, +2304 B heap8 versus the V22
hardware boundary. After full intro disposal and Entrance bootstrap, resident
gameplay is stable at `heap=114600 heap8=48676 largest8=36852`, +2316 B
heap8 versus V22 before lazy dialog owners.

Most importantly, a real combat kill exercises the replacement owner:

```text
[MONSTERCOMBAT] COMMIT ... alive=1->0 ... gibFX=deferred ...
[GIBFX] PAINT ... legacyParticleSystem=no
[GIBFX] REPAINT ...
[GIBFX] EXPIRE ... gameplayRng=untouched
```

The same session also validates a real monster attack/retaliation, movement,
door close/open, resource/weapon pickup, renderer compact-guard recovery and
post-kill monster movement. The live heap remains stable at
`heap=111140 heap8=45216 largest8=36852` across the kill/overlay expiry.

This is the hardware proof that the desktop ParticleSystem is no longer merely
unused: its live gib presentation responsibility is owned by the bounded native
`EspNativeGameplayGibFx` path.

Detailed milestone:
[MILESTONE_ESP32_RETIRE_LEGACY_PARTICLE_STARTUP_V22.md](MILESTONE_ESP32_RETIRE_LEGACY_PARTICLE_STARTUP_V22.md)

### Legacy ParticleSystem startup retirement V22 — REAL-CYD PASS (2026-10-01)

Commit `28cc43cff7d0bee49731ff2c3382939914c75e41` removes the unused
desktop-derived `ParticleSystem_startup()` from normal ESP32 prerender startup.
The production PAK preflight drops `gibs_24.bmp`, and the runtime no longer
loads or initializes the 64-node legacy particle pool.

CI #1233 succeeds at 44952 B static RAM / 757505 B linked Flash /
757872 B firmware.bin. Relative to the preceding review-fix image this is
-112 B static RAM and -884 B linked Flash. The final ELF contains only
`ParticleSystem_init` and `ParticleSystem_free`; startup, unlink, render,
spawn and particle-calculation symbols are absent. Active linker wraps remain 49.

The real classic CYD proves the new four-file prerender set
(`p.bmp/q.bmp/j.bmp/entities.db`), with no gibs resource and no
`ParticleSystem_startup` stage. Native MAIN remains stable for 100 seconds at
`heap=120472 heap8=54548 largest8=32756`. Compared with the prior V21 main
menu witness (`101644/35720/23540`), the retired startup returns 18828 B of
free heap8 and raises the largest 8-bit block by 9216 B.

START then completes the entire intro and Entrance bootstrap with exact first
frame FNV `71ca7465`, `shapeData=0x0`, `mediaTexels=0x0`. Resident
gameplay is stable at `heap=112284 heap8=46360 largest8=36852` before the
first lazy dialog owner and `111248/45324/36852` after it. MOVE/TURN,
resource pickups, regular-door open/close, opcode-26 dialog/resume and genuine
`LEGACY_GUARD -> RETRY -> RECOVERED` renderer recovery all remain live.

The legacy `ParticleSystem_t` object itself is intentionally still allocated
in this V22 boundary; its core-stage witness is 2280 B. Retiring that dead core
object is the next bounded step.

Detailed milestone:
[MILESTONE_ESP32_RETIRE_LEGACY_PARTICLE_STARTUP_V22.md](MILESTONE_ESP32_RETIRE_LEGACY_PARTICLE_STARTUP_V22.md)

### V21 post-review MOVE-event diagnostic visibility fix — REAL-CYD regression PASS (2026-10-01)

Merged V21 correctly removed routine `MOVEEVENT` phase chatter, but its blanket
`DRPG_LOGT` demotion also hid the detailed phase result for fail-closed
`INVALID`, `NOT_READY`, `UNSUPPORTED` and `COMPLEX` outcomes. Those
statuses can occur after preflight and lead directly to
`ESP_NATIVE_GAMEPLAY_DISPATCH_COMMIT_FAILED`, where the resident caller only
reports a coarse `reason=move-commit`.

Commit `2c855bd217999453ec21246937ef6730e1697f3c` fixes only this
diagnostic classification. Routine `NO_EVENT`, `NO_ELIGIBLE` and supported
success outcomes remain TRACE. Unsafe statuses, unknown statuses, and an
unexpected post-preflight EXIT dialog/message divergence emit the full phase
record at INFO, preserving event/opcode/mutation/rollback context.

CI #1230 succeeds at 45064 B static RAM / 758389 B linked Flash /
758752 B firmware.bin with 49 active linker wraps. The INFO diagnostic format
costs 600 B linked Flash versus the merged V21 code image and 0 B static RAM.

The real CYD regression run confirms the hot path remains quiet and fast:
ordinary MOVE/TURN still emit no generic `EXIT-PREFLIGHT / ENTER-PREFLIGHT /
EXIT / ENTER` lines, while meaningful `MOVEEVENT COMMIT`,
`WORLD-READY`, door/dialog/action witnesses and repeated
`LEGACY_GUARD -> RETRY -> RECOVERED` remain visible.

Memory is stable at `heap=93448 heap8=27524 largest8=18420` before lazy
dialog owners, `92412/26488/18420` after `DIALOGCHAIN`, and
`89988/24064/18420` after the bounded topology snapshot owner is allocated.
The test did not naturally encounter an unsafe MOVE-event phase, so visibility
of `INVALID/NOT_READY/UNSUPPORTED/COMPLEX` is source/CI verified rather than a
claimed hardware-triggered witness.

Detailed record remains the V21 milestone with its post-review addendum:
[MILESTONE_ESP32_CONSOLIDATION_HOT_INPUT_TURN_TELEMETRY_V21.md](MILESTONE_ESP32_CONSOLIDATION_HOT_INPUT_TURN_TELEMETRY_V21.md)

### Hot input / move / idle-turn telemetry V21 — REAL-CYD PASS (2026-10-01)

Commit `ac5e11127f294a5e2d7d1127febb21214be94458` continues the
compile-time logging consolidation without changing gameplay, rendering,
rollback, ownership or timing behavior.

Normal INFO no longer emits the high-frequency success-only families:

```text
[MOVEEVENT] EXIT-PREFLIGHT / ENTER-PREFLIGHT / EXIT / ENTER
[TOUCHFEEDBACK] FLASH / RESTORE
[AUTOMAP] UNCOVER reason=MOVE / MOVE-DIALOG
[MONSTERTURN] ROTATE-NO-TURN
[MONSTERTURN] SCHEDULE
```

Three idle monster summaries are conditional rather than globally hidden:
`MONSTERTURN COMPLETE candidates=0` is TRACE only when
`specialAIDeferred=0`; `MONSTERMOVE DEFER active-order-not-owned` is TRACE
only for the strict `candidates=0 activeCount=0` case; and
`MONSTERACTIVESEQ COMPLETE` is TRACE only for `activeCount=0 delivered=0`.
Nontrivial cases remain INFO.

The real CYD validates exact Entrance first frame `71ca7465`,
`shapeData=0x0 mediaTexels=0x0`, banal committed MOVE/TURN with the targeted
noise absent, blocked movement, pickups, regular-door animation, dialog
open/resume, and nontrivial move-event witnesses including `WORLD-READY` and
`COMMIT`.

Two genuine renderer compact-guard incidents still expose and complete the
critical recovery chain:

```text
[NATIVEFRAME] LEGACY_GUARD ...
[NATIVEFRAME] RETRY ...
[NATIVEFRAME] RECOVERED ...
```

The same hardware session loads the V9 Sector 1 checkpoint and preserves live
monster diagnostics: four active members are serviced in order, ordinary
movement commits, both subtype-4 three-goal chains complete, and
`MONSTERACTIVESEQ COMPLETE activeCount=4 delivered=4` remains visible.
A no-attack turn with `specialAIDeferred=2` also remains INFO, proving the
conditional classification is not hiding meaningful deferred AI state.

Entrance gameplay is initially stable at
`heap=93448 heap8=27524 largest8=18420`. After the first lazy dialog-chain
allocation, repeated ALIVE samples remain stable at
`heap=92412 heap8=26488 largest8=18420`.

CI #1225 succeeds at 45064 B static RAM / 757789 B linked Flash /
758160 B firmware.bin. Relative to V20, linked Flash drops by 1408 B while
static RAM and the 49 active linker wraps are unchanged. The hardware workstation
reports the expected +16 B image delta: 757805 B linked Flash /
758176 B firmware.bin.

Detailed record:
[MILESTONE_ESP32_CONSOLIDATION_HOT_INPUT_TURN_TELEMETRY_V21.md](MILESTONE_ESP32_CONSOLIDATION_HOT_INPUT_TURN_TELEMETRY_V21.md)

### Runtime ZIP asset source retirement — REAL-CYD PASS (2026-09-30)

Commit `eb18ea2c5fc090161cee148b1f9aea52c7dc91d9` removes the ESP32 runtime
dependency on `DoomRPG.zip`. `src/Z_Zip.c` is excluded from the build and
the remaining desktop-derived startup/menu consumers use the bounded
`EspLegacyAssetSource` bridge over the authoritative
`/DoomRPG-ESP32.pak`.

CI #1083 succeeds at 45760 B static RAM and 807377 B flash, a reduction of
16 B RAM and 8300 B flash relative to merged main. The produced ELF contains
none of `zipFile`, `openZipFile`, `closeZipFile`,
`readZipFileEntry`, `readZipFileEntryInto`, `findAndReadZipDir` or
`tinfl_decompress`; the ZIP parser and miniz decompressor are genuinely absent.

The real classic CYD validates cold boot entirely from the native PAK:
HUD/pre-render/Render startup/mappings all report `backing=pak`, the normal
menu reaches `shapeData=0x0 mediaTexels=0x0`, and ALIVE reports
`SD=ready PAK=ready`.

Start Game then exercises the four intro BMPs from the PAK and forces a real
Sector1 -> Entrance raw-flash rebuild:

```text
[MAPFLASH] REUSE MISS requestedMap=1 ... cachedMap=2 reason=world-identity
[MAPFLASH] READY map=1 ... backing=raw-internal-flash SDGameplayReads=forbidden
[ENGINESESSION] READY map=1 ... shapeData=0x0 mediaTexels=0x0
```

Loading the existing V9 Sector 1 save proves the inverse direction:

```text
[NATIVESAVE] LOAD ... version=9 ... restored-exact
[MAPFLASH] REUSE MISS requestedMap=2 ... cachedMap=1 reason=world-identity
[MAPFLASH] READY map=2 ... backing=raw-internal-flash SDGameplayReads=forbidden
[ENGINESESSION] READY map=2 ... shapeData=0x0 mediaTexels=0x0
```

Post-load gameplay then commits player movement, services the ordered four-member
monster sequence including both subtype-4 three-goal chains, and resolves a real
Bull Demon attack. ALIVE remains stable at
`heap=94016 heap8=28400 largest8=16372`.

Detailed record:
[MILESTONE_ESP32_CONSOLIDATION_RUNTIME_ZIP_RETIREMENT.md](MILESTONE_ESP32_CONSOLIDATION_RUNTIME_ZIP_RETIREMENT.md)

### Explicit MonsterMovement ActiveSequence composition — REAL-CYD PASS (2026-09-30)

Commit `c27b94e263f4ea0d445e7debeea4d836dbd59e4f` removes the final active
monster-domain linker interception,
`--wrap=EspNativeGameplayMonsterMovement_service`.

Permanent ownership is now explicit:

```text
GameplaySession_service
 -> MonsterMovementProbe_service
 -> MonsterActiveSequence_service
 -> MonsterMovementProbe_serviceMember
 -> MonsterMovement_service
 -> MovementPublish
 -> postMoveGoal / three-goal continuation
```

The real classic CYD validates one ordered four-member MOVE: sprites 218 and 237
commit ordinary movement; subtype-4 sprites 0 and 1 each complete their
three-goal chains. The turn closes with
`activeCount=4 delivered=4 sameMonsterTurn=yes ordered=yes publication=per-member`.
Two following ALIVE samples are identical at
`heap=82704 heap8=17152 largest8=10228`.

CI #1069 succeeds with unchanged 45776 B static RAM and 815677 B flash.
Translation units remain 173, active linker wraps drop 58 -> 57, and active
`EspNativeGameplayMonster*` linker wraps drop 1 -> 0.

A later real-CYD PASS_TURN provides the complementary ranged-attack witness:
sprite 218 / subtype 3 / weapon 15 runs through
`ATTACK-PROBE -> MONSTERACT -> MONSTERATKVIS -> MONSTERRETAL`, with
`playerHP=22->20 armor=12->10` committed only after the visual completes and
rollback closes. Following ALIVE samples remain stable at
`heap=82704 heap8=17152 largest8=10228`.

That genuine ranged attack emits no `RANGED-MEMBER`, confirming the boundary:
`RANGED-MEMBER` denotes the unchanged exact-source ranged-AI
movement/repositioning branch, not the attack presentation/resolution path.

Detailed record:
[MILESTONE_ESP32_CONSOLIDATION_MONSTER_MOVEMENT_SERVICE.md](MILESTONE_ESP32_CONSOLIDATION_MONSTER_MOVEMENT_SERVICE.md)

### Explicit MonsterTurn activation-filter composition — REAL-CYD PASS (2026-09-30)

Commit `517c37855e18894c4d2292dab20ec7852107549b` removes
`--wrap=EspNativeGameplayMonsterTurn_view` and replaces the hidden getter
interception with an explicit producer/filter service boundary.

The permanent service order is now:

```text
PlayerResources session service
 -> MonsterTurn observe/probe producer
 -> MonsterActivation_serviceTurn
    -> deferred DestructibleTurn flush
    -> producer snapshot + attack-probe filtering
 -> AttackVisual / Retaliation / Movement read MonsterActivation_turnView
```

`EspNativeGameplayMonsterActivation_turnView()` is side-effect-free and returns
only the last serviced filtered view. The active-sequence movement orchestrator
still needs the raw producer counters and now reads the ordinary
`EspNativeGameplayMonsterTurn_view()` directly, without a linker bypass.
Synthetic no-attack/movement counter overrides mutate only the bounded filtered
cache and restore the saved producer counters after each member.

The deferred destructible-turn bridge is no longer triggered by a getter. Its
flush now runs explicitly in `MonsterActivation_serviceTurn` after the action
service has returned through its rollback window. The request is therefore
armed by line death, cancelled on rollback if necessary, and transported only
after commit.

Build witness for the exact code boundary:

```text
esp32-cyd CI #1064 = SUCCESS
static RAM = 45776 B
flash = 815677 B
artifact id = 11081649324
artifact sha256 = d7b2a5c920ac293f10c021e6bb6e22831b21909d4e3d0d138c270278594d0cb3
translation units = 173
active --wrap entries = 58
```

The real classic CYD validates all three relevant paths.

Ordered movement remains intact across two turns. Four active monsters are
serviced in order; ordinary sprites 218/237 commit normally and both subtype-4
members execute their bounded three-goal chains. Each turn closes with
`activeCount=4 delivered=4 sameMonsterTurn=yes ordered=yes`.

A real subtype-4 three-shot attack proves the explicit filtered view reaches
both consumers:

```text
[MONSTERACT] DELIVER actualProbe=1 deliveredProbe=1 sprite=1 reason=4 activated=yes
[MONSTERATKVIS] ARM ... loops=3 ... gameplayMutation=no
[MONSTERATKVIS] COMPLETE ... resolution=unblocked-after-animation
[MONSTERRETAL] COMMIT ... playerHP=33->30 armor=23->20 ... rollback=closed
```

The formerly hidden destructible side effect is also exercised directly with an
adjacent axe hit on a jammed subtype-3 line:

```text
[ACTIONENGINE] TRACE ... route=JAMMED_DOOR_CLEARED
[DESTRUCTIBLETURN] ARM ... rollback=armed request=deferred-until-action-service-closed
[DESTRUCTIBLE] COMMIT ... turnAdvance=deferred rollback=closed
[DESTRUCTIBLETURN] REQUEST ... rollbackWindow=closed monsterTurn=requested
[MONSTERTURN] SCHEDULE ... reason=PASS_TURN passSeq=3489661144 ...
[MONSTERACTIVESEQ] COMPLETE ... activeCount=4 delivered=0 ... ordered=yes
```

The jammed-door test begins and ends at the same live allocation witness
(`heap=82664 heap8=17112 largest8=7156`). Earlier movement/attack witnesses are
also stable at `heap=82704 heap8=17152 largest8=10228`; the different absolute
largest-block value reflects the later runtime/cache state, not drift during
the tested transaction.

Only one active monster-domain linker wrapper remains:

```text
EspNativeGameplayMonsterMovement_service
```

### Explicit HUB/automap action-feedback gate — REAL-CYD PASS (2026-09-29)

Commit `3f9b862bcbca3d2217efe0b388e3c9914d1d4b23` removes
`--wrap=EspNativeGameplayMonsterState_actionService` and replaces the hidden
linker interception with the explicit permanent API
`EspNativeGameplayHubActionGate_service(...)`.

The composition order is preserved exactly:

```text
MonsterCombat service
 -> HubActionGate service
 -> MonsterState actionService
 -> ActionEngine service
```

The gate still pauses only world/action feedback expiry while HUB or automap owns
the framebuffer; it does not rebase elapsed time and it resumes the unchanged
MonsterState/ActionEngine chain once world presentation is active again.

CI #1056 succeeds with byte-for-byte identical 45776 B static RAM and 815609 B
flash relative to merged main `8d5bbaf5445fb4557bbf4f66df6aa6d692bb9a56`.
Translation units remain 173 and active linker wraps drop 60 -> 59.

The real classic CYD validates both overlay branches:

```text
[HUBACTIONGATE] PAUSE owner=hub ...
[HUBACTIONGATE] PAUSE owner=automap ...
```

The second PAUSE itself proves the earlier HUB pause cycle returned through
RESUME, because the gate logs PAUSE only on a 0 -> 1 paused transition. After
the overlays, normal world movement and monster service resume: the same
four-member ordered turn runs, both subtype-4 three-goal chains complete, and:

```text
[MONSTERACTIVESEQ] COMPLETE turn=1 reason=1 activeCount=4 delivered=4
sameMonsterTurn=yes ordered=yes publication=per-member multiAttack=deferred
[ALIVE] uptime=79136 ms heap=82704 heap8=17152 largest8=10228
[ALIVE] uptime=84137 ms heap=82704 heap8=17152 largest8=10228
```

Remaining monster-domain active linker wraps:

```text
EspNativeGameplayMonsterTurn_view
EspNativeGameplayMonsterMovement_service
```

### Obsolete MonsterState view witness retirement — REAL-CYD PASS (2026-09-29)

Commit `b2c22ee699213a04429669c6b3ca63c479918d1f` removes the final
three-goal census-only translation unit
`esp_native_gameplay_monster_three_goal_witness.c` after
`57da58cb23a9de923a2c1bc3cd158e5b5818a7b0` removes
`--wrap=EspNativeGameplayMonsterState_view`.

The deleted wrapper had no gameplay mutation: it called the real
`EspNativeGameplayMonsterState_view()`, emitted one-shot subtype 4/13
`WITNESS/CENSUS` diagnostics, then returned the same view unchanged. No
replacement API is needed.

CI #1049 succeeds with 45776 B static RAM and 815609 B flash. Relative to merged
main `da8c3632162ad8dc7a0a83e7c398d815a0fbfea2`, the ESP32 source count drops
174 -> 173 and active linker wraps drop 61 -> 60.

The real classic CYD proves the gameplay path is unchanged:

```text
sprite 218 subtype 3: movement COMMIT + MONSTERPOSTMOVE COMPLETE
sprite 237 subtype 5: movement COMMIT + MONSTERPOSTMOVE COMPLETE
sprite 0 subtype 4: 470 -> 471 -> 439 -> 440, MONSTER3GOAL COMPLETE
sprite 1 subtype 4: 534 -> 535 -> 536 -> 537, MONSTER3GOAL COMPLETE
MONSTERACTIVESEQ COMPLETE activeCount=4 delivered=4 ordered=yes
ALIVE uptime=56336 ms heap=82704 heap8=17152 largest8=10228
```

Each subtype-4 continuation still passes `MONSTERMOVEACT ALLOW`, commits one
movement RNG byte, publishes position/topology and closes rollback before the
next member. The retired `MONSTER3GOAL WITNESS/CENSUS` lines are absent, as
expected, while the permanent `MONSTER3GOAL READY/ARM/PLAN/COMMIT/COMPLETE`
runtime remains intact.

Remaining monster-domain active linker wraps:

```text
EspNativeGameplayMonsterState_actionService
EspNativeGameplayMonsterTurn_view
EspNativeGameplayMonsterMovement_service
```

### Explicit MonsterTurn post-move composition — REAL-CYD PASS (2026-09-29)

Commit `2976cf9f157fa3dfd1649efaaec77986450648cc` removes the active
`--wrap=EspNativeGameplayMonsterTurn_postMoveGoal`.

The ordinary-vs-three-goal decision now belongs explicitly to
`EspNativeGameplayMonsterTurn_postMoveGoal(...)`. Subtypes 4/13 are delegated
to `EspNativeGameplayMonsterThreeGoalTurn_postMoveGoal(...)` before ordinary
turn-owner sync/probe accounting, preserving the previous wrapper ordering.
Other monster families continue through the unchanged ordinary post-move path.

CI #1036 succeeds with 45784 B static RAM and 816545 B flash. The active linker
wrap count is now 61, down from 62 on merged main
`0e66004c755cc050c5fa3f6eac91f85f943e4c8d`.

The real classic CYD validates this after V9 LOAD: movement restarts at `n=1`,
four active monsters are delivered in order, sprite 0 completes
470->471->439->440, sprite 1 completes 534->535->536->537, and both end in
`MONSTER3GOAL COMPLETE`. Every continuation passes `MONSTERMOVEACT ALLOW`,
commits exactly one movement RNG byte, publishes topology/position and closes
rollback before the next member. The turn closes with:

```text
[MONSTERACTIVESEQ] COMPLETE turn=1 reason=1 activeCount=4 delivered=4
sameMonsterTurn=yes ordered=yes publication=per-member multiAttack=deferred
[ALIVE] uptime=49239 ms heap=82696 heap8=17144 largest8=10228
```

Remaining monster-domain active linker wraps:

```text
EspNativeGameplayMonsterState_actionService
EspNativeGameplayMonsterTurn_view
EspNativeGameplayMonsterMovement_service
EspNativeGameplayMonsterState_view
```

### Explicit synthetic MovementView publication — REAL-CYD PASS (2026-09-29)

Commit `560e54bd2d32fe1f5d704cd9ef0d3737c57f765b` removes the active
`--wrap=EspNativeGameplayMonsterMovement_view`.

The three-goal continuation no longer intercepts the global movement view.
Instead, the publisher exposes the bounded permanent API
`EspNativeGameplayMonsterMovementPublish_afterProbeWithView(...)`, and the
three-goal owner passes its synthetic movement view explicitly only for the
continuation transaction. The normal movement path continues to publish against
the real `EspNativeGameplayMonsterMovement_view()`.

CI #1028 succeeds with 45784 B static RAM and 816509 B flash. The active linker
wrap count is now 62, down from 65 on merged main.

The real classic CYD proves both subtype-4 chains through the explicit view seam:

```text
sprite 0:
  first goal 470 -> 471 COMMIT
  goal 2/3 471 -> 439 COMMIT
  goal 3/3 439 -> 440 COMMIT
  MONSTER3GOAL COMPLETE

sprite 1:
  first goal 534 -> 535 COMMIT
  goal 2/3 535 -> 536 COMMIT
  goal 3/3 536 -> 537 COMMIT
  MONSTER3GOAL COMPLETE

MONSTERACTIVESEQ COMPLETE activeCount=4 delivered=4 ordered=yes
ALIVE remains steady through uptime=55893 ms
```

Every continuation still passes `MONSTERMOVEACT ALLOW`, every live publication
closes RNG/position/topology/rollback state, and no
`probe-sequence-or-capture-mismatch` occurs. This candidate was therefore the authoritative hardware-tested code boundary
at that point.

Remaining monster-domain active linker wraps after this PASS:

```text
EspNativeGameplayMonsterState_actionService
EspNativeGameplayMonsterTurn_view
EspNativeGameplayMonsterMovement_service
EspNativeGameplayMonsterTurn_postMoveGoal
EspNativeGameplayMonsterState_view
```

### Explicit MovementProbe reset composition — REAL-CYD PASS (2026-09-29)

Candidate `cb45792af62d8ad0946dc4d477b288ef92aecf3a` removes the active
`--wrap=EspNativeGameplayMonsterMovementProbe_reset` and places the same
ThreeGoal -> Publish -> Movement -> Position reset order directly in the
permanent MovementProbe reset API.

CI #1022 succeeds at 45784 B static RAM / 816509 B flash with 63 active linker
wraps. Real-CYD SAVE/LOAD proves full resident teardown, exact V9 monster
state/topology/position/activation restore, owner READY reinitialization and
`ENGINESESSION READY` with `shapeData=0x0 mediaTexels=0x0`.

The first MOVE after LOAD then proves consumers restart cleanly: movement probe
counter restarts at n=1, four restored active monsters are delivered in order,
all live moves commit, and both subtype-4 monsters complete goals 2/3 and 3/3
through the explicit movement activation/publication path. The turn closes with
`activeCount=4 delivered=4 ordered=yes` and a steady `[ALIVE]` witness.
This candidate is therefore hardware validated.

### Active MonsterPosition prepare wrap removal — REAL-CYD PASS (2026-09-29)

The active linker interception of
`EspNativeGameplayMonsterPosition_prepareCardinalMove` is now replaced by the
explicit permanent movement-domain API
`EspNativeGameplayMonsterMovementActivation_prepareCardinalMove`.

The first candidate `3d15ba1393...` converted ordinary movement but missed the
subtype-4/13 three-goal continuation call site. The real CYD caught that exact
dependency through `probe-sequence-or-capture-mismatch`; that candidate is
explicitly rejected as a hardware boundary.

Commit `aa7cb5c778264e1bb61d442d1c9864c09e6f37a3` routes the continuation through
the same explicit activation + publication-capture boundary. A complete
monster-source audit then found no gameplay caller bypassing it.

The corrected real-CYD run proves:

```text
activeCount=4 delivered=4 ordered=yes
sprite 0 subtype 4: first goal + goal 2/3 COMMIT + goal 3/3 COMMIT + COMPLETE
sprite 1 subtype 4: first goal + goal 2/3 COMMIT + goal 3/3 COMMIT + COMPLETE
all continuation moves pass MONSTERMOVEACT ALLOW
all live moves close rollback and topology publication
ALIVE uptime=340952 ms
```

CI #1020 succeeds on docs-only descendant `25cfb5f9...` with unchanged
45784 B static RAM and 816517 B flash. The active linker wrap count is now 64,
down from 65 at merged main, with no RAM/flash growth.

### Structural consolidation v2 / dead wrapper cleanup — REAL-CYD PASS (2026-09-29)

The post-merge audit starts from exact `main`
`3de74fc1899ea619874b9f2bce8fb3679016c1a4`. The production tree contains
175 ESP32 C/C++ translation units, 17 legacy `src/*.c` implementation units
still compiled for ESP32, and 65 linker `--wrap` entries. 50 of those linker
entries target native `Esp*` symbols, so native-to-native composition is now a
first-class consolidation target.

The first bounded cleanup removed the dormant historical
`EspNativeGameplayMonsterRetaliation_* -> EspNativeGameplayMonsterMovement_*`
wrapper footer and the link-only
`esp_native_gameplay_monster_movement_wrap_compat.c`. Those Retaliation
symbols were no longer present in the active `--wrap` list.

```text
translation units = 175 -> 174
active --wrap flags = unchanged at 65
CI #1015 attempt 2 = SUCCESS
static RAM = 45784 B
flash = 816517 B
```

The RAM/flash totals are byte-for-byte identical to a clean build of merged
`main`, consistent with this compatibility code already being dead at final
link.

The real classic CYD then exercised Sector 1 through a complete monster
hit/death transaction, a `PLAYER_ATTACK` monster turn, active-sequence service,
a committed MOVE from tile 508 -> 507, another MOVE-triggered monster turn, and
a steady `[ALIVE]` witness at about 99 seconds. No reboot or gameplay
regression was observed.

This validates only code through `4731d826...`. Later consolidation commits on
this still-active branch remain candidates until separately tested.

Detailed record:

- [`MILESTONE_ESP32_CONSOLIDATION_WRAPPERS_V1.md`](MILESTONE_ESP32_CONSOLIDATION_WRAPPERS_V1.md)

### Consolidation-v1 progression recovery — REAL-CYD PASS (2026-09-29)

The first translation-unit consolidation experiment on this branch was reverted
exactly; the reproduced stack failure was pre-existing and was fixed separately.
The lasting branch value is a set of generic runtime corrections exposed by
progressing from Entrance through Junction into Sector 1.

Hardware-proven current boundary:

```text
Junction PASS_TURN with enemies=0
Junction V9 SAVE with monsters=0 / positions=0
Junction V9 LOAD with empty monster owners
Junction event 62 direct CHANGEMAP showStats=0
/level01.bsp = Sector 1, map=2, gameplayLoadMapId=3, spawn=477, dir=192
Sector 1 first MOVE 477->509 committed
Sector 1 next MOVE 509->508 committed
Level01 monster activation and ordered live movement continue
```

Permanent generic fixes include independent SAVEGAME/CHANGEMAP map strings,
direct transition-door READY handoff, four-cardinal fresh-map spawn,
spawn-time EV_FORCEMESSAGE, and EV_FORCEMESSAGE inside the bounded atomic mixed
MOVEEVENT executor. None of these paths key behavior on Junction, Sector 1 or a
specific event number.

The post-review real-CYD witness is on
`7d64839a31376c4ca0a3ec4f0f5ae10395b04635`; CI #1009 succeeds at
45784 B static RAM and 816517 B flash.

The same hardware run proves the independent EV_SAVEGAME return route survives
the destructive Junction -> Sector 1 handoff and the session reset. A Sector 1
checkpoint save reported exactly:

```text
returnRoute=/junction.bsp/416,1824/192
```

after a committed Sector 1 MOVE from tile 477 -> 509. The subsequent V9 LOAD
restored Sector 1 at the saved pose and reached
`[ENGINESESSION] READY map=2 angle=128 ... TURN+MOVE=armed`.

The review fix that clamps fresh-spawn facing traces at 32x32 map edges mirrors
legacy `Game_trace()`: source/destination tile components are clamped to
`[0,31]` while raw world endpoints remain available for sprite-plane crossing.
CI covers the implementation, but the supplied hardware run did not exercise an
actual spawn within three facing-trace steps of a map edge; do not claim that
specific edge case as hardware-proven.

Detailed record:

- [`MILESTONE_NATIVE_JUNCTION_SECTOR1.md`](MILESTONE_NATIVE_JUNCTION_SECTOR1.md)

### Transition/loading polish — REAL-CYD PASS

The real classic CYD has now validated both checkpoint LOAD entry contexts on
the same native restore pipeline:

```text
MENU_MAIN -> Load Game             PASS
running gameplay -> SYS -> LOAD    PASS
```

The final loading owner remains full-screen through restore and session/cache
priming. Progress is paced through 10/30/60/75/85/90/93/96/100, and the final
100% loading frame is followed by a rebuilt gameplay frame, explicit loading
release, top+bottom HUD repaint, then the first visible gameplay present.

Important ownership corrections in the tested code boundary:

- checkpoint session teardown happens before `beginLoading()`, so the in-game
  HUB cannot clean up old presentation state after loading has acquired the
  shared framebuffer;
- each progress publication reconstructs the complete loading frame because
  gameplay writers can mutate the shared framebuffer even while their presents
  are suppressed;
- warm-cache resume no longer treats zero new LARGE-LEARN stores or post-render
  large-entry eviction as a functional load failure;
- `READY 100%` is not the gameplay framebuffer: `FINAL-READY` rebuilds the
  world, loading ownership is released without presenting, and
  `EspNativeGameplayHud_repaint()` restores both HUD bands before the final
  gameplay present.

The hardware-tested code boundary is `6903431a60700960127be95d95584728698a36c8`.
Subsequent commits on this branch are documentation-only.

Detailed record:
[MILESTONE_NATIVE_TRANSITION_PRESENTATION.md](MILESTONE_NATIVE_TRANSITION_PRESENTATION.md)

### Ordinary attack post-review corrections — build-valid candidate

Two code-review findings after the hardware PASS exposed edge cases in the
presentation/resolution handshake:

1. A failed first `guardedRender()` consumed `observedAttackProbes`, cleared the
   visual sequence and left retaliation waiting forever for a completion that
   could no longer be published. The visual owner now advances the observed
   probe only after the first attack frame is presented. Until then the probe
   remains retryable and `isBusy()` keeps world input closed.
2. The last attack-to-idle redraw published completion immediately. The owner
   now holds that final idle pose for the subtype's full recovered 200–500 ms
   cadence before setting `completedProbe`, matching the legacy
   `Combat_monsterSeq()` transition into stage 2 / `Player_pain`.

Local `esp32-cyd` compilation succeeds at 45096 B static RAM and 782505 B flash.
These exact edge corrections remain candidates until exercised again on the
real CYD; they do not retroactively alter the earlier hardware evidence.

### SYS SAVE live-compass close correction — REAL-CYD PASS

A later real-CYD progression run exposed one remaining SAVE-return regression
that the earlier facing-label test did not cover: after the player had rotated,
the retained full-HUD model still carried the historical compass angle while the
live player view had a newer settled angle. HUB close repainted the stale full
HUD before checking the protected lower band, so a valid checkpoint could commit
but the close transaction returned `NOT_READY` before `Game saved` was queued.

Hardware failure signature:

```text
[NATIVESAVE] SAVE ... angle=64 ...
[GAMEPLAYHUD] REPAINT ... angle=192 ...
[HUB] CLOSE ... exactBottom=NO ...
[RESIDENTGAMEPLAY] HUB-RECOVER ... status=NOT_READY
```

The permanent close path now reuses the already-bounded compass dirty painter
after the base HUD repaint, sourcing the cardinal angle from the settled
`EspPlayerViewState`. No new retained owner or framebuffer-sized scratch was
added.

The real classic CYD validated the corrected sequence at code head
`a5b30a12b4bb51cd4f016d53212b74e967c19d6d`:

```text
[NATIVESAVE] SAVE ... angle=128 ...
[GAMEPLAYHUD] REPAINT ... angle=64 ...
[HUB] CLOSE ... hudBottom=5da12662 expectedBottom=5da12662 exactBottom=yes ...
[NATIVESAVE] SAVE-CLOSE ... feedback="Game saved" ...
[ACTIONFEEDBACK] PAINT kind=12 text="Game saved" ... durationMs=1200
[RESIDENTGAMEPLAY] HUB-CLOSE ... worldRedraw=yes ...
[ACTIONFEEDBACK] EXPIRE ... restored=topbar-only
```

The stale `GAMEPLAYHUD REPAINT angle=64` line is expected: it describes the
base retained HUD repaint before the live compass rectangle is reapplied. The
authoritative close witness is `exactBottom=yes`, followed by
`SAVE-CLOSE`, the 1200 ms message, and the normal facing-label fallback.

Build witness for the exact tested code:

```text
esp32-cyd CI #767 = SUCCESS
static RAM = 45096 B
flash = 782141 B
```

### Ordinary monster attack resolution — REAL-CYD PASS

The current ordinary monster attack path now resolves gameplay only after its
presentation lease completes. The attack probe remains transactional; while the
animation is active, player HP/armor and gameplay RNG remain unchanged and world
input is blocked.

Current rebased real-CYD witness:

```text
[MONSTERATKVIS] ARM ... sprite=315 ... visual=5 ... phaseMs=500 ...
[MONSTERRETAL] WAIT ... resolution=after-animation playerMutation=no rngConsumed=0 worldInput=blocked
[MONSTERATKVIS] COMPLETE ... visual=5->idle ... resolution=unblocked-after-animation
[MONSTERRETAL] COMMIT ... playerHP=23->19 armor=11->7 ... attackVisual=complete-before-resolution
```

The earlier multi-loop Troop test also visually proved the generic repeated-shot
presentation. Its perceived slowness remains a separate system-level performance
issue rather than a reason to retune this owner in isolation.

### Entrance event 74 mixed MOVE batch — REAL-CYD PASS

The Yellow Key trap on Entrance tile 697 uses a real mixed MOVE script:
state changes, SHOW commands and line lock/open operations. After the trap has
already fired, a later traversal can legitimately reduce to a completely handled
no-op batch with `mutation=no rollback=0`.

The first implementation still retained the static mixed rollback owner in that
case, so the next unrelated MOVE failed closed. The current code arms rollback
owners only for real mutations, immediately releases no-op mixed owners, and
passes the actual `mixedBatchOwner.active` value to the BLOCK diagnostic.

The real-CYD retest continued successfully from tile 697 through 665, 633, 601
and back to 633, with subsequent monster movement and combat. No
`stale-mixed-owner`, `transaction-busy` or
`FAILED reason=move-commit` recurred.

Runtime remained alive at the supplied tail:

```text
heap=81788
heap8=16236
largest8=8692
```

Detailed records:

- [`MILESTONE_NATIVE_MONSTER_ATTACK_RESOLUTION.md`](MILESTONE_NATIVE_MONSTER_ATTACK_RESOLUTION.md)
- [`MILESTONE_NATIVE_MOVE_MIXED_EVENT74.md`](MILESTONE_NATIVE_MOVE_MIXED_EVENT74.md)

The earlier barrel values below remain historical hardware boundaries. Their
code is now merged into `main`; keep the exact barrel SHA and its narrower PASS
claims when diagnosing that feature, independently of the current SAVE-return
validation.

### Native barrel subtype 1 — REAL-CYD PASS

The permanent native action route now owns Doom RPG type-12/subtype-1 explosive
barrels without restoring legacy world/entity ownership.

Hardware-proven transaction:

```text
native player attack
 -> root Barrel hit/removal
 -> logical sprite 180, frames 0..2
 -> recovered 8-cell radius
 -> neighboring barrels removed/queued causally
 -> same-radius sibling barrels animate concurrently
 -> each explosion consumes its own recovered RNG word
 -> nonlethal player radius damage uses native PlayerState
 -> PLAYER_ATTACK monster-turn request
 -> exact rollback remains available until commit
```

The chain is bounded to 16 barrels. Radius support is currently barrel + player:
four cardinal cells use the full blast component, four diagonals use half.
Other hurtable entity families remain fail-closed.

Distant real-CYD proof on the three-barrel cluster:

```text
[BARRELRADIUS] PREFLIGHT ... root=283 chain=3 ... visual=wave-batch-concurrent
[BARREL] WAVE-FRAME ... wave=1 active=1 ... ordinal=1/3
[BARREL] WAVE-FRAME ... wave=1 active=1 ... ordinal=2/3
[BARREL] WAVE-FRAME ... wave=1 active=1 ... ordinal=3/3
[BARRELRADIUS] CHAIN source=283 target=276 ... relation=cardinal
[BARRELRADIUS] CHAIN source=283 target=303 ... relation=cardinal
[NATIVESPRITE] TRANSIENT ... anim=0 batch=1/2 pos=1568,1120 ...
[NATIVESPRITE] TRANSIENT ... anim=0 batch=2/2 pos=1696,1120 ...
[BARREL] WAVE-FRAME ... wave=2 active=2 ... ordinal=1/3
[BARREL] WAVE-FRAME ... wave=2 active=2 ... ordinal=2/3
[BARREL] WAVE-FRAME ... wave=2 active=2 ... ordinal=3/3
[BARREL] COMMIT ... chainRemoved=3 playerRadiusHits=0 ... rollback=closed
```

The user explicitly accepted the resulting two-neighbor explosion as visually
correct. This closes the earlier presentation defect where both neighbors were
removed together but their explosions were then animated serially.

The close-range run additionally proved real player-radius mutation and the
current lethal boundary:

```text
[BARRELRADIUS] PLAYER-HIT source=283 ... relation=cardinal
    component=12 messageDamage=24 hp=32->20 armor=20->8
[BARRELRADIUS] PLAYER-HIT source=276 ... relation=diagonal
    component=10 messageDamage=20 hp=20->8 armor=8->0
[BARRELRADIUS] PLAYER-DEFER source=303 ... relation=diagonal
    component=6 status=1 hp=8 armor=0 mutation=no/lethal-deferred
[MONSTERTURN] ATTACK-CANCEL seq=5 cause=action-rollback scheduled=no
[BARRELRADIUS] ROLLBACK ... rollback=yes rng=yes player=yes world=yes
```

Player death is still intentionally outside the native boundary, so the final
would-be-lethal blast correctly cancels the requested turn and restores the
owned transaction instead of partially committing unsupported death state.

The aggregate multi-blast `<N> damage!` summary exists as a bounded candidate
because the full legacy five-message HUD queue is not yet native. It is **not
hardware-proven**: the close-range test reached lethal rollback before that
summary could commit. The user explicitly deferred this presentation retest
until health can be restored conveniently through inventory/medkits.

Detailed record:

- [`MILESTONE_NATIVE_BARREL_SUBTYPE1.md`](MILESTONE_NATIVE_BARREL_SUBTYPE1.md)

### Finger-first main menu redesign — REAL-CYD VISUAL/TOUCH PASS

The classic-CYD `MENU_MAIN` no longer presents four thin J2ME-style text rows.
The permanent presentation is a bounded 2x2 finger-first dashboard while the
existing four-item menu model and action routing remain authoritative:

```text
START   | LOAD
OPTIONS | HELP
```

The real-CYD visual iteration established the final proportions:

```text
Doom RPG logo = 90x62 logical
card target   = 68x23 logical = 136x46 physical at exact 2x
background    = true black between cards
focus         = amber
armed tap     = ivory + amber double border
confirmation  = existing released second tap on the same card
```

The first 74x28-card candidate was hardware-reviewed as too dominant and forced
the title down to 74x51. The final layout restores the proven 90x62 title size,
then removes the full-width grey/blue dashboard slab so the four industrial cards
float independently on black. The user explicitly accepted this final rendering
on the real classic CYD.

The old main-menu cursor feedback stored four 13x10 RGB565 underlay patches
(1040 B plus bookkeeping). The dashboard instead repaints its bounded card band
allocation-free, so permanent cursor-patch storage is zero. Relative to the
current main image, CI reports 1104 fewer bytes of static RAM:

```text
main       = 45744 B static RAM
redesign   = 44640 B static RAM
delta      = -1104 B
flash      = 767381 B
CI         = esp32-cyd #729 SUCCESS
code head  = cd557d7b727d600cecbff610d6e3e6a21d609853
```

`shapeData == NULL` and `mediaTexels == NULL` remain mandatory. The redesign
does not add a framebuffer, map-wide asset owner, ZIP dependency, or gameplay
world mutation.

The inherited V8 cold main-menu LOAD path was already hardware-proven before this
presentation milestone. The final `cd557d7...` polish changed only main-menu
presentation/touch repaint geometry, but a fresh cold `MENU_MAIN -> Load Game`
sanity has not yet been explicitly reported on that exact code head. Do not claim
that replay until the real CYD produces it.

Detailed record:

- [`MILESTONE_MAIN_MENU_FINGER_FIRST.md`](MILESTONE_MAIN_MENU_FINGER_FIRST.md)

### Options dashboard reuse — REAL-CYD VISUAL/TOUCH PASS

The `MENU_MAIN_OPTIONS` child now reuses the same bounded 2x2 card renderer and
geometry as `MENU_MAIN`:

```text
BACK  | VIDEO
INPUT | SOUND
```

`BACK` is the only enabled card until the three settings backends are ported;
the others remain visible but subdued. Its first tap now paints the same bright
ivory/amber armed state as the parent dashboard, and the released second tap
still executes the real `MenuSystem_back()` transition. Touch ownership now
tracks the painter's runtime framebuffer hash rather than the removed fixed
Options framebuffer fingerprint. PlatformIO compilation passes; visual and
touch behavior, including the two-tap `BACK` route and subdued deferred cards,
has passed a focused check on the real CYD.

This rebased integration combines the current `main` four-page HUB/touch-
feedback redesign with the later native gameplay, V8 checkpoint, CHECK_KEY and
event43 work. The boot-time contiguous-heap regression is fixed, checkpoint
LOAD has been revalidated from both SYS and the cold main menu, and the missing
initial RNG seed has been hardware-validated with a non-zero crate consequence.
The original event43 PASS remains anchored to its historical code head, and the
same Entrance sequence has now also been freshly revalidated post-rebase after
the sprite-renderer stack fix: line102 opens 4/4, event43 commits SHOW x4 on
345->377, the SHOW lease closes on rendered frame commit, and 377->409 executes
only CLOSELINE 102 with another complete 4-frame animation.

### SHOW-exit -> ENTER-dialog reachability census — REAL-CYD PASS

The review fix that releases the static SHOW rollback owner after a destination
dialog opens remains correct defensive transaction hygiene. A temporary,
allocation-free/read-only census scanned all 93 Entrance events for all four
cardinal movement directions using both initial BSP script state and the current
restored checkpoint state.

Real-CYD result:

```text
[MOVEEVENTCENSUS] SUMMARY events=93 candidates=0 mode=initial+current mutation=no allocation=no
```

Therefore Entrance contains **no reachable adjacent movement pair** of the exact
form owned by that review corner: homogeneous EXIT-side EV_SHOW batch followed
by first-eligible ENTER-side EV_DIALOG/EV_DIALOGNOBACK. The Bull Demon/Lost Soul
line102 room is confirmed to be a different sequence: ENTER SHOW event43, then
later EXIT CLOSELINE.

The temporary census was removed after this witness. GitHub comparison confirms
the add/remove probe commits leave **zero code-file diff** versus the documented
pre-probe tree; from hardware-tested cold-load code head `133f678...` to the
post-removal tree, only documentation files differ.

### In-game HUB redesign — focused REAL-CYD smoke pass

The current HUB is:

```text
INV | WPN | STAT | SYS
```

`INV` uses a derived four-row scrolling list containing Notebook, carried
items and owned keys. Key rows reuse the same green/yellow/blue/red mini-card
language as STAT; Credits remain exclusive to STAT, while its key display is a
deliberate quick-status mirror. Any populated row can be selected directly by touch. No persistent list/scroll
owner was added, and item activation remains deferred. `WPN` is a complete 3x3 grid for normal
weapon IDs 0..8; familiar IDs 9..11 remain excluded. The weapon icon loader now
converts source BGR565 palettes to framebuffer RGB565, fixing the red Fire
Extinguisher and yellow-handled Axe presentation. `STAT` is read-only and no
longer covered by checkpoint controls. `SYS` owns full-width SAVE/LOAD cards,
two-step `SAVE?` / `LOAD?` confirmation and `NO SAVE` handling.

All four tabs are directly touch-addressable. The HUB temporarily owns a full
industrial top title bar and reconstructs the permanent gameplay HUD on close.
The 28-byte HUB owner and world/turn gating remain unchanged.

The current full-height layout also reclaims the lower 20 logical rows while
the HUB is open: the gameplay portrait/status strip is hidden, and the writable
menu surface is now 160x100 at `y=20..119`. INV uses four 19-row cards, WPN
uses a 3x3 grid extending to row 118, STAT distributes its read-only groups over
the added space without enlarging its HP/Armor cards, and SYS uses taller
SAVE/LOAD targets. Ordinary close still reconstructs the retained gameplay HUD,
reapplies the settled live compass, and requires the lower-HUD fingerprint to
match the pre-open witness exactly. Successful LOAD keeps its existing
whole-session replacement path. Local `esp32-cyd` compilation succeeds at
45224 B static RAM and 796045 B flash. Its presentation, direct inventory touch,
owned yellow-card row and ordinary gameplay-HUD restoration have passed a
focused real-CYD check.

The tab labels now use native 5x7 glyphs instead of a 3x5 bitmap enlarged in
software before the CYD's final 2x presentation. This removes the effective
4x4 physical pixel blocks that made `INV/WPN/STAT/SYS` appear soft, without
changing any of their 38x13 logical touch rectangles.

Two hardware failures were reproduced and fixed during the smoke pass:

```text
large 128x21 LOAD target -> 597 edits exceeded old 512-entry feedback owner
pickup flash/message + SELECT -> legitimate framebuffer drift failed full-FNV restore
```

The redesign initially enlarged the feedback owner to 768 entries with a
6-byte `{offset,saved,painted}` record. After rebasing, that added exactly
2560 B of static RAM versus the event43 hardware-pass image and starved the
contiguous allocation used by legacy `menu.bsp` sprite structures.

The permanent owner is now bounded at **640 compact 4-byte edits**. Each record
stores `saved RGB565` plus a 15-bit framebuffer offset and one halo/core bit;
the painted value is reconstructed exactly with `glowAdd565(saved, additive)`
during reverse restore. The original 128x21 SYS card needed 597 edits; the
full-height layout's 128x28 card needs 625, still within the same fixed owner
without spending another byte of static RAM. The owner saves 2048 B versus the
768x6 form. Overlay creation remains nonfatal and restoration remains
pixel-owned: newer overlays win.

Detailed record:

- [`MILESTONE_NATIVE_GAMEPLAY_HUB_REDESIGN.md`](MILESTONE_NATIVE_GAMEPLAY_HUB_REDESIGN.md)

#### Compact STAT information layout — REAL-CYD VISUAL PASS

The read-only `STAT` content no longer spends the viewport on five legacy 9x12
text rows. It now uses compact 3x5 labels and intermediate 5x7 values in two
slightly reduced HP/Armor cards, level/XP plus a bounded progress bar, and a consistently aligned
2x2 DEF/STR/AGI/ACC grid. The slim card rails are real proportional gauges:
health is green (red at critical level) and armor is light blue. The footer no
longer leaks the internal hexadecimal key mask; owned bits 0..3 render as compact
green/true-yellow/blue/red key-card icons. Values retain a larger visual weight than
their labels. No content hitbox was added: the existing `STAT` tab is still the
page's only touch target. PlatformIO compilation succeeds. The current rebased
image uses 45224 B of static RAM; real-CYD testing accepted the native 5x7 tab
legibility, compact value typography, proportional rails and corrected key-card
colors.

The redesigned LOAD execution path is now hardware-proven from both the in-game
SYS page and the cold main menu. The two-tap SYS route no longer requires an
ordinary HUB-close HUD restoration before replacing the gameplay session;
`EspNativeGameplaySession_reset()` owns that transition.

### SYS SAVE return and facing-label fallback — REAL-CYD PASS (2026-09-25)

The successful two-tap SAVE route now closes the HUB immediately instead of
leaving `SAVED` on the SYS card. The first SELECT still arms `SAVE?`; the second
commits the V8 checkpoint, returns to the settled world and queues `Game saved`
through the shared bounded `STATUS_TEXT` feedback lease for about 1200 ms.

The first real-CYD run exposed a close-time false negative rather than a save
failure:

```text
[NATIVESAVE] SAVE ... version=8 ...
[HUB] CLOSE ... menuUnderlayRestore=exact hudRepaint=yes ... exactHud=NO
[RESIDENTGAMEPLAY] HUB-RECOVER ... status=NOT_READY
```

The full top band hash legitimately differed because it contained the derived
`Door` facing label. Repainting the base HUD before the next world frame cannot
be byte-identical to that old derived top-bar presentation. HUB close integrity
therefore now validates the exact protected lower HUD band; the top bar is
explicitly recomposed by the following world render.

The user then validated the complete visual sequence on the real classic CYD:

```text
Door -> SAVE -> SAVE? -> successful checkpoint -> gameplay
     -> Game saved -> timeout -> Door
```

Feedback expiry uses the normal top-bar priority rather than a stale framebuffer
snapshot: permanent FORCE_MESSAGE status first, current facing-entity label
second, empty bar last. Saving does not advance the gameplay or monster turn.

### Rebased boot regression — REAL-CYD RECOVERY

The first rebased firmware rebooted continuously while loading the real menu map:

```text
[MAPSTRUCT] Render_beginLoadMap result=1 heap8=17244 largest8=10740 ...
[MAPSTRUCT] -> real Render_beginLoadMapData()
Guru Meditation Error: StoreProhibited
EXCVADDR: 0x00000000
```

The exact ELF resolved the fault to `Render_beginLoadMapData()` at `src/Render.c:566`,
immediately after:

```c
render->mapSprites = SDL_calloc(render->numSprites, sizeof(Sprite_t));
mapSprite->x = DoomRPG_shiftCoordAt(...);
```

`SDL_calloc()` had returned NULL because the rebased static feedback owner had
consumed the contiguous internal-RAM margin needed by the legacy structural
loader. ELF/BSS comparison isolated the delta exactly:

```text
event43 hardware-pass image feedback owner = 2068 B
first rebased image feedback owner         = 4628 B
delta                                      = +2560 B
```

The compact 640x4 journal reduces the production image to:

```text
CI #683 = SUCCESS
static RAM = 45736 B
flash = 764349 B
```

An ESP32-only fail-closed check now guards the `mapSprites` allocation and logs
the exact requested byte count if it ever fails again instead of dereferencing
NULL. After flashing `6cd637c`, the user reports that the reboot loop is gone
and the firmware appears to run normally.

### Core gameplay RNG initial seed — REAL-CYD PASS

A long-running crate anomaly exposed a startup bug rather than bad luck: several
different crates, opened in different orders and even across runs, repeatedly
resolved as `TRAPPED_REMOVE` with `first=0`, followed by `rngByte=0`, `blast=5`
and the legacy message `10 damage!`.

The ESP32 bring-up allocates the real `DoomRPG_t` root with `SDL_calloc()`. That
made the embedded 128-byte `Random_t.randTable` all-zero with `nextRand=0`.
`DoomRPG_randNextByte()` does not refill until the table boundary, so the first
128 byte draws were deterministic zeroes. The desktop root is not calloc-zeroed,
and the native port must explicitly materialize the canonical first table.

Code head `6ab5d25216b52f096563a95749f1dbd8b33712dd` now calls exactly one
`DoomRPG_setRand(&doomRpg->random)` when the real core root is created. This
initializes the inherited hidden seed/reset state without changing later byte
or word draw cadence or the RNG replay guard.

The trap damage message itself was not doubled incorrectly. Legacy explosion
processing calls `Game_radiusHurtEntities(..., rnd+5, rnd+5, ...)`, and
`Player_pain()` displays the sum of its health and armor components. Therefore
`rngByte=0` legitimately gives `5 + 5 = 10 damage`; the bug was the repeated
zero RNG input.

Real-CYD proof after checkpoint LOAD:

```text
[CRATE] CONSEQUENCE seq=4 sprite=82
        first=99 second=0 secondValid=0
        outcome=TRANSFORM effectiveDefTile=92
        rngCombat=2 rngConsequence=1
        attackDamage=6 attackArmorDamage=4

[CRATE] COMMIT ...
        outcome=TRANSFORM
        effective=3/21/def92
        removed=0 transformed=1
        rollback=closed
```

`99` lies in the recovered legacy `24..149` bucket, so the resulting
`type=3/subtype=21` Armor Shard is exact. This proves the live RNG stream is no
longer the calloc-zero table after boot/load.

Build reference:

```text
esp32-cyd CI #693 = SUCCESS
static RAM = 45736 B
flash = 764741 B
```
### Facing-entity top-bar label — REAL-CYD PASS

Legacy `DoomCanvas_checkFacingEntity()` performs a short forward trace after a
settled pose. `Hud_drawTopBar()` then uses the current entity definition name
only as the lowest-priority gameplay fallback:

```text
timed HUD message
 > statBarMessage
 > logMessage
 > facingEntity->def->name (eType != 9)
 > empty
```

The native recovery is pointer-free. It keeps one 34-byte derived owner for the
current target and reads only that target's historical 16-byte EntityDef name
from `/entities.db` through the existing PAK-backed catalog. No map-wide name
table and no legacy `Entity_t*` ownership were introduced.

Recovered trace shape:

```text
origin = player center shifted 31 units forward
reach = 3 tile steps
shared-tile ordering = legacy linked order
line entities = supported
type 9 = trace blocker / no displayed label
```

Real-CYD witnesses include:

```text
Civilian  -> source=sprite index=19 distance=1
Computer  -> source=line   index=279 distance=2 then distance=1
Door      -> source=line   index=275 distance=3
empty     -> active=0 display=0 followed by FACINGLABEL CLEAR
```

The same run proved that closing the HUB restores the world with the existing
`Civilian` facing label intact, and pure rotation retargets the label while
`MONSTERTURN ROTATE-NO-TURN` remains unchanged.

Hardware runtime remained alive at the supplied steady witness:

```text
heap=81936
heap8=16384
largest8=11764
```

Detailed record:

- [`MILESTONE_NATIVE_FACING_LABEL.md`](MILESTONE_NATIVE_FACING_LABEL.md)


### EV_CHECK_KEY / Yellow Door — REAL-CYD PASS (2026-09-24)

The production SELECT route now consumes the permanent native
`EspNativeGameplayPlayerState.keys` bitmask instead of the old placeholder
`playerKeys=0`. Opcode 41 is the recovered generic `EV_CHECK_KEY` selector:
0/1/2/3 = Green/Yellow/Blue/Red.

Real-CYD validation on Entrance event 11 / tile 200 proved the missing-Yellow-Key
path end to end:

```text
[ACTION] SELECT seq=62 status=KEY_REQUIRED tile=200 event=11 eligible=1 unsupported=0
[CHECKKEY] BLOCK seq=62 event=11 cmd=0 keyId=1 mask=02 message="Need Yellow Key" sound=5065-deferred continuation=paused worldMutation=no removedMutation=no turnAdvance=deferred
[ACTIONFEEDBACK] PAINT kind=12 text="Need Yellow Key" ... durationMs=1200
```

No line/script/world mutation occurred and the following `EV_OPENLINE` remained
unexecuted, matching the recovered legacy pause semantics. The owned-key
continuation remains the same bounded atomic door-batch preview/commit/rollback
path; non-door variants remain fail-closed.

The previously blocking Entrance tile 377 / event 43 frontier is now hardware proven. The raw event is five commands:

~~~text
off0 EV_SHOW sprite=1  arg2=0x0000020f
off1 EV_SHOW sprite=2  arg2=0x0000020f
off2 EV_SHOW sprite=3  arg2=0x0000020f
off3 EV_SHOW sprite=4  arg2=0x0000020f
off4 EV_CLOSELINE line=102 arg2=0x000000e0
~~~

On ENTER, the four SHOW commands are eligible and execute in order; the CLOSELINE is not. Each SHOW links its target and sets its REMOVE bit. The MOVE transaction uses one bounded static 192-byte SHOW journal, not a stack-resident batch. Preflight applies all four SHOWs then rolls them back in reverse before MOVE commit; the real commit replays the same four results and retains exact rollback until the rendered destination frame commits.

Real-CYD proof at code head 48accf9:

~~~text
[MOVEEVENT] SHOW-OWNER resultBytes=76 ownerBytes=192 max=4 storage=static stackBatchBytes=0
[MOVEEVENT] SHOW-BATCH-PREFLIGHT event=43 count=4 ... exact=yes mutation=no
[MOVEEVENT] ENTER-PREFLIGHT ... tile=377 ... status=SHOW_OK event=43 eligible=4 opcode=7
[MOVEEVENT] SHOW-BATCH-STEP ... cmd=0 sprite=1 tile=613 ... linked=0->1 ... removed=0->1
[MOVEEVENT] SHOW-BATCH-STEP ... cmd=1 sprite=2 tile=619 ... linked=0->1 ... removed=0->1
[MOVEEVENT] SHOW-BATCH-STEP ... cmd=2 sprite=3 tile=455 ... linked=0->1 ... removed=0->1
[MOVEEVENT] SHOW-BATCH-STEP ... cmd=3 sprite=4 tile=457 ... linked=0->1 ... removed=0->1
[MOVEEVENT] SHOW-BATCH event=43 count=4 eligible=4 mutation=yes removedCommands=4 rollback=1
[MOVEEVENT] COMMIT seq=6 exitEffect=0 enterEffect=1 render=ok rollbackLease=closed
[RESIDENTGAMEPLAY] MOVE ... tile=345->377 ... committed=yes
~~~

The next step out of tile 377 proves the four REMOVE bits changed eligibility exactly as intended: only the suffix EV_CLOSELINE line=102 remains eligible on EXIT, it closes the door through the normal four-frame door animation, and the MOVE to tile 409 commits.

The first implementation put the SHOW x4 journal inside EspNativeGameplayMoveEventResult, which overflowed the real loopTask stack during the unrelated four-frame door render and tripped the stack canary. The final implementation moved that journal to one bounded static owner. The same door then completed all four frames, Bull Demon combat/retaliation ran normally, and the event43 entry/exit sequence completed without reset.

Detailed record:

- [MILESTONE_NATIVE_MOVE_SHOW_BATCH_EVENT43.md](MILESTONE_NATIVE_MOVE_SHOW_BATCH_EVENT43.md)

### Previous merged boundary retained

The merged `main` base already contains the validated V7 Automap checkpoint
persistence, minimal neon-blue top-HUD touch locators and standalone
first-weapon acquisition help dialog. Those remain part of the hardware-proven
baseline and are not modified by this milestone.

### Next bounded frontier

This branch is merge-ready after the documentation-only tail. It did not finish
the intended structural consolidation; instead it established stronger generic
runtime behavior through Sector 1.

After merge, re-read the exact new `main` SHA and resume source/wrapper
consolidation from that SHA. Treat Entrance -> Junction -> Sector 1, Junction
zero-enemy SAVE/LOAD/PASS_TURN, and Level01 first movement as hardware
regression guards while reducing translation units, linker wrappers and legacy
implementation dependencies.

## Permanent architecture / hard invariants

```text
Doom RPG original data/behavior
 -> ESP32-native parsers/catalogs
 -> compact immutable EspMapRuntime
 -> small explicit mutable owners
 -> native event/script engine
 -> native gameplay
 -> native renderer
```

```text
board       = ESP32-2432S028R classic CYD
MCU         = ESP32-D0WD-V3 dual core 240 MHz
flash       = 4 MB
PSRAM       = none
framebuffer = 160x120 RGB565 = 38400 B
shapeData   = NULL
mediaTexels = NULL
```

Do not reintroduce map-wide legacy texels, pointer-heavy desktop ownership, runtime ZIP dependence for migrated gameplay/map data, or map-wide decompression. `/DoomRPG-ESP32.pak` is the native backing store.

Gameplay/input and rendering stay decoupled. Doom RPG is turn-based; do not optimize `PlatformVideo_present()` prematurely.

## Native asset backing — hardware PASS

Active gameplay storage path:

```text
/DoomRPG-ESP32.pak on microSD
 -> requested-map raw internal-flash slot
 -> 19 KiB resident RAM cache
 -> native gameplay / renderer
```

Preparation API:

```text
EspAssetPack_mapFlashPrepare(targetMapId)
```

Entrance raw-slot witness:

```text
pack = 2457398 B
entries = 241
index = 4820 B
metadata = 12288 B
excluded non-current BSPs = 12 / 203811 B
staged payload = 2248743 B
partition = 2752512 B
headroom = 491481 B
indexFNV = 3a51cc4d
payloadFNV = 9ec04e22
```

## Entrance canonical witness

```text
resourceMapId = 1
resource = /intro.bsp
name = Entrance
sourceBytes = 21823
crc32 = 623f34e4
sourceFNV = d5cc751f
runtime arena = 14095 B
runtimeFNV = c3882516
resident payload = 17891 B
spawn tile = 904
spawn direction = 64
spawn position = 544,1824
nodes = 223
lines = 480
sprites = 344
events = 93
byteCodes = 265
strings = 94
native topology entities = 220
enemies = 30
destructibles = 13
```

Retained fresh-map fingerprints:

```text
mapStateFNV = cd99b98e
scriptFNV   = f9e3d9df
lineFNV     = e5e74861
textureFNV  = f1fc1875
automapFNV  = 669b1aa7
topologyFNV = 3f321e43
```

The save-v3 hardware mirror test also captured a deliberately mutated script snapshot with `scriptFNV=f9e59e9f`; this is a checkpoint-state fingerprint, not a replacement for the canonical fresh-map `f9e3d9df` above.

Resident cache baseline:

```text
owner = 23592 B
payload = 19456 B
range records = 288 x 12 B
resident entry slots = 24
large exact range = 2048 B
```

## Current hardware-owned gameplay frontier

Hardware-proven native behavior includes movement/turn/strafe, rotation-in-place without gameplay/monster turn advancement, collision/topology, event-first SELECT, bounded event/script families, dialog, regular doors, hardware-proven pure multi-line SELECT door batches and dynamic lines, mutable line textures, player state/resources, pickups, hazards, native weapon rendering/control/combat, outgoing player damage text plus bounded attack-frame blood spray, compact monster state/position/activation/movement/attack families, type-12/subtype-2 crate combat with exact transform RNG and transformed-pickup projection, type-12/subtype-1 barrel destruction with a bounded three-barrel real-CYD chain, concurrent sibling explosion waves and nonlethal player radius damage, the four-page HUB `INV/WPN/STAT/SYS`, raw-flash backing, bounded checkpoint save/load from both HUB and the main menu, resource consumed-overlay persistence, mutable script/event-state persistence, mutable line open/locked + texture-variant persistence, V5 action-owned removed-sprite persistence, checkpoint-resume HUD/cache/input rearm, HUB/world framebuffer ownership gating for transient action feedback, and the bounded native Automap core with live movement, visited-cell reveal, render-derived thin delimiters, pickup ownership retention and SELECT door interaction while the map owns the framebuffer.

The player root remains:

```text
EspNativeGameplayPlayerState = 52 B
```

The HUB root remains:

```text
EspNativeGameplayHubView = 28 B
pages = INV | WPN | STAT | SYS
WPN = complete 3x3 normal arsenal; source BGR565 -> framebuffer RGB565
STAT = read-only
SYS = SAVE / LOAD / EXIT TO MENU; Exit candidate awaits real-CYD testing
world dispatch blocked while HUB active
turn advance disabled while HUB active
```

## Main-menu Load Game — REAL-CYD PASS

The CYD main-menu presentation is now:

```text
0 Start Game
1 Load Game
2 Options
3 Help/About
```

The original J2ME `Exit` row is gone. Double-tap confirmation on `Load Game`
calls the shared native checkpoint service. Historically, a readable V1-V6 record rebuilt
the immutable BSP, restores its versioned mutable owners and configures the
resume session directly in `ST_PLAYING`, without replaying the intro.

A missing or invalid record is fail-closed: the menu stays active, the selected
row displays red `No Save`, and its runtime framebuffer witness is rebased so
the menu remains interactive. The user confirmed both successful resume and
the no-save response on the real CYD at `18c1cfb`.

Detailed record:

- [`MILESTONE_MAIN_MENU_FINGER_FIRST.md`](MILESTONE_MAIN_MENU_FINGER_FIRST.md)
- [`MILESTONE_MAIN_MENU_LOAD.md`](MILESTONE_MAIN_MENU_LOAD.md)

## Native checkpoint save/load v1 — REAL-CYD PASS

The original one-slot checkpoint established the permanent bounded save path:

```text
/sd/DoomRPG-ESP32.sav
magic = DRPGSAV1
version = 1
recordBytes = 132
atomic write = temp + verify + backup rename + commit rename + verify
```

V1 persists immutable BSP identity, full settled `EspPlayerViewState`, full `EspNativeGameplayPlayerState`, player/runtime fingerprints and CRC32. It never serializes pointers or a desktop object graph.

The final load reprime restores the semantic HUD owners directly after the saved settled view is reconstructed:

```text
[NATIVESAVE] REPRIME-HUD ... refresh=pending clear=ready mutation=owners-only turn=no
```

V1 hardware validation proved exact player/pose rollback and repeated-load stability, but map-local mutable owners were rebuilt fresh.

## Native checkpoint save/load v2 resources — REAL-CYD PASS

V2 preserves the exact proven v1 core and adds only one explicit pointer-free section:

```text
magic = DRPGSAV2
version = 2
recordBytes = 276
v1 read compatibility = retained
EspNativeGameplayPlayerResourcesSnapshot = bounded fixed section
max consumed payload = 128 B / 1024 sprites
Entrance used payload = 43 B / 344 sprites
```

The section persists only the semantic consumed-resource overlay plus explicit identity:

```text
sourceArenaFNV1a
spriteCount
consumedCount
consumedBytes
targetMapId
consumedBits[]
```

Hardware validation proved both directions:

```text
resource consumed before SAVE -> remains absent after LOAD
resource consumed after SAVE -> reappears after LOAD
```

The proof was exercised with Armor Shards and a Small Medkit through the normal native topology/render path, not merely as restored bookkeeping.

## Native checkpoint save/load v3 script — REAL-CYD PASS

V3 keeps the v1 core and v2 resource section and appends exactly one compact `EspMapScriptStateSnapshot`:

```text
magic = DRPGSAV3
version = 3
recordBytes = 808
v1/v2 read compatibility = retained
write format = v3
script snapshot max payload = 512 B
```

The script section persists pointer-free semantic bytes only:

```text
sourceArenaFNV1a
eventCount
byteCodeCount
eventStateBytes
removedCommandBytes
storageBytes
storage[] = packed event states + removed-command bits
```

Entrance uses:

```text
events = 93
byteCodes = 265
script storage = 81 B
```

The snapshot/restore API validates runtime identity, exact counts/sizes and unused tail bytes, restores only into the freshly rebuilt native script owner, and verifies the restored semantic fingerprint before session configure. Corrupt or incompatible sections fail closed.

The final hardware mirror SAVE captured already-mutated event state and three consumed resources:

```text
[NATIVESAVE] SAVE ... version=3 bytes=808
             pos=160,1504 angle=64
             playerFNV=549e6620 runtimeFNV=c3882516
             recordCrc=dc35a833
             resources=3/43B sprites=344
             script=93/265/81B scriptFNV=f9e59e9f
             atomic=temp+backup+rename
             world=resources+script-restored+others-fresh
```

Gameplay then diverged after SAVE by consuming a Bullet Clip and Fire Ext; live state reached `playerFNV=a6e115a7`, weapon 1, weapons `0006`, ammo0=10, ammo1=12 and five consumed resources.

LOAD restored the exact checkpoint:

```text
[PLAYERRES] READY ... playerFNV=549e6620
[PLAYERRES] RESTORE ... consumed=3 bytes=43
[NATIVESAVE] LOAD ... version=3 bytes=808
             pos=160,1504 angle=64
             playerFNV=549e6620
             resources=restored/3/43B
             script=restored/93/265/81B/f9e59e9f
             world=resources+script-restored+others-fresh
[ENGINESESSION] HUD ... hp=30/30 armor=8/20 weapon=2 ammo=8
```

The strongest semantic witness came immediately after LOAD: event 79, which had been completed **before** SAVE, remained ineligible instead of reopening:

```text
[MOVEEVENT] EXIT-PREFLIGHT ... tile=738 ... status=NO_ELIGIBLE event=79 eligible=0 ...
[MOVEEVENT] EXIT ... tile=738 ... status=NO_ELIGIBLE event=79 eligible=0 ...
```

Together with the earlier opposite-direction test where post-SAVE dialog/event mutations were rolled back and became executable again, V3 proves:

```text
script/event mutation after SAVE -> rolled back by LOAD
script/event mutation before SAVE -> preserved by LOAD
```

Detailed record:

- [`MILESTONE_NATIVE_GAMEPLAY_SAVE_LOAD_V3_SCRIPT.md`](MILESTONE_NATIVE_GAMEPLAY_SAVE_LOAD_V3_SCRIPT.md)

### Historical v3 world boundary (superseded by V4)

Persisted:

```text
settled player pose
EspNativeGameplayPlayerState
EspNativeGameplayPlayerResources consumed overlay
EspMapScriptState event states + removed-command bits
```

Still intentionally fresh / not yet persisted:

```text
line open/locked state
line texture variants
automap reveal state
monster mutable state/positions/activation/combat consequences
destructibles
gameplay RNG state
```

Continue save persistence owner-by-owner. Never replace this with a monolithic world/object dump.

Long-session hardware steady point observed during the V3 validation:

```text
heap8 = 17724
largest8 = 8692
```

This remained stable across the final LOAD/replay segment. Fragmentation/headroom remains below the advisory target and should stay on the review list, but the run does not demonstrate a new per-LOAD leak.

## Native checkpoint save/load v4 lines — REAL-CYD PASS

V4 keeps the proven v1/v2/v3 semantic sections and appends one compact line-family snapshot:

```text
magic = DRPGSAV4
version = 4
recordBytes = 1212
v1/v2/v3 read compatibility = retained
write format = v4
line count max = 1024
line bitset max = 128 B
Entrance lines = 480
Entrance bitset bytes = 60
```

The section persists only:

```text
open bit per line
locked bit per line
mutable locked/unlocked texture-10 variant bit
runtime identity + exact line count/size
line-state and texture-state fingerprints
```

The real-CYD load first rebuilt canonical Entrance line owners at `locked=7`, `texture10=0`, then restored the saved soldier-door state:

```text
[MAPLINECHECKPOINT] RESTORE ... lines=480 bytes=60
                    open=0 locked=6 texture10=1
                    lineFNV=69334d90 textureFNV=bda09634
[NATIVESAVE] LOAD ... version=4 bytes=1212
             resources=restored/5/43B
             script=restored/93/265/81B/26f291e3
             lines=restored/480/60B/open0/locked6/tex101/69334d90/bda09634
```

After LOAD, line 352 opened normally with `locked=0` without replaying the soldier unlock script:

```text
[ACTION] DOOR line=352 opcode=15 status=OK open=0->1 locked=0 ...
[DOORANIM] COMPLETE ... transaction=committed
```

The first V4 hardware attempt also exposed a `loopTask` stack-canary reset when entering STAT. The final code boundary moved the large read workspace out of the HUB stack, removed large full-record CRC/verification copies, and passed both CI and the real-CYD INV -> STAT -> LOAD sequence without reset.

Detailed record:

- [`MILESTONE_NATIVE_GAMEPLAY_SAVE_LOAD_V4_LINES.md`](MILESTONE_NATIVE_GAMEPLAY_SAVE_LOAD_V4_LINES.md)

### Historical v4 world boundary (superseded by V5)

Persisted:

```text
settled player pose
EspNativeGameplayPlayerState
EspNativeGameplayPlayerResources consumed overlay
EspMapScriptState event states + removed-command bits
EspMapLineState open/locked state
EspMapLineTextureState locked/unlocked texture variants
```

Still intentionally fresh / not yet persisted:

```text
automap reveal state
monster mutable state/positions/activation/combat consequences
destructibles
gameplay RNG state
```

Post-LOAD session invariants remained intact:

```text
shapeData == NULL
mediaTexels == NULL
heap8 = 14076
largest8 = 6644
```

## Native checkpoint save/load v5 action removals — REAL-CYD PASS

V5 preserves the proven V1-V4 sections and appends one compact
`EspNativeGameplayActionRemovedSnapshot`:

```text
magic = DRPGSAV5
version = 5
recordBytes = 1356
V1/V2/V3/V4 read compatibility = retained
write format = V5
max removed-sprite payload = 128 B / 1024 sprites
Entrance payload = 43 B / 344 sprites
```

The section owns only the action engine's semantic removed-sprite bitset plus
runtime/map identity and a semantic fingerprint. It does not serialize legacy
entities, renderer objects or a generic mutable world graph.

Real-CYD SAVE witness:

```text
[NATIVESAVE] SAVE ... version=5 bytes=1356
             pos=288,1248 angle=0
             playerFNV=15cb16e4 runtimeFNV=c3882516
             resources=5/43B
             script=93/265/81B scriptFNV=26f291e3
             lines=480/60B open=1 locked=6 texture10=1
             lineFNV=c50b0721 textureFNV=bda09634
             actionRemoved=1/43B/a54be373
```

LOAD rebuilt the immutable Entrance runtime, restored the exact checkpoint
owners and reproduced the same action-removal fingerprint:

```text
[NATIVESAVE] REPRIME-HUD ... refresh=pending clear=ready ...
[PLAYERRES] RESTORE ... consumed=5 bytes=43
[MAPLINECHECKPOINT] RESTORE ... open=1 locked=6 texture10=1
[NATIVESAVE] LOAD ... version=5 bytes=1356
             actionRemoved=restored/1/43B/a54be373
[ENGINESESSION] RESUME checkpoint=restored freshFirstFrame=skipped dynamicLines=gameplay-wrapper
[RESIDENTGAMEPLAY] READY map=current entry=checkpoint-resume ...
[ENGINESESSION] READY ... shapeData=0x0 mediaTexels=0x0
```

The user visually confirmed that the fire cleared before SAVE stayed absent
after LOAD while another fire that had never been cleared remained lit. A
stronger opposite-direction mirror (clear a second fire after SAVE and prove it
returns on LOAD) was not exercised in the supplied log and is not claimed.

The final code boundary is:

```text
d65e5b9be9947e92c700b2296790b003ff7b7df0
esp32-cyd #387 / 35704985512 = SUCCESS
REAL-CYD = PASS
```

### Current V5 world boundary

Persisted:

```text
settled player pose
EspNativeGameplayPlayerState
EspNativeGameplayPlayerResources consumed overlay
EspMapScriptState event states + removed-command bits
EspMapLineState open/locked state
EspMapLineTextureState locked/unlocked texture variants
EspNativeGameplayActionEngine action-owned removed-sprite overlay
```

Still intentionally fresh / not yet persisted:

```text
automap reveal state
monster mutable state/positions/activation/combat consequences
full entity/sprite dynamic state and transformed definitions
destructible transformed state such as crate -> pickup
power-coupling health/death globals
persistent GSprites
ceiling/floor color
other legacy player metadata not yet owned natively
```

Gameplay RNG is rebuilt fresh, but the recovered original save format does not
serialize RNG state, so this is not listed as a missing original-save field.

Detailed record:

- [`MILESTONE_NATIVE_GAMEPLAY_SAVE_LOAD_V5_ACTION_REMOVALS.md`](MILESTONE_NATIVE_GAMEPLAY_SAVE_LOAD_V5_ACTION_REMOVALS.md)

## HUB/action-feedback framebuffer ownership — REAL-CYD PASS

The v2 hardware test exposed an unrelated visual ownership race: a pickup top-bar message could expire while HUB owned the framebuffer, leaving a stale `Got ...` fragment over the MENU area and causing a later underlay mismatch/recover path.

The bounded fix is:

```text
8b7a4c04dee1622954f2ea453ca1b15792fbf6fa
  ESP32: pause action feedback while HUB owns framebuffer
5a1020fd5d160c111ff09ecb8a480f37ea8d0578
  ESP32: gate world feedback service behind HUB ownership
CI #267 = SUCCESS
```

The timer remains based on real elapsed time, but world feedback/viewport-flash restore work is blocked while HUB owns the framebuffer. The user reproduced the original pickup-message/HUB sequence on the real CYD and confirmed the stale fragment is gone. Therefore `5a1020fd5d160c111ff09ecb8a480f37ea8d0578` is a hardware-valid code boundary for this ownership fix.

## Entrance -> Junction CHANGEMAP — REAL-CYD PASS

Final hardware-tested branch boundary:

```text
code head = e4acb92403dd48810f3c4e989d4dee16605125e3
main = 6cd8b6804cbec75538becab0d6cbe66e3c79d238
CI #829 = SUCCESS
RAM static = 45200 B
Flash = 789345 B
artifact id = 10873930025
```

Entrance event 1 / tile 69 now owns the first hardware-validated native
world-to-world transition:

```text
SAVEGAME -> /junction.bsp, targetMapId 9, savePos 992,1888 angle 64
CHANGEMAP -> /junction.bsp, targetMapId 9, showStats 1, spawnParam 0
OPENLINE -> line 459, shared regular four-frame animation
```

The real CYD proved the complete bounded route: exact target BSP inventory is
read through a scoped authoritative-SD source probe while Entrance raw-flash
gameplay backing remains untouched; WAIT_STATS uses the current explicit one-tap
bridge; map-flash world identity then MISSes map 1 and rebuilds map 9 before the
source runtime is destroyed; Junction rebuilds from raw internal flash, spawns
at tile 943 / position 992,1888 / angle 64, primes the resident cache and reaches
the generic gameplay service with `shapeData=0x0` and `mediaTexels=0x0`.

The final hardware run reconfirmed the first Junction movement. EXIT tile 943
contains a locked `CLOSELINE`; matching legacy behavior, that failed line close
does not abort movement. ENTER tile 911 contains opcode 4 `MESSAGE`; the native
transaction commits the destination frame first and only then publishes
`"Junction"`. The player reaches tile 911 at position 992,1824 and gameplay
remains active.

The same final code head then proved that Junction dialog/continuation gameplay
remains live after the transition. Facing the Scientist on tile 878 selected
event 56 as `DIALOG_READY`; the native dialog opened a 97-byte / six-line
opcode-8 payload, handled fast-forward and page advance, closed with the pack
released, then resumed command offset 1. `DIALOGCHAIN` executed one bounded
state command and `DIALOG-RESUME` reported opcode 11 / `CHANGESTATE` with
`stateMutation=1`, followed by a successful world redraw. The dialog pipeline
now preserves the live PlayerState key context across validation, NOTE-prefix
filtering and continuation planning. The earlier Marine event 45 regression
that exposed the key-context mismatch was not itself replayed in the final
supplied trace; the final hardware witness is Scientist event 56.

Post-PASS code review found one failure-only rollback leak: if the shared
`SELECT-DOOR` render failed after staging the transition door, the line/script
rollback could succeed while the transition owner remained in `waitingDoor`.
The rebased code head now aborts that owner after successful world/script
rollback and before rendering `SELECT-DOOR-ROLLBACK`. The hardware-proven
successful path is unchanged; the reviewed render-failure path itself was not
hardware-triggered.

The statistics screen presentation itself is still deferred; only the explicit
WAIT_STATS acknowledgement bridge is claimed here. Generic Junction -> level
exits and unrelated opcode-4 MESSAGE routes remain separate milestones.

Detailed record:

- [`MILESTONE_NATIVE_CHANGEMAP_ENTRANCE_JUNCTION.md`](MILESTONE_NATIVE_CHANGEMAP_ENTRANCE_JUNCTION.md)

## Native rotation no-turn parity — REAL-CYD PASS

The legacy/J2ME split is now restored:

```text
movement completion -> advances gameplay turn
rotation completion -> updates facing/view only
```

Hardware-tested code boundary:

```text
b548321f477626777800371f0f82a9f3c2375bd9
```

The real-CYD test confirmed that turning in place no longer advances monster behavior. The same supplied hardware run reconfirmed that legitimate turn-producing actions still do:

```text
PLAYER_ATTACK -> [MONSTERTURN] SCHEDULE n=32 reason=PLAYER_ATTACK
MOVE          -> [MONSTERTURN] SCHEDULE n=33 reason=MOVE
MOVE          -> [MONSTERTURN] SCHEDULE n=34 reason=MOVE
MOVE          -> [MONSTERTURN] SCHEDULE n=35 reason=MOVE
```

The correction adds no allocation, gameplay RNG use, topology mutation, renderer mutation or player mutation on rotation. Detailed record:

- [`MILESTONE_NATIVE_ROTATE_NO_TURN.md`](MILESTONE_NATIVE_ROTATE_NO_TURN.md)

## Next bounded milestone

After this branch merges, re-read the exact resulting `main` SHA before
opening the next `agent/*` branch.

Resume structural consolidation rather than adding another map-specific
progression patch. Consolidate by permanent native domain, remove native->native
linker wrappers where direct composition is clearer, and reduce the remaining
legacy implementation-unit dependency family by family. The newly proven
Junction -> Sector 1 path must remain a non-regression witness.

## Hardware-validated intro display polish

Real-CYD visual validation on code head `0d21332bd2524bca73d5284f70e053ca8ba6430d` confirmed the permanent native intro fit split:

```text
logical framebuffer = 160x120 RGB565
content viewport     = 120x120 centered at x=20
starfield background = 160x120 full width
animated intro scene = 160x120 full width
story text           = 156x120 centered at x=2
extra framebuffer    = 0 B
```

The dedicated animated intro scene now maps its starfield/layers/planet/spaceship/line geometry across the full logical display, while ordinary story-page decoration remains in the aspect-preserving 120x120 content viewport. Story glyphs use the separately tuned 156-pixel soft-wide horizontal mapping.

The real-CYD visual verdict for this exact split was PASS ("Superbe"). The production geometry witness is:

```text
[INTROFIT] ... content=120x120@(20,0) background=160x120@(0,0) animation=160x120@(0,0) text=156x120@(2,0) fit=aspect-content+full-animation+soft-wide-text extraFrameBytes=0
```

CI for the hardware-tested head retained the canonical no-PSRAM static-RAM boundary:

```text
RAM static = 44832 B
Flash      = 724633 B
```

No second framebuffer or frame-sized staging allocation was introduced.

## Hardware-validated pure multi-line SELECT door batch

Real-CYD validation on `/intro.bsp` proved the bounded pure-door batch path used by the hidden/secret door.

The SELECT hit tile `195`, event `10`, whose complete command sequence is exactly two eligible `EV_OPENLINE` commands:

```text
[ACTION] SELECT seq=139 status=DOOR_OK tile=195 event=10 eligible=2 unsupported=0
[DOORANIM] SNAP line=471 open=0->1 flags=00000928 reason=non-regular-door
[DOORANIM] SNAP line=470 open=0->1 flags=00001110 reason=non-regular-door
[ACTION] DOOR-BATCH event=10 count=2 status=OK [0]line=471/op=15/open=0->1/removed=0->1 [1]line=470/op=15/open=0->1/removed=0->1
[RESIDENTGAMEPLAY] SELECT n=10 seq=139 doors=2 firstDoor=471 committed=yes redraw=yes collision=live animation=bounded-batch sound=deferred entityRelink=deferred turnAdvance=deferred
[DYNAMICLINES] FRAME angle=64 open=4 adaptedReads=6 animatedReads=0 textureVariants=0 render=ok immutableRuntime=yes
```

Both lines are non-regular door geometry, so the native animator intentionally reports `SNAP` rather than scheduling the four-frame regular-door animation. The line-state transaction still commits both open bits and both remove-if-handled command bits atomically.

Walking through the opened secret-door tile then sees the event as exhausted:

```text
[MOVEEVENT] ENTER-PREFLIGHT ... tile=195 ... status=NO_ELIGIBLE event=10 eligible=0 ...
[MOVEEVENT] EXIT-PREFLIGHT ... tile=195 ... status=NO_ELIGIBLE event=10 eligible=0 ...
```

This validates the permanent rule: a SELECT event may execute a **pure** bounded batch of up to eight eligible line commands (`EV_MOVELINE/OPENLINE/CLOSELINE/MOVELINE2`), matching the legacy/native `openDoors[8]` capacity. The complete batch is previewed before mutation; mixed-family events, duplicate-line batches requiring sequential intermediate-state semantics, and batches beyond the bound remain fail-closed.

Hardware runtime after the traversal remained resident and stable in the submitted log:

```text
[ALIVE] ... heap=78832 heap8=13280 largest8=12276 ... MAPPINGS=ready MENUBSP=ready
```

CI for the validated code head retained the canonical static-RAM boundary:

```text
RAM static = 44832 B
```

## Native transition presentation — REAL-CYD PASS (2026-09-26)

The reusable native transition presentation is hardware-validated at
`a81dd38a6875154b37b9006a145eef5d73e85a66`.

It is shared by both native CHANGEMAP and checkpoint LOAD. The public owner
accepts target-map identity and progress/stage updates; the current visual skin
remains internal and fixed (`c.bmp` first frame, mini-HUB font, compact
amber/steel card and progress bar). This means later font/theme polish can be
implemented as a bounded presentation/config change without altering either
loading caller.

The final checkpoint regression was an ownership bug: session/input reset
requested a NULL touch callback, and the WAIT_STATS bridge reset the unrelated
checkpoint loading owner. The corrected bridge preserves an already-active
checkpoint loading presentation.

The permanent `Esp32PlatformVideo_present` wrapper now blocks gameplay
publication before Action/GIB/HIT decorators while loading owns the screen.
TransitionPresentation itself uses the real present leaf for its own progress
frames.

The same hardware run exposed a second issue: a monster already dead in the V8
checkpoint replayed its GIB effect on resume. Checkpoint resume now seeds the
bounded GIB presentation owner from restored monster state before the first
gameplay present. Dead restored monsters are historical; live monsters remain
eligible for future genuine death bursts.

Accepted visual result on the real CYD:

```text
loading remains full-screen through checkpoint/session priming
top gameplay bar does not leak through
bottom gameplay HUD does not leak through
restored death GIB does not replay
first visible gameplay frame appears only after loading release
```

Build witness:

```text
esp32-cyd CI #867 = SUCCESS
RAM static = 45224 B
Flash      = 795461 B
```

Detailed record:
[MILESTONE_NATIVE_TRANSITION_PRESENTATION.md](MILESTONE_NATIVE_TRANSITION_PRESENTATION.md)

## Intentionally deferred / incomplete families

```text
save-v6 mutable-world persistence beyond each validated section
CHANGEMAP/script shapes beyond hardware-proven Entrance -> Junction -> Sector 1
pre-arm first-frame/HUD SD startup path
L1 range-record eviction/recycle redesign
audio
pickup sound / got-face
secondary hazard feedback
complete mixed movement resource/hazard ordering
action XP migration
materialized monster drops
corpse-pile trimming
monster movement interpolation
simultaneous multi-monster ordering
special subtype-10 AI
player lethal/death transition
multi-loop weapon/monster mechanics
monster projectiles/messages/sound
rocket/BFG radius damage
familiar weapon slots / hazard redirection
remaining type-12 destructible/radius families beyond barrel/crate/jammed-door
special death consequences
Kronos-specific semantics
password late full-HUD repaint replay
EV_GIVEMAP hardware execution + remaining Automap action parity
Automap reveal-state checkpoint persistence
EV_CHECK_KEY production route
HUB Notebook activation
HUB consumable confirmation/use
HUB Automap / Options / store
```

## Development workflow

```text
recover true main + docs
 -> choose one bounded behavior family
 -> recover exact legacy behavior where relevant
 -> design a small permanent native API/owner
 -> keep different families fail-closed
 -> commit + push agent/*
 -> CI esp32-cyd
 -> test on real CYD
 -> Serial is truth
 -> fix failures directly
 -> after PASS, docs-only tail
 -> merge-ready
```

Never merge into `main` without explicit user request.


## V8 monster-topology recovery + V9 spatial checkpoint — REAL-CYD PASS for V8 compatibility

Hardware-tested code boundary:

```text
7b3efeb8d590c027b94f08ac7c31c886938709d4
esp32-cyd CI #980/#981 = SUCCESS
```

This branch retains the complete prior Junction census history through
`49ad2fe9d63f454611f6d41a729e62905f456251`.

A real classic CYD successfully loaded an existing V8 checkpoint created after
the yellow-card trigger had revealed hidden monsters. The missing V8 spatial
owner was reconstructed only from durable script evidence: removed-command bit
set, REMOVE-if-handled set, opcode SHOW/HIDE. The yellow-card monsters were
present again after LOAD and normal gameplay resumed.

The branch also introduces the bounded V9 monster spatial record:

```text
MonsterState
monster topology (linked/unlinked, tile, link order, visual/alive bits)
MonsterPosition
MonsterActivation order
```

The final real-CYD V9 round-trip is now hardware-proven at
`7b3efeb8d590c027b94f08ac7c31c886938709d4`. Entrance writes a 43-record
topology snapshot covering its 30 enemies plus 13 destructibles, then restores
that exact scope on LOAD:

```text
SAVE  topology=43/7a4b0217 monsters=30/dcda5880
LOAD  tracked=43 enemies=30 destructibles=13
      scope=enemy+destructible-v9
      topologyFNV=7a4b0217
      positionFNV=a369df86
      exact=yes
ENGINESESSION READY shapeData=0x0 mediaTexels=0x0
```

The review corner where EV_SHOW kills an enemy blocker is reconciled in the
checkpoint MonsterState snapshot only; no live gameplay side effects are
fabricated. EV_SHOW-mutated destructibles are included in the V9 topology
owner. The provisional enemy-only V9 remains readable through bounded one-shot
SHOW/HIDE replay compatibility.

Validation boundary: V8 compatibility recovery, V9 SAVE, and V9 LOAD of the
final enemy+destructible spatial checkpoint are all hardware-proven.

Detailed record:

- [`MILESTONE_NATIVE_GAMEPLAY_SAVE_LOAD_V9_MONSTER_SPATIAL.md`](MILESTONE_NATIVE_GAMEPLAY_SAVE_LOAD_V9_MONSTER_SPATIAL.md)

## Native main-menu SELECT consolidation — REAL-CYD PASS (2026-09-30)

Hardware-tested code boundary:

```text
cd5f24dd0ff538a85cebc02025de48cb5998401e
esp32-cyd CI #1135/#1136 = SUCCESS
RAM static = 45768 B
Flash      = 801373 B
firmware.bin = 801744 B
```

The finger-first MENU_MAIN no longer routes selections through
`MenuSystem_select()` or `Menu_select()`. The four visible actions are now
composed explicitly as START / LOAD / OPTIONS / HELP by the native dispatcher.

Real-CYD witnesses cover START -> fresh intro -> Entrance gameplay, valid V9
LOAD -> Sector 1 checkpoint resume, OPTIONS -> Back, HELP page-down/page-up ->
Back, and the common failed-dispatch recovery path. Help now owns both bounded
PAK parsing and bounded native paging over its 83 logical records; visual polish
is deferred.

Pre-merge review hardening added:
- bounded `help.txt` parsing with physical-size/item/line checks;
- common MENU_MAIN repaint + touch re-arm recovery while ST_MENU still owns UI;
- typed LOAD results: NO_SAVE / RECOVERED / TRANSITIONED / FATAL;
- shared `native_main_menu_present` hash/graphics-boundary primitives.

The first bounded-parser attempt intentionally failed closed on the real CYD
when it over-read past the 83 logical Help records; the log proved recovery
returned to framebuffer `522dc605` with touch rearmed. The corrected
`cd5f24dd...` parser was then re-tested successfully on hardware.

Final source review also removed a dead legacy START save precheck.
`Game_checkConfigVersion()` tests desktop `Config/Player/Player2/World`
files, not the native `/DoomRPG-ESP32.sav`. START is therefore now
unconditionally new-game-only and LOAD is the sole resume owner.

Final ELF:

```text
Menu_select        absent
MenuSystem_select  absent
```

Remaining explicit transitional seams include `MenuSystem_back()`,
`MenuSystem_setMenu()`, `Menu_initMenu()` and `Menu_startGame()`.
The natural next consolidation target is `MenuSystem_back()`.

Detailed record:

- [`MILESTONE_ESP32_CONSOLIDATION_MAIN_MENU_SELECT_V8.md`](MILESTONE_ESP32_CONSOLIDATION_MAIN_MENU_SELECT_V8.md)

## Native main-menu Back consolidation — REAL-CYD PASS (2026-09-30)

Hardware-tested code boundary:

```text
8d9b6d25f481d76fe4c79bc1af99d59bc314328f
esp32-cyd CI #1149 = SUCCESS
RAM static = 45768 B
Flash      = 801425 B
firmware.bin = 801792 B
```

HELP and OPTIONS no longer return through `MenuSystem_back()`. Both use the
shared semantic `DoomRPG_esp32MainMenuReturnToMain()` owner, which validates
the expected child/ST_MENU boundary, plays the exact Back cue 5042, rebuilds
MENU_MAIN through the native model owner, performs the opaque main repaint, and
requires exact framebuffer/touch/graphics invariants before success.

Real-CYD HELP proof:

```text
[MAINBACK] READY source=help child=2->1 frame=522dc605 touch=rearmed ... sound=5042 router=native noMenuSystemBack=yes noSetMenu=yes
[MAINHELP] READY back=native mainFNV=522dc605 touch=armed
```

Real-CYD OPTIONS proof:

```text
[MAINBACK] READY source=options child=7->1 frame=522dc605 touch=rearmed ... sound=5042 router=native noMenuSystemBack=yes noSetMenu=yes
[OPTIONBACK] FAST End framebufferFNV=522dc605 expected=522dc605 runtimeFNV=522dc605 menu=1 selected=0 touchActive=1 shapeData=0x0 mediaTexels=0x0
```

Heap stayed at `heap8=20916`, `largest8=10740` in both submitted traces.

Final ELF:

```text
MenuSystem_back       absent
MenuSystem_setMenu    present (445 B)
MenuSystem_playSound  present
```

`MenuSystem_setMenu()` remains a separate, broader seam with retained callers
outside this main-menu Back boundary.

Detailed record:

- [`MILESTONE_ESP32_CONSOLIDATION_MAIN_MENU_BACK_V9.md`](MILESTONE_ESP32_CONSOLIDATION_MAIN_MENU_BACK_V9.md)



## Legacy DoomRPG_Init link-anchor retirement — REAL-CYD PASS (2026-09-30)

Hardware-tested code boundary:

```text
base main = 071febee7ec88958286fb74d82cee9ba61083a85
branch = agent/esp32-consolidation-legacy-init-anchor-v10
code head = f9ab3bda2cd9aadee2d1be0fc08d49600b7a9141
esp32-cyd CI #1159 = SUCCESS
RAM static = 45768 B
Flash      = 797697 B
firmware.bin = 798064 B
artifact id = 11101108652
firmware sha256 = 0da05325e6549e80fd887e6bab9e398a321022ff0718f47c0303f00671432d43
ELF sha256 = ea56e4757e9ff940a245fd6963d91a9193e44f4cad06463b347866287aefb8c8
```

The `MenuSystem_setMenu()` audit separated source-level call sites from the
actual linked ESP32 runtime. Source still contains legacy callers in menu,
death, credits, map-stats, store and cheat/debug paths, but the V9 ELF showed
that all of those enclosing functions were already dead except
`DoomCanvas_setupmenu()`.

The remaining linked chain was not a live runtime owner:

```text
main.cpp diagnostic
 -> DoomRPG_engineLinkAnchor()
 -> &DoomRPG_Init
 -> DoomCanvas_setupmenu()
 -> MenuSystem_setMenu()
```

`DoomRPG_engineLinkAnchor()` existed only so the bring-up diagnostic could
print the address of the inherited monolithic `DoomRPG_Init()`. The actual
ESP32 runtime already constructs and starts the engine through the staged
`DoomRPG_initEngineCore()`, layout/startup owners and native menu/gameplay
handoffs. The branch therefore removes only the diagnostic anchor and its API;
it does not introduce a replacement menu router.

Exact ELF comparison against the hardware-tested V9 artifact:

```text
V9 present:
  DoomRPG_Init              597 B
  DoomRPG_engineLinkAnchor    8 B
  DoomCanvas_setupmenu      132 B
  MenuSystem_setMenu        445 B

V10:
  DoomRPG_Init              ABSENT
  DoomRPG_engineLinkAnchor  ABSENT
  DoomCanvas_setupmenu      ABSENT
  MenuSystem_setMenu        ABSENT
  MenuSystem_back           ABSENT
  MenuSystem_select         ABSENT
  Menu_select               ABSENT

still intentionally present:
  Menu_initMenu             PRESENT
  MenuSystem_playSound      PRESENT
```

The link anchor retirement removes 17 global legacy functions in total and adds
no new global function. Besides the four symbols above, the dropped closure
includes `DoomCanvas_LoadMenuMap`, `DoomCanvas_unloadMedia`,
`Render_setGrayPalettes`, `Game_loadMapEntities`,
`Entity_initspawn`, `MenuSystem_moveDir` and `Player_selectWeapon`.
Linked flash and `firmware.bin` both shrink by 3728 B relative to V9 while
static RAM remains unchanged.

The real classic CYD validates the retained runtime after that link closure is
gone. HELP pages down/up and returns through the native Back owner; OPTIONS
returns through the same owner; both reproduce MENU_MAIN framebuffer
`522dc605`, keep `heap8=20916` / `largest8=10740`, and retain
`shapeData=0x0 mediaTexels=0x0`.

START then traverses the full native path:

```text
MENU_MAIN -> START
 -> ST_INTRO
 -> native intro clock/input
 -> bounded intro disposal
 -> TransitionPresentation loading
 -> native /intro.bsp resident runtime
 -> native spawn/session/cache priming
 -> ENGINESESSION READY map=1
```

The final session witness is:

```text
[ENGINESESSION] READY map=1 ... shapeData=0x0 mediaTexels=0x0
[ALIVE] uptime=64666 ms heap=92492 heap8=26824 largest8=18420 ...
[ALIVE] uptime=69669 ms heap=92492 heap8=26824 largest8=18420 ...
```

The submitted runtime excerpt starts after cold boot, so it does not contain the
new informational `[LEGACYINIT] ... anchor=retired` line. That line is not
claimed as a hardware witness. Static ELF inspection proves the link retirement;
the real-CYD run proves the retained menu/intro/native-gameplay runtime remains
functional without that closure.

Detailed record:

- [`MILESTONE_ESP32_CONSOLIDATION_LEGACY_INIT_ANCHOR_V10.md`](MILESTONE_ESP32_CONSOLIDATION_LEGACY_INIT_ANCHOR_V10.md)


## Native main-menu model ownership — REAL-CYD PASS (2026-09-30)

Hardware-tested code boundary:

```text
base main = 071febee7ec88958286fb74d82cee9ba61083a85
branch = agent/esp32-consolidation-legacy-init-anchor-v10
source-consolidation head = e5c52be0ed24c57df65477a1bed4efbf914d8320
final code head = 8cd0b019ebcb80491a27c2ed0bad3cf57f9ad47e
esp32-cyd CI #1175 = SUCCESS
RAM static = 45224 B
Flash      = 782265 B
firmware.bin = 782624 B
artifact id = 11110371227
artifact digest = sha256:31e9f777f42f22917fde7a72421c7e216b285a1cdf941949540722532af178c3
firmware sha256 = d66e4a1eac3f7911046916de9aa89dcb9a7d4c8e944f9424f7747534c3f181d0
ELF sha256 = 00d14a306c726d44737beca1a14aaab395ecd1aa09c0d0d7c7d92b3b84c86d71
```

The hardware-validated main-menu implementation is now consolidated from 13
`native_main_menu_*.c` translation units to 7 without adding a replacement
wrapper layer. LOAD lives with main actions, the tap gate lives with touch, and
presentation/recovery/scene bridge code lives with the touch-layout owner.
The obsolete pre-touch layout and historical overlay probe sources are gone.

On top of that consolidation, the three active ESP32 call sites of
`Menu_initMenu()` were replaced by bounded model construction in the existing
`native_main_menu_model.c`. The native owner supports only the pre-game
models actually needed here: fixed MAIN, fixed CONTINUE, fixed OPTIONS and the
already-bounded PAK-backed HELP parser. It does not recreate a generic menu
router.

Authoritative final-ELF inspection:

```text
Menu_initMenu                         ABSENT
Menu_LoadHelpResource                 ABSENT
Menu_startGame                        PRESENT  0x49 = 73 B
DoomRPG_esp32MainMenuModelBuildMain   PRESENT  0xbb = 187 B
DoomRPG_esp32MainMenuModelEnter       PRESENT  0x45f = 1119 B
```

The linker can therefore discard the broader legacy menu-construction closure,
including `Menu_fillStatus`, `Menu_setStore`, `Menu_setNotes`,
`MenuSystem_buildDivider`, `Menu_setYesNo` and the static
`vendingMenuTable`.

Relative to the hardware-tested source-consolidation baseline
`e5c52be0...`:

```text
                 e5c52be       8cd0b019       delta
static RAM       45768 B        45224 B        -544 B
Flash           797689 B       782265 B      -15424 B
firmware.bin    798048 B       782624 B      -15424 B
```

The real CYD exposes the RAM reduction directly. The stable menu heap moves
from `heap8=20916` to `heap8=21460`, exactly +544 B, and the final gameplay
ALIVE moves from `heap8=26824` to `heap8=27368`, also exactly +544 B.

Cold boot now also provides the previously-missing hardware witness for the V10
anchor retirement:

```text
[LEGACYINIT] STAGED runtime=ESP32-core/layout/startup legacy-DoomRPG_Init-anchor=retired
```

The new model owner preserves the validated presentation fingerprints:

```text
MENU_MAIN modelFNV = 292c7f95
MENU_MAIN finalFNV = 522dc605
HELP page0 FNV     = 5f22cf6b
HELP page8 FNV     = d0788359
OPTIONS modelFNV  = e1ef01f7
OPTIONS frameFNV  = 162d3999
```

Real-CYD traces show `builder=native-fixed-main` on HELP/OPTIONS Back and
`builder=native-fixed-options` entering OPTIONS, with
`shapeData=0x0 mediaTexels=0x0`. START then traverses the complete intro,
bounded disposal, Entrance resident load/cache prime and reaches
`[ENGINESESSION] READY map=1`; final ALIVE is stable at
`heap=93076 heap8=27368 largest8=18420`.

The local PlatformIO upload build reported 45224 B RAM, 782281 B Flash and a
782640 B firmware image; the 16-byte absolute Flash/image difference from CI
does not change the exact -15424 B CI delta or the final ELF symbol result.

Detailed record:

- [`MILESTONE_ESP32_CONSOLIDATION_MAIN_MENU_MODEL_V11.md`](MILESTONE_ESP32_CONSOLIDATION_MAIN_MENU_MODEL_V11.md)


## Native START + intro escape closure retirement — REAL-CYD PASS (2026-09-30)

Hardware-tested code boundary:

```text
base main = 4c6071ebe7de01f47925bf7792123e8c8f9d7ff5
branch = agent/esp32-consolidation-main-menu-start-v12
code head = 6b565cd46172e384209a1d93e355c951f4d6c4fa
esp32-cyd CI #1185 = SUCCESS
RAM static = 45224 B
Flash = 782009 B
firmware.bin = 782368 B
artifact id = 11113402480
```

V12 removes the last legacy pre-game START/intro escape closure without adding
a file or a generic router.

`native_main_menu_start_action.c` now owns only the ESP32 new-game path:
`imgBG=NULL -> Player_reset -> totalDeaths=0 -> ST_INTRO`. The dedicated LOAD
card remains the only resume owner, and `skipIntro != 0` fails closed before
menu cleanup or player mutation rather than entering the desktop load-map path.

`native_story_fit.c` no longer delegates page changes to
`DoomCanvas_changeStoryPage()`. The renderer owns only the automatic bounded
animation transition `storyPage 1 -> 2`. The final page-2 Continue remains
owned by the native input/clock boundary:
`Esp32IntroClock_park("intro-exit-ready") -> Esp32IntroDispose_service()`.

Authoritative final-ELF inspection:

```text
Menu_startGame               ABSENT
DoomCanvas_loadState         ABSENT
DoomCanvas_changeStoryPage   ABSENT
DoomCanvas_disposeIntro      ABSENT
DoomCanvas_loadMap           ABSENT
```

Against merged main:

```text
                 main 4c6071e    V12 6b565cd    delta
static RAM       45224 B         45224 B         0 B
Flash           782265 B        782009 B      -256 B
firmware.bin    782624 B        782368 B      -256 B
```

The real classic CYD exercises the exact new boundary. After entering page 1,
no touch is sent; the native clock advances it after about 10.1 s:

```text
[INTROIN] CONTINUE storyPage=0->1 t=1443700 epoch=1443700
[INTROCLK] AUTO-PAGE 1->2 t=1453800 textPage=0 epoch=1453800
```

The final Continue then stays on the native disposal/bootstrap path:

```text
[INTROIN] FINAL-CONTINUE page=2 textPage=0 t=1481600 fullTextPresented=yes
[INTROCLK] PARK reason=intro-exit-ready ... heap8=43104 largest8=12276
[INTRODISP] READY ... heap8=43104->76876 recovered=33772 ... noMapLoad=yes
[NATIVEBOOT] READY map=1 ... shapeData=0x0 mediaTexels=0x0
[ENGINESESSION] READY map=1 ... shapeData=0x0 mediaTexels=0x0
[ALIVE] ... heap=93076 heap8=27368 largest8=18420 ...
```

Detailed record:
[`MILESTONE_ESP32_CONSOLIDATION_NATIVE_START_INTRO_V12.md`](MILESTONE_ESP32_CONSOLIDATION_NATIVE_START_INTRO_V12.md)


### Explicit intro-dispose -> native-startup composition — REAL-CYD PASS (2026-09-30)

Commit `0f733d1a6ac680b0ff3f7954f4e40bcf44f942f8` removes the two active
linker interceptions around the already resource-only intro disposer:

```text
--wrap=Esp32IntroDispose_reset
--wrap=Esp32IntroDispose_service
```

The same ordering is now explicit at the sole live owner boundary:

```text
Esp32IntroClock_arm
 -> Esp32IntroDispose_reset
 -> EspNativeStartup_reset

Esp32IntroClock_service after intro-exit-ready
 -> Esp32IntroDispose_service
 -> EspNativeStartup_service
```

No new translation unit or generic router was added. CI #1191 succeeds with
unchanged 45224 B static RAM, 782009 B linked Flash and 782368 B firmware.bin.
The final ELF has 55 active `__wrap_*` symbols, down from 57 on merged main.
`Esp32IntroDispose_reset/service` and `EspNativeStartup_reset/service` are
present, while `__wrap_Esp32IntroDispose_reset/service` are absent. The V12
retired symbols `Menu_startGame`, `DoomCanvas_loadState`,
`DoomCanvas_changeStoryPage`, `DoomCanvas_disposeIntro` and
`DoomCanvas_loadMap` remain absent.

The real classic CYD validates the complete path. Native START arms the bootstrap,
the untouched page-1 animation advances automatically to page 2, final Continue
parks the clock, and bounded disposal recovers exactly 33772 B of 8-bit heap
without changing the framebuffer or loading a map:

```text
[NATIVEBOOT] RESET generic resident bootstrap armed
[INTROCLK] AUTO-PAGE 1->2 ...
[INTROCLK] PARK reason=intro-exit-ready ...
[INTRODISP] READY ... heap8=43104->76876 recovered=33772 ... noMapLoad=yes
```

The explicit startup service then takes ownership, builds Entrance and reaches
resident gameplay with legacy render pools still absent:

```text
[NATIVEBOOT] LOADING-TAKEOVER map=1 source=intro-disposed owner=transition-presentation
[NATIVEBOOT] RESIDENT map=1 ... arena=14095 ...
[NATIVEBOOT] READY map=1 ... shapeData=0x0 mediaTexels=0x0
[ENGINESESSION] READY map=1 ... shapeData=0x0 mediaTexels=0x0
[ALIVE] ... heap=93076 heap8=27368 largest8=18420 ...
[ALIVE] ... heap=93076 heap8=27368 largest8=18420 ...
```

Detailed record:
[MILESTONE_ESP32_CONSOLIDATION_INTRO_STARTUP_COMPOSITION_V13.md](MILESTONE_ESP32_CONSOLIDATION_INTRO_STARTUP_COMPOSITION_V13.md)


### Production menu.bsp runtime retirement — REAL-CYD PASS (2026-09-30)

Hardware-tested code head
`dd4161a40b28d2ed9c88370b2f8281b9045f2980` removes the historical
`menu.bsp` structural runtime from the normal `esp32-cyd` boot path.

Before V14, normal boot still built the legacy menu map structures through
`Render_beginLoadMap(MAP_MENU)` / `Render_beginLoadMapData()`, intercepted
the seventh `DoomCanvas_updateLoadingBar()` callback with `longjmp`, then
discarded the resulting 3D menu scene visually by painting the opaque native
dashboard on top.

The normal production path is now direct:

```text
config + immutable mappings
 -> DoomRPG_esp32MainMenuModelBuildMain
 -> DoomRPG_esp32RepaintOpaqueMainMenu
 -> native touch owner
```

The historical menu BSP structure/wall/sprite suite remains available only in
the explicit `esp32-cyd-bringup` profile. Its four linker compatibility flags
are no longer production flags.

CI #1197 succeeds with:

```text
                         merged main     V14           delta
static RAM               45224 B         45128 B        -96 B
linked Flash            782009 B        773089 B      -8920 B
firmware.bin            782368 B        773456 B      -8912 B
active __wrap_*              55             52            -3
```

Final-ELF inspection of the CI artifact confirms these legacy production symbols
are absent:

```text
DoomRPG_probeMenuBspHeader
DoomRPG_probeMenuMapRuntimeStructures
DoomCanvas_updateLoadingBar
Render_beginLoadMap
Render_beginLoadMapData
__wrap_DoomCanvas_updateLoadingBar
__wrap_Render_beginLoadMap
__wrap_longjmp
```

The permanent native menu owners remain present:

```text
DoomRPG_esp32MainMenuModelBuildMain
DoomRPG_esp32RepaintOpaqueMainMenu
```

Real classic-CYD cold boot proves the new production boundary:

```text
[CONFIGMAP] Render_beginLoadMap / BSP still NOT executed
[MAINOPAQUE] ... finalFNV=522dc605 ... heap8=35656 largest8=23540
[MAINBOOT] READY owner=native-opaque menuBspRuntime=skipped legacyMapStructures=not-created frame=522dc605
[ALIVE] ... heap=101580 heap8=35656 largest8=23540 ... MENU=ready
```

No `[MENUBSP]` or `[MAPSTRUCT]` stage appears in the normal boot log.
OPTIONS -> Back and HELP -> Back both restore exact MENU_MAIN FNV
`522dc605` with unchanged `heap8=35656 largest8=23540` and
`shapeData=0x0 mediaTexels=0x0`.

START confirms that the old menu map structures were never created:

```text
[MAINSTART] Begin ... heap8=35656 largest8=23540 shapeData=0x0 mediaTexels=0x0
[MAINMENU] Runtime cleanup ... nodes=0x0 lines=0x0 mapSprites=0x0 ... shapeData=0x0 mediaTexels=0x0
```

The unchanged intro/disposal/bootstrap path then reaches Entrance with the
memory invariants intact:

```text
[INTRODISP] READY ... heap8=43200->76972 recovered=33772 ... noMapLoad=yes
[NATIVEBOOT] READY map=1 ... shapeData=0x0 mediaTexels=0x0
[ENGINESESSION] READY map=1 ... shapeData=0x0 mediaTexels=0x0
[ALIVE] ... heap=93388 heap8=27464 largest8=18420 ...
[ALIVE] ... heap=93388 heap8=27464 largest8=18420 ...
```

A following real FORWARD move commits tile `904 -> 872`, renders the new
frame, services the monster turn, and leaves ALIVE stable at the same values.

Detailed record:
[MILESTONE_ESP32_CONSOLIDATION_MENU_BSP_RUNTIME_RETIREMENT_V14.md](MILESTONE_ESP32_CONSOLIDATION_MENU_BSP_RUNTIME_RETIREMENT_V14.md)


### First-frame fidelity wrapper retirement from production — REAL-CYD PASS (2026-10-01)

Hardware-tested code head
`5460c689b708468e3bdd618d0000753159a24109` removes the production
`--wrap=EspNativeFirstFrame_route` interception while preserving the real
native first-frame owner unchanged.

The removed production wrapper did only post-success diagnostics:

```text
EspNativeFirstFrame_route
 -> read-only viewport COLORSTATS
 -> remember Render* for optional BMP export
```

It did not own renderer state, first-frame publication, FNV calculation,
presentation, failure handling or gameplay transition. Those remain in the
real `esp_native_first_frame.c` implementation and are still consumed by
`EspNativeGameplaySession`.

The wrapper is retained only by `esp32-cyd-bringup` for historical fidelity
inspection.

CI #1202 succeeds with:

```text
                         merged V14 main   V15           delta
static RAM               45128 B           45120 B        -8 B
linked Flash            773089 B          772161 B      -928 B
firmware.bin            773456 B          772528 B      -928 B
active __wrap_*              52                51           -1
```

Final-ELF inspection confirms:

```text
EspNativeFirstFrame_route                 PRESENT
EspNativeFirstFrame_view                  PRESENT
EspNativeFirstFrame_renderGameplayViewport PRESENT
__wrap_EspNativeFirstFrame_route          ABSENT
Esp32FirstFrameDiagnostic_reset           ABSENT
Esp32FirstFrameDiagnostic_exportBmp       ABSENT
pendingDumpRender                         ABSENT
```

The real classic CYD proves that the production first-frame contract is
unchanged. Entrance still publishes the exact known first frame:

```text
[NATIVEFRAME] WALL requests=8 draws=8 spans=160 pixels=4430 ...
[ENGINESESSION] FIRST_FRAME map=1 angle=64 frame=71ca7465 walls=8 pixels=4430 presented=1
```

No `[JUNCTIONFRAME] COLORSTATS` line appears.

The 8-byte static-RAM reduction is visible exactly in hardware versus V14:

```text
MENU_MAIN heap8     35656 -> 35664
intro heap8         43200 -> 43208
resident ALIVE      27464 -> 27472
```

The gameplay session then reaches:

```text
[ENGINESESSION] READY map=1 ... shapeData=0x0 mediaTexels=0x0
[ALIVE] ... heap=93396 heap8=27472 largest8=18420 ...
[ALIVE] ... heap=93396 heap8=27472 largest8=18420 ...
```

Detailed record:
[MILESTONE_ESP32_CONSOLIDATION_FIRST_FRAME_DIAGNOSTIC_WRAP_V15.md](MILESTONE_ESP32_CONSOLIDATION_FIRST_FRAME_DIAGNOSTIC_WRAP_V15.md)


### Compile-time ESP32 logging policy V16 — REAL-CYD PASS (2026-10-01)

Hardware-tested code boundary: `5a320d50a5f88646382db1211114a31b671b34b2`.

V16 adds the allocation-free `doomrpg_log.h` compile-time policy:
ERROR / INFO / DEBUG / TRACE. Normal `esp32-cyd` defaults to INFO and
`esp32-cyd-bringup` explicitly selects TRACE. Disabled DEBUG/TRACE calls and
their format strings can disappear from the final image.

The bounded migration classifies per-present VIDEO timing, PAKIO SAMPLE,
PLANEPROFILE, successful periodic NATIVEPLANE summaries, per-frame
SPRITEPROFILE and expected RNG WORD-OOB-AVOIDED as TRACE. Detailed crate and
interaction inventories are DEBUG. Failures/OOM/FATAL remain ERROR, while
operational transitions and ALIVE remain visible at INFO.

No functional seam is removed. Final-ELF `nm` confirms the exact same 51
`__wrap_*` symbols as merged V15 main, including PlatformVideo, Render
plane/cull and all four EspAssetPack wrappers.

CI #1209:

```text
                         V15/main        V16            delta
static RAM               45120 B         45080 B         -40 B
linked Flash            772161 B        767581 B       -4580 B
firmware.bin            772528 B        767952 B       -4576 B
active __wrap_*              51             51              0
```

The 40-byte RAM reduction is exactly the normal-build removal of PAKIO profiling
state. The INFO ELF contains none of the targeted VIDEO/PAKIO/PLANEPROFILE,
NATIVEPLANE-success, SPRITEPROFILE, RNG WORD-OOB, CRATESTATE WITNESS or
INTERACTMAP OPCODE strings, while NATIVEPLANE failures, RNG FATAL,
CRATESTATE OOM/READY, MAPFLASH error/ARM, ALIVE and ENGINESESSION READY remain.

The real classic CYD validates cold boot, native MENU_MAIN, START, full intro
and disposal, Entrance bootstrap, exact first frame `71ca7465`,
`ENGINESESSION READY ... shapeData=0x0 mediaTexels=0x0`, committed movement
and rotation, crate transform/pickup, dialog resume and door open/close
animation. Multiple ALIVE witnesses remain healthy and none of the targeted
TRACE spam appears.

Remaining verbose families such as NATIVEFRAME, INTERACTCORPUS and semantic
gameplay traces were intentionally left outside this bounded milestone.

Detailed record:
[MILESTONE_ESP32_CONSOLIDATION_LOG_LEVELS_V16.md](MILESTONE_ESP32_CONSOLIDATION_LOG_LEVELS_V16.md)

### Interaction / CHANGEMAP recovery diagnostics scoped to bringup V17 — REAL-CYD PASS (2026-10-01)

Hardware-tested code boundary: `53b548b5adf0d09c2d1e1ed4b673a3ae8054cac2`.

V17 removes the temporary production linker interception around
`EspNativeGameplayInteractionInventory_log()` and prevents the normal INFO
session chain from executing the one-shot interaction inventory at all.
`esp_native_changemap_probe.c` is compiled only at DEBUG/TRACE level, while
`esp32-cyd-bringup` retains the historical
`--wrap=EspNativeGameplayInteractionInventory_log` witness.

This is diagnostic retirement only. The permanent production transition path
remains unchanged:

```text
EspNativeGameplaySession
 -> EspNativeGameplayPlayerResources_sessionService
 -> EspNativeResidentGameplay
 -> EspNativeGameplayTransition
 -> EspNativeGameplayTransitionHandoff
```

CI #1214 succeeds with:

```text
                         merged V16 main   V17           delta
static RAM               45080 B           45072 B         -8 B
linked Flash            767581 B          764757 B      -2824 B
firmware.bin            767952 B          765120 B      -2832 B
active __wrap_*              51                50           -1
```

Artifact `11152203158` has digest
`sha256:3cf7e77078a257d81394e954f54ba3a12ad8ac3a505046b7f72ad88e03448adf`.
The CI firmware SHA-256 is
`06885150bee7ae651340a3b0cfb5557eae161bfc99a394bc9dae8355d2bf6425`;
the ELF SHA-256 is
`5b9bceb0f42f5391f522ac7a61bee9757310c534305b998c6195a72cafa23b37`.

Final-ELF inspection proves exactly 50 active `__wrap_*` symbols.
`__wrap_EspNativeGameplayInteractionInventory_log` and
`EspNativeGameplayInteractionInventory_log` are absent from the linked INFO
image; the real `EspNativeGameplayTransitionHandoff_service` remains present.
No `[CHANGEMAPPROBE]` or `[INTERACTMAP]` format string remains in the INFO
ELF. `[INTERACTCORPUS]` and the older `[JUNCTIONEXITCENSUS]` family remain
intentionally outside this bounded milestone.

The real classic CYD validates cold boot, native MENU_MAIN, START, full intro
and disposal, Entrance bootstrap, exact first frame `71ca7465`, and:

```text
[ENGINESESSION] READY map=1 ... shapeData=0x0 mediaTexels=0x0
```

The same run commits movement and rotation, transforms a crate, resumes a real
dialog chain through opcode 19, picks up two Armor Shards, and keeps ALIVE
healthy. No `CHANGEMAPPROBE` or `INTERACTMAP` line appears.

Observed normal-build memory witnesses include:

```text
MENU_MAIN heap8=35712 largest8=23540
intro     heap8=43256 largest8=12276
gameplay  heap8=27520 largest8=18420   (before later dialog/resource allocations)
```

The local PlatformIO build reports the same 45072 B static RAM and a harmless
16-byte size difference from CI: 764773 B linked Flash / 765136 B firmware.bin.

Detailed record:
[MILESTONE_ESP32_CONSOLIDATION_INTERACTION_DIAGNOSTICS_V17.md](MILESTONE_ESP32_CONSOLIDATION_INTERACTION_DIAGNOSTICS_V17.md)

### Junction exit census scoped to DEBUG/TRACE V18 — REAL-CYD PASS (2026-10-01)

Hardware-tested code boundary: `6733395845c289fa9f69cc6c61001c3a68f2d72d`.

V18 removes the historical `EspNativeGameplayTransition_probeJunctionExitCensus()`
from the normal INFO image. The session call, private
`junctionExitCensusDone` byte and the full census function are compiled only
for DEBUG/TRACE. The normal product build therefore neither calls nor links
this historical Junction SAVEGAME/CHANGEMAP corpus scanner.

The production transition owner is unchanged. In particular
`EspNativeGameplayTransition_trySelect()` and
`EspNativeGameplayTransitionHandoff_service()` remain linked and own the
live transition path.

CI #1216:

```text
                         V17              V18           delta
static RAM               45072 B          45072 B          0 B
linked Flash            764757 B         762617 B      -2140 B
firmware.bin            765120 B         762976 B      -2144 B
active __wrap_*              50               50            0
```

Artifact `11154118496`:
`sha256:fa4e82a7305596cfa36ca65897e3802573309db723d15e22e7993636d51d0ec6`.
Firmware SHA-256:
`fccb055e5d3019612650f8740453480a8f0ad9e418ee5e709031d2b1024d6db0`.
ELF SHA-256:
`a8197c29053631f9dcd22968c331eb868f8c3606efca33ec598ac610ea24b8ec`.

Final-ELF inspection proves `EspNativeGameplayTransition_probeJunctionExitCensus`,
`junctionExitCensusDone` and every `[JUNCTIONEXITCENSUS]` format string are
absent from normal INFO firmware. The 50-wrapper set is unchanged from V17.

The real classic CYD validates cold boot, START, full intro/disposal, Entrance
bootstrap and the exact first frame:

```text
[ENGINESESSION] FIRST_FRAME map=1 angle=64 frame=71ca7465 walls=8 pixels=4430 presented=1
```

The same run reaches `ENGINESESSION READY` with
`shapeData=0x0 mediaTexels=0x0`, commits multiple moves and rotations,
transforms a crate into an Armor Shard, picks up two shards, opens a regular
door with the four-frame native animator, crosses it, then observes its
four-frame auto-close. ALIVE remains stable at:

```text
heap=93444 heap8=27520 largest8=18420
```

No `[JUNCTIONEXITCENSUS]` line appears in the runtime transcript.

The local PlatformIO build reports the same 45072 B static RAM and the known
16-byte environment difference: 762633 B linked Flash / 762992 B firmware.bin.

The perceived gameplay smoothness is consistent with the cumulative
compile-time logging/profiling retirement since V16; V18 itself removes a
one-shot/session-readiness diagnostic rather than a per-frame renderer path.

Detailed record:
[MILESTONE_ESP32_CONSOLIDATION_JUNCTION_EXIT_CENSUS_V18.md](MILESTONE_ESP32_CONSOLIDATION_JUNCTION_EXIT_CENSUS_V18.md)

### Interaction-chain corpus wrapper scoped to bringup V19 — REAL-CYD PASS (2026-10-01)

Hardware-tested code boundary: `a2dffc4243701a5f78fa02abf39a81da68b5c128`.

V19 removes the production linker interception of
`EspNativeResidentGameplay_service()` that existed only to emit the one-shot
`[INTERACTCORPUS]` opcode census. The census function, its private
`corpusLogged` byte and the wrapper are DEBUG/TRACE-only; bringup retains the
historical interposition.

The real event-chain implementation is unchanged. Dialog preflight, resume,
rollback and synchronous bounded event-chain execution remain production-owned.

CI #1218:

```text
                         V18              V19           delta
static RAM               45072 B          45064 B         -8 B
linked Flash            762617 B         762021 B       -596 B
firmware.bin            762976 B         762384 B       -592 B
active __wrap_*              50               49           -1
```

Artifact `11155223388`:
`sha256:647af5bbbc5bda0f091b4a5de83ba05d961cd0774f5ed50a62cfda17d2df6e55`.
Firmware SHA-256:
`b394438989fcfcfa4c1d63e5142fc7e56c8c017aa7134af65344d7faf336fffb`.
ELF SHA-256:
`540660ede5a2d4e5621e60c2d03c473f7c660ac09041e919e5c548187e7a96ba`.

Final-ELF inspection proves exactly 49 active `__wrap_*` symbols.
`EspNativeGameplayEventChain_logCorpus`,
`__wrap_EspNativeResidentGameplay_service`, `corpusLogged` and all
`[INTERACTCORPUS]` strings are absent. The direct
`EspNativeResidentGameplay_service`, transition selection and transition
handoff symbols remain linked.

The real classic CYD validates cold boot, START, full intro/disposal, exact
Entrance first frame `71ca7465`, and:

```text
[ENGINESESSION] READY ... shapeData=0x0 mediaTexels=0x0
```

The same run commits MOVE and TURN, transforms the first crate, then opens and
fully resumes scientist dialog event 88 through opcode 19. ALIVE remains
stable before dialog at:

```text
heap=93448 heap8=27524 largest8=18420
```

and after the lazy dialog-chain owner allocation at:

```text
heap=92412 heap8=26488 largest8=18420
```

No `[INTERACTCORPUS]` line appears anywhere in the runtime transcript.

The hardware free-heap witnesses are all +8 B versus V18 before later lazy
allocations (for example MENU_MAIN `heap8=35720` versus 35712), matching the
8 B static-RAM reduction reported by CI.

Detailed record:
[MILESTONE_ESP32_CONSOLIDATION_INTERACTION_CORPUS_V19.md](MILESTONE_ESP32_CONSOLIDATION_INTERACTION_CORPUS_V19.md)

### Hot redraw success telemetry moved to TRACE V20 — REAL-CYD PASS (2026-10-01)

Hardware-tested code boundary: `552ec9c529d1bd31a656c9847b0b267bacd713fa`.

V20 moves repetitive success-only presentation telemetry out of the normal
INFO firmware without changing rendering, gameplay, FNV guards, rollback or
failure/recovery behavior.

The following success families are TRACE-only in normal production:

```text
[NATIVEFRAME] BSP / WALL
[WEAPON] DRAW
[FACINGLABEL] REFRESH / PAINT / CLEAR
[RESIDENTGAMEPLAY] FRAME
[ACTIONENGINE] FRAME
[ACTIONFEEDBACK] PAINT / CLEAR / REFRESH
[DOORANIM] FRAME
[DYNAMICLINES] FRAME
```

Important recovery/error witnesses remain visible, including
`NATIVEFRAME FAILED`, cache/read failures, `LEGACY_GUARD`, `RETRY`,
`RECOVERED`, `RESIDENTGAMEPLAY RENDER-FAILED`,
`ACTIONFEEDBACK FAILED`, `VIEWFLASH FAILED`, plus functional
`DOORANIM ARM/COMPLETE` boundaries.

CI #1220:

```text
                         V19              V20           delta
static RAM               45064 B          45064 B          0 B
linked Flash            762021 B         759197 B      -2824 B
firmware.bin            762384 B         759568 B      -2816 B
active __wrap_*              49               49            0
```

Artifact `11157510597`:
`sha256:f891d1f121611cade66ed43eb9064919fa2da5336d39ac27d8a97f924802aded`.
Firmware SHA-256:
`956d0593fa16366448736ee602eba321f217a94b106c4dbcf7286503afe976e8`.
ELF SHA-256:
`559f03b35ab5950ce4e9ee51c4fe4c128e81975c708001b6c7aa52952a3b4c10`.

The local hardware workstation reports the same 45064 B static RAM and the
known +16 B environment delta: 759213 B linked Flash / 759584 B firmware.bin.

The real classic CYD validates cold boot, full intro/disposal, exact Entrance
first frame `71ca7465`, `shapeData=0x0 mediaTexels=0x0`, repeated MOVE/TURN,
resource pickups and regular-door animation. The hot success lines above are
absent from the runtime transcript, while a real renderer recovery remains
visible and succeeds:

```text
[NATIVEFRAME] LEGACY_GUARD ...
[NATIVEFRAME] RETRY ...
[NATIVEFRAME] RECOVERED ...
```

Gameplay ALIVE remains stable at:

```text
heap=93448 heap8=27524 largest8=18420
```

The hardware interaction is subjectively much more responsive after the
cumulative logging consolidation. V20 specifically removes repeated serial
traffic from each world redraw; no renderer algorithm or presentation path was
otherwise optimized.

Detailed record:
[MILESTONE_ESP32_CONSOLIDATION_HOT_REDRAW_TELEMETRY_V20.md](MILESTONE_ESP32_CONSOLIDATION_HOT_REDRAW_TELEMETRY_V20.md)
