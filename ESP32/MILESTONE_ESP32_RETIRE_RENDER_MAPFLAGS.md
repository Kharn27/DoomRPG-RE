# Milestone — Retire legacy Render_t::mapFlags in production

Status: **REAL-CYD PASS**

Hardware-tested runtime SHA:
\`2d58cdd52bf74242c35b4a196d388ab802ff573c\`

Branch:
\`agent/esp32-render-mapflags-retirement\`

Base \`main\`:
\`02baa3bddd3b52c2399e33725aa4147a3840ad6e\`

## Boundary and rationale

The source-closed legacy parser \`Render_beginLoadMap*\` still retained
the 1024-byte \`Render_t.mapFlags\` block-map mirror even though normal
CYD startup skips the old parser and native gameplay uses the compact
immutable \`EspMapRuntime\` plus native mutable \`EspMapState\` and
\`EspMapAutomapState\`. The original desktop BSP parser is retained only
for desktop/bringup diagnostic compatibility.

The ESP32 generator asserts the exact legacy source shape before
replacing the normal-firmware loader entry points with explicit false
returns. Every additional \`mapFlags\` consumer fails generation until
audited. The old parser remains behind a desktop/bringup preprocessor
boundary, while \`Render.h\` omits its 1024-byte storage in the normal
firmware. \`render_startup_bridge.c\` asserts \`sizeof(Render_t)==4016\`
only for that normal profile. No pointer-heavy map state or RAM buffer
replaces the removed field.

## CI and code SHA

Normal environment: \`esp32-cyd\`; CI #1675 SUCCESS on
\`2d58cdd52bf74242c35b4a196d388ab802ff573c\`.

\`\`\`text
RAM:   45056 B
Flash: 772441 B
esp32-cyd SUCCESS
\`\`\`

## Real-CYD acceptance

\`\`\`text
Engine structs: Render=4016 Game=4 Canvas=44 Total=4824 bytes
[CORE] Render         used=4032 heap=183164
[CORE] READY objects=5 heap used=5004 remaining=182632 largest=110580
\`\`\`

Compared with the immediately preceding 5040-byte Render / 44-byte
Canvas baseline, the exact +1024 B gain persists:

| Checkpoint | Before heap8 | This milestone heap8 | Gain |
| --- | ---: | ---: | ---: |
| CORE READY | 181608 | 182632 | +1024 |
| LAYOUT | 175000 | 176024 | +1024 |
| mappings resident | 155840 | 156864 | +1024 |
| fresh gameplay ALIVE | 114788 | 115812 | +1024 |
| Exit->Menu | 160684 | 161708 | +1024 |

Runtime state and render witnesses:

\`\`\`text
[MAINOPTIONS] framebufferFNV=162d3999 ... shapeData=0x0 mediaTexels=0x0
[OPTIONBACK] FAST End framebufferFNV=522dc605 expected=522dc605
[MAINHELP] READY back=native mainFNV=522dc605 touch=armed
[INTRO1] READY one deterministic ST_INTRO frame presented once FNV=ade0195d
[INTRODISP] READY ... recovered=34060 ... owner=NULL assets=NULL texts=NULL noMapLoad=yes
[MAPRT] READY arenaBytes=14095 ... arenaFNV=c3882516
[ENGINESESSION] FIRST_FRAME map=1 angle=64 frame=71ca7465 walls=8 pixels=4430 presented=1
[ENGINESESSION] READY ... shapeData=0x0 mediaTexels=0x0
[NATIVEFRAME] LEGACY_GUARD ...
[NATIVEFRAME] RETRY ...
[NATIVEFRAME] RECOVERED ...
[DOORANIM] COMPLETE transitions=1 frames=4 state=stable transaction=committed
[RESIDENTRESET] heap8=143700->161708 released=18008 ... after=0/0/0/0/0/0/0 empty=1
[SYSEXIT] MENU-READY frame=522dc605 session=off resident=empty saveWrite=no checkpoint=unchanged
\`\`\`

Real-CYD input includes OPTIONS/Back, HELP page-down/page-up/Back,
fresh prologue, native MAP_INTRO, movement, turning, automap discovery,
crate attack/transform, two Armor Shard pickups, an animated door,
HUB SYS and confirmed Exit To Menu. No unexpected checkpoint write.

Checkpoint LOAD was not re-exercised on this exact SHA and is therefore
not asserted as a hardware acceptance path here. No code commit is
included after the hardware-tested runtime SHA within this milestone
closure. Continue further Render cuts as distinct milestones with
their own CI and real-CYD verification.

