#!/usr/bin/env python3
"""ARCH-RC24: compile the real C present gate and reject semantic regressions.

No framebuffer/TFT mock takes ownership of the gate: this checks the exact
production C function bodies with a bounded set of injected failure/priority
mutants. The CYD Serial log remains the final hardware authority.
"""
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "src"
GATE = (SRC / "esp_native_gameplay_present_gate.c").read_text()
BRIDGE = (SRC / "platform_video_c_bridge.cpp").read_text()
HEADER = (ROOT / "include/platform_video_c_bridge.h").read_text()
INI = (ROOT / "platformio.ini").read_text()


def require(ok, message):
    if not ok:
        raise AssertionError(message)


def extract(name):
    signature = re.compile(r"^((?:int|void|unsigned int))\s+" +
                           re.escape(name) + r"\s*\([^;]*?\)\s*\{",
                           re.M | re.S)
    matches = list(signature.finditer(GATE))
    require(len(matches) == 1, "missing or duplicate real C " + name)
    start = matches[0].start()
    open_brace = GATE.index("{", start)
    depth = 0
    for pos in range(open_brace, len(GATE)):
        if GATE[pos] == "{":
            depth += 1
        elif GATE[pos] == "}":
            depth -= 1
            if depth == 0:
                return GATE[start:pos + 1]
    raise AssertionError("unbalanced production body: " + name)


NAMES = (
    "EspNativeGameplayPresentGate_armOne",
    "EspNativeGameplayPresentGate_cancel",
    "EspNativeGameplayPresentGate_isArmed",
    "EspNativeGameplayPresentGate_suppressedCount",
    "Esp32PlatformVideo_present",
)
BODIES = {name: extract(name) for name in NAMES}
PUBLIC = "Esp32PlatformVideo_present"


def scan():
    normal = INI.split("[env:esp32-cyd]", 1)[1].split("[env:esp-cyd-inverted]", 1)[0]
    bringup = INI.split("[env:esp32-cyd-bringup]", 1)[1].split(
        "[env:esp32-cyd-trace]", 1)[0]
    flags = lambda s: re.findall(r"^\s+-Wl,--wrap=([A-Za-z_0-9]+)", s, re.M)
    require(flags(normal) == [], "ARCH-RC24 must have ZERO production GNU wrappers")
    require(set(flags(bringup)) == {
        "EspNativeFirstFrame_route",
        "EspNativeGameplayInteractionInventory_log",
        "EspNativeResidentGameplay_service",
    }, "bringup diagnostic GNU inventory drifted")
    require('extern "C" int Esp32PlatformVideo_presentBase(void)' in BRIDGE,
            "raw physical LCD Base leaf is missing")
    require("return PlatformVideo_present() ? 1 : 0;" in BRIDGE,
            "raw physical LCD leaf changed TFT semantics")
    require("int Esp32PlatformVideo_present(void);" in HEADER and
            "int Esp32PlatformVideo_presentBase(void);" in HEADER,
            "public/direct physical APIs must remain exported")
    sources = [p for p in SRC.iterdir() if p.suffix in (".c", ".cpp")]
    require(len(sources) >= 178, "incomplete production source inventory")
    for p in sources:
        value = p.read_text()
        stripped = re.sub(r"/\*[\s\S]*?\*/|//[^\n]*", "", value)
        require(not re.search(r"\b__(?:wrap|real)_Esp32PlatformVideo_present\b",
                              stripped), "dead physical GNU alias in " + p.name)
        require(not re.search(r"\b-Wl,--wrap=Esp32PlatformVideo_present\b", stripped),
                "retired physical GNU flag in " + p.name)
    require("Esp32PlatformVideo_presentBase()" in
            (SRC / "esp_native_transition_presentation.c").read_text(),
            "loading/stats must call physical Base")
    require("Esp32PlatformVideo_presentBase()" in
            (SRC / "esp_native_gameplay_level_up.c").read_text(),
            "levelup modal must call physical Base")
    require("Esp32PlatformVideo_presentBase()" in
            (SRC / "esp_native_gameplay_gib_fx.c").read_text(),
            "HUB bypass must call physical Base")
    require("EspNativeGameplayActionEngine_presentBase()" in
            (SRC / "esp_native_gameplay_action_engine.c").read_text(),
            "action layer must call its own explicit Base")
    require("return EspNativeGameplayActionEngine_present();" in BODIES[PUBLIC],
            "public gate must compose action owner")
    print("PASS RC24 source: zero production wrappers, three bringup diagnostics, named physical Base")


PREFIX = r"""
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
static uint8_t gateArmed;
static unsigned int suppressedCount;
static int loading, stats, level, levelOk, actionOk;
static int actionCalls, levelCalls;
static int EspNativeTransitionPresentation_isLoadingActive(void) { return loading; }
static int EspNativeGameplayTransition_isWaitingStats(void) { return stats; }
static int EspNativeGameplayLevelUp_filterGameplayPresent(void) { return level; }
static int EspNativeGameplayLevelUp_repaintOwned(void) { ++levelCalls; return levelOk; }
static int EspNativeGameplayActionEngine_present(void) { ++actionCalls; return actionOk; }
"""
SUFFIX = r"""
int main(void) {
    for (int n = 0; n < 2048; ++n) {
        loading = stats = level = 0;
        actionCalls = levelCalls = 0;
        levelOk = actionOk = 1;
        EspNativeGameplayPresentGate_cancel();
        assert(!EspNativeGameplayPresentGate_isArmed());
        assert(Esp32PlatformVideo_present() == 1);
        assert(actionCalls == 1 && levelCalls == 0);
        actionOk = 0;
        assert(Esp32PlatformVideo_present() == 0);
        assert(actionCalls == 2);
        actionOk = 1;

        unsigned int count = EspNativeGameplayPresentGate_suppressedCount();
        assert(EspNativeGameplayPresentGate_armOne() == 1);
        assert(EspNativeGameplayPresentGate_isArmed() == 1);
        assert(EspNativeGameplayPresentGate_armOne() == 0);
        loading = stats = level = 1;
        assert(Esp32PlatformVideo_present() == 1);
        assert(EspNativeGameplayPresentGate_suppressedCount() == count + 1);
        assert(EspNativeGameplayPresentGate_isArmed() == 0);
        assert(actionCalls == 2 && levelCalls == 0);
        assert(Esp32PlatformVideo_present() == 1);
        assert(actionCalls == 2 && levelCalls == 0);
        loading = 0;
        assert(Esp32PlatformVideo_present() == 1);
        assert(actionCalls == 2 && levelCalls == 0);
        stats = 0;
        assert(Esp32PlatformVideo_present() == 1);
        assert(actionCalls == 2 && levelCalls == 1);
        levelOk = 0;
        assert(Esp32PlatformVideo_present() == 0);
        assert(actionCalls == 2 && levelCalls == 2);
        level = 0;
        assert(Esp32PlatformVideo_present() == 1);
        assert(actionCalls == 3 && levelCalls == 2);
        loading = 1;
        assert(EspNativeGameplayPresentGate_armOne() == 1);
        EspNativeGameplayPresentGate_cancel();
        assert(EspNativeGameplayPresentGate_isArmed() == 0);
        assert(Esp32PlatformVideo_present() == 1);
        assert(EspNativeGameplayPresentGate_suppressedCount() == count + 1);
        loading = 0;
    }
    puts("PASS RC24 real C: 2048 suppression/loading/stats/levelup/world/fault cycles");
    return 0;
}
"""


def run(bodies):
    code = PREFIX + "\n" + "\n".join(bodies[n] for n in NAMES) + "\n" + SUFFIX
    with tempfile.TemporaryDirectory(prefix="arch-rc24-present-") as directory:
        filename = Path(directory) / "present.c"
        exe = Path(directory) / "present"
        filename.write_text(code)
        build = subprocess.run(
            ["cc", "-std=c11", "-O0", "-Wall", "-Wextra", "-Werror",
             "-Wno-unused-function", str(filename), "-o", str(exe)],
            capture_output=True, text=True)
        require(build.returncode == 0, "real C bodies fail compile: " + build.stderr[:4000])
        out = subprocess.run([str(exe)], capture_output=True, text=True, timeout=15)
        return out.returncode == 0 and "PASS RC24 real C:" in out.stdout


MUTANTS = (
    ("allow double arm", "EspNativeGameplayPresentGate_armOne",
     "if (gateArmed != 0U) return 0;", "if (0) return 0;"),
    ("never arm", "EspNativeGameplayPresentGate_armOne",
     "gateArmed = 1U;", "gateArmed = 0U;"),
    ("never cancel", "EspNativeGameplayPresentGate_cancel",
     "gateArmed = 0U;", "gateArmed = 1U;"),
    ("lie about armed status", "EspNativeGameplayPresentGate_isArmed",
     "return gateArmed != 0U;", "return gateArmed == 0U;"),
    ("lie about suppressed count", "EspNativeGameplayPresentGate_suppressedCount",
     "return suppressedCount;", "return suppressedCount + 1U;"),
    ("disable one-shot gate", PUBLIC,
     "if (gateArmed != 0U) {", "if (0) {"),
    ("consume without clearing", PUBLIC,
     "gateArmed = 0U;", "gateArmed = 1U;"),
    ("drop suppression accounting", PUBLIC,
     "++suppressedCount;", "((void)0);"),
    ("suppress wrong result", PUBLIC,
     "++suppressedCount;\n        return 1;", "++suppressedCount;\n        return 0;"),
    ("bypass loading owner", PUBLIC,
     "if (EspNativeTransitionPresentation_isLoadingActive()) {", "if (0) {"),
    ("bypass stats owner", PUBLIC,
     "if (EspNativeGameplayTransition_isWaitingStats()) {", "if (0) {"),
    ("bypass levelup owner", PUBLIC,
     "if (EspNativeGameplayLevelUp_filterGameplayPresent()) {", "if (0) {"),
    ("ignore levelup failure", PUBLIC,
     "return EspNativeGameplayLevelUp_repaintOwned();",
     "(void)EspNativeGameplayLevelUp_repaintOwned(); return 1;"),
    ("ignore world failure", PUBLIC,
     "return EspNativeGameplayActionEngine_present();",
     "(void)EspNativeGameplayActionEngine_present(); return 1;"),
)


def main():
    scan()
    require(run(BODIES), "unmodified production gate fails real-C oracle")
    for description, owner, old, new in MUTANTS:
        require(BODIES[owner].count(old) == 1, "nonunique mutation: " + description)
        variant = dict(BODIES)
        variant[owner] = variant[owner].replace(old, new)
        require(not run(variant), "adversarial mutant survived: " + description)
    print(f"PASS ARCH-RC24: {len(NAMES)} actual C bodies, {len(MUTANTS)} mutants rejected, zero GNU production wrappers")


if __name__ == "__main__":
    main()
