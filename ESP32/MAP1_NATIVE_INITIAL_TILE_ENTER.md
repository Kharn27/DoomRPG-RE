# ESP32 native Junction initial tile-enter milestone

Branch: `agent/esp32-native-initial-tile-enter`

Base merged `main`:

```text
PR   = #72 — native fresh-map Player_setup
main = 9077ae4496bdcc06b6b99846332ab43b38943a8a
```

Status: **HARDWARE CANDIDATE — NOT YET CYD-PROVEN**.

## Objective

PR #72 hardware-proved the fresh-map player setup boundary and parks the active native Junction player/view at:

```text
world=992/1888
angle=64
PlayerView FNV=c21fba3c
hudRefreshPending=0
facingRefreshPending=1
playerSetupPending=0
tileEnterPending=1
Player_setup semanticFNV=3b27c6a1
```

This milestone owns only the next exact recovered operation: the first fresh-map `Game_executeTile()` call made by `Game_spawnPlayer()`.

It deliberately does **not** own `finishRotation()`, its second tile execution, durable facing, `ST_PLAYING`, entity gameplay or rendering.

## Recovered legacy call

The relevant recovered sequence is:

```c
Render.viewZOld = 4;
Hud.isUpdate = true;
DoomCanvas_checkFacingEntity(...);   // transient old vectors
Player_setup(...);
Game_executeTile(game,
                 doomCanvas->viewX,
                 doomCanvas->viewY,
                 0x40f | DoomCanvas_flagForFacingDir(doomCanvas));
```

For the hardware-proven Junction spawn:

```text
viewX=992
viewY=1888
tileX=15
tileY=29
tileIndex=943
destAngle=64
flagForFacingDir(64)=0x10000000
inputFlags=0x1000040f
```

Recovered `Game_executeTile()` semantics are:

```text
if Game.f658b: return false
skipAdvanceTurn=false
bounds-check x>>6 / y>>6
if tile has an event:
  find event by tile
  Game_runEvent(event, start=0, flags)
return whether an event command reported success
```

The native milestone represents `skipAdvanceTurn=false` in its own semantic owner and requires the currently parked legacy witness to already be false; it does not mutate legacy `Game_t`.

## Permanent owner

Files:

```text
ESP32/include/esp_player_initial_tile.h
ESP32/src/esp_player_initial_tile.c
```

Candidate ABI:

```text
EspPlayerInitialTileState = 24 B
persistent heap = 0 B
```

The owner records:

```text
input flags
tile index
event index / found state
initial mutable event state
event flags
eligible command count
executed command count
removed-command count
BLOCKINPUT result
skipAdvanceTurn semantic
target/load identity
active state
```

Mutable event state and MCODE_FLAG_REMOVE bits remain in the already permanent `EspMapScriptState` overlay. The immutable runtime arena is never patched.

## Existing native machinery reused

This milestone intentionally reuses rather than duplicates:

```text
EspMapEvents_findByTile()
EspMapEvents_describe()
EspMapEvents_getCommand()
EspMapEventFilter_prepare()
EspMapEventFilter_evaluate()
EspMapScriptState event-state / removed-command overlay
EspMapOpcodeExecutor_execute()
```

The recovered key-filter input remains an explicit `playerKeys` argument because cross-map key ownership has not yet moved into the active native player root. The hardware probe passes the current legacy value read-only and proves the legacy player object is not mutated.

## Deliberately narrow opcode boundary

The generic native opcode executor still supports only:

```text
11 EV_CHANGESTATE
19 EV_NEXTSTATE
20 EV_PREVSTATE
```

Therefore the initial tile route performs a full side-effect-free preflight before mutation. If any **eligible** command on tile 943 uses any other opcode, preparation returns:

```text
ESP_PLAYER_INITIAL_TILE_OPCODE_DEFERRED
```

and leaves player/view + script state unchanged.

The probe then prints the exact real command:

```text
event index/value
initial state/event flags
command offset
opcode ID
arg1
arg2
```

That output is the source of truth for the next dedicated opcode-family implementation. No fallback to legacy `Game_executeEvent()` is permitted.

## Atomic execution

If every eligible command is already supported, route execution:

1. executes only through `EspMapOpcodeExecutor`;
2. applies recovered `arg2 & 0x200` removal through `EspMapScriptState` rather than zeroing immutable bytecode;
3. records enough bounded stack state to roll back script mutations if execution or player-view consumption fails;
4. consumes only `tileEnterPending` after the whole dispatch succeeds.

Expected post-route player/view semantic:

```text
hudRefreshPending=0
facingRefreshPending=1
playerSetupPending=0
tileEnterPending=0
```

With the 44-byte ABI unchanged, the predicted FNV for that one-bit ownership transfer is:

```text
post-initial-tile PlayerView FNV=1bd0f09b
```

This is a **candidate expectation**, not a hardware canon until the CYD proves it.

## Hardware probe

Temporary probe files:

```text
ESP32/include/native_junction_initial_tile_probe.h
ESP32/src/native_junction_initial_tile_probe.c
```

The normal `esp32-cyd` chain arms it only after the hardware-proven Player_setup probe has completed.

Two valid diagnostic outcomes exist before this milestone can be promoted:

### A. Complete native route

Expected markers begin with:

```text
=== Doom RPG ESP32-native Junction initial tile-enter ===
[JUNCTIONTILE] READY ...
```

The probe then proves:

```text
tile=943
flags=1000040f
PlayerView c21fba3c -> 1bd0f09b
tileEnterPending=0
facingRefreshPending=1
finishRotation still deferred
no legacy mutation
no framebuffer mutation
zero same-build heap delta
immutable runtime/non-script owners stable
```

Script FNV is allowed to change only if eligible supported state opcodes actually execute.

### B. Real unsupported opcode discovery

Expected marker:

```text
[JUNCTIONTILE] DEFERRED ... code=<real opcode> arg1=<...> arg2=<...> failClosed=yes
```

This is not a hardware PASS for tile-enter ownership. It is a successful fail-closed discovery result: `tileEnterPending` remains 1 and the exact opcode becomes the next bounded prerequisite.

## Mandatory invariants

Always remain true:

```text
shapeData == NULL
mediaTexels == NULL
runtime ZIP map access forbidden
legacy Game.entities = 0
legacy Game.monsters = 0
ST_PLAYING not reached
```

Normal hardware environment:

```text
esp32-cyd
```

## Promotion rule

Do not mark this milestone hardware-proven until the real CYD logs establish either that the initial tile dispatch is fully owned by the current executor, or after any discovered deferred opcode family is implemented and the complete route is re-tested.

After a complete PASS, update `PORTING_STATUS.md`, `DOCUMENTATION.md` and this archive using the exact Serial values; all commits after the flashed firmware SHA must then be documentation-only.
