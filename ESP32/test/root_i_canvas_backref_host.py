#!/usr/bin/env python3
"""Root-I: no native DoomCanvas_t -> DoomRPG_t back-reference.

Check real header ABI in all profiles, explicit root lifetime contracts,
and dormant production source inventory. Desktop/oracle keeps the old field.
"""
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
HEADER = (ROOT / "src/DoomCanvas.h").read_text(encoding="latin-1")
METRICS = (ROOT / "ESP32/include/engine_metrics.h").read_text()
CORE = (ROOT / "ESP32/src/engine_stubs.c").read_text()
assert re.search(
    r"#if !defined\(DOOMRPG_ESP32\) \|\| defined\(DOOMRPG_ESP32_GEOMETRY_ORACLE\)"
    r"\s+struct DoomRPG_s\* doomRpg;\s+#endif", HEADER)
assert "ESP32_DOOMCANVAS_RETIRED_PROCESS_BACKREF_BYTES 4U" in METRICS
assert "ESP32_DOOMCANVAS_RETIRED_PROCESS_BACKREF_BYTES)" in METRICS
assert "sizeof(DoomCanvas_t) == ESP32_DOOMCANVAS_COMPACT_BYTES" in CORE
assert "doomCanvas->doomRpg" not in CORE
assert "EspNativeEngineRoot_initCanvas(DoomCanvas_t* doomCanvas)" in CORE
assert "EspNativeEngineRoot_initCanvas(NULL)" in CORE
assert "EspNativeEngineRoot_releaseCanvas(DoomCanvas_t* doomCanvas, DoomRPG_t* root)" in CORE
assert "EspNativeEngineRoot_releaseCanvas(EspNativeRootOwners_live()->doomCanvas, doomRpg)" in CORE
assert "EspNativeIntroState_release(root);" in CORE
assert "DoomRPG_freeImage(root, &doomCanvas->imgFont);" in CORE
assert "EspNativeEngineLayout_startupCanvas(DoomCanvas_t* doomCanvas, DoomRPG_t* root)" in CORE
assert "EspNativeEngineLayout_startupCanvas(canvas, root);" in CORE
assert "int DoomRPG_startEngineLayout(DoomRPG_t* root, DoomRpgLayoutReport* report)" in CORE
assert "if (doomCanvas == NULL || root == NULL) return;" in CORE
assert 'DoomRPG_createImage(root, "a.bmp", true, &doomCanvas->imgFont);' in CORE
assert "doomRpg = &nativeRootIdentity;" in CORE  # unrelated root identity still live
MENU = (ROOT / "ESP32/src/native_main_menu_model.c").read_text()
MENU_H = (ROOT / "ESP32/include/native_main_menu_model.h").read_text()
assert "doomCanvas->doomRpg" not in MENU
assert "doomCanvasBeginIntro(DoomCanvas_t* doomCanvas, DoomRPG_t* root)" in MENU
assert "EspNativeIntroState_begin(root)" in MENU
assert "EspNativeMainMenu_transitionCanvasState(DoomCanvas_t* doomCanvas, DoomRPG_t* root, int stateNum)" in MENU
assert "struct DoomRPG_s* root, int state" in MENU_H
assert "EspNativeMainMenu_transitionCanvasState(canvas, doomRpg, ST_MENU)" in MENU
assert "EspNativeMainMenu_transitionCanvasState(doomCanvas, doomRpg, ST_INTRO)" in (
    ROOT / "ESP32/src/native_main_menu_start_action.c").read_text()
assert "EspNativeMainMenu_transitionCanvasState(EspNativeRootOwners_live()->doomCanvas, doomRpg, ST_PLAYING)" in (
    ROOT / "ESP32/src/native_main_menu_actions.c").read_text()
assert "EspNativeMainMenu_transitionCanvasState(EspNativeRootOwners_live()->doomCanvas, touchDoomRpg, ST_MENU)" in (
    ROOT / "ESP32/src/native_main_menu_touch.c").read_text()
STORY = (ROOT / "ESP32/src/native_story_fit.c").read_text()
STORY_H = (ROOT / "ESP32/include/native_story_fit.h").read_text()
assert "doomCanvas->doomRpg" not in STORY
assert "Esp32StoryFit_prepare(struct DoomCanvas_s* doomCanvasBase, DoomRPG_t* root)" in STORY
assert "Esp32StoryFit_draw(struct DoomCanvas_s* doomCanvasBase, DoomRPG_t* root)" in STORY
assert "Esp32StoryFit_release(struct DoomCanvas_s* doomCanvasBase, DoomRPG_t* root)" in STORY
assert "struct DoomRPG_s* root" in STORY_H
assert "Esp32StoryFit_prepare(doomCanvas, doomRpg)" in (
    ROOT / "ESP32/src/native_main_menu_start_action.c").read_text()
assert "Esp32StoryFit_draw(canvas, doomRpg)" in (
    ROOT / "ESP32/src/native_intro_first_frame.c").read_text()
assert "Esp32StoryFit_draw(canvas, clockState.doomRpg)" in (
    ROOT / "ESP32/src/native_intro_clock.c").read_text()
assert "Esp32StoryFit_release(canvas, doomRpg)" in (
    ROOT / "ESP32/src/native_intro_dispose.c").read_text()
assert "Esp32StoryFit_release(doomCanvas, root)" in CORE

files = 0
for base in (ROOT / "ESP32/src", ROOT / "ESP32/include"):
    for path in sorted(base.rglob("*")):
        if path.suffix not in (".c", ".cpp", ".h"):
            continue
        files += 1
        source = path.read_text(encoding="latin-1")
        source = re.sub(r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"', " ",
                        source, flags=re.S)
        assert re.search(r"\b(?:doomCanvas|canvas)\s*(?:->|\.)\s*doomRpg\b",
                         source) is None, path
assert files >= 300, files

cc = shutil.which("cc") or "cc"
with tempfile.TemporaryDirectory(prefix="root-i-canvas-") as directory:
    probe = Path(directory) / "probe.c"
    probe.write_text(
        '#include <SDL.h>\n#include "DoomRPG.h"\n#include "DoomCanvas.h"\n'
        'int probe(DoomCanvas_t* c) { return c->doomRpg != 0; }\n')
    for name, flags, visible in (
        ("normal", ["-DDOOMRPG_ESP32=1"], False),
        ("bringup", ["-DDOOMRPG_ESP32=1", "-DDOOMRPG_ESP32_BRINGUP_PROBES=1"], False),
        ("trace", ["-DDOOMRPG_ESP32=1", "-DDOOMRPG_LOG_LEVEL=4"], False),
        ("oracle", ["-DDOOMRPG_ESP32=1", "-DDOOMRPG_ESP32_GEOMETRY_ORACLE=1"], True),
        ("desktop", [], True),
    ):
        result = subprocess.run(
            [cc, "-std=c11", "-Wall", "-Wextra", "-Werror", "-fsyntax-only",
             "-IESP32/include", "-Isrc", *flags, str(probe)],
            cwd=ROOT, capture_output=True, text=True)
        assert (result.returncode == 0) == visible, (name, result.stderr)
        if not visible:
            assert "doomRpg" in result.stderr, (name, result.stderr)
print(f"Root-I PASS: Canvas process backref absent in normal/bringup/trace; "
      f"desktop/oracle kept; {files} sources audited")
