# Milestone — ESP32 consolidation: retire runtime ZIP asset source

Date: 2026-09-30

## Boundary

```text
base main = c735979a1dcd645208946adececc1ef478bf105f
branch = agent/esp32-consolidation-legacy-runtime-v7
hardware-tested code head = eb18ea2c5fc090161cee148b1f9aea52c7dc91d9
esp32-cyd CI #1083 = SUCCESS
static RAM = 45760 B
flash = 807377 B
artifact id = 11088820981
active --wrap entries = 57
legacy desktop src/*.c units compiled = 16
```

## Goal

Remove the last runtime dependency on `DoomRPG.zip` from the ESP32 firmware.
The permanent asset backing remains `/DoomRPG-ESP32.pak`; no ZIP parser,
decompression path or ZIP fallback is allowed during runtime.

## Code change

The ESP32 build now excludes `src/Z_Zip.c`. Remaining desktop-derived startup
and menu consumers use the bounded transitional `EspLegacyAssetSource` API,
which opens one short `EspAssetPack` lease, performs exact lookup/read, then
closes it fail-closed.

Migrated consumers include:

```text
DoomRPG_createImage / DoomRPG_createImageBerserkColor
DoomRPG_fileOpenRead
HUD resource preflight
pre-render resource preflight
Render startup preflight
mappings.bin startup scratch load
main-menu intro asset plan
```

No map-wide buffer, decompression owner or alternate cache was introduced.
`mappings.bin` is read directly from the native PAK into the existing
framebuffer scratch before its compact arrays are installed.

## Build / ELF proof

CI #1083 succeeds on the exact hardware-tested code head:

```text
RAM   45760 B
Flash 807377 B
artifact 11088820981
```

Relative to merged main this saves 16 B static RAM and 8300 B flash.

Inspection of the produced firmware ELF finds none of the former ZIP/decompress
symbols:

```text
zipFile
openZipFile
closeZipFile
readZipFileEntry
readZipFileEntryInto
findAndReadZipDir
tinfl_decompress
```

Therefore `Z_Zip.c` and its miniz decompression code are absent from the final
firmware, not merely bypassed.

## Real-CYD proof

Cold boot on the real classic CYD validates the new source boundary:

```text
[DATA] Native PAK indexed, entries=241
[DATA] HUD resource preflight OK
[PRERENDER] ... backing=pak
[RENDERSTART] ... backing=pak
[MAPPINGS] mappings.bin bytes=8392 backing=pak
[BOOT] NORMAL READY ... shapeData=0x0 mediaTexels=0x0
[ALIVE] ... SD=ready PAK=ready ...
```

Start Game then loads all four intro assets from the PAK and completes the full
intro/disposal/native bootstrap path. Because the previous raw-flash slot held
Sector 1, Entrance forces a real world-identity miss and SD -> flash rebuild:

```text
[MAPFLASH] REUSE MISS requestedMap=1 ... cachedMap=2 reason=world-identity
[MAPFLASH] COPY ... verified=yes
[MAPFLASH] READY map=1 ... backing=raw-internal-flash SDGameplayReads=forbidden
[ENGINESESSION] READY map=1 ... shapeData=0x0 mediaTexels=0x0
```

The inverse direction is also hardware proven. Loading the V9 Sector 1 save
tears down Entrance, streams `/level01.bsp` from SD, restores the complete
checkpoint, then rebuilds the raw-flash slot for map 2:

```text
[NATIVESAVE] LOAD ... version=9 ... world=...restored-exact
[MAPFLASH] REUSE MISS requestedMap=2 ... cachedMap=1 reason=world-identity
[MAPFLASH] COPY ... verified=yes
[MAPFLASH] READY map=2 ... backing=raw-internal-flash SDGameplayReads=forbidden
[ENGINESESSION] READY map=2 ... shapeData=0x0 mediaTexels=0x0
```

Post-load gameplay is real, not a boot-only witness: player movement commits,
the ordered four-member monster sequence runs, both subtype-4 chains execute,
and a Bull Demon attack reaches MonsterAttackVisual/Retaliation commit. ALIVE
remains stable before and after gameplay at:

```text
heap=94016 heap8=28400 largest8=16372
```

## Result

`DoomRPG.zip` is no longer a runtime requirement of the ESP32 firmware.
The original ZIP remains only an offline/reference source where tooling may
need it; the device runtime consumes `DoomRPG-ESP32.pak`.

This milestone changes asset provenance only. Gameplay algorithms, RNG cadence,
map semantics and rendering ownership are unchanged.
