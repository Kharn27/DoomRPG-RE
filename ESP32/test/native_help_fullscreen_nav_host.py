#!/usr/bin/env python3
"""Build the production HELP chevrons, close frame and enlarged grid with C11.

Navigation hit detection is the same header used by the actual touch owner.
The tests check all 160x120 hitpoints and pixel bounds; no alternate painter.
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
    "static void helpPaintExitFrame(",
    "static void helpPaintPageChevron(",
    "static void helpDrawWeaponGridMini(",
))
assert 'EspNativeHelpNav_hit(logicalX, logicalY)' in src
assert "menuSystem->scrollIndex = EspNativeHelpNav_scroll(" in src
assert "if (action == ESP_NATIVE_HELP_NAV_CLOSE)" in src
assert "before != helpExpectedFrameFNV" in src
assert 'EspNativeCanvas_drawText(canvas, "X", 4, 1, 0);' in src
assert "helpPaintExitFrame(framebuffer);" in src
assert "helpPaintPageChevron(framebuffer, 1, menuSystem->scrollIndex > 0);" in src
assert "helpPaintPageChevron(framebuffer, 0, menuSystem->scrollIndex < maxScroll);" in src
assert "DoomRPG_esp32MainMenuReturnToMain(" in src
assert "MAIN_HELP_FOOTER_TOP" not in src
assert 'for (y = 13; y <= 116; ++y)' in src

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
static uint16_t helpWeaponMini[144*60];
static uint8_t helpWeaponMiniReady = 1U;
static uint16_t frame[160*120];
static uint16_t copy[144*60];
static int theme;
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
static void paintContract(void) {
    const uint16_t old = 0x2945U;
    for(unsigned i=0;i<160U*120U;++i)frame[i]=old;
    helpPaintExitFrame(frame);
    for(int y=0;y<120;++y)for(int x=0;x<160;++x) {
        const int border = (y==0||y==13)?(x>=1&&x<=16):
                           (y>=1&&y<=12&&(x==1||x==16));
        assert(frame[y*160+x] == (border?0xf800U:old));
    }
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
    helpDrawWeaponGridMini(frame);
    assert(memcmp(copy,helpWeaponMini,sizeof(copy))==0);
    for(int y=0;y<120;++y)for(int x=0;x<160;++x) {
        if(x>=8&&x<152&&y>=39&&y<115) {
            const int srcY=(y-39)*60/76;
            assert(frame[y*160+x]==copy[srcY*144+x-8]);
        } else assert(frame[y*160+x]==0x2a45U);
    }
    helpWeaponMiniReady=0U;
    for(unsigned i=0;i<160U*120U;++i)frame[i]=0x2a45U;
    helpDrawWeaponGridMini(frame);
    for(unsigned i=0;i<160U*120U;++i)assert(frame[i]==0x2a45U);
}
int main(void) {
    touchContract();
    paintContract();
    gridContract();
    puts("HELP fullscreen: actual 2-edge chevrons/exit frame/144x76 grid + all 19200 touch pixels PASS");
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
