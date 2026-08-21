# Doom RPG ESP32 CYD porting status

This file is the **authoritative current recovery point** for the classic ESP32-2432S028R Doom RPG port.

Use [`README.md`](README.md) for stable build guidance, [`DOCUMENTATION.md`](DOCUMENTATION.md) for the documentation index, and milestone archives for detailed hardware evidence.

## Latest merged hardware baseline

```text
PR   = #51 — bounded native map string reader
main = 526640b12d978fdbe9c8a9239c37db2fca95cddd
```

Hardware-tested firmware content merged through PR #51:

```text
d13d5eb13c4657d5ec5c16fd82939cfc38989c86
```

Detailed merged evidence: [`MAP1_NATIVE_STRING_READER.md`](MAP1_NATIVE_STRING_READER.md).

## Current merge-ready candidate

```text
branch = agent/esp32-map1-native-force-message-owner
base   = 526640b12d978fdbe9c8a9239c37db2fca95cddd
hardware-tested firmware content = d782681c3cd267b9f16c290a593c1b6e5b34df1c
status = REAL-CYD HARDWARE PASS / MERGE-READY
```

Detailed active milestone: [`MAP1_NATIVE_STATUS_MESSAGE.md`](MAP1_NATIVE_STATUS_MESSAGE.md).

The candidate consumes only the three real `EV_FORCEMESSAGE` intents into a compact caller-owned native status-message ref state. It does not mutate legacy `Hud`, world/entities/rendering or enter gameplay.

## Permanent target / ownership

```text
board        = ESP32-2432S028R classic CYD
MCU          = ESP32-D0WD-V3 dual-core 240 MHz
flash        = 4 MB
PSRAM        = none
display      = ILI9341 320x240
touch        = XPT2046
storage      = microSD
framebuffer  = 160x120 RGB565 = 38400 B
presentation = nearest-neighbor 2x
audio        = deferred
```

Permanent invariants:

```text
shapeData   == NULL
mediaTexels == NULL
runtime ZIP dependency = forbidden
native backing store   = /DoomRPG-ESP32.pak
DoomRPG-RE desktop engine = executable specification/reference only
final CYD engine          = ESP32-native ownership
```

Current native direction:

```text
BSP source in native pack
 -> compact immutable EspMapRuntime
 -> allocation-free accessors
 -> mutable EspMapState
 -> tile/event lookup
 -> event descriptor + bytecode linkage
 -> mutable EspMapScriptState
 -> side-effect-free event filter
 -> fail-closed native opcode executor
 -> allocation-free native string spans
 -> compact native UI/player intents
 -> bounded pack-backed one-string reader
 -> small explicit native text/effect owners
 -> native event/script loop
 -> ESP32-native gameplay + renderer
```

## Hardware-proven fingerprints

```text
source BSP FNV   = d5cc751f
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
statusApplyFNV   = 52b25a5f
```

MAP_INTRO `/intro.bsp` / `Entrance`:

```text
source bytes = 21823
CRC32        = 623f34e4
nodes        = 223
lines        = 480
mapSprites   = 344
events       = 93
byteCodes    = 265
strings      = 94
stringData   = 7779 B
maxString    = 313 B
```

## Persistent native RAM ownership

Hardware-proven persistent heap ownership remains:

```text
immutable arena       = 14112 B actual heap
mutable tile state    =  1040 B actual heap
mutable script state  =   100 B actual heap
opcode executor       =     0 B persistent heap
string spans/intents  =     0 B persistent heap
bounded string reader =     0 B persistent heap
status owner probe    =     0 B persistent heap
-----------------------------------------
current proven total  = 15252 B
largest8              = 36852 preserved
```

The new `EspMapStatusMessageState` is an **8-byte caller-owned value type**, not a heap allocation in this milestone. It stores no copied text. A future permanent gameplay/HUD owner embedding that value must account those 8 bytes explicitly.

Measured legacy structural allocation was `55341 B`; native structural/script heap ownership remains `40089 B` smaller (~72.4%).

## Side-effect-free event filter — hardware validated

```text
contexts         = 142848
evaluations      = 407040
eligible         = 4784
eventBlocked     = 2048
stateMismatch    = 379392
keyMismatch      = 0
flagsMismatch    = 20816
blockInputEvents = 8
filterFNV        = a5923b21
resumeFNV        = b98452da
probe elapsed    = 1411 ms on current owner firmware
```

Script-state ownership proof:

```text
payload       = 81 B
actual heap   = 100 B
initialFNV    = f9e3d9df
mutation test = f9e3d9df -> 99003167 -> f9e3d9df
```

## First native opcode execution — hardware validated

Supported state family:

```text
11 EV_CHANGESTATE
19 EV_NEXTSTATE
20 EV_PREVSTATE
```

All other IDs fail closed in `EspMapOpcodeExecutor_execute()`.

Real MAP_INTRO opcode corpus:

```text
2, 7, 8, 9, 10, 11, 13, 15,
16, 18, 19, 24, 26, 27, 40, 41
```

```text
refs           = 265
unique IDs     = 16
opcodeAuditFNV = 6f28df45
EV_CHANGESTATE = 41 refs
EV_NEXTSTATE   = 35 refs
EV_PREVSTATE   = 0 refs
```

First executed real BSP command:

```text
command index  = 50
opcode         = 19 / EV_NEXTSTATE
arg1           = 00000702
arg2           = 00000100
target tile    = 226
target event   = 16
state          = 0 -> 1
mutated        = yes
execFNV        = 646b565c
```

Rollback remains exact:

```text
scriptFNV f9e3d9df -> 9b636dec -> f9e3d9df
```

## Native UI/string intents — hardware validated

Recovered family:

```text
8  EV_DIALOG
24 EV_FORCEMESSAGE
26 EV_DIALOGNOBACK
40 EV_NOTE
```

Permanent allocation-free APIs:

```text
EspMapStrings_getRef()
EspMapUiIntent_supports()
EspMapUiIntent_build()
```

Real UI corpus:

```text
refs                  = 94
EV_DIALOG             = 76
EV_FORCEMESSAGE       = 3
EV_DIALOGNOBACK       = 8
EV_NOTE               = 7
pause intents         = 84
force-empty semantics = 3
zero-length force     = 2
state-exec refused    = 94
intentFNV             = 7fdd6a79
```

## Bounded native string reader — hardware validated

Permanent API:

```text
EspMapStrings_read(sourceEntry, ref, destination, capacity, outLength)
```

Real content sweep:

```text
strings          = 94
payload          = 7779 B
zeroLength       = 1
cEmpty           = 1
sourceNulBytes   = 0
max              = 313 B
prefixMatches    = 94 / 94
guards           = 94 / 94
packPayloadReads = 93
spanFNV          = 713188eb
contentFNV       = e995ee51
```

Canonical payload fingerprints:

```text
string 1  / FORCE_MESSAGE = f6da01bb
string 25 / DIALOG        = 84f743cf
string 30 / DIALOGNOBACK  = 3692ac94
string 85 / NOTE          = ee639dc1
```

## Native FORCE_MESSAGE status owner — hardware validated

Recovered ownership semantics:

```text
non-empty first C byte -> active owner keeps canonical EspMapStringRef
empty first C byte     -> owner cleared
```

Real MAP_INTRO execution proof:

```text
EV_FORCEMESSAGE refs = 3
set refs             = 1
clear refs           = 2
set->clear transition= 1
owner value size     = 8 B
persistent text copy = 0 B
state executor refused = 3 / 3
statusApplyFNV       = 52b25a5f
```

Canonical set:

```text
cmd4 event2 off0 string1@11569+14 payloadFNV=f6da01bb
```

Canonical first clear:

```text
cmd5 event2 off1 string2@11585+0
```

Atomic fail-closed hardware proof:

```text
unsupported = 1
badFlags    = 1
badRef      = 1
shortBuffer = 1
nullIntent  = 1
closedPack  = 1
ownerAtomic = yes
```

Transient native-pack cost on current owner firmware:

```text
heap8 before/open/after = 68796 / 64432 / 68796
PAK-open transient cost = 4364 B
largest8                = 36852 preserved
elapsed                 = 37 ms
heapPersistentBytes     = 0
```

Exact owner-stage integrity:

```text
frameFNV    = faa62417 -> faa62417
arenaFNV    = c3882516 -> c3882516
mapStateFNV = cd99b98e -> cd99b98e
scriptFNV   = f9e3d9df -> f9e3d9df
notebookFNV = 4d7705c5 -> 4d7705c5
legacy Hud  = unchanged
entities    = 0
monsters    = 0
ST_PLAYING  = no
```

The previous merged reader firmware reported `heap8=68804` and `frameFNV=805df09e`; this candidate build reports `68796` and `faa62417`. Every current stage has zero before/after drift and every inherited native fingerprint remains canonical, so these are build-to-build layout/content differences rather than owner allocations or render mutations.

Final PARK:

```text
nativeStatusMessageOwner = yes
ownerValueBytes          = 8
textCopyBytes            = 0
legacyHudMutation        = no
worldMutation            = no
framebufferMutation      = no
entities                 = 0
monsters                 = 0
ST_PLAYING                = no
```

Post-PARK heartbeat from the same tested firmware:

```text
uptime=210115 ms
heap=134560
heap8=68796
largest8=36852
all reported subsystems = ready
```

A later `[ALIVE] uptime=215116 ms` marker was present but truncated in the supplied capture; it is not used as acceptance evidence.

## Current hardware-proven execution path

```text
validated intro disposal
 -> native BSP inventory
 -> compact resident arena
 -> accessor sweep
 -> mutable tile state
 -> tile/event lookup
 -> descriptor/linkage sweep
 -> 81 B script-state build
 -> exhaustive event-filter proof
 -> opcode inventory + real EV_NEXTSTATE execution/rollback
 -> allocation-free string-span sweep
 -> 94 UI/string bytecodes -> native intents
 -> bounded native-pack string-content sweep
 -> real EV_FORCEMESSAGE intents -> native status-message owner
 -> PARK + stable post-PARK heartbeat
```

Still forbidden:

```text
actual DoomCanvas dialog presentation
legacy Hud.statBarMessage mutation from native owner
actual Player notebook mutation
full native Game_runEvent execution loop
world/door/line/sprite mutation
map transitions
entity/monster activation
native gameplay rendering
ST_PLAYING
```

## Merge recommendation

**MERGE `agent/esp32-map1-native-force-message-owner`.**

Hardware-tested firmware content:

```text
d782681c3cd267b9f16c290a593c1b6e5b34df1c
```

All commits after that firmware-bearing SHA must remain documentation-only unless another flash is performed.

## Next bounded milestone after merge

Prefer a compact native `EV_DIALOG` / `EV_DIALOGNOBACK` owner carrying:

```text
immutable text ref
Back / no-Back mode
source event
resume command offset
pause-script / skip-turn semantics
```

Keep actual dialog presentation, legacy `DoomCanvas` mutation, world/entity/render mutation and a full native event loop outside that next boundary.
