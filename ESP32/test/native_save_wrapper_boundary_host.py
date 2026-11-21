#!/usr/bin/env python3
"""ARCH-RC14: native SAVE/System-HUB compositor and raw leaves (no GNU ld wrap).

Scan all firmware TU boundaries; compile the actual SYS touch source with its
existing full geometry/transaction host fixture. Reject 11 contract mutants.
The test does not claim real hardware or exercise SD checkpoint persistence.
"""
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "src"
TEST = ROOT / "test"
RETIRED = (
    "EspNativeGameplayHub_handleAction",
    "EspNativeGameplayHubTouchUi_classify",
    "EspNativeGameplayInput_consume",
    "EspNativeGameplaySession_configure",
)


def require(condition, message):
    if not condition:
        raise AssertionError(message)


def strip_comments(text):
    return re.sub(r"/\*[\s\S]*?\*/|//[^\n]*", "", text)


def scan():
    sources = {p.name: strip_comments(p.read_text(encoding="utf-8"))
               for p in SRC.iterdir() if p.suffix in (".c", ".cpp")}
    require(len(sources) >= 178, "firmware corpus unexpectedly small")
    flags = (ROOT / "platformio.ini").read_text(encoding="utf-8")
    for name in RETIRED:
        require("-Wl,--wrap=" + name not in flags,
                "GNU interposition resurrected: " + name)
        for filename, source in sources.items():
            require(not re.search(r"\b__(?:wrap|real)_" + name + r"\b", source),
                    "retired linker alias still compiled in " + filename)
    require("-Wl,--wrap=EspNativeGameplayFrame_renderTurn" not in flags,
            "retired RC23 renderTurn GNU wrapper resurrected")
    require("-Wl,--wrap=Esp32PlatformVideo_present" not in flags,
            "remaining physical-present wrapper inadvertently removed")

    exact = {
        ("esp_native_gameplay_hub.c", "EspNativeGameplayHub_handleActionBase("): 4,
        ("esp_native_gameplay_save.cpp", "EspNativeGameplayHub_handleActionBase("): 3,
        ("esp_native_gameplay_save.cpp", "EspNativeGameplayHub_handleAction(uint8_t action)"): 1,
        ("esp_native_gameplay_hub_touch_ui.c",
         "EspNativeGameplayHubTouchUi_classifyBase("): 1,
        ("esp_native_gameplay_save_touch.c",
         "EspNativeGameplayHubTouchUi_classifyBase("): 3,
        ("esp_native_gameplay_save_touch.c",
         "EspNativeGameplayHubTouchUi_classify("): 1,
        ("esp_native_gameplay_input.c", "EspNativeGameplayInput_consumeBase("): 1,
        ("esp_native_gameplay_save_touch.c",
         "EspNativeGameplayInput_consumeBase("): 2,
        ("esp_native_gameplay_save_touch.c",
         "EspNativeGameplayInput_consume("): 1,
        ("esp_native_gameplay_session.c",
         "EspNativeGameplaySession_configureBase("): 1,
        ("esp_native_gameplay_save_reprime.c",
         "EspNativeGameplaySession_configureBase("): 2,
        ("esp_native_gameplay_save_reprime.c",
         "EspNativeGameplaySession_configure("): 1,
    }
    for (file, witness), count in exact.items():
        require(sources[file].count(witness) == count,
                f"raw owner/API witness drift: {file} {witness}")
    hub = sources["esp_native_gameplay_hub.c"]
    require("EspNativeGameplayHub_handleAction(" not in hub,
            "internal HUB close may reenter SAVE/System wrapper")
    print("PASS RC14 178+ real TU scan: four SAVE/HUB wraps retired, "
          "raw leaves and exclusive composition preserved, cardinal door transaction intact")


REAL = (SRC / "esp_native_gameplay_save_touch.c").read_text(encoding="utf-8")
FIXTURE = (TEST / "test_sys_touch.c").read_text(encoding="utf-8")

MUTANTS = (
    ("SYS left bound", "logicalX < ESP_NATIVE_SYS_BUTTON_LEFT ||",
     "logicalX < 0 ||"),
    ("SYS right bound", "logicalX > ESP_NATIVE_SYS_BUTTON_RIGHT)",
     "logicalX > 159)"),
    ("SYS row inclusive top", "logicalY >= top && logicalY <= bottom",
     "logicalY > top && logicalY <= bottom"),
    ("SYS row inclusive bottom", "logicalY >= top && logicalY <= bottom",
     "logicalY >= top && logicalY < bottom"),
    ("slot top limit", "logicalY >= 47 && logicalY <= 116",
     "logicalY >= 48 && logicalY <= 116"),
    ("slot bottom limit", "logicalY >= 47 && logicalY <= 116",
     "logicalY >= 47 && logicalY < 116"),
    ("slot actionable SELECT", "outHit->action = ESP_NATIVE_GAMEPLAY_ACTION_SELECT;",
     "outHit->action = ESP_NATIVE_GAMEPLAY_ACTION_NONE;"),
    ("blank SYS absorption", "logicalY >= 21 && logicalY < 120",
     "logicalY >= 22 && logicalY < 120"),
    ("invalid cursor fail closed", "current >= ESP_NATIVE_SYS_COUNT",
     "current > ESP_NATIVE_SYS_COUNT"),
    ("cursor navigation", "target == (uint8_t)((current + 1U) % ESP_NATIVE_SYS_COUNT)",
     "target == (uint8_t)((current + 2U) % ESP_NATIVE_SYS_COUNT)"),
    ("slot non-confirm must not dispatch", "outIntent->action = ESP_NATIVE_GAMEPLAY_ACTION_NONE;",
     "outIntent->action = ESP_NATIVE_GAMEPLAY_ACTION_SELECT;"),
)


def exercise(source):
    with tempfile.TemporaryDirectory(prefix="doom-rc14-") as dirname:
        dest = Path(dirname)
        c_file = dest / "actual_save_touch.c"
        executable = dest / "probe"
        c_file.write_text(source, encoding="utf-8")
        build = subprocess.run(
            ["cc", "-std=c11", "-O0", "-Wall", "-Wextra", "-Werror",
             "-I" + str(ROOT / "test/oracles"),
             "-I" + str(ROOT / "include"),
             str(TEST / "test_sys_touch.c"), str(c_file),
             "-o", str(executable)], capture_output=True, text=True)
        require(build.returncode == 0,
                "SYS compositor C fixture failed to compile:\n" + build.stderr[:2500])
        run = subprocess.run([str(executable)], capture_output=True,
                             text=True, timeout=15)
        return run.returncode == 0 and "SYS touch: PASS" in run.stdout


def main():
    scan()
    require(exercise(REAL), "actual SYS compositor failed its 9-route fixture")
    for label, before, after in MUTANTS:
        require(REAL.count(before) >= 1, "negative C fixture source drift: " + label)
        mutated = REAL.replace(before, after, 1)
        require(not exercise(mutated), "adversarial SYS mutation escaped: " + label)
    print("PASS ARCH-RC14 actual SAVE SYS C: 11 semantic mutants rejected, "
          "real immutable/public+base routing and bounds")


if __name__ == "__main__":
    main()
