# Host-only desktop/SDL behavioral oracles

This directory contains **reference code, not ESP32 firmware sources**.

## SDL-F33 — exact legacy SDL shim oracle (never linked to firmware)

- `esp32_sdl.cpp` was moved **unchanged** from
  `ESP32/src/esp32_sdl.cpp` to this directory. Git blob:
  `b8385250706b8b5396c3f3808e5084ad1f318425`.
- Its original SDL-A/B/C/D/E/F implementation is still **compiled and
  executed by host fixtures**, especially `sdl_shim_permanent_host.py` against
  native texture primitives; the file remains an executable oracle, not an
  ESP32 production translation unit.
- `sdl_f3_uncompiled_shim_host.py` pins the full Git blob, source
  location, negative PlatformIO src_filter and the native display owner.
  `doomrpg_oracle_retirement_host.py` executes the actual SCons pre-script
  against an adversarial fake project: a shim reintroduced under `src` or a
  missing host oracle must both fail closed.
- `ESP32/include/SDL.h` and `ESP32/include/SDL_mixer.h` still
  support historical desktop/geometry ABI oracles. They are **not** retired
  by F34; moving them needs its own compile/dependency audit.

## SDL-F34 — retired display attachment header now host-only

- `esp32_sdl_platform.h` moved byte-for-byte from
  `ESP32/include/esp32_sdl_platform.h` into this directory, adjacent to
  the uncompiled `esp32_sdl.cpp`. Its Git blob is
  `6d89897ebbcccaba4dd7e041520fc7776bae6142`.
- The old `Esp32Sdl_attachDisplay(TFT_eSPI*)` declaration is now **only
  available to the shim host oracle**. The shim's quoted include continues
  resolving directly from its directory, with zero shim source changes.
- The production pre-script refuses a returned header in `ESP32/include`
  or a missing test/oracles header; the real SCons pre-script is exercised
  under both adversarial conditions by `doomrpg_oracle_retirement_host.py`.
- The F3 host gate pins the original full Git blob and scans production
  sources and includes for forbidden imports. The SDL-A host C++ fixture
  still compiles/executes the real shim and native texture owner.
- **No production TU, framebuffer, renderer, gameplay, PAK owner, audio,
  gamma or backlight is modified**; the old declaration was never a live
  firmware ABI. No new CYD hardware PASS or RAM improvement is claimed.

## SDL-F35 — byte-exact host mixer ABI

- `SDL_mixer.h` moved unchanged from `ESP32/include` to this directory,
  blob `59fe49318cb550277f999b5c6307c746eb7f6ad8`.
- The real `src/Sound.h` remains unchanged: native `Mix_Chunk` is opaque,
  desktop and geometry C11 tests explicitly use the host include directory.
- Pre-script fails closed if the old production header returns or the host
  oracle vanishes. The F23/F31 before-after ABI tests are preserved.
- `SDL.h` stays in the production include directory pending a separate audit.

## SDL-F32 — retired SDL_RWops indexed BMP reader

- `esp32_bmp.cpp` is the byte-exact historical ESP32 SDL compatibility
  decoder (Git blob `88a62dfdd3d4fbb2c991dba4d480d4596834563c`).
- `esp32_bmp.h` is its byte-exact historical SDL ABI header
  (Git blob `f896aa522c6c1cbf3d38852cadc086331f98405a`).
- Both previously sat under `ESP32/src` and `ESP32/include`. They were
  **already excluded from compilation and absent from the linked firmware**
  in SDL-F22; F32 only separates them physically from the production tree.
- `ESP32/test/sdl_f22_bmp_tu_retirement_host.py` verifies both blob hashes,
  proves these files are absent from production directories, excludes the
  decoder via real PlatformIO compiledb/ELF checks, and requires the permanent
  native `EspNativeIndexedBmp_open` PAK-backed reader.
- `ESP32/test/sdl_dead_duplicate_bmp_host.py` preserves the earlier SDL-B7
  legacy behavior and no-legacy-caller checks.

The build flags `-<esp32_bmp.cpp>` and `-<esp32_sdl.cpp>` intentionally stay
as fail-closed safeguards. Never point runtime asset loading back to this
legacy decoder or to the ZIP; `/DoomRPG-ESP32.pak` remains the backing store.

This **source-isolation-only** cleanup changes no native function bodies,
firmware ABI, framebuffer, map data, gameplay RNG or persistent allocations.
CI verification of identical compiled owner sets and no SDL symbols must run
before review; no new physical CYD behavioral PASS is claimed by this move.
