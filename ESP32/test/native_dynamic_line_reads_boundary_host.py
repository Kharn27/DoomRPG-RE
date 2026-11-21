#!/usr/bin/env python3
"""ARCH-RC19: compile the four REAL C getters, prove explicit raw/render boundary.

This is a host oracle, not an ESP32 hardware test. The dynamic getter bodies,
the immutable packed-line decoder and the mutable raw view are taken directly
from their production owners, with small bounded stubs for downstream services.
Each adversarial mutant must fail the exact same assertions.
"""
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "src"
SOURCES = {
    "access": (SRC / "esp_map_access.c").read_text(),
    "state": (SRC / "esp_map_line_state.c").read_text(),
    "dynamic": (SRC / "esp_native_dynamic_line_render.c").read_text(),
}
CONFIG = (ROOT / "platformio.ini").read_text()
RETIRED = ("EspMapRuntime_getLine", "EspMapLineState_view")


def require(ok, message):
    if not ok:
        raise AssertionError(message)


def extract(source, signature):
    require(source.count(signature) == 1, "ambiguous real source body " + signature)
    start = source.index(signature)
    opening = source.index("{", start)
    depth = 0
    for pos in range(opening, len(source)):
        if source[pos] == "{":
            depth += 1
        elif source[pos] == "}":
            depth -= 1
            if depth == 0:
                return source[start:pos + 1]
    raise AssertionError("unclosed C body: " + signature)


SIGNATURES = {
    "le16": ("access", "static uint16_t readLe16("),
    "le32": ("access", "static uint32_t readLe32("),
    "raw_line": ("access", "int EspMapRuntime_getLineBase("),
    "raw_view": ("state", "const EspMapLineStateView* EspMapLineState_viewBase("),
    "line": ("dynamic", "int EspMapRuntime_getLine("),
    "view": ("dynamic", "const EspMapLineStateView* EspMapLineState_view("),
}


def scan():
    source_files = {p.name: re.sub(r"/\*[\s\S]*?\*/|//[^\n]*", "", p.read_text())
                    for p in SRC.iterdir() if p.suffix in (".c", ".cpp")}
    require(len(source_files) >= 178, "incomplete production source inventory")
    normal = CONFIG.split("[env:esp32-cyd]", 1)[1].split(
        "[env:esp-cyd-inverted]", 1)[0]
    require(len(re.findall(r"^\s+-Wl,--wrap=", normal, re.M)) == 1,
            "expected exactly one production GNU wrapper after RC23")
    for name in RETIRED:
        require("-Wl,--wrap=" + name not in CONFIG, "retired flag " + name)
        for path, source in source_files.items():
            require(not re.search(r"\b__(?:wrap|real)_" + name + r"\b", source),
                    "obsolete linker alias in " + path)
    require("-Wl,--wrap=EspNativeGameplayFrame_renderTurn" not in CONFIG,
            "RC23 cardinal transaction facade must be explicit")
    for name, owner in (("EspMapRuntime_getLine", "esp_native_dynamic_line_render.c"),
                        ("EspMapRuntime_getLineBase", "esp_map_access.c"),
                        ("EspMapLineState_view", "esp_native_dynamic_line_render.c"),
                        ("EspMapLineState_viewBase", "esp_map_line_state.c")):
        bodies = {}
        for path, source in source_files.items():
            count = len(re.findall(r"^(?:int|const EspMapLineStateView\*)\s+" +
                                   name + r"\s*\([^;]*?\)\s*\{",
                                   source, re.S | re.M))
            if count:
                bodies[path] = count
        require(bodies == {owner: 1}, "duplicate/bypass owner " + name + " " + str(bodies))
    dynamic = SOURCES["dynamic"]
    require(dynamic.count("EspMapLineState_viewBase()") == 2,
            "render scope must fetch canonical openCount without recursion")
    require(dynamic.count("EspMapRuntime_getLineBase(index, outLine)") == 1,
            "public line must execute raw packed decoder exactly once")
    require("if (!EspMapLineState_getOpen(index, &open)) return 0;" in dynamic,
            "fail-closed mutable overlay lost")
    require("if (dynamicFrameActive) return 0;" in dynamic,
            "renderTurn nested fail-closed protection lost")
    for path, symbol in (("include/esp_map_runtime.h", "EspMapRuntime_getLineBase"),
                         ("include/esp_map_line_state.h", "EspMapLineState_viewBase")):
        require(symbol + "(" in (ROOT / path).read_text(),
                "raw native leaf API not exposed: " + symbol)
    print("PASS RC19 scan: 178+ real C/C++ units, 4+3 GNU wraps, "
          "two exclusive public adapters and two explicit Base leaves")


PREFIX = r"""
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define ESP_MAP_LINE_RECORD_BYTES 10U
#define RENDER_LINE_FLAG_OCCLUDER_ONLY 0x20000000UL
#define RENDER_LINE_FLAG_SPRITE_SPAN 0x00000002UL
typedef struct {
    uint16_t x1, y1, x2, y2, texture;
    uint32_t flags;
} EspMapLine;
typedef struct {
    const uint8_t* lines;
    uint32_t lineCount, lineBytes;
} EspMapRuntimeView;
typedef struct {
    uint32_t openCount, lockedCount;
} EspMapLineStateView;
typedef struct { uint8_t valid; } AnimatedClipContext;
static uint8_t data[] = {
    3,4,5,6, 0x34,0x12, 0x00,0x00,0x00,0x20,
    9,8,7,6, 0x78,0x56, 0x02,0x00,0x00,0x00
};
static EspMapRuntimeView runtime = {data,1,10};
static EspMapLineStateView lineStateView = {4,1}, renderLineStateView;
static AnimatedClipContext animatedClip;
static uint8_t dynamicFrameActive, dynamicAnimationFault;
static uint32_t dynamicOpenLineReads, dynamicAnimatedLineReads;
static uint32_t dynamicTextureVariantReads;
static int ready=1, lineReady=1, open=0, failOpen=0, textures=0;
static int failTexture=0, moveLine=0, failAnimate=0;
static int openCalls=0, textureCalls=0;
static int animateCalls=0, displacementCalls=0;
const EspMapRuntimeView* EspMapRuntime_view(void) {
    return ready ? &runtime : NULL;
}
int EspMapLineState_isReady(void) { return lineReady; }
int EspMapLineState_getOpen(uint32_t index, uint8_t* out) {
    ++openCalls;
    if (failOpen || index != 0 || out == NULL) return 0;
    *out=(uint8_t)open;
    return 1;
}
int EspMapLineTextureState_isReady(void) { return textures; }
static int resolveDynamicTexture(uint32_t index, uint16_t before, uint16_t* out) {
    ++textureCalls;
    if (failTexture || index != 0 || out == NULL) return 0;
    *out=(uint16_t)(before + 1U);
    return 1;
}
int EspNativeDoorAnimator_getLineDisplacement(uint32_t index, int16_t* value) {
    ++displacementCalls;
    if (!moveLine || index != 0 || value==NULL) return 0;
    *value=16;
    return 1;
}
static int applyAnimatedLine(uint32_t index, EspMapLine* line, int16_t shift) {
    ++animateCalls;
    if (failAnimate || index != 0 || line==NULL || shift != 16) return 0;
    line->x1=(uint16_t)(line->x1 + shift);
    return 1;
}
int EspMapRuntime_getLineBase(uint32_t index, EspMapLine* line);
const EspMapLineStateView* EspMapLineState_viewBase(void);
int EspMapRuntime_getLine(uint32_t index, EspMapLine* line);
const EspMapLineStateView* EspMapLineState_view(void);
"""

CHECK = r"""
int main(void) {
    EspMapLine line={0};
    EspMapLine raw={0};
    uint8_t copy[sizeof(data)];
    memcpy(copy,data,sizeof(data));
    assert(EspMapRuntime_getLineBase(0,&raw)==1);
    assert(raw.x1==24 && raw.y1==32 && raw.x2==40 && raw.y2==48);
    assert(raw.texture==0x1234 && raw.flags==0x20000000UL);
    assert(EspMapRuntime_getLineBase(1,&raw)==0);
    assert(EspMapRuntime_getLineBase(0,NULL)==0);
    runtime.lineBytes=9;
    assert(EspMapRuntime_getLineBase(0,&raw)==0);
    runtime.lineBytes=10;
    ready=0;
    assert(EspMapRuntime_getLineBase(0,&raw)==0);
    ready=1;

    assert(EspMapRuntime_getLine(0,&line)==1);
    assert(line.flags==raw.flags && line.texture==raw.texture);
    assert(openCalls==0 && textureCalls==0 && displacementCalls==0);
    assert(EspMapRuntime_getLine(1,&line)==0);
    dynamicFrameActive=1;
    assert(EspMapRuntime_getLine(1,&line)==0);
    animatedClip.valid=9;
    assert(EspMapRuntime_getLine(0,&line)==1);
    assert(animatedClip.valid==0 && line.flags==raw.flags);
    assert(openCalls==1 && dynamicOpenLineReads==0);

    open=1;
    assert(EspMapRuntime_getLine(0,&line)==1);
    assert(line.flags==2U && dynamicOpenLineReads==1);
    textures=1;
    assert(EspMapRuntime_getLine(0,&line)==1);
    assert(line.texture==0x1235 && dynamicTextureVariantReads==1);
    assert(line.flags==2U && dynamicOpenLineReads==2);
    textures=0;
    moveLine=1;
    assert(EspMapRuntime_getLine(0,&line)==1);
    assert(line.x1==40 && dynamicAnimatedLineReads==1);
    assert(dynamicOpenLineReads==2 && animateCalls==1);
    failAnimate=1;
    assert(EspMapRuntime_getLine(0,&line)==0);
    assert(dynamicAnimationFault==1);
    failAnimate=0; dynamicAnimationFault=0; moveLine=0;
    textures=1; failTexture=1;
    assert(EspMapRuntime_getLine(0,&line)==0);
    assert(dynamicAnimationFault==1);
    failTexture=0; textures=0; dynamicAnimationFault=0;
    failOpen=1;
    assert(EspMapRuntime_getLine(0,&line)==0);
    failOpen=0;
    assert(memcmp(copy,data,sizeof(data))==0); /* Immutable BSP arena */

    dynamicFrameActive=0;
    const EspMapLineStateView* canonical=EspMapLineState_viewBase();
    assert(canonical==&lineStateView && canonical->openCount==4U);
    assert(EspMapLineState_view()==canonical);
    dynamicFrameActive=1;
    const EspMapLineStateView* adapted=EspMapLineState_view();
    assert(adapted!=canonical && adapted==&renderLineStateView);
    assert(adapted->openCount==0 && adapted->lockedCount==canonical->lockedCount);
    assert(canonical->openCount==4U);
    assert(EspMapLineState_viewBase()==canonical);
    lineReady=0;
    assert(EspMapLineState_viewBase()==NULL);
    assert(EspMapLineState_view()==NULL);
    lineReady=1; dynamicFrameActive=0;
    assert(EspMapLineState_view()==canonical);
    assert(canonical->openCount==4 && memcmp(copy,data,sizeof(data))==0);
    puts("PASS RC19 real C immutable line/raw view/scoped facade");
    return 0;
}
"""

MUTANTS = (
    ("wrong-packed-coordinate", "raw_line", "raw[0] << 3", "raw[0] << 2"),
    ("raw-bounds-overrun", "raw_line", "index >= view->lineCount",
     "index > view->lineCount"),
    ("skip-raw-failure", "line",
     "if (!EspMapRuntime_getLineBase(index, outLine)) return 0;",
     "if (0) return 0;"),
    ("disable-render-scope", "line", "if (!dynamicFrameActive) return 1;",
     "if (dynamicFrameActive) return 1;"),
    ("lose-overlay-fail-closed", "line",
     "if (!EspMapLineState_getOpen(index, &open)) return 0;",
     "if (!EspMapLineState_getOpen(index, &open)) { open = 0U; return 1; }"),
    ("wrong-open-flag", "line",
     "outLine->flags |= RENDER_LINE_FLAG_SPRITE_SPAN;",
     "outLine->flags &= RENDER_LINE_FLAG_SPRITE_SPAN;"),
    ("skip-open-geometry", "line", "if (open == 0U) return 1;",
     "if (open != 0U) return 1;"),
    ("ignore-animated-read", "line", "++dynamicAnimatedLineReads;",
     "++dynamicOpenLineReads;"),
    ("ignore-texture-overlay", "line",
     "if (outLine->texture != effectiveTexture)",
     "if (outLine->texture == effectiveTexture)"),
    ("view-raw-not-ready", "raw_view",
     "return EspMapLineState_isReady() ? &lineStateView : NULL;",
     "return &lineStateView;"),
    ("view-no-temporary-copy", "view",
     "if (!dynamicFrameActive || real == NULL) return real;",
     "if (1) return real;"),
    ("view-count-unmasked", "view", "renderLineStateView.openCount = 0U;",
     "renderLineStateView.openCount = real->openCount;"),
)


def exercise(chunks):
    c = PREFIX + "\n".join(chunks[k] for k in SIGNATURES) + CHECK
    with tempfile.TemporaryDirectory(prefix="doomrpg-rc19-") as td:
        source=Path(td)/"probe.c"
        binary=Path(td)/"probe"
        source.write_text(c)
        compile_run=subprocess.run(
            ["cc","-std=c11","-O0","-Wall","-Wextra","-Werror",
             str(source),"-o",str(binary)],
            capture_output=True,text=True)
        require(compile_run.returncode==0,
                "real C body compile failed: " + compile_run.stderr[:4000])
        run=subprocess.run([str(binary)],capture_output=True,text=True)
        return run.returncode==0 and "PASS RC19 real C" in run.stdout


def main():
    scan()
    real={k:extract(SOURCES[src], sig) for k,(src,sig) in SIGNATURES.items()}
    require(exercise(real), "real C baseline failed")
    for label,key,before,after in MUTANTS:
        changed=dict(real)
        require(changed[key].count(before)==1,"missing mutant anchor: "+label)
        changed[key]=changed[key].replace(before,after,1)
        require(not exercise(changed),"mutant survived: "+label)
    print("PASS ARCH-RC19: 6 real C bodies, 12 mutants rejected, "
          "immutable world and bounded renderer scope preserved")


if __name__=="__main__":
    main()
