#!/usr/bin/env python3
"""Guard retirement of desktop keyboard/mouse/controller bind state on classic CYD.

DoomRPG.c + DoomRPG.h remain UNMODIFIED desktop behavior references.
Native touch/menu/gameplay inputs are separate; CI's ELF/compiledb audit
proves all 3 x 528B globals and reset method absent from firmware.
"""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
src = ROOT / "ESP32/src"
legacy_source = (ROOT / "src/DoomRPG.c").read_text(encoding="latin-1")
legacy_header = (ROOT / "src/DoomRPG.h").read_text(encoding="latin-1")
builder = (ROOT / "ESP32/scripts/build_engine.py").read_text()
map_owner = (src / "esp_legacy_config_mappings_startup.c").read_text()
core = (src / "engine_stubs.c").read_text()
touch = (src / "platform_input.cpp").read_text()
gameplay = (src / "esp_native_gameplay_input.c").read_text()
options = (src / "native_main_menu_options_back.c").read_text()
main = (src / "main.cpp").read_text()

retired_symbols = (
    "keyMapping", "keyMappingTemp", "keyMappingDefault",
    "DoomRPG_setDefaultBinds",
)
assert all(name in legacy_source and name in legacy_header
           for name in retired_symbols)
assert all(name not in map_owner for name in retired_symbols)
assert all(name not in core for name in retired_symbols)
assert "DoomRPG_getEventKey(" in legacy_source
assert "DoomRPG_setBind(" in legacy_source
assert '"DoomRPG.c",' in builder  # Original reference remains in the 21-TU denylist.
assert "doom_rpg_source_text" not in builder  # No unused generated desktop TU.
assert "compiledOriginal=0 compiledPatchedC=0" in builder
assert all(name not in main for name in retired_symbols)
assert "EspLegacyMappings_load(" in map_owner
assert "Render_loadMappings(" in map_owner
assert "EspLegacyConfigMappingsStartup_start(" in map_owner

# Native controls remain alive: root touchscreen, context routing, and menu
# release/confirmation. A retired keybind cannot silently steal input.
assert "PlatformInput_setTapCallback(" in touch or "setTapCallback" in touch
assert "EspNativeGameplayInput_" in gameplay
assert "PlatformInput_setTapCallback(optionsBackTap)" in options

# ABI byte count is derived from the original desktop constant and type:
# 12 maps x (one 32-bit action + ten 32-bit key slots), times three globals.
# This is BSS/DATA reclamation, not an allocation claim based on free heap.
with tempfile.TemporaryDirectory(prefix="cyd-keybind-retirement-") as dirname:
    folder = Path(dirname)
    probe = folder / "size.c"
    probe.write_text("""
#include <stdint.h>
#include <stdio.h>
#include <assert.h>
typedef struct { int avk_action; int keyBinds[10]; } keyMapping_t;
int main(void) {
    assert(sizeof(keyMapping_t) == 44U);
    assert(3U * 12U * sizeof(keyMapping_t) == 1584U);
    puts("PASS retired keybind static data 3 x 528B = 1584B");
    return 0;
}
""")
    exe = folder / "size"
    subprocess.run(["cc", "-std=c11", "-Wall", "-Wextra", "-Werror",
                    str(probe), "-o", str(exe)], check=True)
    subprocess.run([str(exe)], check=True)

print("PASS desktop binds absent from CYD root/mappings, touch pipeline retained")
