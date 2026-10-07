# Milestone — Compact DoomCanvas dormant text payloads

Status: **REAL-CYD PASS**

Hardware-tested code boundary:
`ccad65e7f56a05a7f5d2e0c6e4768ce0a364d454`

Branch:
`agent/esp32-doomcanvas-bridge`

## Goal

Remove the remaining large desktop-only text payloads from permanent
`DoomCanvas_t` storage, then make the explicit bridge ABI match the linked ELF
after the old score helper becomes unreachable.

## Retired storage

```text
epilogueText[2][150] = 300 B
printMsg[128]         = 128 B
total                 = 428 B
```

The desktop epilogue renderer/run-loop is not compiled on ESP32. The bridge
retains the lifecycle side effects of the dormant epilogue hook but no longer
stores text pages that no current renderer can consume. A future visible
epilogue must use the native UI/string architecture.

No compiled ESP32 code writes `printMsg`; the compatibility LOADING/SAVING
branch therefore always selected its fallback `"Processing..."`. The fixed
fallback remains while the unused 128-byte buffer is removed.

No replacement 428-byte BSS/static buffer is introduced.

## ABI narrowing

Once epilogue text generation disappeared,
`DoomCanvas_getOverall()` had no remaining caller. ELF inspection showed only
14 linked `DoomCanvas_*` symbols. The bridge implementation, header exposure
for ESP32, build-time whitelist and boot witness were narrowed accordingly from
15 to 14 source ABI exports.

## Layout

```text
desktop DoomCanvas_t                  3740 B
after dialog-store retirement         1180 B
after zero-reference field retirement  940 B
after dormant text retirement          512 B
total reclaimed                       3228 B (~86.3%)
```

The compact layout remains pinned by `_Static_assert`.

## CI

Normal `esp32-cyd` CI #1646: **SUCCESS**.

```text
RAM:   45464 B
Flash: 779181 B
esp32-cyd SUCCESS
```

## Real-CYD acceptance

Boot:

```text
Engine structs: Render=5040 Game=4 Canvas=512 Total=6316 bytes
[DOOMCANVASBRIDGE] INIT exports=14 desktopTU=no bytes=512 retiredDialogStores=2560 retiredZeroRefLayout=240 retiredDormantText=428...
[CORE] DoomCanvas     used=528 heap=186320 largest=110580
```

The previous 940-byte object consumed 956 bytes including allocator overhead.
The new 512-byte object consumes 528 bytes. The hardware allocator improvement
is exactly 428 bytes.

The same +428-byte recovery is visible at stable checkpoints:

```text
CORE READY       180304 -> 180732
LAYOUT           162720 -> 163148
mappings         143560 -> 143988
fresh LAZY_POST  103320 -> 103748
fresh ALIVE      102508 -> 102936
```

Fresh gameplay covers intro, exact disposal, map load, crate combat/removal,
pickups, regular/deferred doors, an opcode-26 native dialog and chained state
resume, a standalone weapon-help dialog, HUB/System and clean Exit To Menu.

The first-frame regression witness remains:

```text
[ENGINESESSION] FIRST_FRAME map=1 angle=64 frame=71ca7465 walls=8 pixels=4430 presented=1
```

The checkpoint path loads version 11, restores the native world/session,
accepts resumed actions, activates a monster through door visibility, completes
the ordered attack visual and commits retaliation:

```text
[MONSTERRETAL] COMMIT ... playerHP=34->32 armor=19->17 ... rollback=closed
```

`shapeData` and `mediaTexels` remain NULL throughout.

The known compact-renderer guard recovery remains accepted and unrelated.

## Note

The current boot log lacks one separator between
`retiredDormantText=428` and `clip=160x120`. This is cosmetic only and will
be corrected with a later code change, not by modifying the hardware-tested
boundary.

## Closure

Hardware-tested code ends at
`ccad65e7f56a05a7f5d2e0c6e4768ce0a364d454`.

This closure commit is documentation-only.
