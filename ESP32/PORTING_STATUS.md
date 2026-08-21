# Doom RPG ESP32 CYD porting status

This file is the **authoritative current recovery point** for the classic ESP32-2432S028R Doom RPG port.

Use [`README.md`](README.md) for stable build guidance, [`DOCUMENTATION.md`](DOCUMENTATION.md) for the documentation index, and milestone archives for detailed hardware evidence.

## Latest merged hardware baseline

```text
PR   = #53 — native DIALOG/NOBACK pause owner
main = 395418510207bf24ac45ddbb4c4c15db3ddc8998
hardware-tested firmware content = 85aa89c4218a819e7f18cbf77f64dfbef3c5bac9
```

Detailed merged evidence: [`MAP1_NATIVE_DIALOG_OWNER.md`](MAP1_NATIVE_DIALOG_OWNER.md).

## Current merge-ready candidate

```text
branch = agent/esp32-map1-native-notebook-owner
base   = 395418510207bf24ac45ddbb4c4c15db3ddc8998
hardware-tested firmware content = f619aefc85402d28c4de6edab5ca32ea1eb514dd
status = REAL-CYD HARDWARE PASS / MERGE-READY
```

Detailed active milestone: [`MAP1_NATIVE_NOTEBOOK.md`](MAP1_NATIVE_NOTEBOOK.md).

The candidate consumes only the seven real `EV_NOTE` intents into a bounded caller-owned native map notebook. It reads individual note strings through the native pack, but does not mutate legacy `Player.NotebookString`, Hud, Game continuation, world/entities/rendering or enter gameplay.

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
 -> explicit native effect/player owners
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
dialogApplyFNV   = d0254f3d
noteApplyFNV     = 43183162
notebookContentFNV = 599609e0
notebookStorageFNV = 75cf54e0
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
effect-owner probes   =     0 B persistent heap
-----------------------------------------
current proven total  = 15252 B
largest8              = 36852 preserved
```

Caller-owned value types proven on the classic CYD:

```text
EspMapStatusMessageState =   8 B
EspMapDialogOwnerState   =  12 B
EspMapNotebookState      = 514 B
```

The notebook probe keeps its 514-byte owner on stack and therefore adds 0 persistent heap bytes. A future permanent native player owner embedding it must account those 514 bytes explicitly.

Measured legacy structural allocation was `55341 B`; hardware-proven native structural/script heap ownership remains `40089 B` smaller (~72.4%).

## Hardware-proven event/UI boundary

State-only native opcode executor supports only:

```text
11 EV_CHANGESTATE
19 EV_NEXTSTATE
20 EV_PREVSTATE
```

All other opcodes remain fail-closed there.

Real MAP_INTRO UI/string corpus:

```text
EV_DIALOG       = 76
EV_FORCEMESSAGE = 3
EV_DIALOGNOBACK = 8
EV_NOTE         = 7
total UI refs   = 94
pause intents   = 84
uiIntentFNV     = 7fdd6a79
```

Bounded string reader remains hardware-proven:

```text
strings          = 94
payload          = 7779 B
zeroLength       = 1
max              = 313 B
spanFNV          = 713188eb
contentFNV       = e995ee51
packPayloadReads = 93
```

Canonical payload fingerprints include:

```text
string 1  / FORCE_MESSAGE = f6da01bb
string 25 / DIALOG        = 84f743cf
string 30 / DIALOGNOBACK  = 3692ac94
string 85 / NOTE          = ee639dc1
```

## Native FORCE_MESSAGE owner — hardware validated

```text
refs           = 3
set            = 1
clear          = 2
ownerBytes     = 8
textCopyBytes  = 0
statusApplyFNV = 52b25a5f
ownerAtomic    = yes
```

## Native DIALOG/NOBACK pause owner — hardware validated

```text
refs             = 84
Back             = 76
noBack           = 8
pause            = 84
skipTurn         = 84
resumeExact      = 84
stateExecRefused = 84
ownerBytes       = 12
textCopyBytes    = 0
packIO           = no
persistentHeap   = 0
dialogApplyFNV   = d0254f3d
```

Canonical samples:

```text
Back   cmd11 event6 off0 resume1 flags07 string25@13558+23
noBack cmd19 event6 off8 resume9 flags06 string30@13679+14
```

## Native NOTE notebook owner — hardware validated

Recovered legacy state:

```text
Player.NotebookString[512]
reset on Player_setup()
EV_NOTE appends map string + "||"
Menu_setNotes later splits on '|'
```

Permanent native state:

```text
EspMapNotebookState
text capacity   = 512 B
max payload     = 511 B + NUL
length field    = uint16_t
owner size      = 514 B
heap allocation = 0
```

Real-CYD corpus proof:

```text
refs             = 7
separators       = 7
appendMatches    = 7
stateExecRefused = 7
sourceBytes      = 256
finalLen         = 270
ownerBytes       = 514
textCapacity     = 512
noteApplyFNV     = 43183162
contentFNV       = 599609e0
storageFNV       = 75cf54e0
elapsed          = 83 ms
```

Canonical sample:

```text
cmd103 event40 off8 string85@18964+54 payloadFNV=ee639dc1
```

Hardware bounds proof:

```text
separator  = 1
truncation = 1
fullStable = 1
guards     = 7/7
terminator = yes
```

Atomic fail-closed hardware proof:

```text
unsupported=1 badFlags=1 badKind=1 badRef=1 badEvent=1 badGlobal=1
shortBuffer=1 nullIntent=1 closedPack=1 ownerAtomic=yes reset=1
```

Native-pack I/O and recovery:

```text
entry             = /intro.bsp
size              = 21823
crc32             = 623f34e4
heapOpen          = 64408
transientHeapCost = 4364 B
largestOpen       = 36852
packIO            = yes
persistentHeap    = 0 B
```

Current tested-build integrity:

```text
heap8             = 68772 -> 68772
largest8          = 36852 -> 36852
frameFNV          = a3e3cc8e -> a3e3cc8e
arenaFNV          = c3882516 -> c3882516
mapStateFNV       = cd99b98e -> cd99b98e
scriptFNV         = f9e3d9df -> f9e3d9df
legacyNotebookFNV = 4d7705c5 -> 4d7705c5
```

The previous dialog firmware reported `heap8=68780` and `frameFNV=ef79123a`; this build reports `68772` and `a3e3cc8e`. Every stage has exact before/after stability and inherited structural fingerprints remain canonical, so these are build-to-build layout/content differences rather than persistent NOTE-owner allocation or framebuffer mutation.

Final PARK proves:

```text
nativeStatusMessageOwner       = yes
nativeDialogOwner              = yes
nativeNotebookOwner            = yes
ownerValueBytes                = 514
textCapacity                   = 512
legacyNotebookMutation         = no
legacyHudMutation              = no
legacyGameContinuationMutation = no
worldMutation                  = no
framebufferMutation            = no
entities                       = 0
monsters                       = 0
ST_PLAYING                      = no
```

Complete post-PARK heartbeat:

```text
uptime=25893 ms
heap=134536
heap8=68772
largest8=36852
all reported subsystems = ready
```

A later heartbeat was truncated after `VIDEO=` and is not required for acceptance.

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
 -> real EV_NEXTSTATE execution/rollback
 -> string spans + UI intents
 -> bounded native-pack string reader
 -> real EV_FORCEMESSAGE -> native status owner
 -> real EV_DIALOG/NOBACK -> native pause owner
 -> real EV_NOTE -> bounded native notebook owner
 -> PARK + stable post-PARK heartbeat
```

Still forbidden:

```text
actual DoomCanvas dialog presentation
legacy Game continuation mutation by native owner
legacy Hud mutation
legacy Player.NotebookString mutation
actual notes-menu presentation
full native Game_runEvent execution loop
world/door/line/sprite mutation
map transitions
entity/monster activation
native gameplay rendering
ST_PLAYING
```

## Merge recommendation

**MERGE `agent/esp32-map1-native-notebook-owner`.**

Hardware-tested firmware content:

```text
f619aefc85402d28c4de6edab5ca32ea1eb514dd
```

All commits after that firmware-bearing SHA must remain documentation-only unless another flash is performed.

## Next bounded milestone after merge

Do not preselect it. Reread the then-current repository, recovery docs, merged NOTE milestone and exact remaining MAP_INTRO opcode behavior before choosing the next coherent boundary.
