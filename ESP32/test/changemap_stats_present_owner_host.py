#!/usr/bin/env python3
"""Compile the real production physical-present gate and exercise WAIT_STATS.

The first exit screen is published *inside* the source SELECT-door callback.
This must exclude later source-world presents until the one-tap handoff, even
though the source gameplay session is still active. No framebuffer allocation.
"""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
gate = (ROOT / "ESP32/src/esp_native_gameplay_present_gate.c").read_text()
resident = (ROOT / "ESP32/src/esp_native_resident_gameplay.c").read_text()
presentation = (ROOT / "ESP32/src/esp_native_transition_presentation.c").read_text()
handoff = (ROOT / "ESP32/src/esp_native_gameplay_transition_handoff.c").read_text()
trace = (ROOT / "ESP32/src/esp_native_sprite_renderer.c").read_text()

# The real-world input and ambient service must not run while this modal owns
# the screen. This order also prevents first-frame/setup behavior being reached
# by a stale WAIT_STATS transition owner.
start = resident.index("void EspNativeResidentGameplay_service(")
world = resident[start:]
assert "if (EspNativeGameplayTransition_isWaitingStats()) return;" in world
assert world.index("if (EspNativeGameplayTransition_isWaitingStats()) return;") < world.index("if (!gameplayState.active)")
assert "EspNativeTransitionPresentation_showStats(transition)" in handoff
assert "__real_Esp32PlatformVideo_present()" in presentation
assert "void EspNativeTransitionPresentation_reset(void)" in presentation
stats_ui = presentation.split("int EspNativeTransitionPresentation_showStats(", 1)[1].split(
    "int EspNativeTransitionPresentation_beginLoading(", 1)[0]
assert "EspNativeGameplayPlayerState_markCompleted(" not in stats_ui, (
    "premature source level completion during stats paint"
)
assert "EspMapLevelExitStats_commitCompletion(" in handoff, (
    "missing authoritative post-commit progress publication"
)

# INFO must not pay the formerly 16-frame phase-measurement and Serial cost.
assert "#if DOOMRPG_LOG_LEVEL >= DOOMRPG_LOG_TRACE" in trace
assert "[SPRITEORDER] sample=" in trace and "[SPRITEPHASE] sample=" in trace
assert trace.index("#if DOOMRPG_LOG_LEVEL >= DOOMRPG_LOG_TRACE\n    if (ok && spritePhaseSampleCount") < trace.index("[SPRITEORDER] sample=")

with tempfile.TemporaryDirectory(prefix="stats-present-owner-") as d:
    tmp = Path(d)
    (tmp / "esp_native_gameplay_present_gate.c").write_text(gate)
    prototypes = {
        "esp_native_gameplay_present_gate.h": "",
        "esp_native_gameplay_level_up.h": (
            "int EspNativeGameplayLevelUp_filterGameplayPresent(void);\n"
            "int EspNativeGameplayLevelUp_repaintOwned(void);\n"
        ),
        "esp_native_gameplay_transition.h":
            "int EspNativeGameplayTransition_isWaitingStats(void);\n",
        "esp_native_transition_presentation.h":
            "int EspNativeTransitionPresentation_isLoadingActive(void);\n",
    }
    for name, code in prototypes.items():
        (tmp / name).write_text(code)
    (tmp / "owner.c").write_text(r"""
#include <assert.h>
#include <stdio.h>
static int stats, loading, levelUp, actionPresents, levelRepaints;
int EspNativeGameplayTransition_isWaitingStats(void) { return stats; }
int EspNativeTransitionPresentation_isLoadingActive(void) { return loading; }
int EspNativeGameplayLevelUp_filterGameplayPresent(void) { return levelUp; }
int EspNativeGameplayLevelUp_repaintOwned(void) {
    ++levelRepaints;
    return 1;
}
int EspNativeGameplayActionEngine_present(void) {
    ++actionPresents;
    return 1;
}
int __wrap_Esp32PlatformVideo_present(void);
int EspNativeGameplayPresentGate_armOne(void);
unsigned int EspNativeGameplayPresentGate_suppressedCount(void);
int main(void) {
    for (int iteration = 0; iteration < 1000; ++iteration) {
        stats = loading = levelUp = 0;
        actionPresents = levelRepaints = 0;
        /* Before the source map exit: normal gameplay is preserved. */
        assert(__wrap_Esp32PlatformVideo_present() == 1);
        assert(actionPresents == 1 && levelRepaints == 0);

        /* Exit screen won on the direct physical path. The old source-world
         * compositor MUST NOT reach the physical screen during WAIT_STATS. */
        stats = 1;
        for (int n = 0; n < 10; ++n) {
            assert(__wrap_Esp32PlatformVideo_present() == 1);
        }
        assert(actionPresents == 1 && levelRepaints == 0);

        /* Stats acknowledgement begins loading, then source session resets. */
        loading = 1;
        assert(__wrap_Esp32PlatformVideo_present() == 1);
        stats = 0;
        assert(__wrap_Esp32PlatformVideo_present() == 1);
        assert(actionPresents == 1);

        /* Target gameplay owns physical present again after loading. */
        loading = 0;
        assert(__wrap_Esp32PlatformVideo_present() == 1);
        assert(actionPresents == 2);

        /* Unrelated LEVEL UP modal retains its prior precedence. */
        levelUp = 1;
        assert(__wrap_Esp32PlatformVideo_present() == 1);
        assert(levelRepaints == 1 && actionPresents == 2);
        levelUp = 0;

        /* Existing one-shot suppression contract also remains unchanged. */
        const unsigned before = EspNativeGameplayPresentGate_suppressedCount();
        assert(EspNativeGameplayPresentGate_armOne() == 1);
        assert(__wrap_Esp32PlatformVideo_present() == 1);
        assert(EspNativeGameplayPresentGate_suppressedCount() == before + 1U);
        assert(actionPresents == 2);
    }
    puts("PASS WAIT_STATS full-screen exclusive physical owner, loading, levelup, normal, one-shot (1000 cycles)");
    return 0;
}
""")
    exe = tmp / "test"
    subprocess.run([
        "cc", "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror",
        str(tmp / "esp_native_gameplay_present_gate.c"),
        str(tmp / "owner.c"), "-I", str(tmp), "-o", str(exe)
    ], check=True)
    subprocess.run([str(exe)], check=True)
