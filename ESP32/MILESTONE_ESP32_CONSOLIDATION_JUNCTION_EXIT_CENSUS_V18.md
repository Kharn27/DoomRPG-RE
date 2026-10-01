# Milestone ESP32 consolidation — Junction exit census V18

Date: 2026-10-01  
Branch: `agent/esp32-consolidation-interaction-diagnostics-v17`  
Base for V18: V17 docs head `29ce07620d3439af75d9730629aeff91e8c167a3`  
Hardware-tested code boundary: `6733395845c289fa9f69cc6c61001c3a68f2d72d`  
Status: **REAL-CYD PASS**

## Goal

Remove the historical Junction exit SAVEGAME/CHANGEMAP recovery census from
normal INFO firmware while preserving the real native transition engine.

## Implementation

`esp_native_gameplay_session.c` calls
`EspNativeGameplayTransition_probeJunctionExitCensus()` only when
`DOOMRPG_LOG_LEVEL >= DOOMRPG_LOG_DEBUG`.

`esp_native_gameplay_transition.c` likewise compiles the private
`junctionExitCensusDone` owner, its reset and the full census function only at
DEBUG/TRACE.

The normal `esp32-cyd` image therefore has no census code or strings.
`esp32-cyd-bringup` remains TRACE and keeps the diagnostic available.

No production transition behavior changed.

## Ownership audit

The census only:

- checked whether the current map was Junction;
- opened the PAK read-only;
- enumerated event descriptors and commands;
- decoded SAVEGAME / CHANGEMAP string targets;
- compared the script fingerprint before and after;
- printed a recovery corpus;
- set one private completion byte.

None of its outputs feed transition selection, commit, stats pause, spawn or
handoff.

Permanent owners remain:

```text
EspNativeGameplayTransition_trySelect
EspNativeGameplayTransitionHandoff_service
```

## CI / ELF

CI #1216: **SUCCESS**

```text
static RAM       45072 B
linked Flash    762617 B
firmware.bin    762976 B
artifact id     11154118496
artifact digest sha256:fa4e82a7305596cfa36ca65897e3802573309db723d15e22e7993636d51d0ec6
firmware sha256 fccb055e5d3019612650f8740453480a8f0ad9e418ee5e709031d2b1024d6db0
ELF sha256      a8197c29053631f9dcd22968c331eb868f8c3606efca33ec598ac610ea24b8ec
active wraps    50
```

Versus V17:

```text
static RAM       45072 -> 45072 B       0 B
linked Flash    764757 -> 762617 B   -2140 B
firmware.bin    765120 -> 762976 B   -2144 B
active wraps        50 -> 50             0
```

Final normal ELF:

```text
EspNativeGameplayTransition_probeJunctionExitCensus  ABSENT
junctionExitCensusDone                              ABSENT
[JUNCTIONEXITCENSUS] strings                        ABSENT
EspNativeGameplayTransition_trySelect               PRESENT
EspNativeGameplayTransitionHandoff_service          PRESENT
```

The local hardware workstation reports 45072 B RAM / 762633 B linked Flash /
762992 B firmware.bin, exactly +16 B versus CI on the two flash/image sizes.

## Real-CYD validation

Validated path:

```text
cold boot
 -> native MENU_MAIN
 -> START
 -> full intro
 -> bounded disposal
 -> Entrance bootstrap
 -> FIRST_FRAME 71ca7465
 -> ENGINESESSION READY shapeData=0x0 mediaTexels=0x0
 -> committed FORWARD
 -> committed TURN
 -> crate subtype-2 transform
 -> Armor Shard pickup x2
 -> regular door 4-frame open
 -> move through doorway
 -> regular door 4-frame auto-close
 -> stable ALIVE
```

Stable gameplay witness:

```text
[ALIVE] ... heap=93444 heap8=27520 largest8=18420 ...
```

No `[JUNCTIONEXITCENSUS]` output appears.

## Result

V18 is hardware-valid. The normal product image no longer contains the
historical Junction exit recovery census, while bringup retains it and the
native production transition/handoff path remains unchanged.

The next bounded consolidation candidate is the still-visible
`[INTERACTCORPUS]` one-shot interaction corpus. It must be audited separately
before any gating/removal.
