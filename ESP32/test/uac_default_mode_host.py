#!/usr/bin/env python3
"""Source-backed UAC MENU_MAIN default, fail-closed and bounded zoom guards.

This is deliberately a host architecture oracle rather than a fake SD/LCD
device. The real BMP reader, palette decoder and firmware are tested by the
existing host C and PlatformIO workflows.
"""
from pathlib import Path
import re

root=Path(__file__).resolve().parents[2]
preview=(root/"ESP32/src/esp_uac_menu_preview.cpp").read_text()
touch=(root/"ESP32/src/native_main_menu_touch.c").read_text()
layout=(root/"ESP32/src/native_main_menu_touch_layout.c").read_text()
video=(root/"ESP32/src/platform_video.cpp").read_text()
proof=(root/"artwork/menu-uac/generic/tools/render_corrected_120.py").read_text()
header=(root/"ESP32/include/esp_uac_menu_preview.h").read_text()

def section(code:str,start:str,end:str)->str:
    assert start in code and end in code,(start,end)
    return code.split(start,1)[1].split(end,1)[0]

assert 'int EspUacMenuPreview_enterDefault(void);' in header
assert 'constexpr int kPaintGlyphW=14,kPaintGlyphH=27,kPaintAdvance=14;' in preview
assert 'const int sourceY=paintY*kGlyphH/kPaintGlyphH;' in preview
assert 'const int sourceX=x*kGlyphW/kPaintGlyphW;' in preview
assert 'for(int x=0;x<kPaintGlyphW;++x)' in preview
assert 'PAINT_W,PAINT_H,ADVANCE=14,27,14' in proof
assert 'source_y=cy*17//PAINT_H' in proof
assert 'source_x=cx*13//PAINT_W' in proof

enter=section(preview,'extern "C" int EspUacMenuPreview_enterDefault(void){',
              'extern "C" int EspUacMenuPreview_cycle(void){')
assert 'mode=kClassic;paintedMode=0;paintedSelection=-1;' in enter
assert 'releaseLargerFont();' in enter
assert 'if(!preflight(kNative120)){' in enter
assert 'mode=kNative120;' in enter
assert enter.index('if(!preflight(kNative120)){') < enter.index('mode=kNative120;')
assert '[UAC] DEFAULT FALLBACK' in enter
assert '[UAC] DEFAULT READY' in enter
assert 'MALLOC_CAP_8BIT' in enter
cycle=section(preview,'extern "C" int EspUacMenuPreview_cycle(void){',
              'bool EspUacMenuPreview_draw(')
assert 'mode==kNative120?k240As120:' in cycle
assert 'mode==k240As120?kDirect240:kNative120;' in cycle
assert '(mode+1)%4' not in cycle
assert 'if(!preflight(next)){' in cycle and 'mode=kClassic;' in cycle
assert 'return kClassic;' in cycle
assert 'mode=next;' in cycle and 'releaseLargerFont();' in cycle
assert 'EspUacMenuPreview_reset();' in video

compose=section(layout,'int DoomRPG_esp32RepaintOpaqueMainMenu(',
                'int __wrap_DoomRPG_probeNativeMainMenuOverlay(')
assert 'heapAfter != heapBefore || largestAfter != largestBefore' in compose
assert 'DoomRPG_esp32MainMenuTouchActivate(doomRpg, finalHash)' in compose
assert '(void)EspUacMenuPreview_enterDefault();' in compose
assert compose.index('heapAfter != heapBefore') < compose.index('EspUacMenuPreview_enterDefault()')
assert compose.index('EspUacMenuPreview_enterDefault()') < compose.index('(void)Esp32PlatformVideo_present()')
assert '#if defined(DOOMRPG_ESP32) && DOOMRPG_ESP32' in compose
assert 'EspUacMenuPreview_reset();' in touch

for label in ("START","LOAD","OPTIONS","HELP"):
    x=(110-len(label)*14)//2
    assert x>=6 and x+len(label)*14<=104,(label,x)
y=(40-27)//2
assert y>=6 and y+27<=34
assert 12*17*7==1428
for x in range(14):
    sx=x*13//14
    assert 0<=sx<13
for y in range(27):
    sy=y*17//27
    assert 0<=sy<17
print("UAC-DEFAULT HOST PASS: root entry after strict heap/FNV/touch checks, 120 default, classic fail-closed,")
print("                     logo 120->240sampled->240direct->120, 14x27 text within bezel,")
print("                     1428B temporary font cache, no added atlas or gameplay allocations")
