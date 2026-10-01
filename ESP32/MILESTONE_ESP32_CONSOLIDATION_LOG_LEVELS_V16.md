# Milestone ESP32 consolidation — compile-time log levels V16

Date: 2026-10-01  
Branch: `agent/esp32-consolidation-log-levels-v16`  
Base main: `8c208baf2d7e55fc84f13bb59a5b2479703837fd`  
Hardware-tested code boundary: `5a320d50a5f88646382db1211114a31b671b34b2`  
Status: **REAL-CYD PASS**

## Goal

Reduce normal-firmware Serial spam without removing behavior, hiding meaningful
failures or confusing instrumentation with functional ownership.

## Implementation

`doomrpg_log.h` defines compile-time ERROR/INFO/DEBUG/TRACE levels and
`DRPG_LOGE/I/D/T`. Normal `esp32-cyd` defaults to INFO;
`esp32-cyd-bringup` explicitly selects TRACE. No runtime allocation, parser,
persistence or console is added.

Migrated hot diagnostics:

- VIDEO per-present timing -> TRACE.
- PAKIO SAMPLE and its readRange profiling state -> TRACE-only compilation.
- PLANEPROFILE and successful NATIVEPLANE summary -> TRACE.
- SPRITEPROFILE per-frame summary -> TRACE; stack-owner census -> DEBUG.
- expected RNG WORD-OOB-AVOIDED -> TRACE; probe/replay witnesses -> DEBUG;
  conflicts/FATAL -> ERROR.
- CRATESTATE WITNESS/success census -> DEBUG; OOM/FAILED -> ERROR; READY -> INFO.
- detailed INTERACTMAP/GIVEMAPTRACE inventory -> DEBUG.

Functional wrappers are preserved:
`__wrap_Esp32PlatformVideo_present`, `__wrap_Render_initColumnScale`,
`__wrap_Render_cullBoundingBox` and all four `__wrap_EspAssetPack_*` seams.

## CI / ELF

CI #1209 SUCCESS:

```text
static RAM       45080 B
linked Flash    767581 B
firmware.bin    767952 B
artifact id     11151256017
artifact digest sha256:25394e93759a5f96ef8ff058ee0d6b5f6f368cfe00930aafebd0464b5d6f3762
firmware sha256 acc7838bf3df8421c230f29b9f3cdedd697800b71632bf699fc8b35426a20d63
ELF sha256      d0276a7f94cc63e7c41211728ac728495904116cc7b74808940fa7db523c9663
```

Versus V15/main: RAM -40 B, linked Flash -4580 B, firmware.bin -4576 B.
Final `nm` reports 51 wrappers, the exact same set as V15/main.

The INFO ELF lacks all targeted hot strings:
VIDEO Present, PAKIO SAMPLE, PLANEPROFILE, successful NATIVEPLANE rows,
SPRITEPROFILE summaries, RNG WORD-OOB-AVOIDED, CRATESTATE WITNESS and
INTERACTMAP OPCODE. Failure/operational strings remain.

## Real-CYD PASS

The normal firmware was built/flashed on the classic CYD. The local build
reports the same 45080 B static RAM and a harmless 16-byte size difference from
CI (767597 B linked Flash / 767968 B firmware image).

Observed hardware path:

```text
cold boot
 -> native MENU_MAIN
 -> START
 -> intro + disposal
 -> Entrance resident bootstrap
 -> ENGINESESSION FIRST_FRAME frame=71ca7465
 -> ENGINESESSION READY shapeData=0x0 mediaTexels=0x0
 -> movement / turns
 -> crate transform + pickup
 -> dialog close + script resume
 -> door open/close animation
 -> multiple ALIVE witnesses
```

None of the targeted TRACE spam appears in the runtime transcript. Remaining
verbose NATIVEFRAME, INTERACTCORPUS and semantic gameplay families are outside
V16 and remain candidates for later classification.

## Result

V16 meets its bounded objective: normal Serial output is materially quieter,
hardware truth remains visible, functional ownership is unchanged, and future
wrapper/consolidation audits can proceed from the same 51-wrapper boundary.
