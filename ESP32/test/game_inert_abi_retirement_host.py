#!/usr/bin/env python3
"""Retire every Game_* runtime provider and the process Game_t allocation.

Desktop Game.c/h remain references. In ESP32, the root's historical game
pointer is a NULL compatibility sentinel, not a separately allocated object.
The original core diagnostics stage remains as a zero-byte stage.
This host test proves the real native metrics function, the explicit
lifecycle guards and that no ESP32 translation unit revives Game_* callers.
CI independently checks three final firmware ELFs and native build admission.
"""
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
SRC = ROOT / "ESP32/src"
core = (SRC / "engine_stubs.c").read_text(encoding="utf-8")
builder = (ROOT / "ESP32/scripts/build_engine.py").read_text(encoding="utf-8")
header = (ROOT / "src/Game.h").read_text(encoding="latin-1")
desktop = (ROOT / "src/Game.c").read_text(encoding="latin-1")

retired = ("Game_init", "Game_activate", "Game_unloadMapData")
assert not (SRC / "esp_legacy_game_bridge.c").exists()
assert "bridge=absent Game_init=retired" in builder
assert "DoomRPG_t* doomRpg = NULL;" in core
assert "Game_init(NULL, doomRpg)" not in core
assert "INIT_CORE_OBJECT(DOOMRPG_CORE_GAME" not in core
assert "doomRpg->game != NULL" in core
assert "recordCoreStage(DOOMRPG_CORE_GAME, before, coreFreeHeap());" in core
assert "metrics->game = 0U;" in core
assert "Game object retired gameBytes=0" in core
assert "bridge=absent exports=0" in core
assert "SDL_free(doomrpg->game)" not in core

for name in retired:
    assert re.search(r"\b" + name + r"\s*\(", desktop)
    assert re.search(r"\b" + name + r"\s*\(", header)

# Ignore historical descriptions and diagnostic strings but NOT code callers.
def without_comments_and_strings(source):
    return re.sub(r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"',
                  " ", source, flags=re.S)

for path in sorted(SRC.rglob("*")):
    if path.suffix not in (".c", ".cpp"):
        continue
    source = without_comments_and_strings(path.read_text(encoding="latin-1"))
    for name in retired:
        assert not re.search(r"\b" + name + r"\s*\(", source), (
            name + " revived in " + str(path.relative_to(ROOT)))

# All real runtime gates must refuse an unexpected Game pointer, rather than
# accepting the absence of the previous 4B allocation as an error.
guards = (
    "esp_legacy_config_mappings_startup.c",
    "esp_native_resident_gameplay.c",
    "esp_native_startup.c",
    "native_intro_clock.c",
    "native_intro_first_frame.c",
    "native_main_menu_start_action.c",
)
for name in guards:
    content = (SRC / name).read_text(encoding="latin-1")
    assert "doomRpg->game == NULL ||" not in content, name
    assert "doomRpg->game != NULL" in content, name
    assert "doomRpg->render" in content, name

cleanup = (SRC / "native_main_menu_start_action.c").read_text(encoding="latin-1")
assert "Render_freeRuntime(render);" in cleanup
assert "return render->nodes == NULL" in cleanup
assert "!EspNativeWallCache_isActive()" in cleanup
assert "!EspNativeSpriteCache_isActive()" in cleanup

def extract(src, signature):
    anchor = signature + "\n{"
    assert src.count(anchor) == 1, signature
    start = src.index(anchor)
    depth = 0
    for i in range(src.index("{", start), len(src)):
        depth += (src[i] == "{") - (src[i] == "}")
        if depth == 0:
            return src[start:i + 1]
    raise AssertionError("unterminated " + signature)

# Exercise the actual *native* metrics body, not a copied calculation.
metrics = extract(core, "void DoomRPG_getEngineMetrics(DoomRpgEngineMetrics* metrics)")
with tempfile.TemporaryDirectory(prefix="cyd-game-owner-retired-") as temp:
    folder = Path(temp)
    (folder / "metrics.c").write_text(
        "#include <stdint.h>\n"
        "#include <stddef.h>\n"
        "#include <assert.h>\n"
        "#include <stdio.h>\n"
        '#include "engine_metrics.h"\n'
        "typedef struct {uint8_t v[280];} DoomRPG_t;\n"
        "typedef struct {uint8_t v[44];} DoomCanvas_t;\n"
        "typedef struct {uint8_t v[1532];} Render_t;\n"
        "typedef struct {uint8_t v[496];} EspNativeMenuState_t;\n"
        + metrics + "\n"
        "int main(void) {\n"
        "  DoomRpgEngineMetrics m = {0};\n"
        "  DoomRPG_getEngineMetrics(&m);\n"
        "  assert(m.game == 0U && m.player == 0U && m.combat == 0U);\n"
        "  assert(m.doomRpg == 280U && m.doomCanvas == 44U);\n"
        "  assert(m.render == 1532U && m.supportObjects == 496U);\n"
        "  assert(m.totalInitialObjects == 2352U);\n"
        "  puts(\"PASS native Game=0B allocated object, metrics exact\");\n"
        "  return 0;\n"
        "}\n", encoding="utf-8")
    executable = folder / "metrics"
    subprocess.run(["cc", "-std=c11", "-O2", "-Wall", "-Wextra",
                    "-Werror", "-IESP32/include", str(folder / "metrics.c"),
                    "-o", str(executable)], cwd=ROOT, check=True)
    subprocess.run([str(executable)], check=True)

print("PASS Game_t owner retired, Game_* linked ABI=0, gate=NULL, lifecycle unchanged")
