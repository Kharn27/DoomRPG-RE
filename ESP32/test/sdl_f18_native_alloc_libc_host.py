#!/usr/bin/env python3
"""SDL-F18: exact original six Git blobs and real old/new C libc equivalence.

Only SDL.h macros for malloc/calloc/realloc/free/strlen/memcpy are changed.
Retire direct SDL.h includes in all six SDL-ABI-free C translation units.
Never change allocation size, order, rollback, PAK handling or lifetime.
"""
from pathlib import Path
import hashlib
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
SRC = ROOT / "ESP32/src"
SDL = (ROOT / "ESP32/include/SDL.h").read_text(encoding="utf-8")
REFERENCES = {
    "esp_legacy_asset_source.c": (
        "32bc61e44d0991b9702dd7120a3887431d4369b7",
        {"malloc": 1, "free": 1}),
    "esp_native_gameplay_event_chain.c": (
        "64ff6b5db21812619ee456a0e0b5c0c1e1449f0f",
        {"calloc": 1, "free": 2, "realloc": 1}),
    "esp_native_gameplay_note_prefix.c": (
        "d05b546c36404fb1388059ca7de7ae8e35b380b3",
        {"free": 2, "calloc": 3}),
    "esp_native_gameplay_weapon.c": (
        "981a8ddd60d7f6e47cca9806564da6edb103cf5b",
        {"calloc": 1}),
    "esp_native_intro_state.c": (
        "8355d15d4b63057f74779eaa77c0bcf7fb513224",
        {"strlen": 1, "calloc": 2, "memcpy": 1, "free": 4}),
    "esp_native_resident_gameplay.c": (
        "24d1ec3a73b4b18f496ef20fb74c43e8639de86d",
        {"malloc": 1, "free": 1}),
}


def git_blob(text):
    data = text.encode("utf-8")
    return hashlib.sha1(b"blob " + str(len(data)).encode() + b"\0" + data).hexdigest()


def function(source, signature):
    assert source.count(signature) == 1, signature
    start = source.index(signature)
    opening = source.index("{", start)
    depth = 0
    for i in range(opening, len(source)):
        if source[i] == "{":
            depth += 1
        elif source[i] == "}":
            depth -= 1
            if depth == 0:
                return source[start:i+1]
    raise AssertionError("Unclosed C function: " + signature)


old_sources = {}
new_sources = {}
total = 0
for filename, (sha, aliases) in REFERENCES.items():
    current = (SRC / filename).read_text(encoding="utf-8")
    # Files including legacy DoomRPG/renderer/chain headers still need the
    # transitional SDL typedef ABI. Only the standalone PAK owner can retire
    # SDL.h outright; all six eliminate their callable SDL libc aliases.
    if filename == "esp_legacy_asset_source.c":
        assert current.startswith("#include <stdlib.h>\n"), filename
        assert "#include <SDL.h>" not in current, filename
        restored = current.replace("#include <stdlib.h>\n", "#include <SDL.h>\n", 1)
    elif filename == "esp_native_gameplay_note_prefix.c":
        # SDL-F19 retires an include after F18; re-instate it to recover the
        # exact pre-F18 blob without weakening the alias parity assertions.
        assert current.startswith("#include <stdlib.h>\n"), filename
        assert "#include <SDL.h>" not in current, filename
        restored = "#include <SDL.h>\n" + current
        restored = restored.replace("#include <SDL.h>\n#include <stdlib.h>\n",
                                   "#include <SDL.h>\n", 1)
    else:
        if filename == "esp_native_gameplay_event_chain.c":
            # SDL-F24 now guards the historic SDL.h include for native builds.
            # First undo only that later header gate, then reconstruct the
            # original F18 Git blob and verify every libc substitution.
            from sdl_f24_native_geometry_event_header_host import HOST_GUARD
            assert current.startswith(HOST_GUARD), filename
            current = "#include <SDL.h>\n" + current[len(HOST_GUARD):]
        if filename in ("esp_native_gameplay_weapon.c",
                        "esp_native_resident_gameplay.c"):
            # SDL-F27 guards these source-only includes after F18.
            # Reconstruct the exact historical pre-F27 prefix exclusively
            # inside the oracle. Production C bodies remain unmodified.
            from sdl_f27_native_gameplay_header_host import GUARD
            assert current.startswith(GUARD), filename
            current = "#include <SDL.h>\n" + current[len(GUARD):]
        assert current.startswith("#include <SDL.h>\n#include <stdlib.h>\n"), filename
        restored = current.replace("#include <SDL.h>\n#include <stdlib.h>\n",
                                   "#include <SDL.h>\n", 1)
    assert not re.search(r"\bSDL_[A-Za-z_][A-Za-z_0-9]*\b", current), filename
    for name, count in aliases.items():
        assert re.search(r"^#define\s+SDL_" + name + r"\s+" + name + r"\s*$",
                         SDL, re.M), name
        pattern = r"\b" + name + r"\("
        assert len(re.findall(pattern, restored)) == count, (filename, name)
        restored = re.sub(pattern, "SDL_" + name + "(", restored)
        total += count
    assert git_blob(restored) == sha, (filename, git_blob(restored), sha)
    old_sources[filename] = restored
    new_sources[filename] = current

assert total == 22, total
assert "-<esp32_sdl.cpp>" in (ROOT / "ESP32/platformio.ini").read_text()

# Execute genuine production C function bodies from both revisions.
# The PAK backend is a tiny deterministic host fixture, not a rewritten C
# readAlloc implementation. Covers open/find/read failures and success.
orig_intro = function(old_sources["esp_native_intro_state.c"],
                      "static char* copyText(const char* text)")
now_intro = function(new_sources["esp_native_intro_state.c"],
                     "static char* copyText(const char* text)")
orig_asset = function(old_sources["esp_legacy_asset_source.c"],
                      "uint8_t* EspLegacyAssetSource_readAlloc(")
now_asset = function(new_sources["esp_legacy_asset_source.c"],
                     "uint8_t* EspLegacyAssetSource_readAlloc(")
open_entry = function(old_sources["esp_legacy_asset_source.c"],
                      "static int openEntry(")
assert open_entry == function(new_sources["esp_legacy_asset_source.c"],
                              "static int openEntry(")


def rename(body, name, new_name):
    assert body.count(name + "(") == 1, name
    return body.replace(name + "(", new_name + "(", 1)


fixture = r"""
#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#define SDL_malloc malloc
#define SDL_calloc calloc
#define SDL_realloc realloc
#define SDL_free free
#define SDL_strlen strlen
#define SDL_memcpy memcpy
typedef struct { uint32_t flags; uint32_t size; } EspAssetPackEntry;
#define ESP_ASSET_PACK_DEFAULT_PATH "/DoomRPG-ESP32.pak"
#define ESP_ASSET_PACK_FLAG_DIRECTORY 1U
static int packOpen;
static int readMode;
static int opens;
static int closes;
int EspAssetPack_isOpen(void) { return packOpen; }
int EspAssetPack_open(const char* path) {
    assert(strcmp(path, ESP_ASSET_PACK_DEFAULT_PATH) == 0);
    assert(!packOpen);
    opens++;
    packOpen = 1;
    return 1;
}
void EspAssetPack_close(void) { assert(packOpen); packOpen = 0; closes++; }
int EspAssetPack_findEntry(const char* name, EspAssetPackEntry* entry) {
    assert(name && entry && packOpen);
    if (readMode == 2) return 0;
    entry->flags = readMode == 3 ? ESP_ASSET_PACK_FLAG_DIRECTORY : 0;
    entry->size = readMode == 4 ? 0U : (readMode == 5 ? 0xFFFFFFFFU : 4U);
    return 1;
}
int EspAssetPack_readRange(const EspAssetPackEntry* entry, uint32_t offset,
                           void* dst, size_t len) {
    assert(entry && packOpen && offset == 0 && len == 4);
    if (readMode == 1) return 0;
    memset(dst, 0xa5, len);
    return 1;
}
__REAL_BODIES__
int main(void) {
    const char* strings[] = {NULL, "", "DOOM", "Long introduction text on Mars!"};
    for (size_t i=0; i<sizeof(strings)/sizeof(strings[0]); i++) {
        char* a = copyText_old(strings[i]);
        char* b = copyText_new(strings[i]);
        assert((a == NULL) == (b == NULL));
        if (a && b) {
            assert(strlen(a) == strlen(b));
            assert(memcmp(a, b, strlen(a) + 1) == 0);
        }
        free(a); free(b);
    }
    for (readMode = 0; readMode <= 5; readMode++) {
        for (int which = 0; which < 2; which++) {
            packOpen = 0; opens = 0; closes = 0;
            int length = 444;
            uint8_t* data = which == 0
                ? EspLegacyAssetSource_readAlloc_old("c.bmp", &length)
                : EspLegacyAssetSource_readAlloc_new("c.bmp", &length);
            assert(!packOpen && opens == 1 && closes == 1);
            if (readMode == 0) {
                assert(data != NULL && length == 4);
                for (int i=0; i<length; i++) assert(data[i] == 0xa5);
            } else {
                assert(data == NULL && length == 0);
            }
            free(data);
        }
    }
    for (int which = 0; which < 2; which++) {
        packOpen = 0; opens = 0; closes = 0;
        int length = 1;
        uint8_t* value = which == 0
            ? EspLegacyAssetSource_readAlloc_old(NULL, &length)
            : EspLegacyAssetSource_readAlloc_new(NULL, &length);
        assert(value == NULL && length == 0 && opens == 0 && closes == 0);
    }
    return 0;
}
"""
real = "\n\n".join([
    open_entry,
    rename(orig_intro, "copyText", "copyText_old"),
    rename(now_intro, "copyText", "copyText_new"),
    rename(orig_asset, "EspLegacyAssetSource_readAlloc",
           "EspLegacyAssetSource_readAlloc_old"),
    rename(now_asset, "EspLegacyAssetSource_readAlloc",
           "EspLegacyAssetSource_readAlloc_new"),
])
fixture = fixture.replace("__REAL_BODIES__", real)
with tempfile.TemporaryDirectory(prefix="sdl-f18-libc-") as tmp:
    c = Path(tmp) / "parity.c"
    exe = Path(tmp) / "parity"
    c.write_text(fixture, encoding="utf-8")
    subprocess.run(["cc", "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror",
                    str(c), "-o", str(exe)], check=True)
    subprocess.run([str(exe)], check=True)

print("SDL-F18 PASS 6 whole-file original Git SHA blobs, 22 SDL macro-only "
      "libc substitutions, no remaining SDL identifiers, real C intro text "
      "and PAK allocation old/new parity including error rollback")
