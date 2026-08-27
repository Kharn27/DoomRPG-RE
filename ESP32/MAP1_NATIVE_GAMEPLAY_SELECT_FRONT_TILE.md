# MAP1 native gameplay SELECT front-tile observer

Status: **REAL-CYD HARDWARE PASS / MERGE-READY after docs-only closeout audit**

```text
branch = agent/esp32-native-gameplay-select-front-tile
base main = e0a250f0bfd6e5519298f942f4bed65c230c3652
hardware-tested implementation SHA = ca5560c0eb849c8a11b21eb8c117e7a8fc4c60ff
```

## Goal

Recover the first exact legacy SELECT interaction boundary on the live native Junction gameplay path without executing bytecode or mutating the world:

```text
SELECT intent
 -> current settled native view/orientation
 -> dest + viewStep
 -> front tile
 -> Game_executeTile-equivalent run flags 0x500 / 1280
 -> tile event lookup + descriptor + script-state/filter provenance
 -> optional compact line-derived entity witness on the same front tile
 -> read-only result
```

This milestone deliberately does **not** open or close a door, change event state, display dialogue, play sound, advance a turn, run broad entity/combat trace, mutate legacy Game/Hud/Player/Render, or execute a general Game_executeEvent equivalent.

## Legacy ordering recovered

The recovered SELECT path first calls the tile-event route at:

```text
frontX = destX + viewStepX
frontY = destY + viewStepY
flags  = 1280 / 0x500
```

Only later does legacy SELECT fall through to broader entity/weapon interaction when the tile event does not consume the action.

The native milestone therefore observes the tile-event path first and keeps the later broad entity/combat branch fail-closed.

## Permanent native owners

New permanent components:

```text
ESP32/include/esp_native_gameplay_select.h
ESP32/src/esp_native_gameplay_select.c
ESP32/include/esp_map_line_topology_query.h
ESP32/src/esp_map_line_topology_query.c
```

Hardware-visible ABI:

```text
EspNativeGameplaySelectResult = 28 B
EspMapLineTopologyRef         = 16 B
```

Both paths are allocation-free and reuse already resident owners:

```text
EspPlayerViewState
EspNativeGameplayTurnState
EspMapEvents
EspMapScriptState
EspMapEventFilter
EspMapLineState
EspEntityDefTypeCatalog
EspMapRuntime
```

No duplicate 1024-entry front-tile index, Entity_t world, line copy, or map-wide scratch owner is introduced.

The input owner adds only a weak SELECT observer hook. TURN and MOVE ownership/dispatch remain unchanged.

## Strict observer contract

The interactive probe runs before the transient touch feedback draw and records the exact pre-feedback frame/heap/legacy/resident boundary.

For each SELECT it requires:

```text
frame unchanged
heap8 unchanged
largest block unchanged
legacy structs unchanged
resident snapshot unchanged
PAK closed after observation
no bytecode execution
no door mutation
no broad entity trace
no turn advance
no render
```

Current native key ownership is intentionally absent, so the observer uses:

```text
keyBits = 0
keySource = native-unowned-zero
```

This is provenance only; it does not invent pickups or inventory state.

## Real-CYD Test 1 — fresh spawn North

At canonical Junction spawn:

```text
player = 992,1888
angle  = 64 / North
frame  = ba3e5182
```

One SELECT produced:

```text
front=992,1824
tile=911
status=TILE_EVENT
event=59
state=0
eventFlags=0
commands=1
range=308..309
```

Linked command:

```text
global=308
opcode=4 / UNOWNED
decision=FLAGS_MISMATCH
arg1=00000004
arg2=00000004
```

Integrity result:

```text
resultBytes=28
lineRefBytes=16
runFlags=00000500
heap8=38924->38924
largest8=29684->29684
frame=ba3e5182 exact=yes
legacyExact=yes
residentExact=yes
packClosed=yes
bytecodeExec=no
doorMutation=no
broadEntityTrace=no
turnAdvance=no
render=no
```

The normal neon feedback then drew/restored exactly, proving the SELECT observer did not interfere with the input-feedback lifecycle.

## Real-CYD Test 2 — arrival door line 35

After two `TURN_RIGHT` actions from fresh North, the player faced South at the unchanged spawn position. TURN remained healthy and reached the expected South frame:

```text
angle=192
frame=da1c4297
heap8=38924 stable
largest8=29684 stable
legacyStable=yes
residentStable=yes
```

SELECT then proved the exact Junction front-tile route:

```text
front=992,1952
tile=975
status=TILE_EVENT
event=63
state=0
eventFlags=0
commands=1
range=316..317
```

The same front tile resolves the canonical arrival-door line-derived entity:

```text
line=35
texture=7
flags=00000505
type=0
defTile=312
open=0
locked=1
linked=1
topology=legacy-line-entity
```

The event command is:

```text
global=316
opcode=15 / EV_OPENLINE
decision=ELIGIBLE
arg1=00000023   # decimal 35
arg2=00000100
```

This is a major recovered gameplay fact:

```text
SELECT on the real arrival door
 -> tile event 63
 -> eligible EV_OPENLINE line 35
 -> line 35 is simultaneously LOCKED
```

The event filter only decides command eligibility. It does **not** bypass the separate legacy door rule that refuses a locked line before changing the open state. Therefore this milestone correctly observes the eligible EV_OPENLINE while performing **no mutation**.

Integrity remained exact:

```text
frame=da1c4297 exact=yes
heap8=38924->38924
largest8=29684->29684
legacyExact=yes
residentExact=yes
packClosed=yes
bytecodeExec=no
doorMutation=no
broadEntityTrace=no
turnAdvance=no
render=no
```

### Important recovery correction

A pre-test prediction incorrectly assumed tile 975 could not have a tile event because the old `/intro.bsp` event corpus ended at tile 968.

That inference was invalid for this stage: the active resident map is `/junction.bsp`, not `/intro.bsp`.

The physical CYD is authoritative and proved:

```text
Junction tile 975 -> event 63 -> EV_OPENLINE(35)
```

Do not reuse MAP_INTRO event-table bounds as Junction runtime facts.

## Real-CYD Test 3 — live interaction corpus

The user navigated around Junction and physically SELECTed several visible interaction targets, including a scientist, a computer and a soldier. The observer remained stable while exposing real multi-command scripts.

### Event 56 / tile 878

```text
front=928,1760
tile=878
event=56
commands=18
range=274..292
```

Observed command families:

```text
EV_DIALOG       / opcode 8
EV_CHANGESTATE  / opcode 11
EV_NEXTSTATE    / opcode 19
```

Current state/key context correctly filtered commands as `KEY_MISMATCH` or `STATE_MISMATCH`; no command executed.

### Event 49 / tile 845

```text
front=864,1696
tile=845
event=49
commands=14
range=242..256
```

A linked line-derived entity was also present:

```text
line=98
texture=54
flags=00004000
type=7
defTile=359
open=0
locked=0
linked=1
```

Observed script families were again bounded dialogue/state commands; no world mutation occurred.

### Event 45 / tile 816

```text
front=1056,1632
tile=816
event=45
commands=12
range=223..235
```

Observed families:

```text
EV_DIALOG
EV_CHANGESTATE
```

Again, current filter context rejected all commands and the observer stayed read-only.

The logs prove the SELECT resolver works after multiple real MOVE/TURN actions at noncanonical positions, not only at fresh spawn.

## Gameplay regression proof

The same physical session continued to execute normal TURN and MOVE around all SELECT observations.

Representative results:

```text
TURN North -> East/South/West/North family remained live
canonical North round-trip frame=ba3e5182 exact
MOVE 943->911 OK
MOVE 911->879 OK
MOVE 847->846 OK
MOVE 846->847 OK
MOVE 847->815 OK
heap8=38924 stable
largest8=29684 stable
stackHighWater=860
legacyStable=yes
residentStable=yes
orientationStable=yes on MOVE
turnAdvance=no
tileDispatch=no
```

No `SELECTPROBE FAILED`, Guru Meditation, reboot, allocation drift, resident drift, or legacy mutation was reported.

## Final boundary

Hardware-proven now:

```text
native SELECT intent observation=yes
front coordinate from dest+viewStep=yes
legacy SELECT flags 0x500=yes
front tile -> event lookup=yes
current script state/filter provenance=yes
front tile -> line-derived entity witness=yes
arrival door tile 975 -> event 63=yes
arrival door event -> EV_OPENLINE(35)=yes
arrival door locked line 35=yes
SELECT read-only integrity=yes
MOVE/TURN regression-free=yes
```

Still intentionally absent:

```text
actual SELECT bytecode execution
actual EV_OPENLINE/EV_CLOSELINE mutation from gameplay
locked-door refusal result owner
unlock/key pickup gameplay ownership
HUD blocked-key/door message
sound
visual door animation
broad entity/combat SELECT fallback
dialog rendering/ownership on the live gameplay path
Game_advanceTurn semantics
post-MOVE tile dispatch
```

## Next bounded milestone

The strongest next boundary is now concrete rather than speculative:

```text
real SELECT on tile 975
 -> event 63
 -> eligible EV_OPENLINE arg1=35
 -> inspect line 35 mutable state
 -> LOCKED => refuse with no line mutation
 -> unlocked line => bounded EspMapLineState open mutation
 -> collision consumer already follows open/closed state
```

Implement only the dedicated `EV_OPENLINE / EV_CLOSELINE` gameplay family plus the legacy lock guard. Keep sound, animation, broad entity trace and unrelated opcodes out.

A successful unlocked-door witness should use a real unlocked line/event discovered from the Junction corpus rather than bypassing line 35's lock bit.

## Hardware acceptance

```text
REAL-CYD HARDWARE PASS
hardware-tested implementation SHA = ca5560c0eb849c8a11b21eb8c117e7a8fc4c60ff
```

Every commit after this SHA must remain documentation-only unless another firmware flash is performed.
