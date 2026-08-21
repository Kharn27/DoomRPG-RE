# ESP32 documentation map

This file defines how the ESP32 CYD port documentation is organized and which file is authoritative for each kind of information.

## Source-of-truth hierarchy

### `README.md` — stable port guide

Use [`README.md`](README.md) for build/flash instructions, target hardware, stable architecture, SD/native-pack layout, touch model and high-level execution flow.

### `PORTING_STATUS.md` — authoritative current recovery point

Use [`PORTING_STATUS.md`](PORTING_STATUS.md) when resuming development. It owns the latest merged baseline, active branch, exact RAM/state boundary, current native contracts and next bounded milestone.

### Milestone archives — detailed evidence

Merged milestones:

| Archive | Purpose | PR | Merged `main` |
| --- | --- | ---: | --- |
| [`START_GAME.md`](START_GAME.md) | fresh Start action + lifecycle cleanup | #36 | `5275e4a1c6eca703b51221e80f3b199178015a01` |
| [`FIRST_INTRO_FRAME.md`](FIRST_INTRO_FRAME.md) | first fitted `ST_INTRO` frame | #37 | `b934e21c7f2dbf6463a4d2dfa13d1e06614e2b96` |
| [`INTRO_CLOCK.md`](INTRO_CLOCK.md) | bounded intro clock | #38 | `58edfe5d7080a7e9e64ff5b516697ddf3cca31da` |
| [`INTRO_INPUT.md`](INTRO_INPUT.md) | intro input progression | #39 | `7ba68955a9b0979924c5e759736fb483589be744` |
| [`INTRO_DISPOSE.md`](INTRO_DISPOSE.md) | intro teardown + RAM recovery | #41 | `897e982f4b37039d984b13265beaa68a83dce98b` |
| [`MAP1_STRUCTURAL_LOAD.md`](MAP1_STRUCTURAL_LOAD.md) | native BSP inventory + compact plan | #42 | `c71ac1fb07c2e281bc3f8a70c102dd22c7b9300e` |
| [`MAP1_NATIVE_RUNTIME.md`](MAP1_NATIVE_RUNTIME.md) | 14,095-byte immutable arena | #43 | `503fdd66fae625a45446fb4ea0853abc71d7dda3` |
| [`MAP1_NATIVE_ACCESS.md`](MAP1_NATIVE_ACCESS.md) | allocation-free compact accessors | #44 | `ddcf19e6166f210a6f63fec1c608234ee3e253ea` |
| [`MAP1_NATIVE_STATE.md`](MAP1_NATIVE_STATE.md) | 1,024-byte mutable tile state | #45 | `feec8a7fcb839dbd9f6de708f56f26b69a1e79e9` |
| [`MAP1_NATIVE_EVENTS.md`](MAP1_NATIVE_EVENTS.md) | allocation-free tile -> event lookup | #46 | `438cffabaaaaa3dc3b45486f56eacec1a047edcf` |
| [`MAP1_NATIVE_EVENT_DESCRIPTOR.md`](MAP1_NATIVE_EVENT_DESCRIPTOR.md) | event descriptor + exact bytecode linkage | #47 | `a3e629ba0be6b4dcc6329b17f18a0c3ca9828958` |
| [`MAP1_NATIVE_EVENT_FILTER.md`](MAP1_NATIVE_EVENT_FILTER.md) | 81-byte mutable script state + side-effect-free Game_runEvent filtering | #48 | `0c8a52549ebb436139f7cd5c8b4ee63bdd175907` |
| [`MAP1_NATIVE_OPCODE_EXEC1.md`](MAP1_NATIVE_OPCODE_EXEC1.md) | full MAP_INTRO opcode inventory + first real native EV_NEXTSTATE execution/rollback | #49 | `6e43ef059db52783b7264e84579216cb2572a1e2` |
| [`MAP1_NATIVE_UI_INTENT.md`](MAP1_NATIVE_UI_INTENT.md) | 94 allocation-free string spans + all real UI/string bytecodes translated to caller-owned intents | #50 | `9a5e8ade361180d220f2b3614a443e5efb0d27bd` |

Current merge-ready milestone:

- [`MAP1_NATIVE_STRING_READER.md`](MAP1_NATIVE_STRING_READER.md) — bounded `EspMapStrings_read()` over `/DoomRPG-ESP32.pak`, one canonical `EspMapStringRef` into caller-owned storage. Real CYD: 94/94 prefix checks, 94/94 guard checks, `stringContentFNV=e995ee51`, 93 non-empty payload reads, all fail-closed paths proven, 4364 B transient PAK-open cost fully recovered, three stable post-PARK heartbeats; **REAL-CYD HARDWARE PASS / MERGE-READY**.

## Architecture rule

DoomRPG-RE is the executable specification/reference, **not the permanent engine architecture**.

Long-term direction:

```text
Doom RPG data / recovered behavior
 -> native pack-backed parsers
 -> compact immutable native map
 -> allocation-free accessors
 -> small explicit mutable spatial/script state
 -> native event lookup + descriptor/linkage
 -> side-effect-free event filtering
 -> fail-closed native opcode execution
 -> compact native effect intents/owners
 -> bounded pack-backed string/text access
 -> ESP32-native gameplay + renderer
```

Do not promote desktop `Render_t`, `DoomCanvas_t`, pointer-heavy map structs, map-wide strings/texels, runtime ZIP access or legacy resource ownership into permanent requirements.

## Current recovery point

Latest merged hardware baseline:

```text
PR   = #50
main = 9a5e8ade361180d220f2b3614a443e5efb0d27bd
hardware-tested firmware content = 045b219dd7d6d06630eb446424e8d3d3fa3d249e
```

Current merge-ready branch:

```text
branch = agent/esp32-map1-native-string-reader
base   = 9a5e8ade361180d220f2b3614a443e5efb0d27bd
hardware-tested firmware content = d13d5eb13c4657d5ec5c16fd82939cfc38989c86
status = REAL-CYD HARDWARE PASS / MERGE-READY
```

Hardware-proven fingerprints now include:

```text
arenaFNV         = c3882516
decodedFNV       = a426dd18
mapStateFNV      = cd99b98e
lookupFNV        = 63430151
descriptorFNV    = 27115328
linkageFNV       = 5727902c
scriptFNV        = f9e3d9df
filterFNV        = a5923b21
resumeFNV        = b98452da
opcodeAuditFNV   = 6f28df45
firstExecFNV     = 646b565c
stringSpanFNV    = 713188eb
uiIntentFNV      = 7fdd6a79
stringContentFNV = e995ee51
```

Persistent native map/world/script ownership remains:

```text
15252 B
```

The string-span, UI-intent and bounded string-reader layers each add **0 persistent bytes**.

String reader hardware proof:

```text
94 strings
7779 payload bytes
1 zero-length string
0 embedded source NUL bytes
max payload = 313 B
94/94 on-disk length prefixes match
94/94 caller-buffer guards preserved
93 non-empty PAK payload reads
spanFNV    = 713188eb
contentFNV = e995ee51
```

Canonical fixture payload fingerprints:

```text
string 1  = f6da01bb
string 25 = 84f743cf
string 30 = 3692ac94
string 85 = ee639dc1
```

Transient storage proof:

```text
heap8 before/open/after = 68804 / 64440 / 68804
PAK-open transient cost = 4364 B
largest8                = 36852 preserved
persistentBytes         = 0
```

Integrity remained exact through the reader stage:

```text
frameFNV    = 805df09e -> 805df09e
arenaFNV    = c3882516 -> c3882516
mapStateFNV = cd99b98e -> cd99b98e
scriptFNV   = f9e3d9df -> f9e3d9df
notebookFNV = 4d7705c5 -> 4d7705c5
entities    = 0
monsters    = 0
ST_PLAYING  = no
```

Three post-PARK `[ALIVE]` heartbeats remained stable at `heap=134568`, `heap8=68804`, `largest8=36852` with all reported subsystems ready.

## Milestone workflow

1. Branch from exact latest hardware-validated `main`.
2. Keep one coherent bounded objective per branch.
3. Recover exact legacy semantics before designing native ownership.
4. Fail closed before unimplemented or unsafe effects.
5. Validate normal optimized firmware on the real classic CYD.
6. Preserve exact RAM/fingerprint/hardware evidence.
7. Mark merge-ready only after implementation + hardware + docs agree.
8. Keep all post-hardware commits docs-only unless another flash is performed.

After this branch is merged, the next bounded milestone should introduce only one small explicit native text/effect owner consuming the already-proven `EspMapUiIntent` plus bounded `EspMapStrings_read()` contract. Keep world/entity/render mutation out of that boundary.

See [`PORTING_STATUS.md`](PORTING_STATUS.md), merged [`MAP1_NATIVE_UI_INTENT.md`](MAP1_NATIVE_UI_INTENT.md), active [`MAP1_NATIVE_STRING_READER.md`](MAP1_NATIVE_STRING_READER.md), and merged [`MAP1_NATIVE_OPCODE_EXEC1.md`](MAP1_NATIVE_OPCODE_EXEC1.md).
