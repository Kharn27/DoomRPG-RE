#!/usr/bin/env python3
"""Compile and execute the REAL UAC cached-patch scanout body with TFT mocks.

No SD, no sound ISR: checks two exact 110x40 rectangles, channel-wise dim,
resource lifecycle/static guards in the firmware owner. Hardware still required.
"""
from pathlib import Path
import subprocess
import tempfile

ROOT=Path(__file__).resolve().parents[2]
src=(ROOT/"ESP32/src/esp_uac_menu_preview.cpp").read_text()
def require(value,why):
    if not value: raise AssertionError(why)

def body(signature):
    require(src.count(signature)==1,"missing/ambiguous production body: "+signature)
    i=src.index(signature); begin=src.index("{",i); level=0
    for j in range(begin,len(src)):
        if src[j]=="{":level+=1
        elif src[j]=="}":
            level-=1
            if level==0:return src[i:j+1]
    raise AssertionError("unbalanced C++ body")

dim=body("static uint16_t dimRgb565(uint16_t pixel)")
real=body("static bool renderCachedSelection(TFT_eSPI* lcd,int m,int selected,int previous,")
preflight=body("static bool preflight(void)")
draw=body("static bool draw(const uint16_t* fb,uint16_t stride,TFT_eSPI* lcd,")
require("buttonPath(p,sizeof(p),i,true);" in preflight,
        "five-BMP preflight must require only active labelled sources")
require("buttonPath(path,sizeof(path),active,true);" in draw,
        "cold paint should use the same bright source as hot paint")
require("bright[x]=dimRgb565(bright[x]);" in draw,
        "cold paint missing pixel-identical software dimming")
require(draw.index("memcpy(dst+")<draw.index("bright[x]=dimRgb565(bright[x]);"),
        "bright surface must be cached BEFORE dimming")
require("freeCache(\"menu-exit\")" in src,"cache leaked on menu exit")
require("EspUacMenuPreview_cycle" not in src,"comparison mode switch survived")
require("freeCache(\"menu-activate\")" in src,"fresh MENU_MAIN must discard retired assets")
require("if(!preflight())" in src,"invalid SD BMP must fail closed")
require("mode=kClassic;" in src,"procedural fallback lost")
require("const int scale=1;" in src,"renderer must use native 240 pixels")
require("assetTier(" not in src and "k240As120" not in src,"experimental modes survived")
require("allocateCache(mode)" in src,"mode cache must be transient")
require("warmCache(fb,stride,current)" in src,"no one-time precomposition")
require("MALLOC_CAP_8BIT" in src and "kCacheReserve8=20U*1024U" in src,"no heap8 headroom guard")
require("cachePixels[active]" in src,"full/patch capture must use bright state")
require("static uint16_t* cachePixels[4]={};" in src,"only four RGB565 patches permitted")
require("slots=%u/4 bytes=%u" in src,"cache allocation cap must be four patches")
require("cacheReady[4]" in src and "cachePixels[4][2]" not in src,"old 8-slot cache survived")
require("for(int i=0;i<4;++i)ready+=cacheReady[i]?1U:0U;" in src,
        "no second-state warmup permitted")
require("if(!warming){" in src,"warming must not touch TFT")
require("return false;  /* SD failure is still fail-closed */" in src,"cache must not mask SD corruption")

# Verify the real C++ mode/activation/reset bodies against MAINOPAQUE's
# invariant: MENU_MAIN callback activation must NOT read SD or allocate RAM.
menu_layout=(ROOT/"ESP32/src/native_main_menu_touch_layout.c").read_text()
menu_touch=(ROOT/"ESP32/src/native_main_menu_touch.c").read_text()
mode_body=body('extern "C" int EspUacMenuPreview_mode(void)')
activate_body=body('extern "C" int EspUacMenuPreview_activate(void)')
present_activation=body('static bool activateOnFirstPresent(void)')
reset_body=body('extern "C" void EspUacMenuPreview_reset(void)')
paint_body=body('bool EspUacMenuPreview_draw(TFT_eSPI* lcd,const uint16_t* fb,uint16_t stride)')
require('activationPending=true;' in activate_body,"activation must be deferred")
for forbidden in ("preflight(", "allocateCache(", "freeCache(", "SD.", "heap_caps_"):
    require(forbidden not in activate_body,
            "heap/SD touched during software menu invariant: "+forbidden)
require('if(activationPending&&!activateOnFirstPresent())return false;' in paint_body,
        "first physical present must resolve deferred activation")
require('if(!preflight())' in present_activation and 'allocateCache(mode)' in present_activation,
        "SD validation and allocation must occur on first present")
require('activationPending=false;' in reset_body,"reset must cancel pending activation")
require(menu_touch.count('EspUacMenuPreview_activate();')==1,
        "MENU_MAIN input owner must issue one activation")
start=menu_layout.index('if (!DoomRPG_esp32MainMenuTouchActivate(doomRpg, finalHash))')
heap_check=menu_layout.index('if (heapAfter != heapBefore || largestAfter != largestBefore)',start)
actual_present=menu_layout.index('(void)Esp32PlatformVideo_present();',heap_check)
require(start<heap_check<actual_present,"heap invariant must precede the UAC first present")

mock=r"""
#include <cassert>
#include <cstdio>
enum { kClassic=0,kUac240=3 };
int mode=kClassic,paintedMode=0,paintedSelection=-1;
bool activationPending=false;
int sdCalls=0,allocCalls=0,freeCalls=0,heap=94008;
bool assetsValid=true;
struct MockSerial {
    template <typename... Args> void printf(const char*,Args...) {}
    void println(const char*) {}
} Serial;
bool preflight(){++sdCalls;return assetsValid;}
void allocateCache(int){++allocCalls;heap-=35200;}
void freeCache(const char*){++freeCalls;heap=94008;}
__MODE__
__ACTIVATE__
__FIRST__
__RESET__
int main(){
    const int before=heap;
    assert(EspUacMenuPreview_activate()==1);
    assert(EspUacMenuPreview_mode()==3);
    assert(heap==before && sdCalls==0 && allocCalls==0 && freeCalls==0);
    assert(activateOnFirstPresent());
    assert(sdCalls==1 && allocCalls==1 && heap==before-35200);
    EspUacMenuPreview_reset();
    assert(heap==before && mode==0 && !activationPending);
    assetsValid=false;sdCalls=0;allocCalls=0;freeCalls=0;
    assert(EspUacMenuPreview_activate()==1);
    assert(heap==before && sdCalls==0);
    assert(!activateOnFirstPresent());
    assert(mode==0 && sdCalls==1 && allocCalls==0);
    EspUacMenuPreview_reset();
    assert(heap==before);
    puts("UAC DEFER PASS: owner activation heap/SD-free; physical present alloc+fallback");
}
""".replace("__MODE__",mode_body).replace("__ACTIVATE__",activate_body).replace("__FIRST__",present_activation).replace("__RESET__",reset_body)
with tempfile.TemporaryDirectory(prefix="uac-defer-") as dirname:
    root=Path(dirname); cpp=root/"defer.cpp";exe=root/"defer"
    cpp.write_text(mock)
    subprocess.run(["c++","-O2","-std=c++17","-Wall","-Wextra","-Werror",
                    str(cpp),"-o",str(exe)],check=True)
    subprocess.run([str(exe)],check=True)

test=r"""
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
enum { ESP_UAC_BUTTON_WIDTH=110, ESP_UAC_BUTTON_INSET=4 };
enum { kPanelX=202 };
int kTop[4]={20,74,128,182};
int cacheMode=3;
uint16_t all[4][110*40]{};
uint16_t* cachePixels[4]{};
uint8_t cacheReady[4]{};
struct DrawStats {
    uint32_t sdUs=0,lcdUs=0;
    unsigned batches=0,buttonOpens=0,rowsSent=0,audioPumps=0,audioBlocks=0,cacheHits=0;
};
uint32_t micros(){static uint32_t t=100;return t+=10;}
unsigned EspEntranceMusic_pumpMenuPreview(){return 0;}
struct TFT_eSPI {
    uint16_t framebuffer[320*240]{};
    int x=0,y=0,w=0,h=0,pos=0,starts=0,ends=0,windows=0;
    void startWrite(){++starts;}
    void endWrite(){++ends;}
    void setAddrWindow(int px,int py,int pw,int ph){
        x=px;y=py;w=pw;h=ph;pos=0;++windows;
    }
    void pushPixels(uint16_t* data,int n){
        assert(n==w && pos/w<h);
        for(int i=0;i<n;++i)
            framebuffer[(y+pos/w)*320+x+i]=data[i];
        pos+=n;
    }
};
__DIM__
__BODY__
int main(){
    TFT_eSPI lcd;
    DrawStats stats;
    for(int i=0;i<4;++i){
        cachePixels[i]=all[i];
        for(int p=0;p<110*40;++p)all[i][p]=(uint16_t)(1000+i*100+(p%17));
    }
    assert(!renderCachedSelection(&lcd,3,2,0,&stats));
    assert(stats.batches==0 && lcd.windows==0);
    cacheReady[0]=cacheReady[2]=1;
    assert(renderCachedSelection(&lcd,3,2,0,&stats));
    assert(lcd.starts==1 && lcd.ends==1 && lcd.windows==2);
    assert(stats.rowsSent==80 && stats.batches==2 && stats.cacheHits==2);
    assert(stats.sdUs==0 && stats.audioPumps==1);
    for(int row=0;row<40;++row)for(int col=0;col<110;++col){
        assert(lcd.framebuffer[(20+row)*320+206+col]==
               dimRgb565(all[0][row*110+col]));
        assert(lcd.framebuffer[(128+row)*320+206+col]==all[2][row*110+col]);
    }
    assert(lcd.framebuffer[19*320+206]==0 && lcd.framebuffer[20*320+205]==0);
    assert(!renderCachedSelection(&lcd,3,0,0,&stats)); /* no redraw */
    assert(!renderCachedSelection(&lcd,2,2,0,&stats)); /* wrong asset tier */
    assert(!renderCachedSelection(&lcd,3,3,2,&stats)); /* missing states */
    puts("UAC CACHE PASS: 4 bright patches, dim inactive before TFT, exact 110x40 pixels, SD=0, 35.2KB heap");
}
""".replace("__DIM__",dim).replace("__BODY__",real)
with tempfile.TemporaryDirectory(prefix="uac-hot-cache-") as dirname:
    root=Path(dirname); source=root/"test.cpp"; exe=root/"test"
    source.write_text(test)
    subprocess.run(["c++","-O2","-std=c++17","-Wall","-Wextra","-Werror",
                    str(source),"-o",str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
