#!/usr/bin/env python3
"""Guard the real action-present early gate across EXIT -> MAIN -> SAVESLOTS.

The first post-reset SAVE selector must not be repainted by the gameplay
fallback, even if a facing-label dirty bit survived a reset. Compile and
exercise the extracted production gate, not a hardcoded replacement.
"""
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
source = (ROOT / "ESP32/src/esp_native_gameplay_action_engine.c").read_text()
facing = (ROOT / "ESP32/src/esp_native_gameplay_facing_label.c").read_text()
saves = (ROOT / "ESP32/src/esp_native_gameplay_save.cpp").read_text()
resident = (ROOT / "ESP32/src/esp_native_resident_gameplay.c").read_text()
flow = (ROOT / "ESP32/src/esp_native_gameplay_session.c").read_text()
main = (ROOT / "ESP32/src/native_main_menu_actions.c").read_text()

start = source.index("int __wrap_Esp32PlatformVideo_present(void) {")
body = source[start:]
guard = re.search(
    r"if \(!EspNativeResidentGameplay_isActive\(\)\) \{\s*"
    r"return __real_Esp32PlatformVideo_present\(\);\s*\}", body)
assert guard, "missing non-world bypass of the gameplay compositor"
assert guard.start() < body.index("EspNativeGameplayControls_isActive()")
assert guard.start() < body.index("actionState.feedbackPending")
assert guard.start() < body.index("EspNativeGameplayFacingLabel_isDirty()")
assert "facing.dirty = 1U;" in facing
assert "EspNativeGameplayFacingLabel_reset();" in resident
assert "EspNativeResidentGameplay_reset();" in flow
assert "fillRect(fb, 0, 0, 159, 119, ESP_HUB_COLOR_BG);" in saves
assert "paintMainSlots();" in saves
assert "EspNativeGameplaySave_mainSelectorBegin();" in main
assert "__wrap_Esp32PlatformVideo_present(void)" in source

# Extract the exact guard *including its call* as the first block. Stub only
# the downstream renderer and a one-bit facing fallback. Prove that the gate
# cannot consume the dirty bit in menu/selector, but does not suppress a world
# frame, or mask a physical present failure.
guard_c = guard.group(0)
code = r"""
#include <assert.h>
#include <stdio.h>
static int worldActive, dirty, presentOk, presentCalls, hudPaints;
static int EspNativeResidentGameplay_isActive(void) { return worldActive; }
static int __real_Esp32PlatformVideo_present(void) {
    ++presentCalls;
    return presentOk;
}
static int nativePresent(void) {
""" + guard_c + r"""
    if (dirty) {
        ++hudPaints;
        dirty = 0;
    }
    return __real_Esp32PlatformVideo_present();
}
int main(void) {
    for(int i=0;i<4096;i++) {
        worldActive=0; dirty=1; presentOk=1; presentCalls=0; hudPaints=0;
        /* Menu opaque painter used the direct physical path: first LOAD
         * selector is a FULL 160x120 frame behind this wrapper. */
        assert(nativePresent()==1);
        assert(presentCalls==1 && dirty==1 && hudPaints==0);
        assert(nativePresent()==1); /* BACK -> second LOAD */
        assert(presentCalls==2 && dirty==1 && hudPaints==0);
        worldActive=1;
        assert(nativePresent()==1);
        assert(presentCalls==3 && dirty==0 && hudPaints==1);
        assert(nativePresent()==1);
        assert(presentCalls==4 && hudPaints==1);

        /* Hardware/presentation failure cannot be called success. */
        worldActive=0; dirty=1; presentOk=0;
        assert(nativePresent()==0);
        assert(dirty==1 && hudPaints==1);
    }
    puts("PASS non-world full-screen present guard 4096 EXIT/LOAD/BACK cycles and fail-closed");
    return 0;
}
"""
with tempfile.TemporaryDirectory(prefix="nonworld-present-owner-") as d:
    source_file = Path(d) / "test.c"
    exe = Path(d) / "test"
    source_file.write_text(code)
    subprocess.run([shutil.which("cc") or "cc", "-std=c11", "-O2",
                    "-Wall", "-Wextra", "-Werror", str(source_file),
                    "-o", str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
