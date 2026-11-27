#!/usr/bin/env python3
"""Compile the actual native plane renderer with a bounded synthetic PAK.
Both resolutions use the same sampler. No desktop renderer or real SD needed.
"""
import configparser
from pathlib import Path
import subprocess
import tempfile

ESP = Path(__file__).resolve().parents[1]
cfg = configparser.ConfigParser(interpolation=None, inline_comment_prefixes=(";",))
assert cfg.read(ESP / "platformio.ini")
flags = ["-D"+x for x in cfg["env:esp32-cyd-res60"]["build_flags"].split()
         if x.startswith("DOOMRPG_RES60_")]
assert "-DDOOMRPG_RES60_P25_TEXTURED_PLANES=1" in flags
assert "DOOMRPG_RES60_P25_TEXTURED_PLANES" not in cfg["env:esp32-cyd"]["build_flags"]
probe = (ESP / "src/esp_res60_world_probe.c").read_text()
preview = (ESP / "src/esp_res60_world_preview.c").read_text()
assert "EspNativePlaneRenderer_renderRes60(" in probe
assert "EspRes60Preview_renderWithPlanes(" in probe
assert "if (wall || !texturedBackground)" in preview
assert "rowsRendered != 96U" in probe and "pixelsRendered != 18432U" in probe
assert probe.index("EspNativePlaneRenderer_renderRes60(") < probe.index("EspAssetPack_close();", probe.index("EspNativePlaneRenderer_renderRes60(")) < probe.index("EspRes60Preview_renderWithPlanes(")

HARNESS = r"""
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "Render.h"
#include "esp_asset_pack.h"
#include "esp_map_runtime.h"
#include "esp_native_plane_renderer.h"

static uint8_t maps[114], palettes[292], wall[18436], cells[2048];
static unsigned mapSize,paletteSize,wallSize;
static EspMapRuntimeView runtime;
static int openPak = 1;
static unsigned metadataReads;
#if DOOMRPG_RES60_P28_DESCRIPTOR_LEASE
static unsigned paletteCRC=1;
static int expectWarm;
#endif
static uint16_t fb[192*144+2];
#if DOOMRPG_RES60_P29_BORROW_PLANE_SCRATCH
static uint8_t loan[6144+2];
#endif

static void le32(uint8_t* p, uint32_t v) {
    for (unsigned i=0;i<4;i++) p[i]=(uint8_t)(v>>(i*8));
}
static void le16(uint8_t* p, uint16_t v) {
    p[0]=(uint8_t)v; p[1]=(uint8_t)(v>>8);
}
static uint16_t paletteSource(unsigned id,unsigned n) {
    return (uint16_t)(id*0x1000U+n*0x21U);
}
static unsigned texel(unsigned id,unsigned x,unsigned y) {
    return (x+3U*y+5U*id)&15U;
}
static void fixture(unsigned count) {
    assert(count==2||count==9);
    memset(maps,0,sizeof(maps));memset(palettes,0,sizeof(palettes));
    memset(wall,0,sizeof(wall));memset(cells,0,sizeof(cells));
    mapSize=16+count*8+8+count*2;
    paletteSize=4+count*32;
    wallSize=4+count*2048;
    le32(maps,count);le32(maps+4,1);le32(maps+8,count);
    le32(palettes,count*32);le32(wall,count*2048);
    for(unsigned id=0;id<count;id++){
        le32(maps+16+id*8,id*4096);le32(maps+20+id*8,id*16);
        le16(maps+16+count*8+8+id*2,id);
        for(unsigned p=0;p<16;p++) le16(palettes+4+2*(id*16+p),paletteSource(id,p));
        for(unsigned y=0;y<64;y++) for(unsigned x=0;x<64;x+=2)
            wall[4+id*2048+y*32+x/2]=(uint8_t)(texel(id,x,y)|(texel(id,x+1,y)<<4));
    }
    if(count==2)memset(cells+1024,1,1024);
    else for(unsigned i=0;i<2048;i++)cells[i]=(uint8_t)(((i&31U)+3U*((i>>5)&31U)+i/1024U)%count);
    runtime.planeMap=cells;runtime.planeMapBytes=sizeof(cells);
    runtime.arenaFNV1a=0x12345678+count;runtime.sourceCrc32=0x87654321+count;
}
int EspAssetPack_isOpen(void){return openPak;}
int EspMapRuntime_isLoaded(void){return 1;}
const EspMapRuntimeView* EspMapRuntime_view(void){return &runtime;}
int EspAssetPack_findEntry(const char* name,EspAssetPackEntry* e){
    memset(e,0,sizeof(*e));
    if(!strcmp(name,"mappings.bin")){e->offset=1;e->size=mapSize;}
    else if(!strcmp(name,"palettes.bin")){e->offset=2;e->size=paletteSize;
#if DOOMRPG_RES60_P28_DESCRIPTOR_LEASE
        e->crc32=paletteCRC;
#endif
    }
    else if(!strcmp(name,"wtexels.bin")){e->offset=3;e->size=wallSize;}
    else return 0;
    return openPak;
}
int EspAssetPack_readRange(const EspAssetPackEntry* e,uint32_t off,void* dst,size_t n){
    const uint8_t* src=e->offset==1?maps:e->offset==2?palettes:wall;
    if(!openPak||off>e->size||n>e->size-off)return 0;
    if(e->offset==1||e->offset==2)++metadataReads;
    memcpy(dst,src+off,n);return 1;
}
static uint16_t expected(int w,int height,int x,int y,int angle,int col,int row){
    const int ceiling=row<height/2;
    int d=2*row-height;
    if(d<0)d=-d;
    const int projected=w*(ceiling?28:36)*8/(d+1);
    const int step=projected*64/w;
    const int c=angle==0?1:angle==128?-1:0;
    const int s=angle==64?1:angle==192?-1:0;
    const int64_t lateral=(int64_t)step*128*(2*col+1-w);
    const uint32_t u=(uint32_t)((int64_t)x*65536+c*(int64_t)projected*8192+s*lateral);
    const uint32_t v=(uint32_t)((int64_t)y*65536-s*(int64_t)projected*8192+c*lateral);
    const unsigned id=cells[(unsigned)ceiling*1024U+((v>>22)&31U)*32U+((u>>22)&31U)];
    const uint16_t p=paletteSource(id,texel(id,(u>>16)&63U,(v>>16)&63U));
    return (uint16_t)(((p&31U)<<11)|(p&0x7e0U)|(p>>11));
}
static void fill(void){for(size_t i=0;i<sizeof(fb)/sizeof(fb[0]);i++)fb[i]=0xdead;}
static void trial(int w,int h,int hud,int angle){
    const int x=544,y=1840;
    fill();metadataReads=0;
    if(w==160){
        Render_t r={0};
        r.framebuffer=(uint8_t*)(fb+1);r.pixels=(short*)(fb+1+w*hud);
        r.pitch=w*2;r.screenWidth=r.screenRight=w;
        r.screenHeight=r.screenBottom=h-2*hud;
        r.viewX=x;r.viewY=y;r.viewZ=36;
        r.viewCos=angle==0?65536:angle==128?-65536:0;
        r.viewSin=angle==64?65536:angle==192?-65536:0;
        assert(EspNativePlaneRenderer_render(&r));
    }
#if DOOMRPG_RES60_P25_TEXTURED_PLANES
    else {
#if DOOMRPG_RES60_P29_BORROW_PLANE_SCRATCH
        memset(loan,0xa5,sizeof(loan));
        assert(EspNativePlaneRenderer_renderRes60WithScratch(fb+1,(size_t)w*h*2,x,y,angle,36,loan+1,6144));
        assert(loan[0]==0xa5&&loan[6145]==0xa5);
#else
        assert(EspNativePlaneRenderer_renderRes60(fb+1,(size_t)w*h*2,x,y,angle,36));
#endif
    }
#endif
    const EspNativePlaneRenderStats* st=EspNativePlaneRenderer_view();
    assert(st&&st->rowsRendered==(unsigned)(h-2*hud)&&st->pixelsRendered==(unsigned)(w*(h-2*hud)));
    assert(st->uniqueLogicalTextures==2&&st->cacheMisses==2&&st->texelReadBytes==4096);
#if DOOMRPG_RES60_P28_DESCRIPTOR_LEASE
    assert(metadataReads==(w==192?(expectWarm?0U:5U):8U));
    if(w==192)expectWarm=1;
#elif DOOMRPG_RES60_P27_METADATA_WINDOWS
    assert(metadataReads==(w==192?5U:8U));
#else
    assert(metadataReads==8U);
#endif
    assert(st->cacheHits+st->cacheMisses==st->pixelsRendered);
    for(int row=0;row<h;row++)for(int col=0;col<w;col++){
        uint16_t want=row<hud||row>=h-hud?0xdead:expected(w,h-2*hud,x,y,angle,col,row-hud);
        assert(fb[1+row*w+col]==want);
    }
    assert(fb[0]==0xdead&&fb[1+w*h]==0xdead);
}
int main(void){
    fixture(2);
    for(int angle=0;angle<256;angle+=64){
        trial(160,120,20,angle);
#if DOOMRPG_RES60_P25_TEXTURED_PLANES
        trial(192,144,24,angle);
#endif
    }
#if DOOMRPG_RES60_P28_DESCRIPTOR_LEASE
    /* Changed PAK identity rejects BEFORE any pixels, discards old lease.
     * A new explicit session resolves cold, then hits again. */
    fill(); ++paletteCRC;
    assert(!EspNativePlaneRenderer_renderRes60(fb+1,55296,544,1840,64,36));
    assert(!EspNativePlaneRenderer_view());
    for(size_t i=0;i<sizeof(fb)/sizeof(fb[0]);i++)assert(fb[i]==0xdead);
    EspNativePlaneRenderer_resetRes60Metadata();
    expectWarm=0;trial(192,144,24,64);trial(192,144,24,128);
    EspNativePlaneRenderer_resetRes60Metadata();
    expectWarm=0;trial(192,144,24,192);
    EspNativePlaneRenderer_resetRes60Metadata();
#endif
#if DOOMRPG_RES60_P29_BORROW_PLANE_SCRATCH
    /* Nine actual materials exercise all three borrowed slots, not just
     * their admission. Exact sampler parity and ownership canaries. */
    fixture(9);
    for(int angle=0;angle<256;angle+=64){
        fill();memset(loan,0xa5,sizeof(loan));
        assert(EspNativePlaneRenderer_renderRes60WithScratch(fb+1,55296,544,1840,angle,36,loan+1,6144));
        const EspNativePlaneRenderStats* st=EspNativePlaneRenderer_view();
        assert(st&&st->uniqueLogicalTextures==9&&st->cacheMisses==9&&st->texelReadBytes==18432);
        assert(st->cacheEvictions==0&&st->cacheHits+st->cacheMisses==18432);
        for(unsigned slot=0;slot<3;slot++){
            unsigned changed=0;
            for(unsigned i=0;i<2048;i++)changed+=(loan[1+slot*2048+i]!=0xa5);
            assert(changed>0);
        }
        assert(loan[0]==0xa5&&loan[6145]==0xa5&&fb[0]==0xdead&&fb[192*144+1]==0xdead);
        for(int row=0;row<144;row++)for(int col=0;col<192;col++){
            uint16_t want=row<24||row>=120?0xdead:expected(192,96,544,1840,angle,col,row-24);
            assert(fb[1+row*192+col]==want);
        }
    }
    EspNativePlaneRenderer_resetRes60Metadata();
    fixture(2);
#endif
#if DOOMRPG_RES60_P25_TEXTURED_PLANES
    fill();
#if DOOMRPG_RES60_P29_BORROW_PLANE_SCRATCH
    assert(!EspNativePlaneRenderer_renderRes60WithScratch(fb+1,55296,544,1840,64,36,loan+1,6143));
    assert(!EspNativePlaneRenderer_renderRes60WithScratch(fb+1,55296,544,1840,64,36,fb+2,6144));
#endif
    assert(!EspNativePlaneRenderer_renderRes60(fb+1,55294,544,1840,64,36));
    assert(!EspNativePlaneRenderer_renderRes60(fb+1,55296,544,1840,1,36));
    openPak=0;
    assert(!EspNativePlaneRenderer_renderRes60(fb+1,55296,544,1840,64,36));
    assert(!EspNativePlaneRenderer_view());
    for(size_t i=0;i<sizeof(fb)/sizeof(fb[0]);i++)assert(fb[i]==0xdead);
#endif
    puts("RES60-P25 plane pixel oracle PASS");
    return 0;
}
"""
with tempfile.TemporaryDirectory(prefix="res60-p25-") as tmp:
    p=Path(tmp)
    (p/"DoomRPG.h").write_text("/* no desktop owner needed */\n")
    (p/"Render.h").write_text("""
#include <stdint.h>
typedef struct Render_s {
    int screenWidth,screenHeight,screenLeft,screenTop,screenRight,screenBottom;
    int viewX,viewY,viewZ,viewSin,viewCos,pitch;
    uint8_t* framebuffer;
    short* pixels;
} Render_t;
""")
    (p/"test.c").write_text(HARNESS)
    for name,defs in (("classic",[]),("res60",flags)):
        exe=p/name
        subprocess.run(["cc","-std=c11","-O2","-Wall","-Wextra","-Werror",
                        "-DDOOMRPG_ESP32=1","-I",tmp,"-I",str(ESP/"include"),
                        *defs,str(p/"test.c"),str(ESP/"src/esp_native_plane_renderer.c"),
                        "-o",str(exe)],check=True)
        subprocess.run([str(exe)],check=True)
print("RES60-P25 HOST PASS: native 4bpp samples, 160/192 cardinal frames, HUD and canaries")
