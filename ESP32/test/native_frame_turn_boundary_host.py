#!/usr/bin/env python3
"""ARCH-RC23: real-C cardinal renderer Base/facade door-transaction oracle.

Extract and compile the unmodified production bodies (not Python replicas).
Only physical present, immutable line owner, door animator, event publisher and
SFX sink are test doubles. The facade, scoped render and Base leaf are REAL C.
Hardware Serial remains necessary; this is a fail-closed host regression gate.
"""
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "src"
FRAME = (SRC / "esp_native_gameplay_frame.c").read_text()
DYNAMIC = (SRC / "esp_native_dynamic_line_render.c").read_text()
INI = (ROOT / "platformio.ini").read_text()
RAW = "EspNativeGameplayFrame_renderTurnBase"
PUBLIC = "EspNativeGameplayFrame_renderTurn"


def require(condition, explanation):
    if not condition:
        raise AssertionError(explanation)


def extract(source, name):
    signature = re.compile(
        r"^(?:static\s+)?int\s+" + re.escape(name)
        + r"\s*\([^;]*?\)\s*\{", re.M | re.S)
    found = list(signature.finditer(source))
    require(len(found) == 1, "missing/duplicate REAL C body: " + name)
    begin = found[0].start()
    opened = source.index("{", found[0].start())
    depth = 0
    for pos in range(opened, len(source)):
        if source[pos] == "{":
            depth += 1
        elif source[pos] == "}":
            depth -= 1
            if depth == 0:
                return source[begin:pos + 1]
    raise AssertionError("unbalanced REAL body " + name)


BODIES = {
    "base": extract(FRAME, RAW),
    "scope": extract(DYNAMIC, "renderDynamicFrame"),
    "public": extract(DYNAMIC, PUBLIC),
}


def scan():
    section = INI.split("[env:esp32-cyd]", 1)[1].split(
        "[env:esp-cyd-inverted]", 1)[0]
    flags = re.findall(r"^\s+-Wl,--wrap=([A-Za-z_0-9]+)", section, re.M)
    require(flags == ["Esp32PlatformVideo_present"],
            "only physical present must remain wrapped in normal firmware")
    require("-Wl,--wrap=" + PUBLIC not in INI,
            "retired cardinal wrap must never return")
    require("int " + PUBLIC + "(" in (ROOT / "include/esp_native_gameplay_frame.h").read_text(),
            "public facade absent from exported native header")
    require("int " + RAW + "(" in (ROOT / "include/esp_native_gameplay_frame.h").read_text(),
            "raw Base leaf absent from exported native header")
    require(DYNAMIC.count(RAW + "(render, angle, outStats)") == 1,
            "one and only one raw call from the scoped native frame owner")
    require("EspNativeGameplayFrame_renderTurnBase" in BODIES["scope"],
            "scoped native render bypasses raw Base")
    require("EspNativeGameplayMoveEvents_onFrameResult" in BODIES["public"],
            "public frame no longer publishes movement result")
    # Read every compiled source rather than just inspecting a hand-picked unit.
    all_sources = {p.name: re.sub(r"/\*[\s\S]*?\*/|//[^\n]*", "", p.read_text())
                   for p in SRC.iterdir() if p.suffix in (".c", ".cpp")}
    require(len(all_sources) >= 178, "source inventory incomplete")
    for filename, source in all_sources.items():
        require(not re.search(r"\b__(?:real|wrap)_" + PUBLIC + r"\b", source),
                "GNU cardinal alias survived in " + filename)
        owner = len(re.findall(r"\bint\s+" + RAW + r"\s*\(", source))
        facade = len(re.findall(r"\bint\s+" + PUBLIC + r"\s*\(", source))
        require(owner == (1 if filename == "esp_native_gameplay_frame.c" else 0),
                "raw leaf must be defined exactly in frame owner " + filename)
        require(facade == (1 if filename == "esp_native_dynamic_line_render.c" else 0),
                "public facade must be owned by dynamic-line renderer " + filename)
    print("PASS RC23 source: one production GNU wrap, raw/public exclusive owners, no GNU cardinal alias")


PREFIX = r"""
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define DRPG_LOGT(...) ((void)0)
#define ESP_NATIVE_DOOR_ANIMATION_FRAMES 3U

struct Render_s { int tag; };
struct EspPlayerViewState_s { int active; };
typedef struct EspNativeGameplayFrameStats_s {
    uint32_t frameAfterFNV;
    uint8_t finalPresented;
} EspNativeGameplayFrameStats;
typedef struct { uint32_t openCount; } EspMapLineStateView;
typedef struct {
    uint8_t ordinal, totalFrames, activeLines, geometryActive;
} EspNativeDoorAnimationFrame;
typedef struct { uint32_t completedTransitions; } EspNativeDoorAnimatorView;
typedef struct { uint8_t valid; } AnimatedClipContext;

static uint8_t dynamicFrameActive, dynamicAnimationFault;
static uint32_t dynamicOpenLineReads, dynamicAnimatedLineReads, dynamicTextureVariantReads;
static AnimatedClipContext animatedClip;

static struct Render_s liveRender;
static struct EspPlayerViewState_s liveView;
static EspMapLineStateView lineView;
static EspNativeDoorAnimatorView doorView;
static int canPresent=1, injectFault;
static int rawCalls, physicalCalls, pending, holding, validateOk=1;
static int prepareCalls, finishCalls, resetCalls, prepFail, finishFail, finished;
static int eventCalls, lastEvent=-1, openSfxCalls, closeSfxCalls, musicPumps;
static int openSfxResult, closeSfxResult;

static const struct EspPlayerViewState_s* EspPlayerView_view(void) {
    return &liveView;
}
static int renderComposed(struct Render_s* render,
                         const struct EspPlayerViewState_s* view,
                         uint8_t angle, int preview, int publish,
                         EspNativeGameplayFrameStats* stats) {
    assert(render == &liveRender && view == &liveView);
    assert((angle & 63U) == 0U && preview == 0 && publish == 1);
    assert(dynamicFrameActive == 1U);
    ++rawCalls;
    ++physicalCalls;
    if (stats != NULL) {
        stats->finalPresented = canPresent ? 1U : 0U;
        stats->frameAfterFNV = canPresent ? 0xabcde123UL : 0U;
    }
    if (injectFault) dynamicAnimationFault = 1U;
    return canPresent;
}
static const EspMapLineStateView* EspMapLineState_viewBase(void) {
    return &lineView;
}
static int EspNativeDoorAnimator_hasPendingFrames(void) {
    return pending != 0;
}
static const EspNativeDoorAnimatorView* EspNativeDoorAnimator_view(void) {
    return &doorView;
}
static int EspNativeDoorAnimator_validateLineState(void) {
    if (!validateOk) { pending = 0; holding = 0; return 0; }
    return 1;
}
static int EspNativeDoorAnimator_isHoldingDeferredClose(void) {
    return holding;
}
static uint8_t EspNativeDoorAnimator_deferredCloseCount(void) {
    return holding ? 1U : 0U;
}
static int EspNativeDoorAnimator_prepareFrame(EspNativeDoorAnimationFrame* out) {
    ++prepareCalls;
    if (prepFail) return 0;
    assert(pending > 0 && !holding);
    out->ordinal = (uint8_t)(finished + 1);
    out->totalFrames = ESP_NATIVE_DOOR_ANIMATION_FRAMES;
    out->activeLines = 1U;
    out->geometryActive = 1U;
    return 1;
}
static int EspNativeDoorAnimator_finishFrame(int ok) {
    ++finishCalls;
    if (finishFail) return 0;
    if (ok) {
        assert(pending > 0);
        --pending;
        ++finished;
        ++doorView.completedTransitions;
    }
    return 1;
}
static void EspNativeDoorAnimator_reset(void) {
    ++resetCalls;
    pending = 0;
    holding = 0;
}
static void EspNativeGameplayMoveEvents_onFrameResult(int ok) {
    ++eventCalls;
    lastEvent = ok;
}
static uint8_t EspNativeDoorSfx_onFirstPresentedOpeningFrame(void) {
    assert(finished == 1 && canPresent);
    ++openSfxCalls;
    return (uint8_t)openSfxResult;
}
static uint8_t EspNativeDoorSfx_onFirstPresentedHeldClosingFrame(void) {
    assert(finished == 1 && canPresent);
    ++closeSfxCalls;
    return (uint8_t)closeSfxResult;
}
static void EspEntranceMusic_pumpGameplay(void) {
    assert(openSfxCalls == 1 && closeSfxCalls == 1);
    ++musicPumps;
}
"""

SUFFIX = r"""
static void clearWitness(void) {
    dynamicFrameActive = 0;
    dynamicAnimationFault = 0;
    dynamicOpenLineReads = dynamicAnimatedLineReads = dynamicTextureVariantReads = 0;
    memset(&animatedClip, 0, sizeof(animatedClip));
    memset(&doorView, 0, sizeof(doorView));
    canPresent = validateOk = 1;
    injectFault = 0;
    rawCalls = physicalCalls = pending = holding = 0;
    prepareCalls = finishCalls = resetCalls = prepFail = finishFail = finished = 0;
    eventCalls = openSfxCalls = closeSfxCalls = musicPumps = 0;
    openSfxResult = closeSfxResult = 0;
    lastEvent = -1;
}
int main(void) {
    EspNativeGameplayFrameStats stats;
    clearWitness();
    memset(&stats, 0, sizeof(stats));
    assert(EspNativeGameplayFrame_renderTurn(&liveRender, 64, &stats) == 1);
    assert(rawCalls == 1 && physicalCalls == 1 && eventCalls == 1 && lastEvent == 1);
    assert(stats.finalPresented == 1 && stats.frameAfterFNV == 0xabcde123UL);
    assert(dynamicFrameActive == 0 && animatedClip.valid == 0);
    assert(openSfxCalls == 0 && closeSfxCalls == 0);

    clearWitness();
    dynamicFrameActive = 1;
    assert(EspNativeGameplayFrame_renderTurn(&liveRender, 64, &stats) == 0);
    assert(rawCalls == 0 && eventCalls == 0 && dynamicFrameActive == 1);
    dynamicFrameActive = 0;

    clearWitness();
    canPresent = 0;
    assert(EspNativeGameplayFrame_renderTurn(&liveRender, 64, &stats) == 0);
    assert(eventCalls == 1 && lastEvent == 0 && physicalCalls == 1);
    assert(dynamicFrameActive == 0 && openSfxCalls == 0);

    clearWitness();
    injectFault = 1;
    assert(EspNativeGameplayFrame_renderTurn(&liveRender, 64, &stats) == 0);
    assert(rawCalls == 1 && eventCalls == 1 && lastEvent == 0);

    clearWitness();
    pending = 3;
    openSfxResult = 1;
    assert(EspNativeGameplayFrame_renderTurn(&liveRender, 64, &stats) == 1);
    assert(pending == 0 && prepareCalls == 3 && finishCalls == 3 && finished == 3);
    assert(rawCalls == 3 && eventCalls == 1 && lastEvent == 1 && resetCalls == 0);
    assert(openSfxCalls == 1 && closeSfxCalls == 1 && musicPumps == 1);
    assert(dynamicFrameActive == 0 && animatedClip.valid == 0);

    clearWitness();
    pending = 3;
    holding = 1;
    closeSfxResult = 1;
    assert(EspNativeGameplayFrame_renderTurn(&liveRender, 64, &stats) == 1);
    assert(pending == 3 && rawCalls == 1 && prepareCalls == 0 && finishCalls == 0);
    assert(eventCalls == 1 && lastEvent == 1);
    assert(openSfxCalls == 0 && closeSfxCalls == 0 && musicPumps == 0);
    /* After the monster turn releases the deferred close, publish all frames. */
    holding = 0;
    assert(EspNativeGameplayFrame_renderTurn(&liveRender, 64, &stats) == 1);
    assert(pending == 0 && prepareCalls == 3 && finishCalls == 3 && rawCalls == 4);
    assert(eventCalls == 2 && lastEvent == 1);
    assert(openSfxCalls == 1 && closeSfxCalls == 1 && musicPumps == 1);

    clearWitness();
    pending = 3;
    prepFail = 1;
    assert(EspNativeGameplayFrame_renderTurn(&liveRender, 64, &stats) == 0);
    assert(resetCalls == 1 && pending == 0 && prepareCalls == 1);
    assert(rawCalls == 0 && eventCalls == 1 && lastEvent == 0 && openSfxCalls == 0);

    clearWitness();
    pending = 3;
    finishFail = 1;
    assert(EspNativeGameplayFrame_renderTurn(&liveRender, 64, &stats) == 0);
    assert(resetCalls == 1 && pending == 0 && finishCalls == 1);
    assert(rawCalls == 1 && eventCalls == 1 && lastEvent == 0 && openSfxCalls == 0);

    clearWitness();
    pending = 3;
    canPresent = 0;
    assert(EspNativeGameplayFrame_renderTurn(&liveRender, 64, &stats) == 0);
    assert(resetCalls == 1 && pending == 0 && rawCalls == 1);
    assert(eventCalls == 1 && lastEvent == 0 && openSfxCalls == 0);

    clearWitness();
    pending = 3;
    injectFault = 1;
    assert(EspNativeGameplayFrame_renderTurn(&liveRender, 64, &stats) == 0);
    assert(resetCalls == 1 && pending == 0 && rawCalls == 1);
    assert(eventCalls == 1 && lastEvent == 0 && openSfxCalls == 0);

    clearWitness();
    pending = 3;
    validateOk = 0;
    assert(EspNativeGameplayFrame_renderTurn(&liveRender, 64, &stats) == 1);
    assert(pending == 0 && rawCalls == 1 && prepareCalls == 0);
    assert(eventCalls == 1 && lastEvent == 1 && openSfxCalls == 0);

    puts("PASS RC23 real C: Base->scoped frame->public facade; "
         "nested, held-close, release, success, rollback and post-present SFX");
    return 0;
}
"""

MUTANTS = [
    ("nested admission bypass", "public",
     "if (dynamicFrameActive) return 0;", "if (0) return 0;"),
    ("discard pending animation", "public",
     "if (EspNativeDoorAnimator_hasPendingFrames()) {",
     "if (0) {"),
    ("skip lease validation", "public",
     "if (!EspNativeDoorAnimator_validateLineState()) {",
     "if (0) {"),
    ("consume held frame", "public",
     "EspNativeDoorAnimator_isHoldingDeferredClose()) {",
     "0) {"),
    ("held result falsely failed", "public",
     "EspNativeGameplayMoveEvents_onFrameResult(ok);\n            printf",
     "EspNativeGameplayMoveEvents_onFrameResult(0);\n            printf"),
    ("skip preparation failure", "public",
     "if (!EspNativeDoorAnimator_prepareFrame(&animationFrame)) {",
     "if (0) {"),
    ("skip finish failure", "public",
     "if (!EspNativeDoorAnimator_finishFrame(ok)) {",
     "if (0) {"),
    ("ignore physical render failure", "public",
     "if (!ok) {\n                EspNativeGameplayMoveEvents_onFrameResult(0);",
     "if (0) {\n                EspNativeGameplayMoveEvents_onFrameResult(0);"),
    ("drop first-present SFX", "public",
     "if (animationFrame.ordinal == 1U) {", "if (0) {"),
    ("drop opening SFX", "public",
     "EspNativeDoorSfx_onFirstPresentedOpeningFrame();", "0U;"),
    ("drop closing SFX", "public",
     "EspNativeDoorSfx_onFirstPresentedHeldClosingFrame();", "0U;"),
    ("skip post-SFX music pump", "public",
     "EspEntranceMusic_pumpGameplay();", "((void)0);"),
    ("commit result wrong", "public",
     "EspNativeGameplayMoveEvents_onFrameResult(1);",
     "EspNativeGameplayMoveEvents_onFrameResult(0);"),
    ("stable result wrong", "public",
     "EspNativeGameplayMoveEvents_onFrameResult(ok);\n    return ok;",
     "EspNativeGameplayMoveEvents_onFrameResult(0);\n    return ok;"),
    ("scope never armed", "scope",
     "dynamicFrameActive = 1U;", "dynamicFrameActive = 0U;"),
    ("scope leaks after frame", "scope",
     "dynamicFrameActive = 0U;", "dynamicFrameActive = 1U;"),
    ("ignore dynamic geometry fault", "scope",
     "if (dynamicAnimationFault != 0U) ok = 0;", "if (0) ok = 0;"),
    ("bypass raw base", "scope",
     "EspNativeGameplayFrame_renderTurnBase(render, angle, outStats);",
     "renderComposed(render, EspPlayerView_view(), angle, 0, 1, outStats);"),
    ("raw leaf becomes preview", "base",
     "angle, 0, 1, outStats);", "angle, 1, 1, outStats);"),
    ("raw leaf becomes no-present", "base",
     "angle, 0, 1, outStats);", "angle, 0, 0, outStats);"),
]


def compile_run(bodies):
    source = (PREFIX + "\n" + bodies["base"] + "\n" + bodies["scope"]
              + "\n" + bodies["public"] + "\n" + SUFFIX)
    with tempfile.TemporaryDirectory(prefix="rc23-real-c-") as directory:
        p = Path(directory)
        code, exe = p / "test.c", p / "test"
        code.write_text(source)
        built = subprocess.run(
            ["cc", "-std=c11", "-O0", "-Wall", "-Wextra", "-Werror",
             "-DARDUINO", "-DDOOMRPG_ESP32", str(code), "-o", str(exe)],
            text=True, capture_output=True)
        require(built.returncode == 0,
                "actual C bodies fail compile: " + built.stderr[:5000])
        run = subprocess.run([str(exe)], capture_output=True, text=True,
                             timeout=15)
        return run.returncode == 0 and "PASS RC23 real C:" in run.stdout


def main():
    scan()
    require(compile_run(BODIES), "unmodified REAL C contract failed")
    for description, owner, before, after in MUTANTS:
        require(BODIES[owner].count(before) == 1,
                "non-unique mutation anchor " + description)
        variant = dict(BODIES)
        variant[owner] = variant[owner].replace(before, after)
        require(not compile_run(variant), "adversarial mutant survived: " + description)
    print(f"PASS ARCH-RC23: 3 real C bodies, {len(MUTANTS)} adversarial "
          "mutants rejected, public raw topology and post-present SFX")


if __name__ == "__main__":
    main()
