#!/usr/bin/env python3
"""Retire only the inert Game activation/unload ABI, not Game_t's live shell.

The original desktop Game.c/Game.h remain reference sources. The two old
compatibility exports performed no mutation. Missing definitions fail at link,
rather than silently allowing a new legacy world owner into native gameplay.
"""
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
SRC = ROOT / "ESP32/src"
BRIDGE = SRC / "esp_legacy_game_bridge.c"
src = BRIDGE.read_text(encoding="utf-8")
core = (SRC / "engine_stubs.c").read_text(encoding="utf-8")
builder = (ROOT / "ESP32/scripts/build_engine.py").read_text(encoding="utf-8")
legacy = (ROOT / "src/Game.c").read_text(encoding="latin-1")
header = (ROOT / "src/Game.h").read_text(encoding="latin-1")

retired = ("Game_activate", "Game_unloadMapData")
assert src.count("Game_t* Game_init(") == 1
assert all(re.search(r"\b" + n + r"\s*\(", legacy) for n in retired)
assert all(re.search(r"\b" + n + r"\s*\(", header) for n in retired)
assert all(not re.search(r"\b" + n + r"\s*\(", src) for n in retired)
assert 'bridge=Game_init-only' in builder
assert "exports=1(Game_init) inertExports=retired" in core
assert "Game_init(NULL, doomRpg)" in core
assert "_Static_assert(sizeof(Game_t) == sizeof(void*)" in core

# Reject new native code that starts depending on one of the removed
# functions, even if it happened to land in a discarded translation unit.
# Preserve the desktop originals: their functions are specification only.
def code_without_comments_and_literals(s):
    return re.sub(r"/\*.*?\*/|//[^\n]*|\\"(?:\\.|[^\\"\\])*\\"",
                  " ", s, flags=re.S)

for path in sorted(SRC.rglob("*")):
    if path.suffix not in (".c", ".cpp") or path == BRIDGE:
        continue
    c = code_without_comments_and_literals(path.read_text(encoding="latin-1"))
    for name in retired:
        assert not re.search(r"\b" + name + r"\s*\(", c), (
            "legacy Game ABI resurrected by " + str(path.relative_to(ROOT)))

with tempfile.TemporaryDirectory(prefix="cyd-game-inert-abi-") as temp:
    work = Path(temp)
    (work / "SDL.h").write_text(
        "#pragma once\n#include <stddef.h>\n"
        "void* SDL_malloc(size_t);\nvoid* SDL_memset(void*, int, size_t);\n")
    (work / "DoomRPG.h").write_text(
        "#pragma once\ntypedef struct {int marker;} DoomRPG_t;\n")
    (work / "Game.h").write_text(
        '#pragma once\n#include "DoomRPG.h"\n'
        "typedef struct { DoomRPG_t* doomRpg; } Game_t;\n"
        "Game_t* Game_init(Game_t*, DoomRPG_t*);\n")
    (work / "test.c").write_text(r"""
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "SDL.h"
#include "Game.h"
static unsigned allocations, resets;
static int fail_alloc;
void* SDL_malloc(size_t n) {
    allocations++;
    return fail_alloc ? NULL : malloc(n);
}
void* SDL_memset(void* ptr, int value, size_t n) {
    resets++;
    return memset(ptr, value, n);
}
int main(void) {
    assert(sizeof(Game_t) == sizeof(void*));
    DoomRPG_t root = {123};
    Game_t* game = Game_init(NULL, &root);
    assert(game && game->doomRpg == &root);
    assert(allocations == 1 && resets == 1);
    Game_t owned = { (DoomRPG_t*)1 };
    assert(Game_init(&owned, &root) == &owned);
    assert(owned.doomRpg == &root);
    assert(allocations == 1 && resets == 2);
    free(game);
    fail_alloc = 1;
    assert(Game_init(NULL, &root) == NULL);
    assert(allocations == 2 && resets == 2);
    puts("PASS Game_init real bridge: allocation, caller-owned, failure");
    return 0;
}
""")
    obj = work / "bridge.o"
    cc = ["cc", "-O2", "-std=c11", "-Wall", "-Wextra", "-Werror",
          "-I", str(work)]
    subprocess.run(cc + ["-c", str(BRIDGE), "-o", str(obj)], check=True)
    symbols = subprocess.check_output(["nm", "-g", str(obj)], text=True)
    defined = [
        m.group(1) for line in symbols.splitlines()
        if (m := re.search(r"\b[Tt]\s+(Game_\w+)$", line))
    ]
    assert defined == ["Game_init"], "Unexpected Game bridge definitions: " + str(defined)
    exe = work / "test"
    subprocess.run(cc + [str(obj), str(work / "test.c"), "-o", str(exe)],
                   check=True)
    subprocess.run([str(exe)], check=True)
    # A new caller MUST fail to link: the historical no-op is not an option.
    for retired_name in retired:
        bad = work / "invalid.c"
        bad.write_text("extern void " + retired_name
                       + "(void*,void*);\nint main(void) { "
                       + retired_name + "(0,0); return 0; }\n")
        result = subprocess.run(
            cc + [str(obj), str(bad), "-o", str(work / "invalid")],
            text=True, capture_output=True)
        assert result.returncode != 0 and retired_name in result.stderr, (
            retired_name + " unexpectedly linked")

print("PASS Game bridge linked ABI=1; native world activation/unload remain unowned")
