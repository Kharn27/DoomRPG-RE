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
| [`MAP1_NATIVE_RUNTIME.md`](MAP1_NATIVE_RUNTIME.md) | compact immutable map arena | #43 | `503fdd66fae625a45446fb4ea0853abc71d7dda3` |
| [`MAP1_NATIVE_ACCESS.md`](MAP1_NATIVE_ACCESS.md) | allocation-free compact accessors | #44 | `ddcf19e6166f210a6f63fec1c608234ee3e253ea` |
| [`MAP1_NATIVE_STATE.md`](MAP1_NATIVE_STATE.md) | compact mutable tile state | #45 | `feec8a7fcb839dbd9f6de708f56f26b69a1e79e9` |
| [`MAP1_NATIVE_EVENTS.md`](MAP1_NATIVE_EVENTS.md) | allocation-free tile -> event lookup | #46 | `438cffabaaaaa3dc3b45486f56eacec1a047edcf` |
| [`MAP1_NATIVE_EVENT_DESCRIPTOR.md`](MAP1_NATIVE_EVENT_DESCRIPTOR.md) | event descriptor + exact bytecode linkage | #47 | `a3e629ba0be6b4dcc6329b17f18a0c3ca9828958` |
| [`MAP1_NATIVE_EVENT_FILTER.md`](MAP1_NATIVE_EVENT_FILTER.md) | mutable script state + side-effect-free event filtering | #48 | `0c8a52549ebb436139f7cd5c8b4ee63bdd175907` |
| [`MAP1_NATIVE_OPCODE_EXEC1.md`](MAP1_NATIVE_OPCODE_EXEC1.md) | opcode audit + first real native state mutation/rollback | #49 | `6e43ef059db52783b7264e84579216cb2572a1e2` |
| [`MAP1_NATIVE_UI_INTENT.md`](MAP1_NATIVE_UI_INTENT.md) | allocation-free string spans + UI intents | #50 | `9a5e8ade361180d220f2b3614a443e5efb0d27bd` |
| [`MAP1_NATIVE_STRING_READER.md`](MAP1_NATIVE_STRING_READER.md) | bounded native-pack string reads | #51 | `526640b12d978fdbe9c8a9239c37db2fca95cddd` |
| [`MAP1_NATIVE_STATUS_MESSAGE.md`](MAP1_NATIVE_STATUS_MESSAGE.md) | first native effect owner: FORCE_MESSAGE status ref | #52 | `40b61af5e2115266d4d03dddcc3175850538b0f5` |
| [`MAP1_NATIVE_DIALOG_OWNER.md`](MAP1_NATIVE_DIALOG_OWNER.md) | DIALOG/NOBACK pause + static continuation owner | #53 | `395418510207bf24ac45ddbb4c4c15db3ddc8998` |
| [`MAP1_NATIVE_NOTEBOOK.md`](MAP1_NATIVE_NOTEBOOK.md) | bounded EV_NOTE native notebook owner | #54 | `03002f79eb03bdcb4c9e430c43e4693dab47e44b` |

Current merge-ready milestone:

- [`MAP1_NATIVE_KEY_GATE.md`](MAP1_NATIVE_KEY_GATE.md) — pure native evaluator for real `41 / EV_CHECK_KEY`. Real CYD proved one yellow-key command (`cmd38 event11 off0`, mask `02`), the complete 16-context truth table (`8 PASS / 8 BLOCKED`), 12-byte caller-local result, exact four message mappings, ignored high key bits, full fail-closed behavior, zero persistent allocation/PAK I/O, unchanged Player/Hud/Game/world state, `keyGateFNV=9ace79cd`, and stable post-PARK recovery. Hardware-tested firmware `3b4844e8fa5d38d522e1adc70ffac646978f130d`; **REAL-CYD HARDWARE PASS / MERGE-READY**.

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
 -> compact native effect intents
 -> bounded pack-backed string/text access
 -> small explicit native effect/player owners
 -> pure dynamic gates
 -> native event/script loop
 -> ESP32-native gameplay + renderer
```

Do not promote desktop `Render_t`, `DoomCanvas_t`, pointer-heavy map structs, map-wide strings/texels, runtime ZIP access or legacy resource ownership into permanent requirements.

## Current recovery point

Latest merged hardware baseline:

```text
PR   = #54
main = 03002f79eb03bdcb4c9e430c43e4693dab47e44b
hardware-tested firmware content = f619aefc85402d28c4de6edab5ca32ea1eb514dd
```

Current merge-ready branch:

```text
branch = agent/esp32-map1-native-key-gate
base   = 03002f79eb03bdcb4c9e430c43e4693dab47e44b
hardware-tested firmware content = 3b4844e8fa5d38d522e1adc70ffac646978f130d
status = REAL-CYD HARDWARE PASS / MERGE-READY
```

Hardware-proven fingerprints now include:

```text
arenaFNV           = c3882516
decodedFNV         = a426dd18
mapStateFNV        = cd99b98e
lookupFNV          = 63430151
descriptorFNV      = 27115328
linkageFNV         = 5727902c
scriptFNV          = f9e3d9df
filterFNV          = a5923b21
resumeFNV          = b98452da
opcodeAuditFNV     = 6f28df45
firstExecFNV       = 646b565c
stringSpanFNV      = 713188eb
uiIntentFNV        = 7fdd6a79
stringContentFNV   = e995ee51
statusApplyFNV     = 52b25a5f
dialogApplyFNV     = d0254f3d
noteApplyFNV       = 43183162
notebookContentFNV = 599609e0
notebookStorageFNV = 75cf54e0
keyGateFNV         = 9ace79cd
```

Persistent native structural/script heap remains hardware-proven at `15252 B`.

Caller-local/value types proven:

```text
status owner   =   8 B
dialog owner   =  12 B
notebook owner = 514 B
key-gate result=  12 B
```

CHECK_KEY hardware proof:

```text
refs             = 1
green/yellow/blue/red = 0/1/0/0
scenarios        = 16
pass             = 8
blocked          = 8
stateExecRefused = 1
resultBytes      = 12
keyGateFNV       = 9ace79cd
sample           = cmd38 event11 off0 key1 mask02 arg2=00000100
message          = Need Yellow Key
sound            = 5065
```

Integrity on the tested firmware:

```text
heap8             = 68756 -> 68756
largest8          = 36852 -> 36852
frameFNV          = c56f998b -> c56f998b
arenaFNV          = c3882516 -> c3882516
mapStateFNV       = cd99b98e -> cd99b98e
scriptFNV         = f9e3d9df -> f9e3d9df
legacyNotebookFNV = 4d7705c5 -> 4d7705c5
legacyKeys        = 00000000 -> 00000000
hudFNV            = 505b1255 -> 505b1255
persistentBytes   = 0
```

Complete post-PARK heartbeat: `uptime=25410 ms`, `heap=134520`, `heap8=68756`, `largest8=36852`, all reported subsystems ready.

## Remaining MAP_INTRO opcode families

```text
2  EV_CHANGEMAP
7  EV_SHOW
9  EV_GIVEMAP
10 EV_PASSWORD
13 EV_UNLOCK
15 EV_OPENLINE
16 EV_CLOSELINE
18 EV_HIDE
27 EV_SAVEGAME
```

PASSWORD remains a bounded pause/input candidate. SHOW/HIDE/GIVEMAP/UNLOCK/OPENLINE/CLOSELINE cross into explicit world/render overlays. CHANGEMAP and SAVEGAME remain larger later boundaries.

## Milestone workflow

1. Branch from exact latest hardware-validated `main`.
2. Keep one coherent bounded objective per branch.
3. Recover exact legacy semantics before designing native ownership.
4. Fail closed before unimplemented or unsafe effects.
5. Validate normal optimized firmware on the real classic CYD.
6. Preserve exact RAM/fingerprint/hardware evidence.
7. Mark merge-ready only after implementation + hardware + docs agree.
8. Keep all post-hardware commits docs-only unless another flash is performed.

After this branch is merged, reread the new `main`, `PORTING_STATUS.md`, merged CHECK_KEY milestone and exact remaining MAP_INTRO behavior before selecting the next bounded milestone.
