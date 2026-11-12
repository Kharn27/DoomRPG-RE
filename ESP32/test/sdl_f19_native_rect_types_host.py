#!/usr/bin/env python3
"""SDL-F19: exact native SDL_Rect alias replacement and standalone NOTE header.

Five full C sources must reconstruct their exact main Git blobs, except for
eight local SDL_Rect spelling changes and removal of the unused SDL.h include.
This is an ABI/type cleanup, NOT removal of the transitional legacy SDL header.
"""
from pathlib import Path
import hashlib
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
SRC = ROOT / "ESP32/src"
BASELINE = {
    "engine_stubs.c": ("af33c5e909dcb20fee3c3472b59363ce20d43003", 3),
    "native_main_menu_actions.c": ("fb381e164319b8b629f97c6e904471b60b3585f8", 3),
    "native_main_menu_options_action.c": ("1da4bd1053c04ae28653fcb42d816d124c5330a3", 1),
    "native_main_menu_touch_layout.c": ("f3ba41f15b8531f51c022385e01f2e514eebc105", 1),
}
NOTE_SHA = "8cdb22f78f576b28ab75f50b2c900f8600485d2a"


def git_blob(s: str) -> str:
    data = s.encode("utf-8")
    return hashlib.sha1(b"blob " + str(len(data)).encode() + b"\0" + data).hexdigest()


for name, (sha, count) in BASELINE.items():
    text = (SRC / name).read_text(encoding="utf-8")
    assert len(re.findall(r"\bEspNativeRect\b", text)) == count, name
    assert not re.search(r"\bSDL_Rect\b", text), name
    assert "#include <SDL.h>" in text, (name, "legacy header still required")
    restored = re.sub(r"\bEspNativeRect\b", "SDL_Rect", text)
    assert git_blob(restored) == sha, (name, git_blob(restored), sha)

note = (SRC / "esp_native_gameplay_note_prefix.c").read_text(encoding="utf-8")
assert note.startswith("#include <stdlib.h>\n"), "explicit C allocator include"
assert "#include <SDL.h>" not in note, "NOTE must not depend on SDL compatibility"
assert not re.search(r"\bSDL_[A-Za-z0-9_]+\b", note), "no SDL identifier"
assert git_blob("#include <SDL.h>\n" + note) == NOTE_SHA

header = (ROOT / "ESP32/include/SDL.h").read_text(encoding="utf-8")
native = (ROOT / "ESP32/include/esp_native_texture.h").read_text(encoding="utf-8")
assert re.search(r"^typedef EspNativeRect SDL_Rect;.*$", header, re.M)
assert "int x, y, w, h;" in native

# Compile the REAL linked native/compat header typedefs, proving that old and
# new layout instances have exactly the same C type (not just matching sizes).
harness = r"""
#include <SDL.h>
#include <stddef.h>
_Static_assert(sizeof(SDL_Rect) == sizeof(EspNativeRect), "size changed");
_Static_assert(_Alignof(SDL_Rect) == _Alignof(EspNativeRect), "align changed");
_Static_assert(offsetof(SDL_Rect, x) == offsetof(EspNativeRect, x), "x");
_Static_assert(offsetof(SDL_Rect, y) == offsetof(EspNativeRect, y), "y");
_Static_assert(offsetof(SDL_Rect, w) == offsetof(EspNativeRect, w), "w");
_Static_assert(offsetof(SDL_Rect, h) == offsetof(EspNativeRect, h), "h");
static int native_sum(const EspNativeRect *rect) {
    return rect->x + rect->y + rect->w + rect->h;
}
static int compat_sum(const SDL_Rect *rect) {
    return native_sum(rect);
}
int main(void) {
    SDL_Rect old = {2, -3, 144, 72};
    EspNativeRect current = old;
    const SDL_Rect *again = &current;
    return compat_sum(&old) == 215 && native_sum(again) == 215 ? 0 : 1;
}
"""
with tempfile.TemporaryDirectory(prefix="doomrpg-sdl-f19-") as d:
    source = Path(d) / "rect.c"
    binary = Path(d) / "rect"
    source.write_text(harness, encoding="utf-8")
    subprocess.run(["cc", "-std=c11", "-Wall", "-Wextra", "-Werror",
                    "-I", str(ROOT / "ESP32/include"), str(source),
                    "-o", str(binary)], check=True, cwd=ROOT)
    subprocess.run([str(binary)], check=True, cwd=ROOT)

print("SDL-F19 PASS: five exact main source blobs, 8 native rect typedef uses, "
      "NOTE SDL.h retired, C11 native/compat layout and pointer parity")
