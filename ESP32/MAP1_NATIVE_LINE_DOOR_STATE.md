# ESP32 MAP_INTRO native line-door world state milestone

Branch: `agent/esp32-map1-native-line-door-state`

Base merged `main`:

```text
PR   = #56 — native PASSWORD pause owner
main = 3c113cc047aeb613f2ba4ab7905e92487c796f80
```

Firmware candidate content:

```text
376f45bcdd12264d3cba1ee83e7197a52e248210
```

Status: **IMPLEMENTED; REAL-CYD HARDWARE VALIDATION PENDING**.

## Objective

Cross the first explicit mutable-world boundary without reintroducing legacy `Render_t`, entities or map-wide mutable line objects:

```text
480 immutable EspMapRuntime lines
 -> 60 B packed open bits
 -> 60 B packed locked bits
 -> real EV_OPENLINE / EV_CLOSELINE
 -> one native open-bit transition
 -> deferred animation / entity-link / sound metadata
```

This milestone supports only opcodes:

```text
15 EV_OPENLINE
16 EV_CLOSELINE
```

It does **not** support `EV_UNLOCK`, mutate line texture, instantiate collision entities, animate doors, play sound, mutate legacy Render/Game/Entity state, render gameplay or enter `ST_PLAYING`.

## Why this family first

The hardware-proven opcode inventory already established that global bytecode command `0` is a real `16 / EV_CLOSELINE` command. OPEN/CLOSE is therefore not a synthetic architectural exercise: it is the earliest real unsupported command in the MAP_INTRO corpus.

Unlike `EV_UNLOCK`, OPEN/CLOSE has one compact canonical world mutation: legacy `Game_performDoorEvent()` changes the line `0x40` open bit. `EV_UNLOCK` additionally changes the `0x400` lock bit, can switch texture `9 <-> 10`, changes the special collision entity definition and can play another sound, so it remains a later dedicated milestone.

SHOW/HIDE operate on sprites/entities and GIVEMAP touches automap visibility across lines/sprites/tiles. OPEN/CLOSE is the smallest permanent world owner.

## Recovered legacy behavior

For OPEN/CLOSE, legacy `Game_executeEvent()` delegates to `Game_performDoorEvent(game, codeId, arg1, flags)`.

Recovered line semantics:

```text
line = lines[arg1]

if line.flags & 0x400:
    return false                 // locked

if OPENLINE and line.flags & 0x40:
    return false                 // already open

if CLOSELINE and !(line.flags & 0x40):
    return false                 // already closed

line.flags ^= 0x40
DoomCanvas_updatePlayerDoors(line)
sync the matching special collision entity link
sound = 5063 when now open
sound = 5064 when now closed
return true
```

For these two opcodes the special MOVELINE-only input-flag condition is irrelevant.

The outer legacy `Game_runEvent()` then removes a handled command when its source `arg2` carries `0x200 / MCODE_FLAG_REMOVE`. Native line execution therefore returns `removeCommandIfHandled` but deliberately does **not** mutate `EspMapScriptState` in this milestone. The future native event loop owns that outer-loop behavior.

## Permanent native world state

New files:

```text
ESP32/include/esp_map_line_state.h
ESP32/src/esp_map_line_state.c
```

The immutable runtime already owns all 480 compact line records. Only the dynamic predicates needed by this family are materialized:

```text
openBits   = ceil(lineCount / 8)
lockedBits = ceil(lineCount / 8)
```

For MAP_INTRO:

```text
lineCount    = 480
bitsetBytes  = 60
storageBytes = 120 B payload
```

No geometry, texture, flags word, line pointer or entity pointer is duplicated.

The state is heap-owned like `EspMapState` / `EspMapScriptState`; the real classic CYD must establish the allocator cost. The probe allows only a bounded allocator overhead above the 120-byte payload.

View:

```c
typedef struct EspMapLineStateView_s {
    const uint8_t* openBits;
    const uint8_t* lockedBits;
    uint32_t lineCount;
    uint32_t bitsetBytes;
    uint32_t storageBytes;
    uint32_t stateFNV1a;
    uint32_t openCount;
    uint32_t lockedCount;
} EspMapLineStateView;
```

Permanent primitives:

```text
EspMapLineState_reset()
EspMapLineState_buildFromRuntime()
EspMapLineState_isReady()
EspMapLineState_view()
EspMapLineState_getOpen()
EspMapLineState_getLocked()
EspMapLineState_setOpen()
EspMapLineState_setLocked()
```

`setLocked()` is only a generic state primitive in this branch. There is no opcode-13 dispatcher and no `EV_UNLOCK` semantics are authorized yet.

## Permanent OPEN/CLOSE executor

`EspMapLineState_applyDoorCommand()` revalidates the supplied descriptor against the current immutable runtime, reads only its canonical linked command and supports only IDs 15/16.

Result ABI target:

```c
typedef struct EspMapLineDoorResult_s {
    uint16_t sourceEventIndex;
    uint16_t globalCommandIndex;
    uint16_t lineIndex;
    uint16_t soundId;
    uint8_t sourceCommandOffset;
    uint8_t codeId;
    uint8_t openBefore;
    uint8_t openAfter;
    uint8_t locked;
    uint8_t mutated;
    uint8_t effectFlags;
    uint8_t removeCommandIfHandled;
} EspMapLineDoorResult;
```

Expected classic-ESP32 ABI:

```text
resultBytes = 16
```

Semantic statuses:

```text
NOT_READY       -> no state/result mutation
UNSUPPORTED     -> non-15/16 command, fail closed
INVALID         -> bad descriptor/offset/etc.
LINE_OUT_OF_RANGE -> malformed source index
LOCKED          -> valid semantic no-op, legacy return false
ALREADY_TARGET  -> valid semantic no-op, legacy return false
OK              -> open bit mutated, legacy return true
```

A successful result carries only deferred effect metadata:

```text
DOOR_ANIMATION
ENTITY_RELINK
PLAY_SOUND
```

No effect is applied to the legacy world.

## Temporary real-CYD probe

New files:

```text
ESP32/include/native_map1_line_door_probe.h
ESP32/src/native_map1_line_door_probe.c
```

The probe runs only after the hardware-proven PASSWORD stage.

### Initial world-state proof

It builds the 120-byte line overlay and independently checks every one of the 480 lines against immutable source flags:

```text
open bit   <-> source flags & 0x40
locked bit <-> source flags & 0x400
```

The first hardware PASS will establish:

```text
initialOpen count
initialLocked count
lineStateFNV
actual persistent heap cost
allocator overhead
```

### Real command corpus

Every real OPENLINE/CLOSELINE command in the canonical `93 event / 265 bytecode` corpus is classified from a restored initial line state.

For each ref:

```text
state-only opcode executor must refuse it
canonical descriptor/command provenance must match
real line index must be < 480
LOCKED / ALREADY_TARGET / OK must match current native state
OK must change exactly one open bit
OK must return sound 5063 or 5064
OK must return deferred effect flags 0x07
removeCommandIfHandled must match source arg2 & 0x200
```

After every successful real mutation the probe restores the prior open bit and requires the exact initial `lineStateFNV` again.

Acceptance requires at least one real `OK` transition, so this milestone must prove an actual Doom RPG world mutation rather than only build an overlay.

New hardware canons intentionally left pending:

```text
OPEN/CLOSE ref counts
mutated / locked / already-target counts
removable handled refs
first successful real command sample
lineDoorFNV
first mutated line-state FNV
```

### Idempotence + lock gate

The first successful real command is replayed:

```text
first apply  -> OK + real open-bit mutation
second apply -> ALREADY_TARGET + exact state stability
```

The same real command is also tested with its native lock bit temporarily set:

```text
locked apply -> LOCKED + no open mutation/effects
```

Both tests restore the full initial 120-byte state before PARK.

### Fail-closed proof

The probe requires:

```text
notReady=1
unsupported=1
badOffset=1
badDescriptor=1
nullDescriptor=1
nullResult=1
badOpenIndex=1
badLockedIndex=1
stateAtomic=yes
worldRestored=yes
```

## RAM / integrity boundary

This is the first milestone expected to add persistent native world heap.

Hardware acceptance requires:

```text
payload = 120 B
persistentHeapCost >= 120 B
persistentHeapCost <= 184 B
no transient heap drift after build
largest 8-bit block remains safely >= 32768 B
```

The real allocation cost is hardware-pending and must be documented from Serial rather than guessed.

Before/after the command audit, all previous owners and immutable data must remain exact:

```text
arenaFNV          = c3882516
mapStateFNV       = cd99b98e
scriptFNV         = f9e3d9df
legacyNotebookFNV = 4d7705c5
legacy Player.keys unchanged
Hud witness unchanged
DoomCanvas password witness unchanged
Game continuation witness unchanged
framebuffer unchanged
pack remains closed
legacy Render runtime remains fully clear
entities=0
monsters=0
```

The new line state remains allocated at PARK but must be restored to its initial content FNV.

## Expected Serial family

```text
[MAPDOORPROBE] ARMED ...

=== Doom RPG ESP32-native MAP_INTRO line door world state ===
[MAPDOORPROBE] CONTRACT ...
[MAPLINESTATE] READY lines=480 bitsetBytes=60 storageBytes=120 open=... locked=... stateFNV=...
[MAPDOOR] READY refs=... open=... close=... mutated=... locked=... alreadyTarget=... removable=... resultBytes=16 stateExecRefused=... lineDoorFNV=... elapsed=...ms
[MAPDOOR] SAMPLE cmd=... event=... off=... line=... opcode=... open=...->... locked=... sound=... effects=07 removeIfHandled=...
[MAPDOOR] WORLD lines=480 bitsetBytes=60 storageBytes=120 initialOpen=... initialLocked=... lineStateFNV=... mutatedFNV=... rollback=.../... idempotent=1 lockedGuard=1
[MAPDOOR] FAILCLOSED notReady=1 unsupported=1 badOffset=1 badDescriptor=1 nullDescriptor=1 nullResult=1 badOpenIndex=1 badLockedIndex=1 stateAtomic=yes worldRestored=yes
[MAPDOORPROBE] RAM heap8=...->... persistentHeapCost=... payload=120 allocatorOverhead=... largest8=...->... frameFNV=...->... arenaFNV=c3882516->c3882516 mapStateFNV=cd99b98e->cd99b98e scriptFNV=f9e3d9df->f9e3d9df
[MAPDOORPROBE] LEGACY ... packIO=no legacyRuntimeClear=yes
[MAPDOORPROBE] PARK ... nativeLineState=yes nativeDoorExec=yes storageBytes=120 resultBytes=16 worldMutationProven=yes worldRestored=yes legacyWorldMutation=no ...
[ALIVE] ...
```

Use normal optimized:

```text
esp32-cyd
```

No CI status is currently published for firmware candidate `376f45bcdd12264d3cba1ee83e7197a52e248210`. Do not claim local build or hardware PASS until the real classic CYD supplies it.

## Remaining boundary after PASS + merge

Do not preselect it before recovery. After this branch passes and merges, reread the new true `main`, recovery docs, this milestone and remaining real corpus:

```text
2  EV_CHANGEMAP
7  EV_SHOW
9  EV_GIVEMAP
13 EV_UNLOCK
18 EV_HIDE
27 EV_SAVEGAME
```

`EV_UNLOCK` will then be a natural adjacent line-state milestone, but its texture/entity-definition behavior must be recovered and owned explicitly rather than silently folded into OPEN/CLOSE.
