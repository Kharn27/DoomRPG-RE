#!/usr/bin/env python3
"""Host witness: compile the REAL LOADING title painter against bounded UI stubs.

The extractor deliberately imports the implementation from the production C
file; it cannot pass by duplicating the renderer in this test. No ESP32 SDK.
"""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
production = (root / "src/esp_native_transition_presentation.c").read_text()
prefix = "static void drawLoadingTitle(void) {"
start = production.index(prefix)
open_brace = production.index("{", start)
depth = 0
end = None
for index in range(open_brace, len(production)):
    if production[index] == "{":
        depth += 1
    elif production[index] == "}":
        depth -= 1
        if depth == 0:
            end = index + 1
            break
assert end is not None
painter = production[start:end]
definitions = "\n".join(
    line for line in production.splitlines()
    if line.startswith("#define LOADING_TITLE_")
)

# Ownership, fallback, and preparation order are part of the route contract.
begin = production.split("int EspNativeTransitionPresentation_beginLoading", 1)[1]
begin = begin.split("void EspNativeTransitionPresentation_progress", 1)[0]
assert begin.index("prepareLoadingFontSd();") < begin.index(
    "EspAssetPack_open(ESP_ASSET_PACK_DEFAULT_PATH)"
)
assert begin.index("drawFixedStarfield") < begin.index(
    "prepareLoadingFontPak();"
) < begin.index("paintLoadingCard(target)")
assert "font=%s theme=%u" in begin
assert 'EspNativeIndexedBmp_open("a.bmp"' in production
assert 'EspNativeIndexedBmp_openSdFontStandalone(' in production
assert "EspNativeFontSd_suspend();" in production
assert "EspNativeIndexedBmp_blit(" in production
assert "sizeof(presentation.titleFont.ink)" in production
assert "EspUacFontTheme_verticalTone" in painter
assert "EspNativeFontTint_apply" in painter
assert "EspUacFontTheme_tintColor" in painter
assert "drawMiniTextAt" in painter
assert "EspAssetPack_" not in painter
assert "EspNativeIndexedBmp_" not in painter
assert "malloc(" not in production[start:production.index(
    "static void drawMetricCard", start
)]

harness = r"""
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "esp_uac_font_theme.h"
#include "esp_native_font_tint.h"
#define DOOMRPG_LOGICAL_WIDTH 160
#define DOOMRPG_LOGICAL_HEIGHT 120
""" + definitions + r"""
typedef struct EspNativeLoadingTitleFont_s {
    uint8_t ink[LOADING_TITLE_INK_BYTES];
    uint16_t palette[16];
    uint8_t ready, neutral, fromSd;
} EspNativeLoadingTitleFont;
static struct {
    EspNativeLoadingTitleFont titleFont;
} presentation;
static uint16_t fb[DOOMRPG_LOGICAL_WIDTH * DOOMRPG_LOGICAL_HEIGHT];
static unsigned writes, fallbackCalls, activeTheme;
static uint16_t EspMainMenuSkin_letterTone(int row, int rows) {
    const uint16_t tone[3][3] = {
        {0x1812U,0x7a60U,0xffe0U}, /* classic: blue to amber */
        {0x8000U,0xc942U,0xffdeU}, /* inferno */
        {0x0185U,0x35e9U,0xafffU}  /* phosphore */
    };
    assert(rows==9 && (row==0 || row==4 || row==8));
    return tone[activeTheme][row/4];
}
static int miniTextWidth(const char* text, int scale) {
    return (int)strlen(text)*4*scale - scale;
}
static void drawMiniTextAt(const char* text, int x, int y, int scale,
                           uint16_t color, int gradient) {
    (void)text;(void)x;(void)y;(void)scale;(void)color;(void)gradient;
    ++fallbackCalls;
}
static void pixel(int x, int y, uint16_t color) {
    assert(x>=0 && x<160 && y>=0 && y<120);
    ++writes;
    fb[y*160+x]=color;
}
""" + painter + r"""
static void loadFullWhiteFont(void) {
    memset(&presentation.titleFont,0,sizeof(presentation.titleFont));
    memset(presentation.titleFont.ink,0x11,sizeof(presentation.titleFont.ink));
    presentation.titleFont.palette[1]=0xffffU;
    presentation.titleFont.palette[2]=0x0000U; /* black contour */
    presentation.titleFont.ink[0]=0x01U; /* first pixel transparent, next ink */
    presentation.titleFont.ink[5]=0x22U; /* black strokes on second source row */
    presentation.titleFont.ready=1U;
    presentation.titleFont.neutral=1U;
    presentation.titleFont.fromSd=1U;
}
int main(void) {
    uint16_t classicTop, classicBottom, infernoTop, phosphoreTop;
    const int left=(160-7*12)/2;
    assert(sizeof(presentation.titleFont.ink)==420U);
    assert(left==38);
    memset(fb,0x55,sizeof(fb));
    drawLoadingTitle();
    assert(fallbackCalls==1U && writes==0U);
    loadFullWhiteFont();
    drawLoadingTitle();
    assert(writes!=0U && fallbackCalls==1U);
    assert(fb[32*160+left]==0x5555U); /* index 0 never overpaints */
    assert(fb[32*160+left+2]!=0x5555U);
    assert(fb[34*160+left]==0x0000U); /* true black retained */
    assert(fb[32*160+left-1]==0x5555U);
    assert(fb[32*160+left+84]==0x5555U);
    classicTop=fb[32*160+left+2];
    classicBottom=fb[45*160+left+2];
    assert(classicTop!=classicBottom); /* row-wise 3-stop gradient */
    activeTheme=1U;
    drawLoadingTitle();
    infernoTop=fb[32*160+left+2];
    assert(infernoTop!=classicTop);
    activeTheme=2U;
    drawLoadingTitle();
    phosphoreTop=fb[32*160+left+2];
    assert(phosphoreTop!=classicTop && phosphoreTop!=infernoTop);
    /* Colored source path matches MENU_MAIN's tint policy too. */
    presentation.titleFont.neutral=0U;
    presentation.titleFont.palette[1]=0x7befU;
    drawLoadingTitle();
    assert(fb[32*160+left+2]!=0x5555U);
    assert(fb[34*160+left]==0x0000U);
    printf("loading a.bmp title: source painter, palette, colorkey, bounds, three themes, mini fallback PASS\n");
    return 0;
}
"""
with tempfile.TemporaryDirectory(prefix="doomrpg-loadfont-") as tmp:
    source = Path(tmp) / "main.c"
    binary = Path(tmp) / "loading-font-host"
    source.write_text(harness)
    subprocess.run([
        "cc", "-O2", "-std=c11", "-Wall", "-Wextra", "-Werror",
        "-I", str(root / "include"), str(source), "-o", str(binary)
    ], check=True)
    subprocess.run([str(binary)], check=True)


# LOADING-LCD-REGION: compile the REAL TFT partial presenter.
# The ILI9341 retains the original c.bmp full-screen scanout in controller
# GRAM. Only the opaque progress plate is recomposed on the shared FB, which
# may have been overwritten by the BSP. No second 9600 B heap allocation.
def real_body(src, signature):
    begin = src.index(signature)
    brace = src.index("{", begin)
    depth = 0
    for pos in range(brace, len(src)):
        if src[pos] == "{":
            depth += 1
        elif src[pos] == "}":
            depth -= 1
            if depth == 0:
                return src[begin:pos + 1]
    raise AssertionError("unbalanced real source: " + signature)

begin_loader = real_body(production, "int EspNativeTransitionPresentation_beginLoading(")
progress_painter = real_body(production, "static void presentOverall(")
assert begin_loader.index("drawFixedStarfield(") < begin_loader.index(
    "paintLoadingCard(target)") < begin_loader.index("paintProgressBar(0U)")
assert "Esp32PlatformVideo_presentBase()" in begin_loader
assert "paintProgressBar(percent)" in progress_painter
assert "Esp32PlatformVideo_presentRegionBase(" in progress_painter
assert "paintLoadingCard(" not in progress_painter
assert "drawFixedStarfield(" not in progress_painter
assert "Esp32PlatformVideo_presentBase()" not in progress_painter
for forbidden in ("EspAssetPack_open(", "EspNativeIndexedBmp_open(",
                  "EspNativeIndexedBmp_blit(", "EspNativeFontSd_",
                  "malloc(", "free(", "restoreBackground("):
    assert forbidden not in progress_painter
for forbidden in ("captureBackground", "restoreBackground", "backgroundPacked",
                  "TRANSITION_BACKGROUND_BYTES"):
    assert forbidden not in production
assert 'assetReadsDuringProgress=0' in production
assert 'background=lcd-retained' in progress_painter
for fn in ("abortLoading", "releaseLoading", "endLoading", "reset"):
    part = real_body(production, "void EspNativeTransitionPresentation_" + fn + "(")
    assert "clearPresentation();" in part

video = (root / "src/platform_video.cpp").read_text()
video_hdr = (root / "src/platform_video.h").read_text()
bridge = (root / "src/platform_video_c_bridge.cpp").read_text()
bridge_hdr = (root / "include/platform_video_c_bridge.h").read_text()
method = real_body(video, "bool PlatformVideo_presentRegion(")
assert "bool PlatformVideo_presentRegion(" in video_hdr
assert "Esp32PlatformVideo_presentRegionBase(" in bridge
assert "Esp32PlatformVideo_presentRegionBase(" in bridge_hdr
assert "setAddrWindow(" in method
assert "pushPixels(" in method
assert "malloc(" not in method and "new " not in method
assert "PlatformVideo_present()" not in method

tft_harness = r"""
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#define DOOMRPG_LOGICAL_WIDTH 160
#define DOOMRPG_LOGICAL_HEIGHT 120
#define DOOMRPG_PHYSICAL_WIDTH 320
#define DOOMRPG_INTEGER_SCALE 2
struct TFT_eSPI {
    uint16_t lcd[320U*240U];
    int x0=0, y0=0, width=0, height=0, cursor=0, starts=0, ends=0;
    void startWrite() { ++starts; }
    void setAddrWindow(int x,int y,int w,int h) {
        assert(x>=0 && y>=0 && w>0 && h>0 && x+w<=320 && y+h<=240);
        x0=x;y0=y;width=w;height=h;cursor=0;
    }
    void pushPixels(uint16_t* row, int count) {
        assert(count>0 && count<=width && cursor+count<=width*height);
        for (int i=0;i<count;++i) {
            int p=cursor+i;
            lcd[(y0+p/width)*320+x0+p%width]=row[i];
        }
        cursor+=count;
    }
    void endWrite() {
        ++ends;
        assert(cursor==width*height);
    }
};
static TFT_eSPI actualDisplay;
static TFT_eSPI* platformDisplay=&actualDisplay;
static uint16_t fb[160U*120U];
static uint16_t* framebuffer=fb;
""" + method + r"""
int main() {
    for (unsigned i=0;i<320U*240U;i++) actualDisplay.lcd[i]=0x1234U;
    for (unsigned i=0;i<160U*120U;i++) fb[i]=0xEEEEU;
    /* Simulate BSP overwriting every byte of shared framebuffer, then gauge
     * painting opaque only inside its 128x18 region. */
    for (int y=82;y<100;y++)
        for (int x=16;x<144;x++)
            fb[y*160+x]=(uint16_t)(0x1000U+x+(y-82)*13U);
    assert(PlatformVideo_presentRegion(16,82,128,18));
    assert(actualDisplay.starts==1 && actualDisplay.ends==1);
    assert(actualDisplay.width==256 && actualDisplay.height==36);
    for (int py=0;py<240;py++) for (int px=0;px<320;px++) {
        const int x=px/2,y=py/2;
        const uint16_t expected=(x>=16 && x<144 && y>=82 && y<100)
            ? fb[y*160+x] : 0x1234U;
        assert(actualDisplay.lcd[py*320+px]==expected);
    }
    assert(!PlatformVideo_presentRegion(-1,82,128,18));
    assert(!PlatformVideo_presentRegion(16,82,0,18));
    assert(!PlatformVideo_presentRegion(33,82,128,18));
    assert(!PlatformVideo_presentRegion(16,103,128,18));
    assert(actualDisplay.starts==1 && actualDisplay.ends==1);
    puts("LOADING-LCD-REGION PASS: real C++ 2x update, retained c.bmp, exact bounds, zero heap");
    return 0;
}
"""
with tempfile.TemporaryDirectory(prefix="doomrpg-lcd-region-") as tmp:
    source = Path(tmp) / "region.cpp"
    binary = Path(tmp) / "region-host"
    source.write_text(tft_harness)
    subprocess.run(["c++", "-O2", "-std=c++11", "-Wall", "-Wextra",
                    "-Werror", "-pedantic", str(source), "-o", str(binary)],
                   check=True)
    subprocess.run([str(binary)], check=True)
