#!/usr/bin/env python3
"""Guard actual ESP32 wall-sprite projection and legacy parity sources."""
from pathlib import Path
import subprocess
import os
import shlex
import tempfile

ROOT=Path(__file__).resolve().parents[2]
desktop=(ROOT/"src/Render.c").read_text(encoding="latin-1")
native=(ROOT/"ESP32/src/esp_native_sprite_renderer.c").read_text()
visibility=(ROOT/"ESP32/src/esp_native_bsp_visibility.c").read_text()
assert "if ((mapSprite->info & 0x800000) != 0)" in desktop
assert "if ((sprite->info & 0x800000) != 0)" in desktop
assert "sortZ = MAXINT;" in desktop
assert "indx < 184 || indx > 190" in desktop
assert "sortZ = 0;" in desktop
assert native.count("EspNativeSpriteVisual_coordinates(") == 2
assert native.count("EspNativeSpriteVisual_sortDepth(") == 1
assert visibility.count("EspNativeSpriteVisual_coordinates(") == 1
assert "visualX > (int)split" in visibility
assert "visualY > (int)split" in visibility
with tempfile.TemporaryDirectory(prefix="doomrpg-wall-sprite-") as d:
    output=str(Path(d)/"test")
    cc=shlex.split(os.environ.get("CC","cc"))
    subprocess.run(cc+["-std=c11","-Wall","-Wextra","-Werror",
                       "-IESP32/include","ESP32/test/wall_sprite_visual_host.c",
                       "-o",output],cwd=ROOT,check=True)
    subprocess.run([output],cwd=ROOT,check=True)
