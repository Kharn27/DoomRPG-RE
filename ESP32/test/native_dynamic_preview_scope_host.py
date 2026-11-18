#!/usr/bin/env python3
"""ARCH-RC10: two real C dynamic-preview entrypoints after retiring GNU ld wraps.

The host fixture compiles and executes the actual begin/end + rotate/move
scopes extracted from the firmware source, not a model reimplementation.
Other frame, map-state, and door-animation interpositions remain untouched.
"""
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "src"
DYNAMIC = SRC / "esp_native_dynamic_line_render.c"
RESIDENT = SRC / "esp_native_resident_gameplay.c"
HEADER = ROOT / "include/esp_native_dynamic_line_render.h"
CONFIG = ROOT / "platformio.ini"
POSE = "EspNativeGameplayFrame_renderVisualPose"
MOVE = "EspNativeGameplayFrame_renderVisualMove"
SCOPED_POSE = "EspNativeDynamicLineRender_renderVisualPose"
SCOPED_MOVE = "EspNativeDynamicLineRender_renderVisualMove"


def require(condition: bool, why: str) -> None:
    if not condition:
        raise AssertionError(why)


def cleaned(value: str) -> str:
    value = re.sub(r"/\*[\s\S]*?\*/|//[^\n]*", "", value)
    return re.sub(r'"(?:\\.|[^"\\])*"', '""', value)


def scan() -> None:
    all_source = {p.name: cleaned(p.read_text(encoding="utf-8"))
                  for p in SRC.iterdir() if p.suffix in (".c", ".cpp")}
    require(len(all_source) >= 178, "incomplete real firmware source scan")
    flags = CONFIG.read_text(encoding="utf-8")
    for raw in (POSE, MOVE):
        require(f"-Wl,--wrap={raw}" not in flags,
                f"retired preview interposition resurrected: {raw}")
        require(not any(f"__wrap_{raw}(" in text or
                        f"__real_{raw}(" in text
                        for text in all_source.values()),
                f"retired GNU ld alias remains: {raw}")
    require("-Wl,--wrap=EspNativeGameplayFrame_renderTurn" in flags,
            "must not retire the cardinal/door-anim wrapper with previews")
    expected = {
        POSE: {"esp_native_dynamic_line_render.c": 1,
               "esp_native_gameplay_frame.c": 1},
        MOVE: {"esp_native_dynamic_line_render.c": 1,
               "esp_native_gameplay_frame.c": 1},
        SCOPED_POSE: {"esp_native_dynamic_line_render.c": 1,
                      "esp_native_resident_gameplay.c": 1},
        SCOPED_MOVE: {"esp_native_dynamic_line_render.c": 1,
                      "esp_native_resident_gameplay.c": 1},
    }
    for name, owners in expected.items():
        found = {}
        for filename, content in all_source.items():
            count = len(re.findall(r"\b" + name + r"\s*\(", content))
            if count:
                found[filename] = count
        require(found == owners, f"{name}: bypass or duplicate: {found}")
    header = HEADER.read_text(encoding="utf-8")
    for name in (SCOPED_POSE, SCOPED_MOVE):
        require(header.count(f"int {name}(") == 1,
                f"missing permanent API: {name}")
    dyn = DYNAMIC.read_text(encoding="utf-8")
    require(dyn.count("if (!beginDynamicPreview()) return 0;") == 4,
            "shared fail-closed scope drift for ambient, compose, rotate, move")
    require(dyn.count('return endDynamicPreview(ok, "rotate");') == 1,
            "rotate scope may leak")
    require(dyn.count('return endDynamicPreview(ok, "move");') == 1,
            "move scope may leak")
    print(f"PASS ARCH-RC10: {len(all_source)} real C/C++ units, "
          "2 scoped exports, exclusive callsites, cardinal wrapper retained")


def extract(source: str, name: str) -> str:
    pattern = re.compile(r"^(?:static )?int " + re.escape(name) +
                         r"\s*\([^;]*?\)\s*\{", re.M | re.S)
    match = pattern.search(source)
    require(match is not None, f"missing real C body: {name}")
    opening = source.index("{", match.start())
    depth = 0
    for pos in range(opening, len(source)):
        if source[pos] == "{":
            depth += 1
        elif source[pos] == "}":
            depth -= 1
            if depth == 0:
                return source[match.start():pos + 1]
    raise AssertionError("unclosed real C body " + name)


PREFIX = r"""
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
struct Render_s { int unused; };
struct EspPlayerViewState_s { int unused; };
typedef struct EspNativeGameplayFrameStats_s { uint32_t frameAfterFNV; }
    EspNativeGameplayFrameStats;
typedef struct AnimatedClipContext_s { uint8_t valid; } AnimatedClipContext;
static uint8_t dynamicFrameActive, dynamicAnimationFault;
static uint32_t dynamicOpenLineReads, dynamicAnimatedLineReads;
static uint32_t dynamicTextureVariantReads;
static AnimatedClipContext animatedClip;
static int poseResult = 1, moveResult = 1, faultMove;
static unsigned int poseCalls, moveCalls;
static unsigned int observedPoseScope, observedMoveScope, observedClean;
static const struct Render_s* lastRender;
static const struct EspPlayerViewState_s* lastBefore;
static const struct EspPlayerViewState_s* lastAfter;
static uint8_t lastAngle, lastStep, lastDenominator;
int EspNativeGameplayFrame_renderVisualPose(
    struct Render_s* render, const struct EspPlayerViewState_s* visual,
    uint8_t settledAngle, EspNativeGameplayFrameStats* stats) {
    (void)stats;
    ++poseCalls;
    observedPoseScope = dynamicFrameActive;
    observedClean = dynamicOpenLineReads == 0 &&
        dynamicAnimatedLineReads == 0 && dynamicTextureVariantReads == 0 &&
        animatedClip.valid == 0;
    lastRender = render;
    lastBefore = visual;
    lastAngle = settledAngle;
    return poseResult;
}
int EspNativeGameplayFrame_renderVisualMove(
    struct Render_s* render, const struct EspPlayerViewState_s* before,
    const struct EspPlayerViewState_s* after, uint8_t step,
    uint8_t denominator, EspNativeGameplayFrameStats* stats) {
    (void)stats;
    ++moveCalls;
    observedMoveScope = dynamicFrameActive;
    observedClean = dynamicOpenLineReads == 0 &&
        dynamicAnimatedLineReads == 0 && dynamicTextureVariantReads == 0 &&
        animatedClip.valid == 0;
    lastRender = render;
    lastBefore = before;
    lastAfter = after;
    lastStep = step;
    lastDenominator = denominator;
    if (faultMove) dynamicAnimationFault = 1;
    return moveResult;
}
int NeverCallPose(struct Render_s* r, const struct EspPlayerViewState_s* v,
                  uint8_t angle, EspNativeGameplayFrameStats* stats) {
    (void)r; (void)v; (void)angle; (void)stats;
    return 1;
}
int NeverCallMove(struct Render_s* r, const struct EspPlayerViewState_s* a,
                  const struct EspPlayerViewState_s* b, uint8_t step,
                  uint8_t denominator, EspNativeGameplayFrameStats* stats) {
    (void)r; (void)a; (void)b; (void)step; (void)denominator; (void)stats;
    return 1;
}
"""
CHECK = r"""
int main(void) {
    struct Render_s render = {0};
    struct EspPlayerViewState_s before = {0}, after = {0};
    EspNativeGameplayFrameStats stats = {0};

    dynamicOpenLineReads = 99;
    dynamicAnimatedLineReads = 55;
    dynamicTextureVariantReads = 44;
    animatedClip.valid = 7;
    assert(EspNativeDynamicLineRender_renderVisualPose(
        &render, &after, 64, &stats) == 1);
    assert(poseCalls == 1 && observedPoseScope == 1 && observedClean);
    assert(lastRender == &render && lastBefore == &after && lastAngle == 64);
    assert(dynamicFrameActive == 0 && animatedClip.valid == 0);

    dynamicFrameActive = 1;
    assert(EspNativeDynamicLineRender_renderVisualPose(
        &render, &after, 64, &stats) == 0);
    assert(poseCalls == 1 && dynamicFrameActive == 1);
    dynamicFrameActive = 0;

    moveResult = 0;
    animatedClip.valid = 7;
    assert(EspNativeDynamicLineRender_renderVisualMove(
        &render, &before, &after, 2, 3, &stats) == 0);
    assert(moveCalls == 1 && observedMoveScope == 1 && observedClean);
    assert(lastRender == &render && lastBefore == &before && lastAfter == &after);
    assert(lastStep == 2 && lastDenominator == 3);
    assert(dynamicFrameActive == 0 && animatedClip.valid == 0);

    moveResult = 1;
    faultMove = 1;
    assert(EspNativeDynamicLineRender_renderVisualMove(
        &render, &before, &after, 1, 2, &stats) == 0);
    assert(moveCalls == 2 && dynamicFrameActive == 0 &&
           animatedClip.valid == 0);

    faultMove = 0;
    assert(EspNativeDynamicLineRender_renderVisualPose(
        &render, &after, 128, &stats) == 1);
    assert(poseCalls == 2 && observedPoseScope == 1 && observedClean);
    assert(dynamicFrameActive == 0 && animatedClip.valid == 0);
    puts("PASS RC10 real C scope: entry, nested fail-closed, move failure, "
         "fault, restore, exclusive raw leaf calls");
    return 0;
}
"""


def replace_once(text: str, before: str, after: str) -> str:
    require(text.count(before) == 1, "mutant anchor mismatch " + before)
    return text.replace(before, after, 1)


def exercise(chunks: dict[str, str]) -> bool:
    source = PREFIX + "\n".join(chunks[n] for n in (
        "beginDynamicPreview", "endDynamicPreview",
        SCOPED_POSE, SCOPED_MOVE)) + CHECK
    with tempfile.TemporaryDirectory(prefix="doomrpg-rc10-") as temp:
        base = Path(temp)
        cfile = base / "preview.c"
        executable = base / "preview"
        cfile.write_text(source, encoding="utf-8")
        build = subprocess.run(
            ["cc", "-O0", "-std=c11", "-Wall", "-Wextra", "-Werror",
             str(cfile), "-o", str(executable)], capture_output=True, text=True)
        if build.returncode != 0:
            raise AssertionError("real extracted C did not compile:\n" +
                                 build.stderr[:6000])
        run = subprocess.run([str(executable)], capture_output=True, text=True)
        return run.returncode == 0 and "PASS RC10 real C scope" in run.stdout


def main() -> None:
    scan()
    source = DYNAMIC.read_text(encoding="utf-8")
    chunks = {name: extract(source, name) for name in (
        "beginDynamicPreview", "endDynamicPreview", SCOPED_POSE, SCOPED_MOVE)}
    require(exercise(chunks), "real RC10 C failed its complete behavior fixture")
    mutants = [
        ("nested scope allowed", "beginDynamicPreview",
         "if (dynamicFrameActive != 0U) return 0;", "if (0) return 0;"),
        ("scope not armed", "beginDynamicPreview",
         "dynamicFrameActive = 1U;", "dynamicFrameActive = 0U;"),
        ("counters not cleared", "beginDynamicPreview",
         "dynamicOpenLineReads = 0U;", "dynamicOpenLineReads = 9U;"),
        ("fault not reset", "beginDynamicPreview",
         "dynamicAnimationFault = 0U;", "dynamicAnimationFault = 1U;"),
        ("fault ignored", "endDynamicPreview",
         "const int ok = rendered && dynamicAnimationFault == 0U;",
         "const int ok = rendered;"),
        ("scope leaks", "endDynamicPreview",
         "dynamicFrameActive = 0U;", "dynamicFrameActive = 1U;"),
        ("animation clip leaks", "endDynamicPreview",
         "animatedClip.valid = 0U;", "animatedClip.valid = 1U;"),
        ("rotate bypasses end", SCOPED_POSE,
         'return endDynamicPreview(ok, "rotate");', "return ok;"),
        ("move bypasses end", SCOPED_MOVE,
         'return endDynamicPreview(ok, "move");', "return ok;"),
        ("rotate bypasses raw", SCOPED_POSE,
         "ok = EspNativeGameplayFrame_renderVisualPose(",
         "ok = NeverCallPose("),
        ("move bypasses raw", SCOPED_MOVE,
         "ok = EspNativeGameplayFrame_renderVisualMove(",
         "ok = NeverCallMove("),
    ]
    for label, name, before, after in mutants:
        altered = dict(chunks)
        altered[name] = replace_once(altered[name], before, after)
        require(not exercise(altered), "mutant survived: " + label)
    print(f"PASS ARCH-RC10 compiled {len(chunks)} real C bodies, "
          f"{len(mutants)} behavior mutants rejected")


if __name__ == "__main__":
    main()
