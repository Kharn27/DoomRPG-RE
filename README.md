# Doom RPG ESP32 CYD

A native Doom RPG engine for the **ESP32-2432S028R classic Cheap Yellow Display (CYD)**, targeting the original no-PSRAM board.

> **Status:** active work in progress, developed and validated on real classic CYD hardware.

![ESP32 CYD Build](https://github.com/Kharn27/DoomRPG-RE/actions/workflows/esp32-cyd.yml/badge.svg)

## What this project is

This project is building a purpose-designed embedded Doom RPG runtime for the classic CYD rather than keeping the desktop reverse-engineered engine as the permanent architecture.

The target architecture is:

```text
original Doom RPG data / behavior
 -> ESP32-native parsers
 -> compact immutable map runtime
 -> small explicit mutable gameplay owners
 -> native event / gameplay engine
 -> native renderer
 -> 160x120 RGB565 framebuffer
 -> exact 2x presentation on the 320x240 ILI9341
```

The classic CYD has no PSRAM, so the engine is deliberately designed around bounded RAM ownership, SD-backed assets and compact native state instead of map-wide texture buffers or pointer-heavy desktop structures.

Current hardware-validated work already includes the native map/runtime path, rendering, touch gameplay controls, event execution, doors, pickups/resources, generic monster combat/retaliation and the first live native monster movement publication. The exact current boundary is always recorded in [`ESP32/PORTING_STATUS.md`](ESP32/PORTING_STATUS.md).

## Target hardware

```text
Board       ESP32-2432S028R classic CYD
MCU         ESP32-D0WD-V3, dual core @ 240 MHz
Flash       4 MB
PSRAM       none
Display     ILI9341, 320x240
Touch       XPT2046
Storage     microSD
Framebuffer 160x120 RGB565 = 38400 bytes
Output      nearest-neighbor 2x
```

`/DoomRPG-ESP32.pak` is the native backing store for the migrated map/gameplay runtime. The current firmware still has a **transitional startup dependency on `/DoomRPG.zip`** for legacy HUD/layout resources and will not complete engine initialization if those ZIP resources are unavailable. Removing that runtime ZIP dependency remains part of the migration toward the fully native ESP32 architecture.

## Build

The ESP32 target uses PlatformIO:

```bash
cd ESP32
pio run -e esp32-cyd
```

For flashing, asset preparation and the current hardware workflow, see [`ESP32/README.md`](ESP32/README.md).

GitHub Actions runs the same normal `esp32-cyd` compile as a pre-hardware check. A green CI build means the firmware compiles; **real CYD Serial logs remain the final runtime authority**.

## Documentation

Start here:

- [`ESP32/README.md`](ESP32/README.md) — build, flash and ESP32 overview
- [`ESP32/PORTING_STATUS.md`](ESP32/PORTING_STATUS.md) — authoritative hardware-tested recovery point
- [`ESP32/DOCUMENTATION.md`](ESP32/DOCUMENTATION.md) — documentation map and milestone archive
- [`ESP32/ARCHITECTURE.md`](ESP32/ARCHITECTURE.md) — permanent native engine direction

## Project lineage and credits

This project would not exist without the **DoomRPG-RE** reverse-engineering work by **Erick Vásquez García / [GEC]**.

Their project recovered Doom RPG behavior and formats and provides the executable desktop reference used throughout this port:

- [Erick194/DoomRPG-RE](https://github.com/Erick194/DoomRPG-RE)
- [Doomworld: Doom RPG Reverse Engineering](https://www.doomworld.com/forum/topic/129997)

That reverse-engineering work belongs to its original authors and contributors. This repository does **not** claim authorship of it.

The work developed here is the new **ESP32-native CYD engine and embedded architecture**: compact storage/runtime formats, explicit memory ownership, native gameplay/event execution, native renderer/input integration and the hardware-specific path required to run Doom RPG on a no-PSRAM classic CYD.

During migration, some desktop/J2ME-derived reference code remains in the repository because it is still useful as an executable specification. It is not the intended permanent ESP32 engine architecture and may eventually disappear from the ESP32 build completely.

## Game data and trademarks

This project is an engine/port and does not grant rights to the original Doom RPG game data. Users are responsible for supplying any required original game assets legally.

DOOM and Doom RPG are trademarks/properties of their respective owners. This is an independent community project and is not affiliated with or endorsed by id Software, Bethesda or ZeniMax.

## License

The repository is distributed under the existing [GNU GPL v3](LICENSE). See the repository history and upstream project for authorship and attribution of inherited reverse-engineered source material.
