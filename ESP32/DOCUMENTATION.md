# ESP32 documentation map

This file defines the current ESP32 CYD documentation map.

## Source of truth

- [`README.md`](README.md): stable build/flash guide.
- [`PORTING_STATUS.md`](PORTING_STATUS.md): authoritative current recovery point.
- Milestone archives: detailed implementation and hardware evidence.

## Merged milestones

| Archive | Purpose | PR | Merged `main` |
| --- | --- | ---: | --- |
| [`START_GAME.md`](START_GAME.md) | start/lifecycle cleanup | #36 | `5275e4a1c6eca703b51221e80f3b199178015a01` |
| [`FIRST_INTRO_FRAME.md`](FIRST_INTRO_FRAME.md) | first fitted intro frame | #37 | `b934e21c7f2dbf6463a4d2dfa13d1e06614e2b96` |
| [`INTRO_CLOCK.md`](INTRO_CLOCK.md) | bounded intro clock | #38 | `58edfe5d7080a7e9e64ff5b516697ddf3cca31da` |
| [`INTRO_INPUT.md`](INTRO_INPUT.md) | intro input progression | #39 | `7ba68955a9b0979924c5e759736fb483589be744` |
| [`INTRO_DISPOSE.md`](INTRO_DISPOSE.md) | intro teardown | #41 | `897e982f4b37039d984b13265beaa68a83dce98b` |
| [`MAP1_STRUCTURAL_LOAD.md`](MAP1_STRUCTURAL_LOAD.md) | BSP inventory | #42 | `c71ac1fb07c2e281bc3f8a70c102dd22c7b9300e` |
| [`MAP1_NATIVE_RUNTIME.md`](MAP1_NATIVE_RUNTIME.md) | compact immutable runtime | #43 | `503fdd66fae625a45446fb4ea0853abc71d7dda3` |
| [`MAP1_NATIVE_ACCESS.md`](MAP1_NATIVE_ACCESS.md) | allocation-free access | #44 | `ddcf19e6166f210a6f63fec1c608234ee3e253ea` |
| [`MAP1_NATIVE_STATE.md`](MAP1_NATIVE_STATE.md) | mutable tile state | #45 | `feec8a7fcb839dbd9f6de708f56f26b69a1e79e9` |
| [`MAP1_NATIVE_EVENTS.md`](MAP1_NATIVE_EVENTS.md) | tile-event lookup | #46 | `438cffabaaaaa3dc3b45486f56eacec1a047edcf` |
| [`MAP1_NATIVE_EVENT_DESCRIPTOR.md`](MAP1_NATIVE_EVENT_DESCRIPTOR.md) | event descriptor/linkage | #47 | `a3e629ba0be6b4dcc6329b17f18a0c3ca9828958` |
| [`MAP1_NATIVE_EVENT_FILTER.md`](MAP1_NATIVE_EVENT_FILTER.md) | script state/filtering | #48 | `0c8a52549ebb436139f7cd5c8b4ee63bdd175907` |
| [`MAP1_NATIVE_OPCODE_EXEC1.md`](MAP1_NATIVE_OPCODE_EXEC1.md) | first state opcode execution | #49 | `6e43ef059db52783b7264e84579216cb2572a1e2` |
| [`MAP1_NATIVE_UI_INTENT.md`](MAP1_NATIVE_UI_INTENT.md) | UI/string intents | #50 | `9a5e8ade361180d220f2b3614a443e5efb0d27bd` |
| [`MAP1_NATIVE_STRING_READER.md`](MAP1_NATIVE_STRING_READER.md) | bounded pack strings | #51 | `526640b12d978fdbe9c8a9239c37db2fca95cddd` |
| [`MAP1_NATIVE_STATUS_MESSAGE.md`](MAP1_NATIVE_STATUS_MESSAGE.md) | status-message owner | #52 | `40b61af5e2115266d4d03dddcc3175850538b0f5` |
| [`MAP1_NATIVE_DIALOG_OWNER.md`](MAP1_NATIVE_DIALOG_OWNER.md) | dialog owner | #53 | `395418510207bf24ac45ddbb4c4c15db3ddc8998` |
| [`MAP1_NATIVE_NOTEBOOK.md`](MAP1_NATIVE_NOTEBOOK.md) | notebook owner | #54 | `03002f79eb03bdcb4c9e430c43e4693dab47e44b` |
| [`MAP1_NATIVE_KEY_GATE.md`](MAP1_NATIVE_KEY_GATE.md) | key gate | #55 | `03c4275f2abfd6671c8bf499c075435d7b61ab97` |
| [`MAP1_NATIVE_PASSWORD_OWNER.md`](MAP1_NATIVE_PASSWORD_OWNER.md) | password pause/submit owner | #56 | `3c113cc047aeb613f2ba4ab7905e92487c796f80` |
| [`MAP1_NATIVE_LINE_DOOR_STATE.md`](MAP1_NATIVE_LINE_DOOR_STATE.md) | OPEN/CLOSE world state | #57 | `e4fb32f41b7074bbb433e64f4c824edb2167cf50` |
| [`MAP1_NATIVE_UNLOCK_STATE.md`](MAP1_NATIVE_UNLOCK_STATE.md) | UNLOCK world state | #58 | `7503b379185db3f05713eb34f1762173edb977d0` |
| [`MAP1_NATIVE_GIVEMAP_STATE.md`](MAP1_NATIVE_GIVEMAP_STATE.md) | GIVEMAP automap state | #59 | `9891a25d700f9ffe1be044ac4a7629c3487604ec` |
| [`MAP1_NATIVE_SAVE_ROUTE.md`](MAP1_NATIVE_SAVE_ROUTE.md) | SAVEGAME future-save route | #60 | `50ed329801fe99917ef2f848ee13e742ae7734ab` |
| [`MAP1_NATIVE_CHANGE_MAP_INTENT.md`](MAP1_NATIVE_CHANGE_MAP_INTENT.md) | CHANGEMAP pending transition intent | #61 | `fc39ac60757e0d992e3729a5044a9d83e9994971` |

## Current candidate

[`MAP1_NATIVE_SHOW_HIDE_TOPOLOGY.md`](MAP1_NATIVE_SHOW_HIDE_TOPOLOGY.md) owns the final two real MAP_INTRO opcode families, `7 / EV_SHOW` and `18 / EV_HIDE`, through one compact native map-sprite/entity topology.

```text
branch = agent/esp32-map1-native-show-hide-topology
base   = fc39ac60757e0d992e3729a5044a9d83e9994971
firmware candidate = 1e9760de2269f57ec24dcea0fc16774a119ae65a
status = IMPLEMENTED; REAL-CYD HARDWARE VALIDATION PENDING
```

The owner intentionally avoids legacy pointer topology:

```text
344 map sprites
7 B / sprite
payload = 2408 B

entity type       1 B
entity subtype    1 B
visual state      1 B
link state/tile   2 B
link order        2 B
```

Expected result ABIs:

```text
EspMapShowResult = 26 B
EspMapHideResult = 18 B
```

SHOW projects the directly owned visual/link consequences and reports deferred blocker gameplay effects rather than invoking legacy `Entity_died()`. The RNG crate blocker path is fail-closed before mutation. HIDE reproduces non-enemy map-sprite hide/unlink traversal using compact link order rather than pointer chains.

The topology is built once from immutable BSP sprites plus a bounded `/entities.db` read from `/DoomRPG-ESP32.pak`; SHOW/HIDE execution itself performs no PAK I/O. Runtime ZIP access remains forbidden.

## Current hardware-proven boundary through PR #61

```text
persistent native heap = 15584 B
arenaFNV               = c3882516
mapStateFNV            = cd99b98e
scriptFNV              = f9e3d9df
lineStateFNV           = e5e74861
lineTextureStateFNV    = f1fc1875
automapStateFNV        = 669b1aa7
giveMapFNV             = 98c7ac59
saveRouteOwnerFNV      = 06ea6ea8
saveRouteResultFNV     = c2ecb064
saveRouteContentFNV    = 725845aa
changeMapOwnerFNV      = f75eb7c7
changeMapResultFNV     = 2f40c9be
changeMapContentFNV    = f7a79d99
legacyTransitionFNV    = 79ab740c
playerStatsFNV         = 0b2ae445
```

Latest hardware-proven CHANGEMAP witness:

```text
refs=1 pending=1 showStats=1 directLoad=0
name="/junction.bsp" targetMap=9 spawnParam=0 effects=03
ownerBytes=16 resultBytes=20 persistentHeapBytes=0
rollback=1/1 reapplyExact=1 closedPackApply=1
heap8=68176->68176 largest8=34804->34804
transitionTriggered=no statsMutation=no menuMutation=no mapLoad=no
```

## SHOW/HIDE validation target

Hardware must establish the actual compact topology counts and fingerprints rather than inheriting desktop assumptions:

```text
entityDefCount
entityCount
EntityDef-backed / fallback counts
initial linked / hidden counts
enemy / destructible counts
nextLinkOrder
topologyFNV
actual persistent topology heap cost
```

It must also audit the complete real opcode corpus:

```text
SHOW refs / HIDE refs
SHOW/HIDE mutation counts
blocker counts and deferred gameplay metadata
HIDE hidden entity total
SHOW/HIDE result and state fingerprints
rollback = refs/refs
showRepeatGuard=1
hideIdempotent=1
```

RAM target:

```text
topology payload = 2408 B
2408 <= actual heap cost <= 2536 B
largest8 >= 32768 B
new total persistent heap = 15584 B + hardware-measured topology allocation
```

Final integrity still requires:

```text
legacy Render runtime clear
legacy entity topology unchanged
entities=0 monsters=0
framebuffer unchanged
shapeData=NULL
mediaTexels=NULL
ST_PLAYING not reached
```

## Architecture rule

DoomRPG-RE desktop/J2ME remains the executable behavior/format reference, not the final ESP32 architecture.

```text
original behavior/data
 -> native pack-backed parsers
 -> compact immutable map
 -> explicit small mutable owners
 -> native event/script ownership
 -> native gameplay/effect consumers
 -> native renderer
```

Never reintroduce runtime ZIP access, map-wide `shapeData`, map-wide `mediaTexels`, pointer-heavy desktop map structures or legacy `Render_t` ownership as a shortcut.

## MAP_INTRO event-family frontier

If this candidate passes, there will be **no remaining real MAP_INTRO opcode ID without an explicit native ownership/execution boundary**.

That milestone closes event-family ownership only. It does not complete the game port: native entity/monster gameplay, deferred effect consumers, map-transition consumption, rendering and actual progression to `ST_PLAYING` remain later work.

Do not merge this candidate until real-CYD hardware validation passes and the post-test commits are verified documentation-only.
