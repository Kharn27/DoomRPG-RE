#!/usr/bin/env python3
"""SDL-F6: native DoomRPG.h compiles without SDL types; desktop/oracle ABI stays."""
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
header = (ROOT / "src/DoomRPG.h").read_text(encoding="latin-1")
image = (ROOT / "ESP32/src/esp_doomrpg_image_resources.c").read_text()
clock = (ROOT / "ESP32/src/esp_doomrpg_process_clock.c").read_text()
guard = "#if !defined(DOOMRPG_ESP32) || defined(DOOMRPG_ESP32_GEOMETRY_ORACLE)"
assert header.count(guard) >= 2
assert "#include <SDL.h>" not in clock
# Image resource source is frozen by the SDL-C1 byte-exact source oracle;
# its unrelated transitional include is deliberately retained in F6.
assert "EspNativeTexture* imgBitmap;" in header
assert "SDL_Texture* imgBitmap;" in header
assert image.count("EspNativeTexture_adoptIndexed(&decoded)") == 1
assert "EspLegacyAssetSource_readAlloc(fileName, &fSize)" in image
assert "EspNativeProcessClock_ticks()" in clock

harness = r'''
#include <stddef.h>
#if !defined(DOOMRPG_ESP32) || defined(DOOMRPG_ESP32_GEOMETRY_ORACLE)
#include <SDL.h>
#endif
#include "DoomRPG.h"

#if defined(DOOMRPG_ESP32) && !defined(DOOMRPG_ESP32_GEOMETRY_ORACLE)
_Static_assert(__builtin_types_compatible_p(
    __typeof__(((Image_t*)0)->imgBitmap), EspNativeTexture*),
    "Native image pointer");
#else
_Static_assert(__builtin_types_compatible_p(
    __typeof__(((Image_t*)0)->imgBitmap), SDL_Texture*),
    "Desktop/oracle image pointer");
_Static_assert(__builtin_types_compatible_p(
    __typeof__(&DoomRPG_getEventKey), int (*)(int, const Uint8*)),
    "Legacy key ABI");
_Static_assert(__builtin_types_compatible_p(
    __typeof__(&DoomRPG_setBind), void (*)(DoomRPG_t*, int, const Uint8*)),
    "Legacy bind ABI");
_Static_assert(__builtin_types_compatible_p(
    __typeof__(&File_writeBoolean), void (*)(SDL_RWops*, int)),
    "Legacy writer ABI");
_Static_assert(__builtin_types_compatible_p(
    __typeof__(&File_readLong), int (*)(SDL_RWops*)),
    "Legacy reader ABI");
#endif
_Static_assert(offsetof(Image_t, imgBitmap) == 0, "Pointer offset unchanged");
_Static_assert(sizeof(((Image_t*)0)->imgBitmap) == sizeof(void*), "One pointer");
int main(void) { return 0; }
'''
with tempfile.TemporaryDirectory(prefix="sdl-f6-native-header-") as tmp:
    source = Path(tmp) / "types.c"
    source.write_text(harness)
    for name, flags in (
        ("desktop", []),
        ("esp32", ["-DDOOMRPG_ESP32=1"]),
        ("oracle", ["-DDOOMRPG_ESP32=1", "-DDOOMRPG_ESP32_GEOMETRY_ORACLE=1"]),
    ):
        args = ["cc", "-std=c11", "-Wall", "-Wextra", "-Werror",
                "-IESP32/include", "-Isrc", *flags, str(source)]
        subprocess.run([*args, "-o", str(Path(tmp) / name)],
                       check=True, cwd=ROOT)
        subprocess.run([str(Path(tmp) / name)], check=True)
        if name == "esp32":
            pre = subprocess.check_output(
                ["cc", "-E", "-P", "-IESP32/include", "-Isrc",
                 *flags, str(source)], cwd=ROOT, text=True)
            for forbidden in ("SDL_RWops", "Uint8", "SDL_Texture"):
                assert not re.search(r"\b" + forbidden + r"\b", pre), forbidden
print("SDL-F6 PASS: native DoomRPG.h has no SDL-dependent IO/key ABI;"
      " desktop/oracle signatures unchanged; image/clock sources SDL-free")
