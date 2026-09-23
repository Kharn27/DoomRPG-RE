# Milestone — Native gameplay save/load V7 Automap persistence

Status: **REAL-CYD PASS** for bounded Automap checkpoint persistence.

## Git / hardware boundary

```text
base main = 38a70412b0e28cf5afb8d33aa4a0a82ae73172c6
branch = agent/esp32-native-automap-save-v7-hud-notches
hardware-tested V7 save code boundary = 69e9bca3173f584086ea39c0608c8eace5bab317
current hardware-tested combined branch head = 2d9dcfcbc022e02a4810da3aa2f3eb60bedf933e
esp32-cyd CI #602 = SUCCESS
static RAM = 44944 B
flash = 745165 B
```

The V7 save implementation itself was exercised on hardware before the later
HUD/pickup-help fixes. Those later commits do not modify the save file path.
The current combined branch head is separately hardware-tested for gameplay
continuation and CI-valid.

## Goal

Persist only the compact native Automap semantic owner required to restore the
player's explored map after LOAD. No framebuffer, renderer cache, legacy entity
graph, map-wide texture memory or ZIP runtime dependency is serialized.

The permanent extension is:

```text
V6 record = 1532 B
+ EspMapAutomapSnapshot = 404 B
V7 record = 1936 B
magic = DRPGSAV7
version = 7
V1-V6 read compatibility = retained
current writes = V7
```

`EspMapAutomapSnapshot` stores bounded pointer-free reveal state:

```text
runtime/map identity
line count + line reveal bitset
sprite count + sprite reveal bitset
visited-cell projection remains owned by EspMapState
```

The V7 loader validates source/runtime identity, counts, bitset sizes and tail
bits before restore. The record CRC covers the complete streamed V7 payload.

## Real-CYD PASS

The hardware test saved a partially explored Entrance map, continued playing,
then loaded the checkpoint. The loader reported the restored Automap snapshot:

```text
[NATIVESAVE] LOAD ... version=7 bytes=1936 ...
  automap=restored/50L/46S/39V/08fde8e2
  world=resources+script+lines+action-removals+crate-transforms+automap-restored+others-fresh
```

Opening the map after LOAD immediately recovered the saved exploration instead
of resetting discovery to the player's current position:

```text
[AUTOMAP] FRAME ... visited=27 lines=50 specials=1 items=4 ...
```

The user physically confirmed that previously discovered Automap geometry
survives SAVE/LOAD.

## Compatibility / invariants

The V7 change preserves:

```text
shapeData == NULL
mediaTexels == NULL
no PSRAM
/DoomRPG-ESP32.pak remains runtime backing store
no raw framebuffer persistence
no legacy pointer graph persistence
V1-V6 saves remain read-only compatible
```

## Result

**Automap checkpoint persistence: REAL-CYD PASS.**

The previous Automap milestone's deferred "checkpoint persistence of Automap
reveal state" item is closed by V7. Remaining Automap work is limited to other
action parity / GIVEMAP hardware execution and any future optional enhancements.
