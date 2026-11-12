#!/usr/bin/env python3
"""SDL-F20: prove exact single-include removals on eight native leaf owners.

The normal ESP32 profile does not include SDL.h in these owners, while the
old SDL ABI include is retained only for desktop/host/oracle compilation.
C11 compiles real DoomRPG/Render native headers WITHOUT pre-including SDL.h.
"""
from pathlib import Path
import hashlib
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
SRC = ROOT / "ESP32/src"
BASELINE = {
    "esp_doomrpg_process_rng.c": "9e6db08c2f1860b9cb9fef300c96bf6170619da3",
    "esp_native_gameplay_combat_math.c": "1405da8839ebee0868ed0bacf4c345e1e8be17de",
    "esp_native_gameplay_player_state.c": "670e430b31119435879eeffe9db0fe3de74dd9f4",
    "esp_native_rng_replay_guard.c": "63a784b9809fa7fcc5e3a545beda2df72fa3aee5",
    "esp_native_gameplay_monster_movement_probe.c": "ac82398e63bdc15e15b58eafc4aaf9e444c251df",
    "native_wall_lru_cache.c": "315a05a43677195d5aab2ee2c00ee260ad0f5728",
    "native_sprite_lru_cache.c": "75a99e15e6909f1fb5a34e1bba819d2555e81526",
    "esp_legacy_prerender_startup.c": "f4bdcf385ed74ee78e05708d46e79c45d0cb73d9",
}


def git_blob(source):
    raw = source.encode("utf-8")
    return hashlib.sha1(b"blob " + str(len(raw)).encode() + b"\0" + raw).hexdigest()


for name, old_sha in BASELINE.items():
    current = (SRC / name).read_text(encoding="utf-8")
    host_header = ("#if !defined(DOOMRPG_ESP32) || "
                   "defined(DOOMRPG_ESP32_GEOMETRY_ORACLE)\n"
                   "#include <SDL.h>\n#endif\n")
    # Native firmware skips this compatibility-only header entirely. Historical
    # host oracles may compile without -DDOOMRPG_ESP32 and need desktop SDL
    # struct typedefs from the original source.
    assert current.startswith(host_header), name
    assert current.count("#include <SDL.h>") == 1, name
    assert not re.search(r"\bSDL_[A-Za-z0-9_]+\b",
                         current[len(host_header):]), name
    restored = "#include <SDL.h>\n" + current[len(host_header):]
    assert git_blob(restored) == old_sha, name
    assert '#include "DoomRPG.h"' in current, name

# The real legacy structures are natively self-contained in the production
# profile. SDL-specific members are already guarded out; this test intentionally
# does not include SDL.h, nor mock either header.
native = r"""
#include <stdint.h>
#include "DoomRPG.h"
#include "Render.h"
static EspNativeTexture* texture(Image_t* image) {
    return image->imgBitmap;
}
int main(void) {
    Image_t image = {0};
    EspNativeRect r = {0, 20, 160, 80};
    return texture(&image) == 0 && r.w == 160 && r.h == 80 ? 0 : 1;
}
"""
with tempfile.TemporaryDirectory(prefix="doomrpg-sdl-f20-") as folder:
    code = Path(folder) / "native.c"
    executable = Path(folder) / "native"
    code.write_text(native, encoding="utf-8")
    subprocess.run(["cc", "-std=c11", "-Wall", "-Wextra", "-Werror",
                    "-DDOOMRPG_ESP32=1", "-I", str(ROOT / "ESP32/include"),
                    "-I", str(ROOT / "src"), str(code), "-o",
                    str(executable)], check=True, cwd=ROOT)
    subprocess.run([str(executable)], check=True, cwd=ROOT)

print("SDL-F20 PASS: eight exact Git blobs, eight SDL.h native-compile guards; "
      "native DoomRPG/Render headers compile and execute without SDL.h")
