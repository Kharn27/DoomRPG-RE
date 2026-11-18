#!/usr/bin/env python3
"""ARCH-RC11: retire ambient/compose wraps with exclusive explicit scoped calls.

Scan all real ESP32 C/C++ TUs. Compile and execute extracted *real* C
begin/end/ambient/compose bodies with fail-closed fault and nested probes.
Do not stub out or change actual wrappers for renderTurn/getLine/view.
"""
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "src"
DYNAMIC = SRC / "esp_native_dynamic_line_render.c"
HEADER = ROOT / "include/esp_native_dynamic_line_render.h"
CONFIG = ROOT / "platformio.ini"

RAW_AMBIENT = "EspNativeGameplayFrame_renderAmbient"
RAW_COMPOSE = "EspNativeGameplayFrame_composeTurn"
OWN_AMBIENT = "EspNativeDynamicLineRender_renderAmbient"
OWN_COMPOSE = "EspNativeDynamicLineRender_composeTurn"


def require(ok: bool, why: str) -> None:
    if not ok:
        raise AssertionError(why)


def cleaned(text: str) -> str:
    text = re.sub(r"/\*[\s\S]*?\*/|//[^\n]*", "", text)
    return re.sub(r'"(?:\\.|[^"\\])*"', '""', text)


def scan() -> None:
    files = {p.name: cleaned(p.read_text(encoding="utf-8"))
             for p in SRC.iterdir() if p.suffix in (".c", ".cpp")}
    require(len(files) >= 178, "incomplete firmware-wide C/C++ audit")
    flags = CONFIG.read_text(encoding="utf-8")
    for name in (RAW_AMBIENT, RAW_COMPOSE):
        require(f"-Wl,--wrap={name}" not in flags,
                "retired RC11 linker interposition present " + name)
        require(not any(f"__real_{name}(" in src or f"__wrap_{name}(" in src
                        for src in files.values()),
                "obsolete GNU alias body/prototype present " + name)
    for left in ("EspNativeGameplayFrame_renderTurn", "EspMapRuntime_getLine",
                 "EspMapLineState_view"):
        require(f"-Wl,--wrap={left}" in flags,
                "RC11 must not retire cardinal/door line read " + left)

    expected = {
        RAW_AMBIENT: {"esp_native_dynamic_line_render.c": 1,
                      "esp_native_gameplay_frame.c": 1},
        RAW_COMPOSE: {"esp_native_dynamic_line_render.c": 1,
                      "esp_native_gameplay_frame.c": 1},
        OWN_AMBIENT: {"esp_native_dynamic_line_render.c": 1,
                      "esp_native_resident_gameplay.c": 1},
        OWN_COMPOSE: {"esp_native_dynamic_line_render.c": 1,
                      "esp_native_gameplay_event_chain.c": 1},
    }
    for api, target in expected.items():
        found = {}
        for name, src in files.items():
            count = len(re.findall(r"\b" + api + r"\s*\(", src))
            if count:
                found[name] = count
        require(found == target,
                f"RC11 leaked direct leaf caller or duplicated owner: {api} {found}")
    header = HEADER.read_text(encoding="utf-8")
    require(header.count("int " + OWN_AMBIENT + "(") == 1 and
            header.count("int " + OWN_COMPOSE + "(") == 1,
            "native ownership API missing/duplicate")
    require('#include "esp_native_dynamic_line_render.h"' in
            (SRC / "esp_native_gameplay_event_chain.c").read_text(encoding="utf-8"),
            "unpublished script capture missing owner import")

    dynamic = DYNAMIC.read_text(encoding="utf-8")
    require(dynamic.count("if (!beginDynamicPreview()) return 0;") == 4,
            "shared scope admission changed for rotate/move/ambient/compose")
    require(dynamic.count('return endDynamicPreview(ok, "ambient");') == 1,
            "ambient dynamic-line scope not closed")
    require(dynamic.count('return endDynamicPreview(ok, "compose");') == 1,
            "unpublished script composition dynamic-line scope not closed")
    print(f"PASS ARCH-RC11 source scan: {len(files)} actual C/C++ units, "
          "two retired wraps, exactly two owner callsites, renderTurn retained")


def extract(source: str, name: str) -> str:
    match = re.search(r"^(?:static )?int " + re.escape(name) +
                      r"\s*\([^;]*?\)\s*\{", source, re.M | re.S)
    require(match is not None, "missing actual C function " + name)
    start = match.start()
    begin = source.index("{", start)
    depth = 0
    for index in range(begin, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[start:index + 1]
    raise AssertionError("unclosed real C body " + name)


PREFIX = r"""
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
struct Render_s { int field; };
typedef struct EspNativeGameplayFrameStats_s { uint32_t frameAfterFNV; }
    EspNativeGameplayFrameStats;
typedef struct AnimatedClipContext_s { uint8_t valid; } AnimatedClipContext;
static uint8_t dynamicFrameActive, dynamicAnimationFault;
static uint32_t dynamicOpenLineReads, dynamicAnimatedLineReads;
static uint32_t dynamicTextureVariantReads;
static AnimatedClipContext animatedClip;
static int ambientResult = 1, composeResult = 1;
static int faultAmbient, faultCompose;
static unsigned ambientCalls, composeCalls, ambientScoped, composeScoped;
static unsigned cleanAtLeaf;
static struct Render_s* lastRender;
static EspNativeGameplayFrameStats* lastStats;
static uint8_t lastAngle;

int EspNativeGameplayFrame_renderAmbient(
    struct Render_s* render, EspNativeGameplayFrameStats* stats) {
    ++ambientCalls;
    ambientScoped = dynamicFrameActive;
    cleanAtLeaf = dynamicOpenLineReads == 0 && dynamicAnimatedLineReads == 0 &&
                  dynamicTextureVariantReads == 0 && animatedClip.valid == 0;
    lastRender = render;
    lastStats = stats;
    if (faultAmbient) dynamicAnimationFault = 1;
    return ambientResult;
}
int EspNativeGameplayFrame_composeTurn(
    struct Render_s* render, uint8_t angle, EspNativeGameplayFrameStats* stats) {
    ++composeCalls;
    composeScoped = dynamicFrameActive;
    cleanAtLeaf = dynamicOpenLineReads == 0 && dynamicAnimatedLineReads == 0 &&
                  dynamicTextureVariantReads == 0 && animatedClip.valid == 0;
    lastRender = render;
    lastStats = stats;
    lastAngle = angle;
    if (faultCompose) dynamicAnimationFault = 1;
    return composeResult;
}
int NeverCallAmbient(struct Render_s* r, EspNativeGameplayFrameStats* s) {
    (void)r; (void)s; return 1;
}
int NeverCallCompose(struct Render_s* r, uint8_t a,
                     EspNativeGameplayFrameStats* s) {
    (void)r; (void)a; (void)s; return 1;
}
"""

CHECK = r"""
int main(void) {
    struct Render_s render = {0};
    EspNativeGameplayFrameStats stats = {0};

    dynamicOpenLineReads = 47;
    dynamicAnimatedLineReads = 39;
    dynamicTextureVariantReads = 19;
    animatedClip.valid = 12;
    assert(EspNativeDynamicLineRender_renderAmbient(&render, &stats) == 1);
    assert(ambientCalls == 1 && ambientScoped == 1 && cleanAtLeaf);
    assert(lastRender == &render && lastStats == &stats);
    assert(dynamicFrameActive == 0 && animatedClip.valid == 0);

    dynamicFrameActive = 1;
    assert(EspNativeDynamicLineRender_renderAmbient(&render, &stats) == 0);
    assert(ambientCalls == 1 && dynamicFrameActive == 1);
    dynamicFrameActive = 0;

    ambientResult = 0;
    assert(EspNativeDynamicLineRender_renderAmbient(&render, &stats) == 0);
    assert(ambientCalls == 2 && dynamicFrameActive == 0);
    ambientResult = 1;

    faultAmbient = 1;
    assert(EspNativeDynamicLineRender_renderAmbient(&render, &stats) == 0);
    assert(ambientCalls == 3 && dynamicFrameActive == 0);
    faultAmbient = 0;

    dynamicOpenLineReads = 9;
    dynamicAnimatedLineReads = 8;
    dynamicTextureVariantReads = 7;
    animatedClip.valid = 9;
    assert(EspNativeDynamicLineRender_composeTurn(&render, 64, &stats) == 1);
    assert(composeCalls == 1 && composeScoped == 1 && cleanAtLeaf);
    assert(lastRender == &render && lastStats == &stats && lastAngle == 64);
    assert(dynamicFrameActive == 0 && animatedClip.valid == 0);

    dynamicFrameActive = 1;
    assert(EspNativeDynamicLineRender_composeTurn(&render, 128, &stats) == 0);
    assert(composeCalls == 1 && dynamicFrameActive == 1);
    dynamicFrameActive = 0;

    composeResult = 0;
    assert(EspNativeDynamicLineRender_composeTurn(&render, 192, &stats) == 0);
    assert(composeCalls == 2 && lastAngle == 192 && dynamicFrameActive == 0);
    composeResult = 1;

    faultCompose = 1;
    assert(EspNativeDynamicLineRender_composeTurn(&render, 0, &stats) == 0);
    assert(composeCalls == 3 && lastAngle == 0 && dynamicFrameActive == 0);
    faultCompose = 0;

    assert(EspNativeDynamicLineRender_renderAmbient(&render, &stats) == 1);
    assert(ambientCalls == 4 && ambientScoped == 1 && cleanAtLeaf);
    assert(dynamicFrameActive == 0 && animatedClip.valid == 0);
    puts("PASS RC11 real C ambient/compose scope: nested, failure, fault, cleanup");
    return 0;
}
"""


def replace_once(value: str, old: str, new: str) -> str:
    require(value.count(old) == 1, "mutant anchor mismatch: " + old)
    return value.replace(old, new, 1)


def exercise(chunks: dict[str, str]) -> bool:
    source = PREFIX + "\n".join(chunks[n] for n in (
        "beginDynamicPreview", "endDynamicPreview",
        OWN_AMBIENT, OWN_COMPOSE)) + CHECK
    with tempfile.TemporaryDirectory(prefix="doomrpg-rc11-") as temp:
        cfile = Path(temp) / "scoped.c"
        exe = Path(temp) / "scoped"
        cfile.write_text(source, encoding="utf-8")
        built = subprocess.run(
            ["cc", "-O0", "-std=c11", "-Wall", "-Wextra", "-Werror",
             str(cfile), "-o", str(exe)], capture_output=True, text=True)
        if built.returncode != 0:
            raise AssertionError("actual extracted C fails to compile:\n" +
                                 built.stderr[:6000])
        run = subprocess.run([str(exe)], capture_output=True, text=True,
                             timeout=15)
        return run.returncode == 0 and "PASS RC11 real C" in run.stdout


def main() -> None:
    scan()
    real = DYNAMIC.read_text(encoding="utf-8")
    chunks = {name: extract(real, name) for name in (
        "beginDynamicPreview", "endDynamicPreview",
        OWN_AMBIENT, OWN_COMPOSE)}
    require(exercise(chunks), "RC11 actual C behavior probe failed")
    mutants = [
        ("nested scope accepted", "beginDynamicPreview",
         "if (dynamicFrameActive != 0U) return 0;", "if (0) return 0;"),
        ("scope not armed", "beginDynamicPreview",
         "dynamicFrameActive = 1U;", "dynamicFrameActive = 0U;"),
        ("dirty line counters", "beginDynamicPreview",
         "dynamicOpenLineReads = 0U;", "dynamicOpenLineReads = 3U;"),
        ("fault state not cleared", "beginDynamicPreview",
         "dynamicAnimationFault = 0U;", "dynamicAnimationFault = 1U;"),
        ("fault not propagated", "endDynamicPreview",
         "const int ok = rendered && dynamicAnimationFault == 0U;",
         "const int ok = rendered;"),
        ("scope leaked", "endDynamicPreview",
         "dynamicFrameActive = 0U;", "dynamicFrameActive = 1U;"),
        ("clip leaked", "endDynamicPreview",
         "animatedClip.valid = 0U;", "animatedClip.valid = 1U;"),
        ("ambient no begin", OWN_AMBIENT,
         "if (!beginDynamicPreview()) return 0;", "if (0) return 0;"),
        ("compose no begin", OWN_COMPOSE,
         "if (!beginDynamicPreview()) return 0;", "if (0) return 0;"),
        ("ambient no end", OWN_AMBIENT,
         'return endDynamicPreview(ok, "ambient");', "return ok;"),
        ("compose no end", OWN_COMPOSE,
         'return endDynamicPreview(ok, "compose");', "return ok;"),
        ("ambient bypass raw", OWN_AMBIENT,
         "ok = EspNativeGameplayFrame_renderAmbient(",
         "ok = NeverCallAmbient("),
        ("compose bypass raw", OWN_COMPOSE,
         "ok = EspNativeGameplayFrame_composeTurn(",
         "ok = NeverCallCompose("),
        ("compose wrong angle", OWN_COMPOSE,
         "render, angle, outStats);",
         "render, (uint8_t)(angle ^ 1U), outStats);"),
    ]
    for label, name, before, after in mutants:
        altered = dict(chunks)
        altered[name] = replace_once(altered[name], before, after)
        require(not exercise(altered), "RC11 behavior mutant survived: " + label)
    print(f"PASS ARCH-RC11 compiled {len(chunks)} real C bodies, "
          f"{len(mutants)} mutation faults rejected")


if __name__ == "__main__":
    main()
