#!/usr/bin/env python3
"""Compile and execute the REAL UAC cached-patch scanout body with TFT mocks.

No SD, no sound ISR: checks the two exact 110x40 rectangles and states,
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

real=body("static bool renderCachedSelection(TFT_eSPI* lcd,int m,int selected,int previous,")
require("freeCache(\"menu-exit\")" in src,"cache leaked on menu exit")
require("freeCache(next==0?" in src,"mode cycle did not drop old assets")
require("allocateCache(mode)" in src,"mode cache must be transient")
require("warmCache(fb,stride,current)" in src,"no one-time precomposition")
require("MALLOC_CAP_8BIT" in src and "kCacheReserve8=20U*1024U" in src,"no heap8 headroom guard")
require("cachePixels[active][selected==active]" in src,"full/patch capture must use rendered state")
require("if(!warming){" in src,"warming must not touch TFT")
require("return false;  /* SD failure is still fail-closed */" in src,"cache must not mask SD corruption")
test=r"""
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
enum { ESP_UAC_BUTTON_WIDTH=110, ESP_UAC_BUTTON_INSET=4 };
enum { kPanelX=202 };
int kTop[4]={20,74,128,182};
int cacheMode=3;
uint16_t all[4][2][110*40]{};
uint16_t* cachePixels[4][2]{};
uint8_t cacheReady[4][2]{};
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
__BODY__
int main(){
    TFT_eSPI lcd;
    DrawStats stats;
    for(int i=0;i<4;++i)for(int s=0;s<2;++s){
        cachePixels[i][s]=all[i][s];
        for(int p=0;p<110*40;++p)all[i][s][p]=(uint16_t)(1000+i*100+s*30+(p%17));
    }
    assert(!renderCachedSelection(&lcd,3,2,0,&stats));
    assert(stats.batches==0 && lcd.windows==0);
    cacheReady[0][0]=cacheReady[2][1]=1;
    assert(renderCachedSelection(&lcd,3,2,0,&stats));
    assert(lcd.starts==1 && lcd.ends==1 && lcd.windows==2);
    assert(stats.rowsSent==80 && stats.batches==2 && stats.cacheHits==2);
    assert(stats.sdUs==0 && stats.audioPumps==1);
    for(int row=0;row<40;++row)for(int col=0;col<110;++col){
        assert(lcd.framebuffer[(20+row)*320+206+col]==all[0][0][row*110+col]);
        assert(lcd.framebuffer[(128+row)*320+206+col]==all[2][1][row*110+col]);
    }
    assert(lcd.framebuffer[19*320+206]==0 && lcd.framebuffer[20*320+205]==0);
    assert(!renderCachedSelection(&lcd,3,0,0,&stats)); /* no redraw */
    assert(!renderCachedSelection(&lcd,2,2,0,&stats)); /* wrong asset tier */
    assert(!renderCachedSelection(&lcd,3,3,2,&stats)); /* missing states */
    puts("UAC CACHE PASS: real two 110x40 RGB565 windows, exact states, SD=0, mode guard, reset/heap policies");
}
""".replace("__BODY__",real)
with tempfile.TemporaryDirectory(prefix="uac-hot-cache-") as dirname:
    root=Path(dirname); source=root/"test.cpp"; exe=root/"test"
    source.write_text(test)
    subprocess.run(["c++","-O2","-std=c++17","-Wall","-Wextra","-Werror",
                    str(source),"-o",str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
