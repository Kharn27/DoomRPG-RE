# Native gameplay checkpoint v4 — line state + texture variants

Status: **REAL-CYD PASS**

This milestone extends the bounded native checkpoint format by one coherent mutable owner family: native line open/locked state plus the mutable locked/unlocked door texture variant. The save remains pointer-free and owner-by-owner; immutable BSP lines, renderer objects and legacy world graphs are never serialized.

## Code boundary

Active branch:

```text
agent/esp32-native-checkpoint-v4-lines
```

V4 line checkpoint commits:

```text
d71173f745a8414d1332119c00ea6183ef2fa5f6
  ESP32: add bounded line checkpoint snapshot API
381c7d6e1252f2e421119b55abb7dc83c1349818
  ESP32: implement bounded line checkpoint snapshot
d9bdfca80af2ccd3add439823bdcff8244484300
  ESP32: persist native line state in checkpoint v4
3058db9dfe4fa2b466f52cc2de9336f3cc2d4b93
  ESP32: keep checkpoint v4 payloads off HUB stack
2efb9634ffc1c2fb433c4c3340ff9c722c5c7b4d
  ESP32: guard checkpoint capture workspace pointer
```

GitHub Actions `esp32-cyd` run #293 / run ID `35344853078` passed on exact code boundary `2efb9634ffc1c2fb433c4c3340ff9c722c5c7b4d`.

## V4 format

```text
path = /sd/DoomRPG-ESP32.sav
magic = DRPGSAV4
version = 4
recordBytes = 1212
legacy v1/v2/v3 read compatibility = retained
write format = v4
```

V4 keeps the proven v1 core, v2 resource section and v3 script section, then appends exactly one `EspMapLineCheckpointSnapshot`.

The line section contains only semantic compact state:

```text
sourceArenaFNV1a
lineStateFNV1a
textureStateFNV1a
lineCount
bitsetBytes
openBits[]
lockedBits[]
texture10Bits[]
```

Hard bound:

```text
ESP_MAP_LINE_CHECKPOINT_MAX_LINES = 1024
ESP_MAP_LINE_CHECKPOINT_MAX_BYTES = 128
```

Entrance uses:

```text
lines = 480
bitsetBytes = 60
open payload = 60 B
locked payload = 60 B
texture10 payload = 60 B
```

Restore validates immutable runtime identity, exact line count/bitset size, zero tail bits and texture eligibility before mutating any line owner. It restores only the already rebuilt `EspMapLineState` and `EspMapLineTextureState` owners and verifies both semantic fingerprints after mutation.

## Hardware proof — soldier door survives LOAD

The real-CYD witness used the Entrance soldier-controlled door. Before LOAD the save was taken after the soldier script had permanently unlocked the door. A fresh resident rebuild first recreated the canonical line state:

```text
[MAPLINESTATE] READY lines=480 bitsetBytes=60 storageBytes=120
               open=0 locked=7 stateFNV=e5e74861
[MAPLINETEX] READY lines=480 storageBytes=60
             variants=6 texture10=0 stateFNV=f1fc1875
```

V4 restore then reapplied the checkpoint line semantics:

```text
[MAPLINECHECKPOINT] RESTORE arena=c3882516
                    lines=480 bytes=60
                    open=0 locked=6 texture10=1
                    lineFNV=69334d90
                    textureFNV=bda09634
                    mutation=line-overlays-only allocation=no
```

The checkpoint load itself was accepted as V4 and restored all previously proven sections:

```text
[NATIVESAVE] LOAD path=/sd/DoomRPG-ESP32.sav
             version=4 bytes=1212
             map=1 gameplayLoadMapId=1
             pos=224,1248 angle=64
             playerFNV=a6e115a7 runtimeFNV=c3882516
             resources=restored/5/43B
             script=restored/93/265/81B/26f291e3
             lines=restored/480/60B/open0/locked6/tex101/69334d90/bda09634
             world=resources+script+lines-restored+others-fresh
             session=reprime-pending
```

Most importantly, the restored soldier door was immediately usable without replaying the soldier unlock script:

```text
[ACTION] SELECT ... status=DOOR_OK tile=612 event=62 eligible=1
[ACTION] DOOR line=352 opcode=15 status=OK
         open=0->1 locked=0 removed=0->0
[DOORANIM] COMPLETE transitions=1 frames=4
           state=stable transaction=committed
```

This is the behavioral proof that the saved unlock survived LOAD. The saved door was closed but unlocked, so `open=0` after restore is expected; the critical persisted semantics are `locked=0` and the matching unlocked texture variant.

## V4 HUB stack regression and bounded fix

The first V4 hardware attempt exposed a stack-canary reset while switching from INV to STAT:

```text
Guru Meditation Error: Core 1 panic'ed
Debug exception reason: Stack canary watchpoint triggered (loopTask)
```

The cause was not SD or rendering. V4 enlarged the fixed checkpoint record, and a `LoadedSaveRecord` automatic in the HUB wrapper plus other large temporary checkpoint copies pushed `loopTask` over its stack boundary before the STATUS renderer returned.

The bounded fix keeps the read workspace in static BSS, removes full-record CRC copies, writes capture directly into the destination buffer, and avoids a second verification record on the stack. Hardware then reached and rendered STAT normally:

```text
[HUB] FRAME ... page=status ... exact=yes ... mutation=no turn=no
[HUB] PAGE page=inventory->status direction=left ...
[NATIVESAVE] UI page=status rows=SAVE/LOAD ...
```

The same session proceeded through LOAD, resident rebuild, cache reprime and continued gameplay without reset.

## Session / memory invariants after LOAD

The permanent invariants remained true:

```text
shapeData == NULL
mediaTexels == NULL
```

The post-LOAD session reached:

```text
[ENGINESESSION] READY ... shapeData=0x0 mediaTexels=0x0
heap8 = 14076
largest8 = 6644
```

The static V4 read workspace costs bounded permanent RAM in exchange for removing dangerous large transient stack frames. Fragmentation/headroom remains below the advisory target and stays on the review list.

## Boundary after V4

Checkpoint persistence is now hardware-proven for:

```text
settled player pose
EspNativeGameplayPlayerState
EspNativeGameplayPlayerResources consumed overlay
EspMapScriptState event states + removed-command bits
EspMapLineState open/locked bits
EspMapLineTextureState locked/unlocked texture variants
```

Still intentionally fresh after LOAD:

```text
automap reveal state
monster mutable state/positions/activation/combat consequences
destructibles
gameplay RNG state
```

Continue persistence one explicit owner family at a time. Do not replace these sections with a monolithic world/object dump.
