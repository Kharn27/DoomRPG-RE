# Native gameplay checkpoint v3 — script/event state

Status: **REAL-CYD PASS**

This milestone extends the bounded native checkpoint format by one explicit mutable owner: `EspMapScriptState`. The permanent save architecture remains pointer-free and owner-by-owner; no desktop object graph or monolithic world dump is serialized.

## Code boundary

Active branch:

```text
agent/esp32-native-gameplay-changemap-transition
```

Script checkpoint commits:

```text
ffa5eee4867c7c5a138c49928bdfcb2114577500
  ESP32: expose bounded script-state checkpoint API
3194e341c2f7eea735c737d3c812a4dd33bc5c9d
  ESP32: implement bounded script-state checkpoint snapshot
fa495ebdbcf90518213692bf4c878cb88f656880
  ESP32: persist native script state in checkpoint v3
fd206c5238ac2db62939d100bf3d08ac39081c69
  ESP32: bound script checkpoint payload to 512 bytes
```

GitHub Actions `esp32-cyd` run #275 / run ID `35199788280` passed on exact code boundary `fd206c5238ac2db62939d100bf3d08ac39081c69`.

## V3 format

```text
path = /sd/DoomRPG-ESP32.sav
magic = DRPGSAV3
version = 3
recordBytes = 808
legacy v1/v2 read compatibility = retained
write format = v3
```

V3 keeps the proven 132-byte v1 core and the v2 resource section, then appends exactly one `EspMapScriptStateSnapshot`.

The script section is compact semantic state only:

```text
sourceArenaFNV1a
eventCount
byteCodeCount
eventStateBytes
removedCommandBytes
storageBytes
storage[]
```

`storage[]` contains the existing packed event states followed by removed-command bits. No owner pointers are persisted.

Hard bound:

```text
ESP_MAP_SCRIPT_STATE_SNAPSHOT_MAX_BYTES = 512
```

Entrance uses:

```text
events = 93
byteCodes = 265
event state bytes = 47
removed-command bytes = 34
script storage bytes = 81
```

Restore validates immutable runtime identity, exact event/bytecode counts, exact packed sizes, zeroed unused payload bytes and a post-restore semantic fingerprint before session configure. Incompatible or corrupt sections fail closed.

## Hardware proof — rollback of post-SAVE mutations

An earlier V3 checkpoint was taken before later dialog/script/resource mutations. After more gameplay, LOAD restored the saved player/resource/script state and the post-SAVE dialog/resource interactions became available again. This proved that mutations made after SAVE are not accidentally retained across LOAD.

The fresh Entrance script fingerprint was:

```text
scriptFNV = f9e3d9df
```

A LOAD of that checkpoint restored:

```text
resources=restored/1/43B
script=restored/93/265/81B/f9e3d9df
world=resources+script-restored+others-fresh
```

The previously consumed post-SAVE medkit reappeared and the corresponding dialog/event sequence became executable again.

## Hardware proof — persistence of pre-SAVE mutations

The mirror test then saved **after** consuming two Armor Shards plus the Small Medkit and after advancing the relevant event/script states.

Hardware SAVE witness:

```text
[NATIVESAVE] SAVE path=/sd/DoomRPG-ESP32.sav
             version=3 bytes=808
             map=1 gameplayLoadMapId=1
             pos=160,1504 angle=64
             playerFNV=549e6620 runtimeFNV=c3882516
             sourceBytes=21823 sourceCrc=623f34e4
             recordCrc=dc35a833
             resources=3/43B sprites=344
             script=93/265/81B scriptFNV=f9e59e9f
             atomic=temp+backup+rename
             world=resources+script-restored+others-fresh
```

After that SAVE, gameplay deliberately diverged again:

```text
Bullet Clip consumed -> ammo1 8 -> 12
Fire Ext consumed -> weapon=1 weapons=0006 ammo0=10 ammo1=12
playerFNV -> a6e115a7
resource consumed count -> 5
```

LOAD then restored the exact checkpoint:

```text
[PLAYERRES] READY ... playerFNV=549e6620
[PLAYERRES] RESTORE ... consumed=3 bytes=43
[NATIVESAVE] LOAD ...
             version=3 bytes=808
             pos=160,1504 angle=64
             playerFNV=549e6620
             resources=restored/3/43B
             script=restored/93/265/81B/f9e59e9f
             world=resources+script-restored+others-fresh
```

The restored HUD confirmed the post-SAVE Bullet Clip and Fire Ext state was gone:

```text
[ENGINESESSION] HUD ... hp=30/30 armor=8/20 weapon=2 ammo=8
```

Most importantly, after LOAD the player backed out of tile 738 and the event that had already been completed **before** the SAVE remained ineligible:

```text
[MOVEEVENT] EXIT-PREFLIGHT ... tile=738 ... status=NO_ELIGIBLE event=79 eligible=0 ...
[MOVEEVENT] EXIT ... tile=738 ... status=NO_ELIGIBLE event=79 eligible=0 ...
```

That is the hardware witness that saved mutable event state survives LOAD, rather than merely restoring a matching checksum.

## Two-direction script contract now proven

```text
script/event mutation after SAVE -> rolled back by LOAD
script/event mutation before SAVE -> preserved by LOAD
```

Together with the existing V2 resource proof, V3 now hardware-proves checkpoint persistence for:

```text
settled player pose
EspNativeGameplayPlayerState
EspNativeGameplayPlayerResources consumed overlay
EspMapScriptState event states + removed-command bits
```

## Session / memory invariants after LOAD

The complete session reconstructed successfully through fresh BSP/runtime load, HUD reprime, raw-flash reuse, cache priming and resident input. The permanent memory invariants remained true:

```text
shapeData == NULL
mediaTexels == NULL
```

Observed long-session steady point during this test:

```text
heap8 = 17724
largest8 = 8692
```

This level remained stable across the final LOAD/replay segment. It is still below the desired fragmentation/headroom target and remains a review item, but this hardware run does not show a new per-LOAD leak.

## Boundary after V3

Still intentionally fresh after LOAD:

```text
line open/locked state
line texture variants
automap reveal state
monster mutable state/positions/activation/combat consequences
destructibles
gameplay RNG state
```

Continue persistence one explicit owner family at a time. CHANGEMAP production code remains present on this branch but still requires its separate real-CYD level-exit validation.
