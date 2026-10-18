#!/usr/bin/env python3
"""Compare real DoomRPG presentation bodies after CYD coordinate adaptation.

Both original and permanent native functions execute against the same modeled
ESP32 SDL pixel/clip operations in separate guarded RGB565 host buffers.
Mocks represent the SDL call boundary, not the physical TFT/SPI controller.
"""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile
import zlib

ROOT = Path(__file__).resolve().parents[2]
METHODS = (
    ("void DoomRPG_setClipFalse(DoomRPG_t* doomrpg)", 0x2dd78609),
    ("void DoomRPG_setClipTrue(DoomRPG_t* doomrpg, int x, int y, int w, int h)", 0xa02f2e6e),
    ("void DoomRPG_setColor(DoomRPG_t* doomrpg, int color)", 0x71b1064b),
    ("void DoomRPG_setFontColor(DoomRPG_t* doomrpg, int color)", 0xc7fa369c),
    ("void DoomRPG_fillRect(DoomRPG_t* doomrpg, int x, int y, int w, int h)", 0xbf15d43a),
    ("void DoomRPG_drawLine(DoomRPG_t* doomrpg, int x1, int y1, int x2, int y2)", 0x441e2ca2),
    ("void DoomRPG_flushGraphics(DoomRPG_t* doomrpg)", 0x7ca4dad8),
)


def extract(source, signature):
    anchor = signature + "\n{"
    assert source.count(anchor) == 1, "missing/duplicate signature " + signature
    start = source.index(anchor)
    depth = 0
    for i in range(source.index("{", start), len(source)):
        if source[i] == "{":
            depth += 1
        elif source[i] == "}":
            depth -= 1
            if depth == 0:
                return source[start:i + 1]
    raise AssertionError("unterminated " + signature)


def adapted(s):
    return (s.replace("doomrpg->doomCanvas->displayRect.x + ", "")
             .replace("doomrpg->doomCanvas->displayRect.y + ", ""))


HARNESS = r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL.h>
#include "DoomRPG.h"
#include "DoomCanvas.h"
#include "SDL_Video.h"

enum { W=160, H=120, PIXELS=W*H, GUARD=12 };
struct SDL_Renderer { int slot; };
static struct SDL_Renderer renderers[2] = {{0},{1}};
SDLVideo_t sdlVideo;
typedef struct {
    uint16_t pixels[GUARD+PIXELS+GUARD];
    SDL_Rect clip;
    int enabled, presents, blend;
    uint8_t r,g,b,a;
} Mock;
static Mock mocks[2];
static Mock* target(SDL_Renderer* r) {
    assert(r == &renderers[0] || r == &renderers[1]);
    return &mocks[((struct SDL_Renderer*)r)->slot];
}
static uint16_t rgb(uint8_t r,uint8_t g,uint8_t b) {
    return (uint16_t)(((r&248)<<8)|((g&252)<<3)|(b>>3));
}
static int contains(Mock* m,int x,int y) {
    if (x<0 || y<0 || x>=W || y>=H) return 0;
    return !m->enabled ||
           (x>=m->clip.x && y>=m->clip.y &&
            x<m->clip.x+m->clip.w && y<m->clip.y+m->clip.h);
}
static void pixel(Mock* m,int x,int y) {
    if (contains(m,x,y))
        m->pixels[GUARD+y*W+x]=rgb(m->r,m->g,m->b);
}
int SDL_RenderSetClipRect(SDL_Renderer* r,const SDL_Rect* rect) {
    Mock* m=target(r);
    m->enabled=rect!=NULL;
    m->clip=rect!=NULL?*rect:(SDL_Rect){0,0,W,H};
    return 0;
}
int SDL_SetRenderDrawColor(SDL_Renderer* r,Uint8 red,Uint8 green,Uint8 blue,Uint8 alpha) {
    Mock* m=target(r);
    m->r=red; m->g=green; m->b=blue; m->a=alpha;
    return 0;
}
int SDL_RenderFillRect(SDL_Renderer* r,const SDL_Rect* rect) {
    Mock* m=target(r);
    SDL_Rect a=rect!=NULL?*rect:(SDL_Rect){0,0,W,H};
    for(int y=a.y;y<a.y+a.h;y++)
        for(int x=a.x;x<a.x+a.w;x++) pixel(m,x,y);
    return 0;
}
int SDL_RenderDrawLine(SDL_Renderer* r,int x1,int y1,int x2,int y2) {
    Mock* m=target(r);
    int dx=abs(x2-x1),sx=x1<x2?1:-1,dy=-abs(y2-y1),sy=y1<y2?1:-1;
    int err=dx+dy;
    for(;;) {
        pixel(m,x1,y1);
        if (x1==x2 && y1==y2) break;
        int doubled=err*2;
        if(doubled>=dy) {err+=dy;x1+=sx;}
        if(doubled<=dx) {err+=dx;y1+=sy;}
    }
    return 0;
}
void SDL_RenderPresent(SDL_Renderer* r) { ++target(r)->presents; }

void Reference_DoomRPG_setClipFalse(DoomRPG_t*);
void Reference_DoomRPG_setClipTrue(DoomRPG_t*,int,int,int,int);
void Reference_DoomRPG_setColor(DoomRPG_t*,int);
void Reference_DoomRPG_setFontColor(DoomRPG_t*,int);
void Reference_DoomRPG_fillRect(DoomRPG_t*,int,int,int,int);
void Reference_DoomRPG_drawLine(DoomRPG_t*,int,int,int,int);
void Reference_DoomRPG_flushGraphics(DoomRPG_t*);

static DoomCanvas_t canvases[2];
static DoomRPG_t cores[2];
static uint32_t seed=0x24c0ffeeU;
static uint32_t next(void) { seed=seed*1664525u+1013904223u;return seed; }
static int coord(void) {return (int)(next()%231u)-35;}
static int extent(void) {return (int)(next()%44u)-3;}
static void select_renderer(int i) {sdlVideo.renderer=&renderers[i];}
static void check(const char* phase) {
    for (int i=0;i<2;i++) {
        for (int j=0;j<GUARD;j++) {
            assert(mocks[i].pixels[j]==0xBEEFu);
            assert(mocks[i].pixels[GUARD+PIXELS+j]==0xBEEFu);
        }
    }
    if (memcmp(mocks[0].pixels,mocks[1].pixels,sizeof(mocks[0].pixels)) ||
        mocks[0].enabled!=mocks[1].enabled ||
        memcmp(&mocks[0].clip,&mocks[1].clip,sizeof(SDL_Rect)) ||
        mocks[0].r!=mocks[1].r || mocks[0].g!=mocks[1].g ||
        mocks[0].b!=mocks[1].b || mocks[0].a!=mocks[1].a ||
        mocks[0].presents!=mocks[1].presents ||
        cores[0].graphSetCliping!=cores[1].graphSetCliping ||
        canvases[0].fontColor!=canvases[1].fontColor) {
        fprintf(stderr,"presentation mismatch %s\n",phase);
        exit(1);
    }
}
#define PAIR(call) do { \
 select_renderer(0); Reference_ ## call; \
 select_renderer(1); call; \
 check(#call); \
} while(0)
static uint16_t at(int x,int y) { return mocks[1].pixels[GUARD+y*W+x]; }
int main(void) {
    for(int i=0;i<2;i++) {
        cores[i].doomCanvas=&canvases[i];
        for(int j=0;j<GUARD+PIXELS+GUARD;j++) mocks[i].pixels[j]=0xBEEFu;
    }
    /* Baseline, actual RGB565 values, inclusivity, exact clipping and reset. */
    select_renderer(0); Reference_DoomRPG_setClipFalse(&cores[0]);
    select_renderer(1); DoomRPG_setClipFalse(&cores[1]); check("reset");
    select_renderer(0); Reference_DoomRPG_setColor(&cores[0],(int)0xFF112233u);
    select_renderer(1); DoomRPG_setColor(&cores[1],(int)0xFF112233u); check("color");
    select_renderer(0); Reference_DoomRPG_fillRect(&cores[0],3,5,4,2);
    select_renderer(1); DoomRPG_fillRect(&cores[1],3,5,4,2); check("rect");
    assert(at(3,5)==rgb(0x11,0x22,0x33));
    assert(at(6,6)==rgb(0x11,0x22,0x33));
    assert(at(7,6)==0xBEEF && at(3,7)==0xBEEF);
    select_renderer(0); Reference_DoomRPG_setClipTrue(&cores[0],5,6,2,2);
    select_renderer(1); DoomRPG_setClipTrue(&cores[1],5,6,2,2); check("clip");
    select_renderer(0); Reference_DoomRPG_setColor(&cores[0],0xFFCC4400);
    select_renderer(1); DoomRPG_setColor(&cores[1],0xFFCC4400); check("second color");
    select_renderer(0); Reference_DoomRPG_fillRect(&cores[0],0,0,10,10);
    select_renderer(1); DoomRPG_fillRect(&cores[1],0,0,10,10); check("clipped fill");
    assert(at(5,6)==rgb(0xCC,0x44,0) && at(4,6)!=rgb(0xCC,0x44,0));
    select_renderer(0); Reference_DoomRPG_setClipFalse(&cores[0]);
    select_renderer(1); DoomRPG_setClipFalse(&cores[1]); check("unclip");
    select_renderer(0); Reference_DoomRPG_drawLine(&cores[0],0,0,159,119);
    select_renderer(1); DoomRPG_drawLine(&cores[1],0,0,159,119); check("diagonal");
    assert(at(0,0)==rgb(0xCC,0x44,0) && at(159,119)==rgb(0xCC,0x44,0));
    select_renderer(0); Reference_DoomRPG_setFontColor(&cores[0],0xFFAA00EE);
    select_renderer(1); DoomRPG_setFontColor(&cores[1],0xFFAA00EE); check("font");
    assert(canvases[1].fontColor==(int)0xFFAA00EEu);
    select_renderer(0); Reference_DoomRPG_flushGraphics(&cores[0]);
    select_renderer(1); DoomRPG_flushGraphics(&cores[1]); check("present");
    assert(mocks[1].presents==1);
    for (int t=0;t<1600;t++) {
        int op=(int)(next()%7u);
        int x=coord(), y=coord(), u=coord(), v=coord();
        int w=extent(),h=extent();
        int col=(int)next();
        select_renderer(0);
        switch(op) {
        case 0: Reference_DoomRPG_setClipFalse(&cores[0]);break;
        case 1: Reference_DoomRPG_setClipTrue(&cores[0],x,y,w,h);break;
        case 2: Reference_DoomRPG_setColor(&cores[0],col);break;
        case 3: Reference_DoomRPG_fillRect(&cores[0],x,y,w,h);break;
        case 4: Reference_DoomRPG_drawLine(&cores[0],x,y,u,v);break;
        case 5: Reference_DoomRPG_setFontColor(&cores[0],col);break;
        case 6: Reference_DoomRPG_flushGraphics(&cores[0]);break;
        }
        select_renderer(1);
        switch(op) {
        case 0: DoomRPG_setClipFalse(&cores[1]);break;
        case 1: DoomRPG_setClipTrue(&cores[1],x,y,w,h);break;
        case 2: DoomRPG_setColor(&cores[1],col);break;
        case 3: DoomRPG_fillRect(&cores[1],x,y,w,h);break;
        case 4: DoomRPG_drawLine(&cores[1],x,y,u,v);break;
        case 5: DoomRPG_setFontColor(&cores[1],col);break;
        case 6: DoomRPG_flushGraphics(&cores[1]);break;
        }
        check("seeded differential");
    }
    printf("PRESENTATION PASS: 7 real bodies, 1600 mixed clips/colors/rects/lines/"
           "font/presents, RGB565 pixels and sentinels\n");
    return 0;
}
'''


def main():
    legacy = (ROOT / "src/DoomRPG.c").read_text(encoding="latin-1")
    native = (ROOT / "ESP32/src/esp_doomrpg_presentation.c").read_text(encoding="latin-1")
    assert "SDL_Render" in native
    assert "PlatformVideo_framebuffer" not in native
    names, bodies = [], []
    for signature, crc in METHODS:
        real = adapted(extract(legacy, signature))
        assert zlib.crc32(real.encode("latin-1")) == crc, signature + " drift"
        assert extract(native, signature) == real, signature + " native drift"
        bodies.append(real)
        names.append(re.search(r"(DoomRPG_\w+)\(", signature).group(1))
    renamed = "\n".join("#define %s Reference_%s" % (n, n) for n in names)
    reference = (renamed + '\n#include <SDL.h>\n#include "DoomRPG.h"\n'
                 '#include "DoomCanvas.h"\n#include "SDL_Video.h"\n\n' +
                 "\n\n".join(bodies) + "\n")
    with tempfile.TemporaryDirectory(prefix="doomrpg-presentation-") as tmp:
        temp = Path(tmp)
        (temp/"reference.c").write_text(reference, encoding="latin-1")
        (temp/"test.c").write_text(HARNESS)
        cmd = shlex.split(os.environ.get("CC", "cc")) + [
            "-O2", "-std=c11", "-Wall", "-Wextra", "-Werror",
            "-Wno-unused-parameter", "-DDOOMRPG_ESP32=1",
            "-IESP32/include", "-Isrc",
            "ESP32/src/esp_doomrpg_presentation.c", str(temp/"reference.c"),
            str(temp/"test.c"), "-o", str(temp/"test")]
        cmd += shlex.split(os.environ.get("PRESENTATION_SANITIZE_FLAGS", ""))
        subprocess.run(cmd, check=True, cwd=ROOT)
        subprocess.run([str(temp/"test")], check=True, cwd=ROOT)


if __name__ == "__main__":
    main()
