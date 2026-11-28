#!/usr/bin/env python3
"""RES60-P9: real C11 cardinal wedge oracle against exact pre-P9 P3 raster.

Two separately compiled owners use the very same esp_res60_world_preview.c.
The legacy version has DOOMRPG_RES60_P9_WEDGE=0; the indexed one has =1.
Every accepted framebuffer, source index and original FNV must be identical.
"""
from pathlib import Path
import configparser
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
ini = configparser.ConfigParser(interpolation=None, inline_comment_prefixes=(";",))
assert ini.read(ROOT / "ESP32/platformio.ini")
flags = ini["env:esp32-cyd-res60"]["build_flags"]
assert "-D DOOMRPG_RES60_P9_WEDGE=1" in flags
assert ini["env:esp32-cyd-res60-inverted"]["extends"]=="env:esp32-cyd-res60"
for env in ("env:esp32-cyd","env:esp-cyd-inverted"):
    assert "RES60_P9" not in ini[env]["build_flags"]
src = (ROOT / "ESP32/src/esp_res60_world_preview.c").read_text()
config = (ROOT / "ESP32/include/platform_video_config.h").read_text()
assert "insideCardinalWedge" in src and "wedgeIndices[RES60_P9_MAX_LINES]" in src
assert "RES60_P9_MAX_LINES 512U" in src
assert "EspRes60Layout_fnv(framebuffer, framebufferBytes)" in src
assert "lineIndex=wedgeIndices[index]" in src
assert "RES60-P9 requires isolated P7/P8 cardinal BSP probe" in config
assert "[RES60P9] WEDGE lines=" in (ROOT/"ESP32/src/platform_video.cpp").read_text()

harness = r"""
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "esp_res60_world_preview.h"
extern int EspRes60Preview_renderBaseline(uint16_t*,size_t,
                 const EspRes60PreviewCamera*,uint32_t,
                 EspRes60PreviewReadLine,void*,EspRes60PreviewStats*);
static EspRes60PreviewLine lines[513];
static int broken=-1;
static int readLine(uint32_t idx,EspRes60PreviewLine* out,void* ctx) {
    (void)ctx;
    if(idx>=513 || !out || (int)idx==broken)return 0;
    *out=lines[idx];return 1;
}
static uint32_t seed=0x429bdde2U;
static uint32_t rand32(void) {
    seed^=seed<<13; seed^=seed>>17; seed^=seed<<5;return seed;
}
static uint16_t bounded(uint32_t span) {return (uint16_t)(rand32()%span);}
static void makeFixture(unsigned count) {
    /* Four outer boundaries guarantee 192 real ray hits regardless of angle.
       Other lines may be oblique, degenerate, special or on wedge boundaries. */
    lines[0]=(EspRes60PreviewLine){0,0,2048,0,10,0};
    lines[1]=(EspRes60PreviewLine){2048,0,2048,2048,11,0};
    lines[2]=(EspRes60PreviewLine){2048,2048,0,2048,12,0};
    lines[3]=(EspRes60PreviewLine){0,2048,0,0,13,0};
    for(unsigned i=4;i<count;++i) {
        uint32_t flags=((rand32()&15U)==1U)?0x00000002UL:
                    ((rand32()&15U)==2U)?0x20000000UL:0U;
        lines[i]=(EspRes60PreviewLine){
          bounded(2049),bounded(2049),bounded(2049),bounded(2049),
          bounded(256),flags};
        if(i%13==0) {lines[i].x2=lines[i].x1;lines[i].y2=lines[i].y1;}
        if(i%17==0) lines[i].x1=lines[i].x2;
    }
}
static void sameAccepted(const EspRes60PreviewCamera* camera,
                         uint32_t count,unsigned* culledWitness) {
    static uint16_t a[192U*144U+2U], b[192U*144U+2U];
    EspRes60PreviewStats x,y;
    a[0]=b[0]=0x5a5a;a[192U*144U+1U]=b[192U*144U+1U]=0x5b5b;
    for(unsigned i=1;i<=192U*144U;++i) a[i]=b[i]=0xdead;
    int ok0=EspRes60Preview_renderBaseline(a+1,55296,camera,count,readLine,NULL,&x);
    int ok1=EspRes60Preview_render(b+1,55296,camera,count,readLine,NULL,&y);
    assert(ok0==ok1 && ok0==1);
    assert(memcmp(a,b,sizeof(a))==0); /* full 55,296B + both canaries */
    assert(x.frameFNV==y.frameFNV && x.frameFNV!=0);
    assert(x.centerDepth==y.centerDepth && x.centerLineIndex==y.centerLineIndex);
    assert(x.intersections==y.intersections);
    assert(x.visibleColumns==y.visibleColumns && x.visibleColumns==192);
    assert(x.minDepth==y.minDepth && x.maxDepth==y.maxDepth);
    assert(x.fullHeightColumns==y.fullHeightColumns && x.wallPixels==y.wallPixels);
    assert(x.rayTests==count*192U && y.rayTests==y.wedgeCandidates*192U);
    assert(y.wedgeCandidates+y.wedgeRejected==count);
    assert(y.readerCalls==count+y.rayTests);
    assert(y.preflightLines==count && y.rayTests<=x.rayTests);
    *culledWitness+=y.wedgeRejected;
}
int main(void) {
    EspRes60PreviewCamera cam={1024,1024,64,36,0x1111,0x2222};
    unsigned culled=0;
    for(unsigned n=0;n<64;++n) {
        uint32_t count=24+(n%8)*17;
        makeFixture(count);
        cam.worldX=(uint16_t)(512+bounded(1024));
        cam.worldY=(uint16_t)(512+bounded(1024));
        for(unsigned dir=0;dir<4;++dir) {
            cam.angle=(uint8_t)(dir*64);
            sameAccepted(&cam,count,&culled);
        }
    }
    assert(culled>100); /* actually reject behind-camera source lines */
    makeFixture(24);
    static uint16_t f[192U*144U+2U];
    EspRes60PreviewStats st;
    for(unsigned i=0;i<192U*144U+2U;++i)f[i]=0x3030;
    broken=4;
    assert(!EspRes60Preview_render(f+1,55296,&cam,24,readLine,NULL,&st));
    broken=-1;
    for(unsigned i=0;i<192U*144U+2U;++i)assert(f[i]==0x3030);
    lines[5].x1=2049;
    assert(!EspRes60Preview_render(f+1,55296,&cam,24,readLine,NULL,&st));
    lines[5].x1=0;
    assert(!EspRes60Preview_render(f+1,55296,&cam,513,readLine,NULL,&st));
    for(unsigned i=0;i<192U*144U+2U;++i)assert(f[i]==0x3030);
    printf("RES60-P9 PASS C11 wedge vs exact P3 oracle: 256 cardinal frames, full pixels/FNV, center-line, adversarial geometry, preflight canaries, rejected=%u\n",culled);
    return 0;
}
"""
with tempfile.TemporaryDirectory(prefix="res60-p9-") as d:
    p=Path(d); source=p/"wedge.c"; exe=p/"wedge"; source.write_text(harness)
    defines=[
        "-DDOOMRPG_RES60_P1_DIAGNOSTIC=1",
        "-DDOOMRPG_RES60_P2_LAYOUT=1",
        "-DDOOMRPG_RES60_P3_WORLD=1",
        "-DDOOMRPG_RES60_P5_TITLE=1",
        "-DDOOMRPG_RES60_P6_MENU=1",
        "-DDOOMRPG_RES60_P7_NAV=1",
        "-DDOOMRPG_RES60_P8_TEXEL=1"]
    base=["cc","-std=c11","-O2","-Wall","-Wextra","-Werror",
          "-I",str(ROOT/"ESP32/include")]
    projector=str(ROOT/"ESP32/src/esp_res60_world_preview.c")
    subprocess.run(base+defines+["-DDOOMRPG_RES60_P9_WEDGE=0",
        "-DEspRes60Preview_render=EspRes60Preview_renderBaseline",
        "-c",projector,"-o",str(p/"baseline.o")],check=True)
    subprocess.run(base+defines+["-DDOOMRPG_RES60_P9_WEDGE=1",
        "-c",projector,"-o",str(p/"wedge.o")],check=True)
    subprocess.run(base+defines+[str(source),str(p/"baseline.o"),
        str(p/"wedge.o"),"-o",str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
    normal=p/"normal.o"
    subprocess.run(base+["-DDOOMRPG_RES60_P9_WEDGE=0",
        "-DDOOMRPG_RES60_P3_WORLD=0","-c",projector,
        "-o",str(normal)],check=True)
    assert "EspRes60Preview_render" not in subprocess.check_output(
        ["nm","-g",str(normal)],text=True)
    wrong=subprocess.run(base+["-DDOOMRPG_RES60_P9_WEDGE=1",
        "-fsyntax-only",projector],text=True,capture_output=True)
    assert wrong.returncode!=0 and "requires isolated P7/P8" in wrong.stderr
print("RES60-P9 HOST PASS: conservative bounded wedge, exact P3 pixel/FNV oracle")
