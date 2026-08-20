# DoomRPG-RE

![image](https://github.com/Erick194/DoomRPG-RE/assets/41172072/258e99d9-b122-4cbe-8659-2fd0f4105068)<br />
https://www.doomworld.com/forum/topic/129997

## ESP32 CYD port

This repository also contains an actively developed native port for the classic
ESP32-2432S028R Cheap Yellow Display (CYD), targeting the no-PSRAM board with a
160x120 RGB565 engine framebuffer, exact 2x TFT output, SD-backed resources and
bounded ESP32-specific graphics/resource management.

Start here:

- [`ESP32/README.md`](ESP32/README.md) — build, flash and architecture guide
- [`ESP32/PORTING_STATUS.md`](ESP32/PORTING_STATUS.md) — exact current hardware recovery point
- [`ESP32/DOCUMENTATION.md`](ESP32/DOCUMENTATION.md) — documentation map and milestone archives

## Español
Doom RPG ingeniería inversa por [GEC]<br />
Creado por Erick Vásquez García.

Versión actual 0.2.2

Requiere CMake para crear el proyecto.<br />
Requisitos para el projecto:
  * SDL2
  * SDL2-Mixer
  * Zlib
  * FluidSynth

Configuración por defecto de las teclas.

Move Forward: Up<br />
Move Backward: Down<br />
Move Left: A<br />
Move Right: D<br />
Turn Left: Left<br />
Turn Right: Right<br />
Atk/Talk/Use: Return<br />
Next Weapon: Z<br />
Prev Weapon: X<br />
Pass Turn: C<br />
Automap: Tab<br />
Menu Open/Back: Escape<br />

Trucos originales del juego:

Versión J2ME/BREW:<br />
Abres menu e ingresa los siguientes numeros.<br />
3666 -> Abre el menú debug.<br />
43629 -> Da al jugador maximo de salud y armadura.<br />
4332 -> Da al jugador todas las llaves, items y armas.<br />
3366 -> Inicia el testeo de velocidad, "Benchmark".<br />

## English
Doom RPG Reverse Engineering By [GEC]<br />
Created by Erick Vásquez García.

Current version 0.2.2

You need CMake to make the project.<br />
What you need for the project is:
  * SDL2
  * SDL2-Mixer
  * Zlib
  * FluidSynth

Default key configuration:

Move Forward: Up<br />
Move Backward: Down<br />
Move Left: A<br />
Move Right: D<br />
Turn Left: Left<br />
Turn Right: Right<br />
Atk/Talk/Use: Return<br />
Next Weapon: Z<br />
Prev Weapon: X<br />
Pass Turn: C<br />
Automap: Tab<br />
Menu Open/Back: Escape<br />

Original game cheat codes:

J2ME/BREW Version:<br />
3666 -> Opens debug menu.<br />
43629 -> Gives max health and armor to the player.<br />
4332 -> Gives all keys, items and weapons.<br />
3366 -> Starts speed test "Benchmark".<br />
