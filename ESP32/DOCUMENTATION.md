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
| [`MAP1_NATIVE_KEY_GATE.md`](MAP1_NATIVE_KEY_GATE.md) | pure EV_CHECK_KEY dynamic gate | #55 | `03c4275f2abfd6671c8bf499c075435d7b61ab97` |

Current candidate milestone:

- [`MAP1_NATIVE_PASSWORD_OWNER.md`](MAP1_NATIVE_PASSWORD_OWNER.md) — compact native owner for real `10 / EV_PASSWORD`: two immutable string refs (expected code + prompt), static pause/continuation provenance, 20-byte caller-owned state and 12-byte bounded submit result preserving correct/incorrect/empty outcomes plus the recovered 300 ms matched-length feedback delay. It performs no password presentation or legacy DoomCanvas/Game/Hud/world mutation. Firmware candidate `e2d12085712324444f26528b77ea5122c871d85b`; **IMPLEMENTED, REAL-CYD HARDWARE VALIDATION PENDING**.

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
 -> bounded pause/input owners
 -> native event/script loop
 -> explicit native world/render overlays
 -> ESP32-native gameplay + renderer
```

Do not promote desktop `Render_t`, `DoomCanvas_t`, pointer-heavy map structs, map-wide strings/texels, runtime ZIP access or legacy resource ownership into permanent requirements.

## Current recovery point

Latest merged hardware baseline:

```text
PR   = #55
main = 03c4275f2abfd6671c8bf499c075435d7b61ab97
hardware-tested firmware content = 3b4844e8fa5d38d522e1adc70ffac646978f130d
```

Current candidate:

```text
branch = agent/esp32-map1-native-password-owner
base   = 03c4275f2abfd6671c8bf499c075435d7b61ab97
firmware candidate content = e2d12085712324444f26528b77ea5122c871d85b
status = IMPLEMENTED; REAL-CYD HARDWARE VALIDATION PENDING
```

Hardware-proven fingerprints through PR #55:

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

Caller-local/value types proven through PR #55:

```text
status owner    =   8 B
dialog owner    =  12 B
notebook owner  = 514 B
key-gate result =  12 B
```

Current PASSWORD target:

```text
password owner         = 20 B expected
password submit result = 12 B expected
persistent heap        = 0 B expected
```

## PASSWORD recovered contract

Static opcode behavior:

```text
arg1 low byte  -> expected-code string ref
arg1 high byte -> prompt string ref
pause script
skip advance turn
save current event/command continuation
```

Submission behavior:

```text
max input payload = 7 B
matched expected length -> 300 ms feedback delay
early submitted shorter input -> 0 ms deliberate delay
correct -> "Correct code!" + resume at source offset + 1
non-empty wrong -> "Invalid code!" + no resume
empty wrong -> no forced message + no resume
```

The permanent owner never copies password/prompt text. The submit evaluator reads only the expected-code string from the native pack into caller scratch and returns metadata; it never mutates/presents legacy state.

## Hardware target for current candidate

The real PASSWORD count is discovered by the probe rather than guessed. Full source identity remains protected by `opcodeAuditFNV=6f28df45`.

Acceptance shape:

```text
refs > 0
stateExecRefused = refs
ownerBytes = 20
submitResultBytes = 12
resumeExact = refs
correct = refs
incorrect = refs
emptySemantics = refs
correctResume = refs
incorrectNoResume = refs
matchedLengthDelay = 300 ms
earlySubmitDelay = 0 ms
maxCodeLen <= 7
persistentHeapBytes = 0
```

Hardware must establish the real corpus counts/spans and new `passwordOwnerFNV` / `passwordSubmitFNV`; these are intentionally not predeclared.

Integrity witnesses include heap/largest/framebuffer, arena/map/script fingerprints, legacy notebook/keys, Hud witness, DoomCanvas password buffers/timing witness, `Game.passCode` pointer and Game continuation fields.

## Remaining MAP_INTRO opcode families after PASSWORD

If the candidate passes:

```text
2  EV_CHANGEMAP
7  EV_SHOW
9  EV_GIVEMAP
13 EV_UNLOCK
15 EV_OPENLINE
16 EV_CLOSELINE
18 EV_HIDE
27 EV_SAVEGAME
```

That exhausts the currently bounded non-world UI/control families. The next recovery should choose the first explicit native world/render overlay from SHOW/HIDE/GIVEMAP/UNLOCK/OPENLINE/CLOSELINE, while CHANGEMAP and SAVEGAME remain larger boundaries.

## Milestone workflow

1. Branch from exact latest hardware-validated `main`.
2. Keep one coherent bounded objective per branch.
3. Recover exact legacy semantics before designing native ownership.
4. Fail closed before unimplemented or unsafe effects.
5. Validate normal optimized firmware on the real classic CYD.
6. Preserve exact RAM/fingerprint/hardware evidence.
7. Mark merge-ready only after implementation + hardware + docs agree.
8. Keep all post-hardware commits docs-only unless another flash is performed.

Current validation target: normal `esp32-cyd` from `agent/esp32-map1-native-password-owner`, including `[MAPPASSWORD]` / `[MAPPASSWORDPROBE]` plus a later stable `[ALIVE]` heartbeat.

After PASS + merge, reread the new `main`, `PORTING_STATUS.md`, merged PASSWORD milestone and exact remaining MAP_INTRO behavior before selecting the first world/render overlay milestone.
