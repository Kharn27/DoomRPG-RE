# Native requested-map raw-flash slot reuse — hardware PASS

This milestone reuses an already committed raw internal-flash gameplay slot when
and only when it matches the **map the runtime is actually requesting**.

The policy is deliberately generic. Entrance is only the hardware witness used
for this test; there is no Entrance-specific cache rule. A future New Game,
CHANGEMAP or Save/Load path must pass the real target map id into the same storage
API.

## Locked boundary

```text
base main = 9bd45cc0eb790a7b0894774426f996ee7ae6ce72
branch = agent/esp32-native-map-flash-reuse
hardware-tested code boundary = 8dd0a06f293b801e9afe3097bf19c57d3e1037b7
status = REAL-CYD GENERIC REQUESTED-MAP FLASH SLOT REUSE PASS
branch policy = LOCKED; docs-only tail only
```

Normal GitHub Actions environment `esp32-cyd` run `33867680174` passed on the
exact tested SHA and uploaded:

```text
doom-rpg-esp32-cyd-8dd0a06f293b801e9afe3097bf19c57d3e1037b7
artifact id = 9934627355
```

CI is compile/link evidence only. The real classic CYD Serial trace is the
runtime authority.

## Why this milestone exists

The preceding raw-flash backing milestone deliberately rebuilt the complete
current-map gameplay working set before every resident gameplay session. That
made active gameplay dramatically smoother, but the real-CYD Entrance staging
cost was:

```text
buildUs = 8442586
```

or about 8.44 seconds of erase + SD copy + flash readback verification.

Because the raw slot survives reset/firmware upload unless explicitly erased,
rebuilding an unchanged slot on every load wastes time and flash erase cycles.

The goal here is therefore:

```text
requested map
 -> inspect committed raw slot
 -> prove source PAK + requested-world identity
 -> prove flash payload integrity
 -> exact HIT: arm existing slot
 -> any mismatch/staleness: use the already-proven full rebuild path
```

## Generic requested-map contract

The public storage boundary is now:

```text
EspAssetPack_mapFlashPrepare(targetMapId)
```

The caller supplies the map it is actually trying to load. The storage layer does
not infer "Entrance", "new game", or any fixed startup map.

At the tested bootstrap boundary, `EspPlayerView.targetMapId` is passed directly
to `EspAssetPack_mapFlashPrepare()`.

A reuse HIT requires all of the following to agree:

```text
requested targetMapId
requested BSP name hash from the native map catalog
source PAK byte size
source PAK index/data offsets
entry count
index byte size
source PAK index FNV-1a
payload flash offset
staged byte count
excluded BSP byte count
excluded BSP count
exact excluded-span topology for this requested map
flash index readback FNV
flash payload readback FNV
```

The source index FNV covers the original PAK entry metadata records, including
entry hashes, offsets, sizes, CRC32 values and flags. The requested-map plan is
recomputed from the current native map catalog rather than trusting the cached
header alone.

Therefore a slot staged for map A cannot be accepted for map B merely because it
is committed and structurally valid.

Examples of mandatory MISS/rebuild cases include:

```text
cached map id/hash != requested map
source PAK/index changed
catalog/excluded-map layout changed
slot metadata/layout changed
flash index FNV mismatch
flash payload FNV mismatch
missing/invalid transactional header
```

Any such case returns to the existing complete staging path. Once gameplay is
armed, silent SD fallback remains forbidden.

## Rebuild path deliberately retained

This milestone does **not** replace or weaken the hardware-proven stage path.
`EspAssetPack_mapFlashStage(targetMapId)` remains the conservative fallback:

```text
SD authoritative PAK
 -> erase raw partition
 -> copy original PAK index
 -> copy all data except other known BSPs
 -> flash readback FNV verification
 -> commit header last
 -> arm raw-flash backing
```

That fallback remains valid for a first boot, a stale slot, a different requested
map, or a changed PAK.

## Real-CYD reuse witness

The previous test had already committed the Entrance slot:

```text
requested map = 1
resource = /intro.bsp
sourceIndexFNV = 3a51cc4d
payloadFNV = 9ec04e22
staged payload = 2248743 B
metadata offset = 12288 B
```

On exact hardware-tested SHA `8dd0a06f...`, the real CYD reported:

```text
[MAPFLASH] REUSE HIT requestedMap=1 current=/intro.bsp cachedMap=1 sourceIndexFNV=3a51cc4d payloadFNV=9ec04e22 verifyUs=361875 rebuild=no
[MAPFLASH] ARM map=1 active=1 verified=1 reused=1 staged=2248743 metadata=12288 prepareUs=363258 buildUs=0 resident=1
```

Critical negative witnesses in the supplied trace:

```text
no [MAPFLASH] ERASE
no [MAPFLASH] COPY
no [MAPFLASH] MISS
```

So the committed slot survived, was fully revalidated, and was armed without
rewriting it.

## Load-time result

Previous full rebuild:

```text
8442586 us  ~= 8.44 s
```

Validated reuse:

```text
prepareUs = 363258 us ~= 0.363 s
verifyUs  = 361875 us ~= 0.362 s
buildUs   = 0
```

The reuse preparation is therefore about **23.2x faster** than the preceding
full rebuild witness, while still rereading and FNV-validating the staged flash
payload before accepting it.

Nearly all reuse time is the integrity readback of the staged internal-flash
payload. The source-plan/index identity check is small compared with that full
payload verification.

## Active-gameplay regression witness

After reuse ARM, the backing remained the already-proven raw internal flash:

```text
[PAKIO] ... backing=raw-flash ... resident=1
```

Representative startup cache timings were essentially unchanged from the prior
raw-flash milestone:

```text
SMALL-COLD  = 228559 us
SMALL-WARM  = 178358 us
LARGE-LEARN = 178381 us
LARGE-WARM  = 177638 us
```

Representative sprite profiles remained around 30-37 ms in this trace, and the
resident gameplay service armed normally.

The reuse optimization therefore affects map-load preparation only; it does not
alter the proven gameplay backing/cache behavior.

## RAM / invariant regression

The resident owner boundary remained unchanged:

```text
CACHE_PRE  heap8=50976 largest8=42996
CACHE_POST heap8=27368 largest8=20468
configured resident owner = 23592 B
warmup heap8=24708 largest8=20468
```

The session again reached:

```text
[ENGINESESSION] READY ... shapeData=0x0 mediaTexels=0x0
```

So the permanent invariants remain:

```text
shapeData == NULL
mediaTexels == NULL
no map-wide RAM copy of the staged payload
19 KiB resident RAM cache remains L1
```

The existing audio reserve warning remains unchanged and is outside this storage
milestone.

## Remaining startup debt

Before raw-flash ARM, the current bootstrap still performs some first-frame/HUD
asset work directly from SD. In this hardware run, several pre-arm PAKIO batches
were still hundreds of milliseconds.

That is a separate startup-order/performance boundary. It does not invalidate the
reuse HIT and should not be mixed into this milestone unless a later bounded
startup-storage milestone specifically owns it.

## Save/Load and CHANGEMAP implications

This milestone intentionally prepares the storage API needed by future world
transitions, but it does **not** make SAVEGAME or CHANGEMAP live.

The required future rule is now clear:

```text
load/transition chooses target world
 -> targetMapId passed to EspAssetPack_mapFlashPrepare(targetMapId)
 -> same exact map/source slot => reuse HIT
 -> different/stale slot => rebuild requested world
 -> resident gameplay arms only after prepare succeeds
```

A save that points to Level 05 must therefore prepare Level 05; an Entrance slot
must MISS by world identity and rebuild instead of being reused accidentally.

## What this milestone does not solve

```text
- SAVEGAME / CHANGEMAP production transition ownership remains deferred;
- the pre-arm first-frame/HUD SD path remains startup debt;
- the 288-record L1 recycle policy is unchanged;
- audio RAM remains unresolved;
- gameplay/visual deferred families are unchanged.
```

The next milestone should return to a bounded gameplay family after merge unless
the actual merged repo reveals a more urgent correctness boundary. Strong
candidates include monster attack visual/player-pain animation and other missing
combat presentation assets, while keeping unrelated opcode families fail-closed.
