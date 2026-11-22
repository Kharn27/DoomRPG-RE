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
