#!/usr/bin/env python3
"""RES60-P10 isolated real C11 single-material projection / fail-closed guards."""
from pathlib import Path
import configparser
import subprocess
import tempfile

ROOT=Path(__file__).resolve().parents[2]
cfg=configparser.ConfigParser(interpolation=None,inline_comment_prefixes=(";",))
assert cfg.read(ROOT/"ESP32/platformio.ini")
flags=cfg["env:esp32-cyd-res60"]["build_flags"]
assert "-D DOOMRPG_RES60_P10_WALL_MATERIAL=1" in flags
assert cfg["env:esp32-cyd-res60-inverted"]["extends"]=="env:esp32-cyd-res60"
for name in ("env:esp32-cyd","env:esp-cyd-inverted"):
    assert "DOOMRPG_RES60_P10_WALL_MATERIAL" not in cfg[name]["build_flags"]
p3=(ROOT/"ESP32/src/esp_res60_world_preview.c").read_text()
tex=(ROOT/"ESP32/src/esp_res60_texture_probe.c").read_text()
assert "EspRes60WallMaterial_record(" in p3
assert "EspRes60WallMaterial_publish();" in p3
assert "EspRes60WallMaterial_overlay(" in tex
assert "EspRes60WallMaterial_visibleTextureAt(" in tex
assert "EspRes60WallMaterial_reset();" in tex
assert "rawP3FNV" in (ROOT/"ESP32/src/platform_video.cpp").read_text()
assert "[RES60P10] MATERIAL" in (ROOT/"ESP32/src/platform_video.cpp").read_text()
harness=r"""
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "esp_res60_world_preview.h"
#include "esp_res60_wall_material.h"
static EspRes60PreviewLine lines[4]={
 {960,960,1088,960,7,0},
 {1088,960,1088,1088,7,0},
 {1088,1088,960,1088,7,0},
 {960,1088,960,960,7,0}
};
static uint16_t buffer[192U*144U+2U], original[192U*144U];
static uint8_t packed[2048];
static uint16_t palette[16];
static int readLine(uint32_t n,EspRes60PreviewLine* line,void* ctx) {
 (void)ctx;
 if(n>=4 || !line)return 0;
 *line=lines[n];return 1;
}
int main(void) {
 EspRes60PreviewCamera cam={1024,1024,0,36,0x1111,0x2222};
 EspRes60PreviewStats stats;
 EspRes60WallMaterialStats mat;
 uint16_t* f=buffer+1;
 memset(packed,0x10,sizeof(packed));
 memset(palette,0,sizeof(palette));
 palette[0]=0x001f; /* original BGR565 red -> framebuffer 0xf800 */
 palette[1]=0xf800; /* original BGR565 blue -> framebuffer 0x001f */
 buffer[0]=0xface;buffer[192U*144U+1U]=0xbeef;
 for(unsigned turn=0;turn<4;++turn) {
   cam.angle=(uint8_t)(turn*64U);
   for(unsigned i=0;i<192U*144U;++i)f[i]=0x5a5a;
   assert(EspRes60Preview_render(f,55296,&cam,4,readLine,0,&stats));
   assert(stats.rayTests==stats.wedgeCandidates*192U && stats.wedgeCandidates>0U);
   uint16_t visible=0;
   assert(EspRes60WallMaterial_visibleTextureAt(96,&visible));
   assert(visible==7);
   assert(EspRes60WallMaterial_visibleColumns(7)>0);
   assert(EspRes60WallMaterial_visibleColumns(8)==0);
   EspRes60WallCoverage coverage;
   assert(EspRes60WallMaterial_measure(&coverage));
   assert(coverage.materials==1 && coverage.columns==stats.visibleColumns &&
          coverage.pixels==stats.wallPixels && coverage.unsupportedColumns==0);
   /* Pretend P8 already drew the real swatch: P10 must not clobber it. */
   for(unsigned y=24;y<72;++y)for(unsigned x=144;x<192;++x)
     f[y*192+x]=0x1234;
   memcpy(original,f,sizeof(original));
   assert(!EspRes60WallMaterial_overlay(f,55294,packed,sizeof(packed),
               palette,7,&mat));
   assert(!memcmp(original,f,sizeof(original)));
   assert(EspRes60WallMaterial_overlay(f,55296,packed,sizeof(packed),
               palette,7,&mat));
   assert(mat.columns>0 && mat.pixels>0 && mat.sampledFNV!=0);
   assert(mat.columns==EspRes60WallMaterial_visibleColumns(7));
   assert(mat.pixels+coverage.insetPixels==coverage.pixels);
   assert(buffer[0]==0xface && buffer[192U*144U+1U]==0xbeef);
   assert(f[0]==original[0] && f[143U*192U]==original[143U*192U]);
   for(unsigned y=24;y<72;++y)for(unsigned x=144;x<192;++x)
     assert(f[y*192+x]==0x1234);
   assert(memcmp(original,f,sizeof(original))!=0);
   /* Wrong resource MUST NOT draw anything or touch the P8 inset. */
   memcpy(f,original,sizeof(original));
   assert(EspRes60WallMaterial_overlay(f,55296,packed,sizeof(packed),
               palette,8,&mat));
   assert(mat.columns==0 && mat.pixels==0);
   assert(!memcmp(original,f,sizeof(original)));
 }
 EspRes60WallMaterial_reset();
 assert(!EspRes60WallMaterial_overlay(f,55296,packed,sizeof(packed),
         palette,7,&mat));
 memcpy(original,f,sizeof(original));
 lines[1].x1=2049;
 assert(!EspRes60Preview_render(f,55296,&cam,4,readLine,0,&stats));
 assert(!memcmp(original,f,sizeof(original)));
 assert(!EspRes60WallMaterial_overlay(f,55296,packed,sizeof(packed),
         palette,7,&mat));
 assert(!memcmp(original,f,sizeof(original)));
 lines[1].x1=1088;
 EspRes60WallCoverage demand;
 assert(!EspRes60WallMaterial_measure(&demand));
 EspRes60WallMaterial_reset();
 assert(EspRes60WallMaterial_record(0,7,-5,10,0));
 assert(EspRes60WallMaterial_record(1,7,95,5,0));
 assert(EspRes60WallMaterial_record(2,8,96,10,0)); /* fully clipped */
 assert(EspRes60WallMaterial_record(144,8,40,20,0));
 assert(EspRes60WallMaterial_record(145,256,-10,100,0));
 EspRes60WallMaterial_publish();
 assert(EspRes60WallMaterial_measure(&demand));
 assert(demand.materials==2 && demand.columns==4 &&
        demand.unsupportedColumns==1 && demand.pixels==116 &&
        demand.insetPixels==56);
 printf("RES60-P10 PASS: original ray geometry + 4 cardinal material projections, one 2KiB 4bpp tile, HUD/swatch/canaries, rollback/fail-closed\n");
 return 0;
}
"""
with tempfile.TemporaryDirectory(prefix="res60-p10-") as d:
    t=Path(d);source=t/"host.c";exe=t/"host";source.write_text(harness)
    defines=[
        "-DDOOMRPG_RES60_P1_DIAGNOSTIC=1",
        "-DDOOMRPG_RES60_P2_LAYOUT=1",
        "-DDOOMRPG_RES60_P3_WORLD=1",
        "-DDOOMRPG_RES60_P5_TITLE=1",
        "-DDOOMRPG_RES60_P6_MENU=1",
        "-DDOOMRPG_RES60_P7_NAV=1",
        "-DDOOMRPG_RES60_P8_TEXEL=1",
        "-DDOOMRPG_RES60_P9_WEDGE=1",
        "-DDOOMRPG_RES60_P10_WALL_MATERIAL=1"]
    common=["cc","-std=c11","-O2","-Wall","-Wextra","-Werror",
             "-I",str(ROOT/"ESP32/include")]
    preview=ROOT/"ESP32/src/esp_res60_world_preview.c"
    mat=ROOT/"ESP32/src/esp_res60_wall_material.c"
    subprocess.run(common+defines+[str(preview),str(mat),str(source),
                                   "-o",str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
    obj=t/"off.o"
    subprocess.run(common+["-DDOOMRPG_RES60_P10_WALL_MATERIAL=0",
       "-c",str(mat),"-o",str(obj)],check=True)
    assert "EspRes60WallMaterial_overlay" not in subprocess.check_output(
        ["nm","-g",str(obj)],text=True)
    invalid=subprocess.run(common+[
       "-DDOOMRPG_RES60_P10_WALL_MATERIAL=1","-fsyntax-only",str(mat)],
       capture_output=True,text=True)
    assert invalid.returncode!=0 and "requires isolated P8/P9" in invalid.stderr
print("RES60-P10 HOST PASS: single source 4bpp material perspective UV, isolation and canaries")
