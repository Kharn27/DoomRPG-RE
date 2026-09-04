# Native current-map raw-flash PAK backing — hardware PASS

This milestone moves the active map's complete gameplay PAK working set off the
microSD critical path and into internal ESP32 flash before resident gameplay is
armed.

The goal is determinism, not minimum loading time: paying a bounded load-time
cost is preferred to unpredictable multi-hundred-millisecond SD stalls during
turn-based gameplay.

## Locked boundary

```text
base main = a6a3334c2235f14a69b8c8c9acd9b1c3a0485c01
branch = agent/esp32-native-flash-pak-backing
hardware-tested code boundary = 3fcd255015315e67222de7bcedf63f76220c7820
status = REAL-CYD CURRENT-MAP RAW-FLASH BACKING PASS
branch policy = LOCKED; docs-only tail only
```

Normal GitHub Actions environment `esp32-cyd` run `33862354211` passed on the
exact tested SHA and uploaded:

```text
doom-rpg-esp32-cyd-3fcd255015315e67222de7bcedf63f76220c7820
artifact id = 9932634573
```

CI is only a compile/link gate. The real classic CYD serial trace is the runtime
authority.

## Why this milestone exists

The preceding measurement milestone proved that renderer PAK I/O, not the fixed
LCD transfer, was the dominant source of erratic stalls.

On the old SD-backed path, one physical seek/read commonly cost about 9 ms, and
64-call PAK batches could cost hundreds of milliseconds. The resident 19 KiB RAM
cache helped dramatically while hot, but its 288 range-record table could fill
and globally recycle. In the fire room this produced the decisive cliff:

```text
range records: 288/288 -> 23/288
SPRITEPROFILE: ~36.7 ms -> ~241.8 ms
frame:         ~335 ms  -> ~736 ms
VIDEO present: remained ~34.4 ms
```

The whole PAK cannot fit the original `no_ota.csv` data partition:

```text
DoomRPG-ESP32.pak = 2457398 B
old data partition = 1966080 B
old usable payload with 4 KiB reserve = 1961984 B
whole-PAK shortfall = 495414 B
```

The chosen architecture therefore stages one complete current-map gameplay
working set instead of attempting a partial cache with SD fallback.

## Permanent contract

The backing hierarchy is now:

```text
/DoomRPG-ESP32.pak on SD
        |
        | map/session load only
        v
raw internal flash current-map slot
        |
        | gameplay backing
        v
19 KiB resident RAM cache (L1)
        |
        v
native renderer/gameplay
```

Rules:

```text
- SD remains the authoritative source archive.
- The raw flash slot is single-map/single-session backing storage.
- The slot is complete before resident gameplay is armed.
- Original PAK index/offset semantics are retained.
- The 19 KiB RAM resident cache remains unchanged as L1.
- Active gameplay does not silently fall back to SD.
- Access to a BSP intentionally excluded from the staged set fails closed.
- Header/commit metadata is written only after copy + flash readback verification.
- shapeData and mediaTexels remain NULL.
```

The partition is currently labelled `spiffs` for compatibility with the ESP32
partition subtype, but it is intentionally **not mounted as SPIFFS**. It is owned
as raw flash through `esp_partition_*`.

## Classic CYD partition layout

The production `esp32-cyd` environment now uses
`ESP32/partitions_cyd_raw_pak.csv`:

```text
nvs       0x009000  0x005000
otadata   0x00e000  0x002000
app0      0x010000  0x140000  (1310720 B)
spiffs    0x150000  0x2A0000  (2752512 B, raw PAK slot)
coredump  0x3F0000  0x010000
```

The tested firmware binary is 644144 B, leaving 666576 B inside the 1.25 MiB
application partition at this boundary.

When flashing this milestone, the new `partitions.bin` is part of the contract.
Flashing only `firmware.bin` onto a board that still has `no_ota.csv` leaves the
old 1966080 B data partition and correctly triggers the fail-closed capacity
check.

## Entrance stage plan — real CYD

Entrance remained the canonical witness:

```text
map = 1
resource = /intro.bsp
runtime arena FNV = c3882516
source CRC32 = 623f34e4
shapeData = NULL
mediaTexels = NULL
```

Exact staging plan:

```text
pack = 2457398 B
PAK entries = 241
PAK index = 4820 B
metadata reservation = 12288 B
excluded non-current BSPs = 12
excluded BSP bytes = 203811 B
staged payload = 2248743 B
raw partition = 2752512 B
headroom = 491481 B
fits = yes
```

Hardware copy/verification:

```text
[MAPFLASH] ERASE bytes=2752512 buffer=4096 owner=transient
[MAPFLASH] COPY indexFNV=3a51cc4d payloadFNV=9ec04e22 verified=yes
[MAPFLASH] READY map=1 staged=2248743 metadata=12288 excluded=12/203811
           buildUs=8442586 backing=raw-internal-flash SDGameplayReads=forbidden
[MAPFLASH] ARM map=1 active=1 verified=1 resident=1
```

The approximately 8.44 s staging cost is real and was visible to the user. This
is an intentional load-time-for-gameplay-determinism tradeoff, not hidden latency.

## Hardware performance result

The raw-flash backend is decisively faster than the SD miss path.

Before staging, first-session SD samples still showed the old behavior:

```text
PAKIO 64 calls: ~580 ms total, ~9.1 ms average
PLANEPROFILE:   ~494.6 ms
VIDEO present:  ~34.4 ms
```

After `MAPFLASH ARM`:

```text
PAKIO backing = raw-flash
common 64-call batches with no L1 miss = ~0.3-1.2 ms total
miss-heavy raw-flash batches = commonly ~4-10 ms total
raw-flash physical-read maxima = commonly < ~0.6 ms in this trace
VIDEO present = still ~34.4 ms
```

Startup cache witnesses:

```text
SMALL-COLD total = 228233 us
SMALL-WARM total = 178176 us
LARGE-LEARN total = 178270 us
LARGE-WARM total = 177531 us
```

For comparison, the preceding SD-backed measurement recorded approximately:

```text
SMALL-COLD = 2100916 us
SMALL-WARM = 324151 us
LARGE-LEARN = 315502 us
LARGE-WARM = 298062 us
```

The cold path therefore dropped from about 2.10 s to 0.23 s while preserving the
same resident RAM-cache architecture.

## Fire-room cache-recycle witness

The important regression test was the room with many fire sprites, where the SD
version felt erratic and previously exposed the 288-record global recycle cliff.

The recycle still occurs, proving this milestone did **not** hide or rewrite the
L1 policy:

```text
frame A: cache entries=288/288, SPRITEPROFILE=29824 us
frame B: cache entries=78/288,  SPRITEPROFILE=36084 us
```

The crucial difference is that refills now come from internal flash. The old
~242 ms sprite cliff did not return. In the supplied route, representative
sprite profiles remained broadly around 27-47 ms and complete gameplay redraws
around 178-225 ms, including doors, hazards and pickups.

The user explicitly reported the fire-room route as "carrément plus fluide",
with the prior intermittent stalls no longer making traversal painful.

## Functional regression witnesses

The same hardware session retained live gameplay across:

```text
TURN and MOVE
SELECT regular door
4-frame door open/close animation
weapon rendering
movement hazard damage
red damage flash + top-bar damage text
health pickup
white pickup flash + pickup text
monster-turn scheduling
native sprite projection
```

Examples from the tested trace:

```text
[RESIDENTGAMEPLAY] READY ... TURN+MOVE=armed
[ENGINESESSION] READY ... shapeData=0x0 mediaTexels=0x0
[ACTION] SELECT ... status=DOOR_OK
[DOORANIM] COMPLETE ... transaction=committed
[HAZARD] COMMIT ... message="3 damage!"
[PLAYERRES] COMMIT ... message="Got Health Vial"
```

No active-gameplay SD fallback or `MAPFLASH MISS` was observed in the supplied
hardware trace.

## RAM boundary

The raw flash payload itself consumes no map-wide RAM. The retained resident L1
still costs its established owner allocation:

```text
CACHE_PRE  heap8=50976 largest8=42996
CACHE_POST heap8=27368 largest8=20468
observed cache delta = 23608 B
configured resident owner = 23592 B
```

After the startup warmup:

```text
heap8=24708
largest8=20468
```

Later live gameplay remained stable around:

```text
heap8=20840
largest8=18420
```

The reserve diagnostic reports:

```text
audioI2SDMA=16384
audioBuffers=8192
general=32768
total8Target=57344
margin8=0
advisory=REVIEW_HEADROOM
```

This is not a failure of the flash-backing milestone, but it is a real warning:
audio remains deferred and must not be enabled under an assumption that this RAM
is already available.

## What this milestone does not solve

```text
- the 288-record L1 global recycle policy still exists;
- current-map flash staging currently costs ~8.44 s on this Entrance run;
- persistent validated reuse of an already-staged same-map slot is not yet owned;
- map transition rebuild/reuse policy is not yet production-complete;
- SD is still required as the authoritative source for staging;
- transitional ZIP startup debt is unchanged;
- audio RAM reservation is unresolved.
```

The important architectural result is that the L1 recycle cliff is no longer an
SD-latency cliff during active gameplay.

## Next bounded storage direction

After merge, a strong storage-only follow-up is **persistent map-slot reuse**:

```text
boot/map load
 -> inspect committed raw-slot header
 -> validate source PAK identity + map identity + metadata
 -> verify flash payload as required
 -> HIT: arm existing slot without erase/copy from SD
 -> MISS/stale: rebuild exactly as this milestone does
```

That can reduce the ~8.44 s repeated Entrance startup without weakening the
validated no-SD-gameplay contract. It must be done on a fresh `agent/*` branch
after re-reading merged `main`; do not extend code on this locked branch.
