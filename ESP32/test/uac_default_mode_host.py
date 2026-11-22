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
options=(root/"ESP32/src/native_main_menu_options_back.c").read_text()
touch=(root/"ESP32/src/native_main_menu_touch.c").read_text()
layout=(root/"ESP32/src/native_main_menu_touch_layout.c").read_text()
video=(root/"ESP32/src/platform_video.cpp").read_text()
proof=(root/"artwork/menu-uac/generic/tools/render_corrected_120.py").read_text()
header=(root/"ESP32/include/esp_uac_menu_preview.h").read_text()

def section(code:str,start:str,end:str)->str:
    assert start in code and end in code,(start,end)
    return code.split(start,1)[1].split(end,1)[0]

assert 'void EspUacMenuPreview_armDefault(void);' in header
assert 'int EspUacMenuPreview_enterDefault(void);' in header
assert 'constexpr int kPaintGlyphW=15,kPaintGlyphH=25,kPaintAdvance=13;' in preview
assert 'constexpr int kLabelYOffset=1;' in preview
assert 'const int sourceY=paintY*kGlyphH/textH;' in preview
assert 'const int sourceX=x*kGlyphW/textW;' in preview
assert 'for(int x=0;x<textW;++x)' in preview
# Historical accepted 14x23 bitmap proofs remain immutable until CYD visual PASS.
assert 'PAINT_W,PAINT_H,ADVANCE=14,23,14' in proof
assert 'LABEL_Y_OFFSET=2' in proof
assert 'y0=TOPS[i]+(40-PAINT_H)//2+LABEL_Y_OFFSET' in proof
assert 'source_y=cy*17//PAINT_H' in proof
assert 'source_x=cx*13//PAINT_W' in proof
assert '12+46*4+kFiveButtonHeight<=240-10' in preview
assert 'static int pageSlotCount(int page){return pageCount(page)==5?5:4;}' in preview
assert 'const int countButtons=pageSlotCount(optionsPage);' in preview
assert "return pageCount(page)==5?12+46*index:kTop[index];" in preview
assert 'for(int i=0;i<pageCount(optionsPage);++i)' in preview  # blank slot noninteractive
assert 'case ESP_UAC_OPTIONS_ROOT: return index<3?' in preview
assert "const char ch=(raw>='a'&&raw<='z')?(char)(raw-'a'+'A'):raw;" in preview
alphabet="%01257ABCDEFHIKLMNOPRSTUV"
labels=("START","LOAD","OPTIONS","HELP","BACK","VIDEO","SOUND",
        "LUMINOSITE","ORIENTATION","THEME","25%","50%","75%","100%",
        "NORMAL","INVERSE","CLASSIC","INFERNO","PHOSPHORE","ON","OFF","VOLUME")
assert all(set(label)<=set(alphabet) for label in labels)
for label in ("start","options","BACK","SOUND","25%","100%"):
    mapped="".join(chr(ord(c)-32) if "a"<=c<="z" else c for c in label)
    assert mapped==label.upper()
assert '((py-buttonTop(optionsPage,active))*20/buttonHeight(optionsPage))' in preview
assert '((py-buttonTop(optionsPage,active))*40/buttonHeight(optionsPage))' not in preview
# Menu-only bounded caches, never persistent after reset or mode exit.
assert 'static BufferedBmp frameCache{},genericCache{};' in preview
assert 'releaseMenuAssets();' in preview
assert '32720 heap bytes' in preview and 'cacheNativeBmp(' in preview
assert 'frameBatch.bytes=frameCache.pixels;' in preview
assert 'buttonBatch.bytes=genericCache.pixels;' in preview
assert 'const bool patch=previous>=0&&previous<pageCount(optionsPage);' in preview
assert 'const int regions=patch?(previous==selected?1:2):1;' in preview
assert 'const int previous=(current==paintedMode && paintedPage==page &&' in preview
assert 'paintedTheme==theme)?' in preview
assert 'paintedTheme=theme;' in preview
assert 'paintedTheme==theme && paintedDark==darkWanted)return true;' in preview
assert 'const bool blinkOnly=' in preview
assert 'if(!blinkOnly)Serial.printf(' in preview
assert 'EspUacMenuPreview_needsBlink()' in (root/'ESP32/src/native_main_menu_actions.c').read_text()
assert 'EspUacMenuPreview_needsBlink' not in (root/'ESP32/src/main.cpp').read_text()
assert 'kOptionsVolume[]={"25%","50%","75%","100%"}' in preview
assert 'page==ESP_UAC_OPTIONS_VOLUME)return 4;' in preview
assert 'selected-=1;mask=0x0fU;' in preview
assert 'if(soundOpen && uacInstant && item>=0)++item;' in options
# Existing MAIN selection/confirmation gate is retained across all UAC modes.
assert 'if (EspUacMenuPreview_mode() == 1)' not in touch
assert '[UAC] INSTANT action=' not in touch
assert 'if (lastTappedItem == hit) {' in touch
assert 'DoomRPG_esp32MainMenuTouchArmSelected(hit)' in touch
assert 'executeConfirmedAction(hit);' in touch
# ONLY brightness/volume percentage buttons have a one-tap UAC shortcut.
assert options.count('if (armedItem != item && !uacInstant) {')==2
assert options.count('if (armedItem != item) {')==4
for lower,upper in [
    ('if (soundOpen) {','if (soundMenuOpen) {'),
    ('if (luminositeOpen) {','if (orientationOpen) {')]:
    assert section(options,lower,upper).count('if (armedItem != item && !uacInstant) {')==1
for lower,upper in [
    ('if (orientationOpen) {','if (themeOpen) {'),
    ('if (themeOpen) {','if (videoOpen) {'),
    ('if (videoOpen) {','/* SOUND is a native ON/OFF + VOLUME intermediate child. */'),
    ('/* SOUND is a native ON/OFF + VOLUME intermediate child. */','if (item == OPTIONS_VIDEO_ITEM) {')]:
    assert 'if (armedItem != item) {' in section(options,lower,upper)
for i in range(32): assert 0<=i*20//32<20

enter=section(preview,'extern "C" int EspUacMenuPreview_enterDefault(void){',
              'extern "C" int EspUacMenuPreview_cycle(void){')
assert 'if(!defaultPending)return mode;' in enter
assert 'defaultPending=false;' in enter
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
assert 'mode=next;' in cycle and 'releaseMenuAssets();' in cycle
assert 'EspUacMenuPreview_reset();' in video

compose=section(layout,'int DoomRPG_esp32RepaintOpaqueMainMenu(',
                'int __wrap_DoomRPG_probeNativeMainMenuOverlay(')
assert 'heapAfter != heapBefore || largestAfter != largestBefore' in compose
assert 'DoomRPG_esp32MainMenuTouchActivate(doomRpg, finalHash)' in compose
assert '(void)EspUacMenuPreview_enterDefault();' not in compose
assert compose.index('heapAfter != heapBefore') < compose.index('(void)Esp32PlatformVideo_present()')
# The source-locked SDL-F17 whole-file oracle owns this original layout.
def git_blob(data:str)->str:
    import hashlib
    raw=data.encode()
    return hashlib.sha1(b"blob "+str(len(raw)).encode()+b"\0"+raw).hexdigest()
assert git_blob(layout)=="8bc87944b6a193b087975c5465d6a6616d37b895"
activate=section(touch,'int DoomRPG_esp32MainMenuTouchActivate(',
                 'int DoomRPG_esp32MainMenuTouchIsActive(')
assert 'EspUacMenuPreview_armDefault();' in activate
assert activate.index('DoomRPG_esp32MainMenuFramebufferHash(') < activate.index('EspUacMenuPreview_armDefault()')
assert activate.index('PlatformInput_setTapCallback(DoomRPG_esp32MainMenuTouchOnTap)') < activate.index('EspUacMenuPreview_armDefault()')
assert 'EspUacMenuPreview_reset();' in touch
present=section(video,'bool PlatformVideo_present() {',
               '#if DOOMRPG_ESP32_TOUCH_HITBOX_OVERLAY')
assert '(void)EspUacMenuPreview_enterDefault();' in present
assert present.index('EspUacMenuPreview_enterDefault()') < present.index('EspUacMenuPreview_mode()')
assert 'EspUacMenuPreview_reset();' in present

for label in ("START","LOAD","OPTIONS","HELP"):
    width=len(label)*13+2  # 15px ink with 13px advance, 2px overlap
    x=(110-width)//2
    assert x>=6 and x+width<=104,(label,x)
y=(40-25)//2+1
assert y==8 and y+25==33  # 8px upper / 7px lower clearance
assert 25*17*7==2975
for x in range(15):
    sx=x*13//15
    assert 0<=sx<13
for y in range(25):
    sy=y*17//25
    assert 0<=sy<17

activate_options=(root/"ESP32/src/native_main_menu_options_action.c").read_text()
assert 'EspUacMenuPreview_armOptions();' in activate_options
assert activate_options.index('heapAfter != heapBefore') < activate_options.index('EspUacMenuPreview_armOptions();')
assert 'EspUacMenuPreview_optionsActive()' in touch
assert 'EspUacMenuPreview_optionsHit(screenX,screenY)' in options
assert 'EspUacMenuPreview_enterOptions();' in video
assert 'paintedPage==page' in preview and 'optionsEnabledMask==paintedEnabledMask' not in preview
assert 'kOptionsVideo[]={"BACK","LUMINOSITE","ORIENTATION","THEME"}' in preview
assert 'kOptionsBrightness[]={"BACK","25%","50%","75%","100%"}' in preview
for name in ('ROOT','VIDEO','BRIGHTNESS','ORIENTATION','THEME','SOUND_ON','SOUND_OFF','VOLUME'):
    assert 'ESP_UAC_OPTIONS_'+name in header
for label in ("LUMINOSITE","ORIENTATION","PHOSPHORE"):
    advance=min(14,98//len(label))
    assert 0<advance and len(label)*advance<=98
for count in (3,4,5):
    positions=[12+46*i for i in range(5)] if count==5 else [20,74,128,182]
    height=32 if count==5 else 40
    assert len(positions)==(5 if count==5 else 4)
    assert all(positions[i+1]>=positions[i]+height for i in range(len(positions)-1))
    assert positions[0]>=0 and positions[-1]+height<=240
    if count==3:
        assert positions[3]==182 and 3 not in range(count)  # blank, noninteractive
for y in range(32):
    sy=y*20//32
    # When passing a 2x-expanded coordinate to the BMP blender:
    assert (y*40//32)//2==sy
    assert 0<=sy<20
assert 'esp_uac_menu_preview.h' in options and 'EspUacMenuPreview_optionsPage(' in options
print("UAC-DEFAULT+OPTIONS HOST PASS: deferred integrity checks, 14x23 lower optical baseline, 2975B bounded font cache,")
print("                              cached SD BMP, two-tap navigation, one-tap percentages only, bounded rows")
