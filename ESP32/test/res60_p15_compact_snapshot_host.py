#!/usr/bin/env python3
"""RES60-P15: real C11 P15 snapshot versus original P14, full framebuffer and UV overlay parity."""
from pathlib import Path
import configparser, subprocess, tempfile

ROOT=Path(__file__).resolve().parents[2]
cfg=configparser.ConfigParser(interpolation=None,inline_comment_prefixes=(";",))
assert cfg.read(ROOT/"ESP32/platformio.ini")
assert "-D DOOMRPG_RES60_P15_COMPACT_SNAPSHOT=1" in cfg["env:esp32-cyd-res60"]["build_flags"]
assert cfg["env:esp32-cyd-res60-inverted"]["extends"]=="env:esp32-cyd-res60"
for env in ("env:esp32-cyd","env:esp-cyd-inverted","env:esp32-cyd-trace","env:esp32-cyd-bringup"):
    assert "DOOMRPG_RES60_P15_COMPACT_SNAPSHOT" not in cfg[env]["build_flags"]
config=(ROOT/"ESP32/include/platform_video_config.h").read_text()
src=(ROOT/"ESP32/src/esp_res60_world_preview.c").read_text()
video=(ROOT/"ESP32/src/platform_video.cpp").read_text()
assert "RES60-P15 requires isolated P14/P13/P9" in config
assert "candidateLines[RES60_P9_MAX_LINES]" in src
assert "candidateLines[candidateCount]=(EspRes60P15SnapshotLine)" in src
assert "EspRes60P15_Line12" in src and "RES60_P15_SOURCE_MASK" in src
assert "EspRes60Layout_fnv(framebuffer, framebufferBytes)" in src
assert "[RES60P15] COMPACT" in video and "savedBss=3072" in video

CC=["cc","-std=c11","-O2","-Wall","-Wextra","-Werror",
    "-I",str(ROOT/"ESP32/include")]
FLAGS=[
    "-DDOOMRPG_RES60_P1_DIAGNOSTIC=1",
    "-DDOOMRPG_RES60_P2_LAYOUT=1",
    "-DDOOMRPG_RES60_P3_WORLD=1",
    "-DDOOMRPG_RES60_P5_TITLE=1",
    "-DDOOMRPG_RES60_P6_MENU=1",
    "-DDOOMRPG_RES60_P7_NAV=1",
    "-DDOOMRPG_RES60_P8_TEXEL=1",
    "-DDOOMRPG_RES60_P9_WEDGE=1",
    "-DDOOMRPG_RES60_P10_WALL_MATERIAL=1",
    "-DDOOMRPG_RES60_P11_PROFILE=1",
    "-DDOOMRPG_RES60_P12_INVENTORY_CACHE=1",
    "-DDOOMRPG_RES60_P13_MAP_LEASE=1",
    "-DDOOMRPG_RES60_P14_LINE_SNAPSHOT=1"
]
harness=r"""
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "esp_res60_world_preview.h"
#include "esp_res60_wall_material.h"
extern int EspRes60Preview_renderBaseline(uint16_t*,size_t,
              const EspRes60PreviewCamera*,uint32_t,
              EspRes60PreviewReadLine,void*,EspRes60PreviewStats*);
static EspRes60PreviewLine lines[513];
static int corrupt=-1, calls=0;
static uint32_t rnd=0x77aaff11U;
uint32_t EspRes60Profile_nowUs(void){static uint32_t t;return ++t;}
static uint32_t random32(void){rnd^=rnd<<13;rnd^=rnd>>17;rnd^=rnd<<5;return rnd;}
static uint16_t coord(void){return (uint16_t)(random32()%2049U);}
static int readLine(uint32_t i,EspRes60PreviewLine* out,void* context){
    (void)context; ++calls;
    if(i>=513U || (int)i==corrupt || out==NULL)return 0;
    *out=lines[i];return 1;
}
static void fixture(uint32_t n) {
    lines[0]=(EspRes60PreviewLine){0,0,2048,0,10,0};
    lines[1]=(EspRes60PreviewLine){2048,0,2048,2048,11,0};
    lines[2]=(EspRes60PreviewLine){2048,2048,0,2048,12,0};
    lines[3]=(EspRes60PreviewLine){0,2048,0,0,13,0};
    for(uint32_t i=4;i<n;++i){
       uint32_t flags=(i%19U==0U)?ESP_RES60_PREVIEW_SPRITE_SPAN:
          (i%17U==0U)?ESP_RES60_PREVIEW_OCCLUDER_ONLY:
          ((i%7U==0U)?0x00000100UL:0U);
       lines[i]=(EspRes60PreviewLine){coord(),coord(),coord(),coord(),
               (uint16_t)(random32()%256U),flags};
       if(i%13U==0U){lines[i].x2=lines[i].x1;lines[i].y2=lines[i].y1;}
       if(i%23U==0U)lines[i].x2=lines[i].x1;
    }
}
static void plan(uint16_t* out, uint8_t* present) {
    for(unsigned i=0;i<ESP_RES60_WIDTH;++i) {
       uint16_t tex=0U;
       present[i]=(uint8_t)EspRes60WallMaterial_visibleTextureAt(i,&tex);
       out[i]=present[i]?tex:0U;
    }
}
static void compare(const EspRes60PreviewCamera* cam, uint32_t n,
                    uint32_t* raysAvoided) {
    static uint16_t a[192U*144U+2U], b[192U*144U+2U];
    static uint16_t rawBefore[192U*144U+2U];
    static uint8_t packed[2048U];
    static uint16_t palette[16U];
    for(unsigned i=0;i<2048U;++i)packed[i]=(uint8_t)(i*19U+7U);
    for(unsigned i=0;i<16U;++i)palette[i]=(uint16_t)(i*0x1111U);
    static uint16_t planA[192],planB[192];
    static uint8_t visA[192],visB[192];
    EspRes60PreviewStats x,y;
    for(unsigned i=0;i<192U*144U+2U;++i)a[i]=b[i]=0x7171U;
    a[0]=b[0]=0xaaaaU;
    a[192U*144U+1U]=b[192U*144U+1U]=0xbbbbU;
    calls=0;
    int ok0=EspRes60Preview_renderBaseline(a+1,55296U,cam,n,readLine,NULL,&x);
    assert(ok0==1 && x.readerCalls==(uint32_t)calls && x.rayTests>0U);
    plan(planA,visA);
    memcpy(rawBefore,a,sizeof(a));
    EspRes60WallMaterialStats overlayA;
    assert(EspRes60WallMaterial_overlay(a+1,55296U,packed,2048U,
                                       palette,lines[0].texture,&overlayA));
    calls=0;
    int ok1=EspRes60Preview_render(b+1,55296U,cam,n,readLine,NULL,&y);
    assert(ok1==ok0 && y.readerCalls==(uint32_t)calls);
    plan(planB,visB);
    assert(memcmp(rawBefore,b,sizeof(a))==0); /* all raw pixels and canaries */
    EspRes60WallMaterialStats overlayB;
    assert(EspRes60WallMaterial_overlay(b+1,55296U,packed,2048U,
                                       palette,lines[0].texture,&overlayB));
    assert(memcmp(a,b,sizeof(a))==0); /* includes P10 sourceU, V, clipping */
    assert(memcmp(&overlayA,&overlayB,sizeof(overlayA))==0);
    assert(memcmp(planA,planB,sizeof(planA))==0);
    assert(memcmp(visA,visB,sizeof(visA))==0);
    assert(y.readerCalls==n && x.readerCalls==n);
    assert(x.rayTests==y.rayTests && y.rayTests==y.wedgeCandidates*192U);
    assert(x.wedgeCandidates==y.wedgeCandidates);
    assert(x.wedgeRejected==y.wedgeRejected);
    assert(x.intersections==y.intersections);
    assert(x.skippedSpecialLines==y.skippedSpecialLines);
    assert(x.preflightLines==y.preflightLines);
    assert(x.frameFNV==y.frameFNV);
    assert(x.centerDepth==y.centerDepth && x.centerLineIndex==y.centerLineIndex);
    assert(x.minDepth==y.minDepth && x.maxDepth==y.maxDepth);
    assert(x.fullHeightColumns==y.fullHeightColumns);
    assert(x.visibleColumns==y.visibleColumns && y.visibleColumns==192U);
    assert(x.wallPixels==y.wallPixels);
    assert(a[0]==0xaaaaU && b[0]==0xaaaaU);
    assert(a[192U*144U+1U]==0xbbbbU && b[192U*144U+1U]==0xbbbbU);
    *raysAvoided+=y.rayTests;
}
int main(void){
    EspRes60PreviewCamera cam={1024,1024,64,36,0x1111,0x2222};
    uint32_t raysAvoided=0,frames=0;
    for(unsigned n=0;n<64U;++n){
        uint32_t count=24U+(n%8U)*17U;
        fixture(count);
        cam.worldX=(uint16_t)(512U+random32()%1024U);
        cam.worldY=(uint16_t)(512U+random32()%1024U);
        for(unsigned dir=0;dir<4U;++dir) {
            cam.angle=(uint8_t)(dir*64U);
            compare(&cam,count,&raysAvoided);++frames;
        }
    }
    /* Includes the real MAP_INTRO line count, still <=512 bounded. */
    fixture(480U);
    for(unsigned dir=0;dir<4U;++dir) {
        cam.angle=(uint8_t)(dir*64U);
        compare(&cam,480U,&raysAvoided);++frames;
    }
    /* 512 candidates, identical-depth walls, original line 0 must win. */
    cam.worldX=1024U;cam.worldY=1024U;cam.angle=0U;
    for(unsigned i=0;i<512U;++i)
        lines[i]=(EspRes60PreviewLine){1536U,0U,1536U,2048U,
                                       (uint16_t)i,0U};
    compare(&cam,512U,&raysAvoided);++frames;
    fixture(24U);
    uint16_t fb[192U*144U+2U];EspRes60PreviewStats stats;
    for(unsigned i=0;i<192U*144U+2U;++i)fb[i]=0x3333U;
    corrupt=7;
    assert(!EspRes60Preview_render(fb+1,55296U,&cam,24U,readLine,NULL,&stats));
    corrupt=-1;
    for(unsigned i=0;i<192U*144U+2U;++i)assert(fb[i]==0x3333U);
    lines[9].x1=2049;
    assert(!EspRes60Preview_render(fb+1,55296U,&cam,24U,readLine,NULL,&stats));
    lines[9].x1=2048;
    assert(!EspRes60Preview_render(fb+1,55296U,&cam,513U,readLine,NULL,&stats));
    for(unsigned i=0;i<192U*144U+2U;++i)assert(fb[i]==0x3333U);
    assert(raysAvoided>10000U && frames==261U);
    printf("RES60-P15 real-C C11 PASS frames=%u original-rays=%lu avoided-line-decodes=%lu, 6144B BSS, 3072B saved vs P14, strict pixel/UV parity\\n",
           frames,(unsigned long)raysAvoided,(unsigned long)raysAvoided);
    return 0;
}
"""
with tempfile.TemporaryDirectory(prefix="res60-p15-") as d:
    d=Path(d)
    source=d/"test.c";source.write_text(harness)
    projector=str(ROOT/"ESP32/src/esp_res60_world_preview.c")
    material=str(ROOT/"ESP32/src/esp_res60_wall_material.c")
    old=d/"old.o";new=d/"new.o";mat=d/"material.o";exe=d/"test"
    subprocess.run(CC+FLAGS+["-DDOOMRPG_RES60_P15_COMPACT_SNAPSHOT=0",
       "-DEspRes60Preview_render=EspRes60Preview_renderBaseline",
       "-c",projector,"-o",str(old)],check=True)
    subprocess.run(CC+FLAGS+["-DDOOMRPG_RES60_P15_COMPACT_SNAPSHOT=1",
       "-c",projector,"-o",str(new)],check=True)
    subprocess.run(CC+FLAGS+["-DDOOMRPG_RES60_P15_COMPACT_SNAPSHOT=1",
       "-c",material,"-o",str(mat)],check=True)
    subprocess.run(CC+FLAGS+[str(source),str(old),str(new),str(mat),
       "-o",str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
    old_symbols=subprocess.check_output(["nm","-S",str(old)],text=True)
    new_symbols=subprocess.check_output(["nm","-S",str(new)],text=True)
    import re
    def sym(symbols,name):
        sizes=[]
        for line in symbols.splitlines():
            m=re.search(r"\b[0-9a-fA-F]+\s+([0-9a-fA-F]+)\s+[bBdD]\s+"+name+r"$",line)
            if m:sizes.append(int(m.group(1),16))
        assert len(sizes)==1,(name,sizes)
        return sizes[0]
    assert sym(old_symbols,"candidateLines")==8192
    assert sym(old_symbols,"wedgeIndices")==1024
    assert sym(new_symbols,"candidateLines")==6144
    assert not re.search(r"\bwedgeIndices$",new_symbols,re.M)
    prod=d/"prod.o"
    subprocess.run(CC+["-DDOOMRPG_RES60_P15_COMPACT_SNAPSHOT=0",
       "-DDOOMRPG_RES60_P3_WORLD=0","-c",projector,
       "-o",str(prod)],check=True)
    assert "EspRes60Preview_render" not in subprocess.check_output(["nm","-g",str(prod)],text=True)
    bad=subprocess.run(CC+["-DDOOMRPG_RES60_P15_COMPACT_SNAPSHOT=1",
       "-fsyntax-only",projector],capture_output=True,text=True)
    assert bad.returncode!=0 and "requires isolated P14/P13/P9" in bad.stderr
print("RES60-P15 HOST PASS: actual P14 snapshot vs P15 compact, exact FNV/material, 261 frames, fail closed")
