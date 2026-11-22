#!/usr/bin/env python3
"""Build the real HELP starfield, chevrons and enlarged grid with C11.

Navigation hit detection is the same header used by production. The C test
checks all 160x120 hitpoints, no framebuffer shading after c.bmp blit, and
the actual native side-arrow/grid painters. No alternate rendering code.
"""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
src = (root / "src/native_main_menu_actions.c").read_text()

def actual_function(signature):
    assert src.count(signature) == 1, signature
    start = src.index(signature)
    brace = src.index("{", start)
    depth = 0
    for end in range(brace, len(src)):
        if src[end] == "{":
            depth += 1
        elif src[end] == "}":
            depth -= 1
            if depth == 0:
                return src[start:end+1]
    raise AssertionError("unterminated " + signature)

functions = "\n\n".join(actual_function(s) for s in (
    "static int helpDrawStarfield(",
    "static void helpPaintPageChevron(",
    "static void helpDrawWeaponGridMini(",
))
assert 'EspNativeHelpNav_hit(logicalX, logicalY)' in src
assert "menuSystem->scrollIndex = EspNativeHelpNav_scroll(" in src
assert "if (action == ESP_NATIVE_HELP_NAV_CLOSE)" in src
assert "before != helpExpectedFrameFNV" in src
assert 'EspNativeCanvas_drawText(canvas, "X", 4, 1, 0);' in src
assert "helpPaintExitFrame" not in src
assert "helpPaintPageChevron(framebuffer, 1, menuSystem->scrollIndex > 0);" in src
assert "helpPaintPageChevron(framebuffer, 0, menuSystem->scrollIndex < maxScroll);" in src
assert "DoomRPG_esp32MainMenuReturnToMain(" in src
assert "MAIN_HELP_FOOTER_TOP" not in src
assert 'for (y = 13; y <= 116; ++y)' not in src
assert '*pixel = (uint16_t)((*pixel >> 1) & 0x7befU);' not in src

harness = r"""
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "esp_native_help_nav.h"
#define DOOMRPG_LOGICAL_WIDTH 160
#define DOOMRPG_LOGICAL_HEIGHT 120
#define MAIN_HELP_WEAPON_MINI_X 8
#define MAIN_HELP_WEAPON_MINI_Y 39
#define MAIN_HELP_WEAPON_MINI_W 144
#define MAIN_HELP_WEAPON_MINI_H 60
#define MAIN_HELP_WEAPON_DRAW_H 76
#define ESP_NATIVE_GAMEPLAY_HUB_WEAPON_GRID_SLOT_COUNT 12U
static uint16_t helpWeaponMini[144*60];
static uint8_t helpWeaponMiniReady = 1U;
static uint16_t frame[160*120];
static uint16_t expected[160*120];
static uint16_t copy[144*60];
static int theme;
static uint32_t fakeTime = 100U;
static uint16_t marker;
typedef struct { int unused; } DoomRPG_t;
typedef struct { int x, y, w, h; } EspNativeRect;
typedef struct { void* imgBitmap; } Image_t;
static DoomRPG_t fakeGame;
static DoomRPG_t* helpSpaceOwner = &fakeGame;
static Image_t helpSpaceBG = { &marker };
static uint32_t helpSpaceEpoch = 100U;
static int helpSpaceActive = 1, helpSpaceShift = -1;
static int blitCalls;
#define MAIN_HELP_STAR_PERIOD_MS 157U
#define MAIN_HELP_STAR_VIRTUAL_WIDTH 192U
static uint32_t DoomRPG_GetUpTimeMS(void) { return fakeTime; }
static void* Esp32PlatformVideo_framebuffer(void) { return frame; }
static size_t Esp32PlatformVideo_framebufferSizeBytes(void) { return sizeof(frame); }
static int EspNativePresentation_clipEnabled(void) { return 0; }
static void DoomRPG_setClipFalse(DoomRPG_t* game) { (void)game; }
/* Mock only the ASSET blit: write both target and exact expected RGB565
 * texture pixel. Production helpDrawStarfield must not darken or modify it. */
static int EspNativeTexture_blit(void* image, const void* source,
                                 const EspNativeRect* dst) {
    assert(image == &marker && source == NULL);
    assert(dst->w == 160 && dst->h == 120 && dst->y == 0);
    ++blitCalls;
    for (int y = 0; y < 120; ++y)
        for (int x = 0; x < 160; ++x) {
            if (x < dst->x || x >= dst->x + dst->w) continue;
            const uint16_t texel =
                (uint16_t)(0x4000U | (uint16_t)(((x-dst->x)*13+y*101) & 0x3fffU));
            frame[y*160+x] = texel;
            expected[y*160+x] = texel;
        }
    return 0;
}
static uint16_t EspMainMenuSkin_letterTone(int row, int rows) {
    assert(row==8 && rows==9);
    return theme ? 0x07ffU : 0xfbe0U;
}
""" + functions + r"""
static void touchContract(void) {
    int x,y,prev=0, next=0,close=0;
    for (y=0; y<120; ++y) {
        for (x=0; x<160; ++x) {
            EspNativeHelpNavAction a=EspNativeHelpNav_hit(x,y);
            assert(a!=ESP_NATIVE_HELP_NAV_NONE);
            if (x<=17 && y<=14) {assert(a==ESP_NATIVE_HELP_NAV_CLOSE); ++close;}
            else if(x<80) {assert(a==ESP_NATIVE_HELP_NAV_PREVIOUS); ++prev;}
            else {assert(a==ESP_NATIVE_HELP_NAV_NEXT); ++next;}
        }
    }
    assert(close==18*15);
    assert(prev==80*120-close && next==80*120);
    assert(EspNativeHelpNav_hit(-1,0)==ESP_NATIVE_HELP_NAV_NONE);
    assert(EspNativeHelpNav_hit(0,-1)==ESP_NATIVE_HELP_NAV_NONE);
    assert(EspNativeHelpNav_hit(160,110)==ESP_NATIVE_HELP_NAV_NONE);
    assert(EspNativeHelpNav_hit(59,120)==ESP_NATIVE_HELP_NAV_NONE);
    assert(EspNativeHelpNav_hit(17,14)==ESP_NATIVE_HELP_NAV_CLOSE);
    assert(EspNativeHelpNav_hit(18,14)==ESP_NATIVE_HELP_NAV_PREVIOUS);
    assert(EspNativeHelpNav_hit(17,15)==ESP_NATIVE_HELP_NAV_PREVIOUS);
    assert(EspNativeHelpNav_hit(79,119)==ESP_NATIVE_HELP_NAV_PREVIOUS);
    assert(EspNativeHelpNav_hit(80,119)==ESP_NATIVE_HELP_NAV_NEXT);
    int page=0;
    for(int i=0;i<15;++i) {
        page=EspNativeHelpNav_scroll(page,112,ESP_NATIVE_HELP_NAV_NEXT);
        assert(page==(i+1)*7);
    }
    assert(page==105);
    assert(EspNativeHelpNav_scroll(page,112,ESP_NATIVE_HELP_NAV_NEXT)==105);
    assert(EspNativeHelpNav_scroll(page,112,ESP_NATIVE_HELP_NAV_CLOSE)==105);
    for(int i=14;i>=0;--i) {
        page=EspNativeHelpNav_scroll(page,112,ESP_NATIVE_HELP_NAV_PREVIOUS);
        assert(page==i*7);
    }
    assert(EspNativeHelpNav_scroll(page,112,ESP_NATIVE_HELP_NAV_PREVIOUS)==0);
    assert(EspNativeHelpNav_scroll(-9,112,ESP_NATIVE_HELP_NAV_NEXT)==7);
    assert(EspNativeHelpNav_scroll(999,112,ESP_NATIVE_HELP_NAV_NEXT)==105);
    assert(EspNativeHelpNav_scroll(0,0,ESP_NATIVE_HELP_NAV_NEXT)==0);
    assert(EspNativeHelpNav_scroll(0,3,ESP_NATIVE_HELP_NAV_NEXT)==0);
}
static void backgroundContract(void) {
    /* Exercise page 1, mid-scroll, and near-maximum c.bmp wrap offset.
     * A transparent HELP means EVERY pixel matches the untouched blit. */
    const uint32_t elapsed[] = {0U, 157U*7U, 157U*191U};
    for (unsigned p=0; p<3U; ++p) {
        fakeTime = helpSpaceEpoch + elapsed[p];
        for (unsigned i=0;i<160U*120U;++i) {
            frame[i] = 0x2d13U;
            expected[i] = 0x2d13U;
        }
        blitCalls = 0;
        assert(helpDrawStarfield(&fakeGame));
        const int phase = (int)((elapsed[p] / 157U) % 192U);
        assert(helpSpaceShift == phase*160/192);
        assert(blitCalls == (helpSpaceShift > 0 ? 2 : 1));
        for (unsigned i=0;i<160U*120U;++i) {
            assert(frame[i] == expected[i]);
            assert(frame[i] != 0x2d13U);
        }
    }
    assert(!helpDrawStarfield(NULL));
    helpSpaceActive = 0;
    assert(!helpDrawStarfield(&fakeGame));
    helpSpaceActive = 1;
}
static void paintContract(void) {
    const uint16_t old = 0x2945U;
    for(unsigned i=0;i<160U*120U;++i)frame[i]=old;
    helpPaintPageChevron(frame,1,1);
    int painted=0;
    for(int y=0;y<120;++y)for(int x=0;x<160;++x) {
        if(frame[y*160+x]!=old) {
            assert(x>=2&&x<=7 && y>=57&&y<=67);
            ++painted;
        }
    }
    assert(painted>=12);
    const uint16_t enabled=frame[62*160+2];
    helpPaintPageChevron(frame,1,0);
    assert(frame[62*160+2]!=enabled);
    for(unsigned i=0;i<160U*120U;++i)frame[i]=old;
    theme=1;
    helpPaintPageChevron(frame,0,1);
    painted=0;
    for(int y=0;y<120;++y)for(int x=0;x<160;++x) {
        if(frame[y*160+x]!=old) {
            assert(x>=152&&x<=157 && y>=57&&y<=67);
            ++painted;
        }
    }
    assert(painted>=12);
    assert(frame[62*160+157]==0x07ffU);
}
static void gridContract(void) {
    for(int y=0;y<60;++y)
        for(int x=0;x<144;++x)
            helpWeaponMini[y*144+x]=(uint16_t)((y+1)*11+x);
    memcpy(copy,helpWeaponMini,sizeof(copy));
    for(unsigned i=0;i<160U*120U;++i)frame[i]=0x2a45U;
    helpDrawWeaponGridMini(frame,0U);
    assert(memcmp(copy,helpWeaponMini,sizeof(copy))==0);
    for(int y=0;y<120;++y)for(int x=0;x<160;++x) {
        if(x>=8&&x<152&&y>=39&&y<115) {
            const int srcY=(y-39)*60/76;
            const int row=srcY/20, col=(x-8)/36;
            const int top=(row*76+2)/3, bottom=((row+1)*76+2)/3-1;
            const int cx=(x-8)%36, dy=y-39;
            const int edge=cx<=1||cx>=34||dy-top<=1||bottom-dy<=1;
            const uint16_t expected=edge
                ? (uint16_t)(row==0&&col==0?0x07e0U:0x4208U)
                : copy[srcY*144+x-8];
            assert(frame[y*160+x]==expected);
        } else assert(frame[y*160+x]==0x2a45U);
    }
    helpWeaponMiniReady=0U;
    for(unsigned i=0;i<160U*120U;++i)frame[i]=0x2a45U;
    helpDrawWeaponGridMini(frame,0U);
    for(unsigned i=0;i<160U*120U;++i)assert(frame[i]==0x2a45U);
}
int main(void) {
    touchContract();
    backgroundContract();
    paintContract();
    gridContract();
    puts("HELP fullscreen: c.bmp unshaded across 19200 pixels/3 phases, red X border absent, chevrons/144x76 grid and all touch hits PASS");
    return 0;
}
"""
with tempfile.TemporaryDirectory(prefix="doom-help-fullscreen-") as tmp:
    cfile = Path(tmp) / "main.c"
    exe = Path(tmp) / "test"
    cfile.write_text(harness)
    subprocess.run([
        "cc", "-O2", "-std=c11", "-Wall", "-Wextra", "-Werror",
        "-I", str(root / "include"), str(cfile), "-o", str(exe)
    ], check=True)
    subprocess.run([str(exe)], check=True)
