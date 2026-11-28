#!/usr/bin/env python3
"""FIRE-VIS-P1: execute real C bitshape reader and sprite span raster.

The production functions are extracted unchanged from the native renderer.
The tiny in-memory PAK fixture is SYNTHETIC, not evidence that original fire
frames are distinct. Real assets require a separate PAK inventory.
"""
from pathlib import Path
import os
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
src = (ROOT / "ESP32/src/esp_native_sprite_renderer.c").read_text()

def extract(signature):
    assert src.count(signature) == 1, signature
    start = src.index(signature)
    opening = src.index("{", start + len(signature))
    depth = 0
    for pos in range(opening, len(src)):
        if src[pos] == "{":
            depth += 1
        elif src[pos] == "}":
            depth -= 1
            if depth == 0:
                return src[start:pos + 1]
    raise AssertionError("unclosed: " + signature)

# Enforce the actual call sites as well as testing the compiled helper.
order = src[src.index("static int buildOrder("):src.index("static int spans(")]
draw = extract("static int drawAt(Render_t* render,")
assert "EspNativeFireVisual_mapMode(" in order
assert "spriteRenderMode(resourceLogical), type, info)" in order
assert "EspNativeFireVisual_mapMode(" in draw
assert "renderMode, entityType, parent->info)" in draw
assert "if (!glow)" in draw
assert "if (!EspMapSpriteTopology_getEntity(" in draw
assert "return 0;" in draw
assert "glowLogical = glowFor(parent->logical);" in src
assert "renderMode == RENDER_MODE_NORMAL" in extract("static int spans(")

location_start = src.index("typedef struct FrameLocation_s {")
location_end = src.index("} FrameLocation;", location_start) + len("} FrameLocation;")
functions = src[location_start:location_end] + "\n\n" + "\n\n".join(extract(s) for s in (
    "static uint16_t le16(",
    "static uint32_t le32(",
    "static uint16_t source565(",
    "static int readRange(",
    "static int loadFrameBounds(",
    "static int loadFramePayload(",
    "static int loadFrame(",
    "static unsigned fullyOccludedColumns(",
    "static int spans(",
))
# Execute the previous raster loop as a separate actual-C reference.
span = extract("static int spans(")
# Reconstruct P2h, then its original per-pixel reference. Keep both
# full raster bodies in the compiled parity fixture.
from perf_game_p2a_history import P2I_WIRING_EDITS
for newer, older in P2I_WIRING_EDITS["esp_native_sprite_renderer.c"]:
    assert span.count(newer) == 1, "P2i reference anchor"
    span = span.replace(newer, older, 1)
p2h_span = span.replace("static int spans(", "static int spansP2h(", 1)
functions += "\n\n" + p2h_span
fast_start = span.index("                /* P2h: prove")
fast_end = span.index("                while (remaining-- > 0)", fast_start)
reference_span = (span[:fast_start] + span[fast_end:]).replace(
    "static int spans(", "static int spansReference(", 1)
functions += "\n\n" + reference_span
host = r'''
#include <assert.h>
#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "esp_native_fire_visual.h"
#include "esp_native_sprite_mask.h"
#define MAP_HEADER 16U
#define PALETTE_HEADER 4U
#define TEXEL_HEADER 4U
#define PAIR_BYTES 8U
#define SHAPE_HEADER 12U
#define MAX_DIM 64
#define MAX_MASK 512U
#define MAX_TEXELS 2048U
#define RENDER_MODE_NORMAL 0U
#define RENDER_MODE_ADD 7U
#define MAXINT INT_MAX
typedef struct { const uint8_t* data; uint32_t size; } EspAssetPackEntry;
typedef struct {
 EspAssetPackEntry mappings,palettes,bitshapes,wtexels,stexels;
 uint32_t texelPairs,bitShapePairs,textureIds,spriteIds;
 uint32_t spritePairBase,spriteIdBase,paletteEntries,wallBytes,spriteBytes;
} Sources;
typedef struct {
 uint16_t logical,actual;
 uint32_t texelOffset;
 int xMin,xMax,yMin,yMax,width,height,pitch;
 uint32_t maskBytes,active,packedBytes;
 uint16_t palette[16],prefix[MAX_DIM+1];
 uint8_t mask[MAX_MASK],texels[MAX_TEXELS];
} Frame;
typedef struct {
 uint32_t packReads,frameLoads,glowFrameLoads,glowFrameBytes;
 uint32_t glowMaxFrameBytes,frameBytes,maxFrameBytes,uniqueLogical;
 uint32_t mode7Pixels,pixelsDrawn,glowPixels,spanRuns,glowSpanRuns;
 uint32_t glowWallOccludedColumns,wallOccludedColumns;
} EspNativeSpriteStats;
typedef struct {int x,y,z;} Vertex_t;
typedef struct {Vertex_t vert1,vert2;} Line_t;
typedef struct {
 int screenLeft,screenTop,screenRight,screenBottom,screenHeight,screenWidth;
 int pitch,viewZ,columnScale[160];
 short *pixels;
} Render_t;
static uint32_t readCount,failReadAt;
static uint8_t mappingBytes[1024], paletteBytes[132], shapeBytes[80], spriteBytes[16];
static int EspAssetPack_readRange(const EspAssetPackEntry* entry,
                                 uint32_t at,void* out,uint32_t length) {
 ++readCount;
 if (readCount == failReadAt || !entry || !out ||
     at > entry->size || length > entry->size-at) return 0;
 memcpy(out,entry->data+at,length);
 return 1;
}
static const int* EspNativeGraphicsCatalog_findSprite(uint16_t logical) {
 static const int item=1;
 return logical==5U ? &item : NULL;
}
static int DoomRPG_FixedMul(int a,int b) {
 return (int)(((int64_t)a*(int64_t)b)>>16);
}
static int EspNativeRenderGeometry_halfScreenHeight(int height) {
 return height/2;
}
static void put16(uint8_t* data,uint32_t offset,uint16_t value) {
 data[offset]=(uint8_t)value;
 data[offset+1]=(uint8_t)(value>>8);
}
static void put32(uint8_t* data,uint32_t offset,uint32_t value) {
 put16(data,offset,(uint16_t)value);
 put16(data,offset+2,(uint16_t)(value>>16));
}
static void initFixture(Sources* s) {
 uint32_t phase;
 memset(s,0,sizeof *s);
 memset(mappingBytes,0,sizeof mappingBytes);
 memset(paletteBytes,0,sizeof paletteBytes);
 memset(shapeBytes,0,sizeof shapeBytes);
 memset(spriteBytes,0,sizeof spriteBytes);
 s->mappings=(EspAssetPackEntry){mappingBytes,sizeof mappingBytes};
 s->palettes=(EspAssetPackEntry){paletteBytes,sizeof paletteBytes};
 s->bitshapes=(EspAssetPackEntry){shapeBytes,sizeof shapeBytes};
 s->stexels=(EspAssetPackEntry){spriteBytes,sizeof spriteBytes};
 s->spriteIds=32U; s->bitShapePairs=64U; s->spriteIdBase=0U;
 s->spritePairBase=512U; s->paletteEntries=64U; s->wallBytes=0U;
 put16(mappingBytes,5U*2U,8U);
 for(phase=0;phase<4U;++phase) {
   const uint32_t pair=512U+(8U+phase)*8U;
   const uint32_t shape=4U+phase*16U;
   put32(mappingBytes,pair,phase*16U);
   put32(mappingBytes,pair+4U,phase*16U);
   put32(shapeBytes,shape,phase*2U);
   shapeBytes[shape+8]=0; shapeBytes[shape+9]=0;
   shapeBytes[shape+10]=0; shapeBytes[shape+11]=3;
   shapeBytes[shape+12]=0x05U;
   /* The decoded 4 phase inputs differ, but this is synthetic only. */
   put16(paletteBytes,4U+(phase*16U+1U)*2U,(uint16_t)(phase+1U));
   spriteBytes[4U+phase]=(uint8_t)(0x11U+phase);
 }
}
static void testAssetReader(void) {
 Sources src;
 Frame frame;
 EspNativeSpriteStats stats;
 uint32_t seen[8]={0};
 uint16_t actuals[4];
 uint8_t values[4];
 uint32_t p;
 initFixture(&src);
 memset(&stats,0,sizeof stats);
 failReadAt=0;readCount=0;
 for(p=0;p<4U;++p) {
  assert(loadFrame(&src,5U,p,0,&frame,seen,&stats));
  actuals[p]=frame.actual;
  values[p]=frame.texels[0];
  assert(frame.mask[0]==5U && frame.active==2U);
  assert(frame.palette[1]!=0U);
  if(p) assert(actuals[p]!=actuals[p-1] && values[p]!=values[p-1]);
 }
 assert(stats.frameLoads==4U && stats.packReads>=24U);
 /* No reuse of an old shape on a resource failure. */
 for(p=1;p<=6U;++p) {
   memset(&frame,0xa5,sizeof frame);
   memset(&stats,0,sizeof stats);
   readCount=0;failReadAt=p;
   assert(!loadFrame(&src,5U,0,0,&frame,seen,&stats));
 }
 failReadAt=0; readCount=0;
 assert(!loadFrame(&src,999U,0,0,&frame,seen,&stats));
 assert(!loadFrame(&src,6U,0,0,&frame,seen,&stats));
 /* Out of bounds shape offset is rejected by the real bounded reader. */
 put32(mappingBytes,512U+8U*8U,9000U);
 assert(!loadFrame(&src,5U,0,0,&frame,seen,&stats));
 puts("FIRE-VIS asset reader: four synthetic phases and fault injection PASS");
}
static void testModesAndRaster(void) {
 Frame f;
 EspNativeSpriteStats stats;
 Render_t r;
 Line_t line;
 uint16_t framebuffer[160U*80U];
 uint8_t mode;
 memset(&f,0,sizeof f);
 memset(&r,0,sizeof r);
 memset(&line,0,sizeof line);
 memset(&stats,0,sizeof stats);
 f.width=1;f.height=4;f.pitch=1;
 f.mask[0]=0x05U;f.prefix[0]=0;f.prefix[1]=2;
 f.packedBytes=1;f.texels[0]=0x21U;
 f.palette[1]=0xf800U;f.palette[2]=0x07e0U;
 line.vert1.x=0;line.vert2.x=1<<16;
 line.vert1.y=131072;line.vert2.y=131072;
 r.pitch=320;r.screenWidth; /* none: viewport is 160x80 */
 r.screenLeft=0;r.screenTop=0;r.screenRight=160;r.screenBottom=80;
 r.screenHeight=80;r.viewZ=64;r.pixels=(short*)framebuffer;
 r.columnScale[0]=32768;
 assert(EspNativeFireVisual_mapMode(0U,10U,91U)==7U);
 assert(EspNativeFireVisual_mapMode(0U,1U,91U)==0U);
 assert(EspNativeFireVisual_mapMode(7U,1U,136U)==7U);
 assert(EspNativeFireVisual_mapMode(0U,10U,0x0400005bUL)==0U);
 assert(EspNativeFireVisual_mapMode(0U,0xffU,91U)==0U);
 memset(framebuffer,0,sizeof framebuffer);
 framebuffer[40U*160U]=framebuffer[41U*160U]=
 framebuffer[42U*160U]=framebuffer[43U*160U]=0x001fU;
 mode=EspNativeFireVisual_mapMode(0U,10U,91U);
 assert(spans(&r,&line,&f,mode,0,&stats));
 assert(framebuffer[40U*160U]==0xf01eU);
 assert(framebuffer[41U*160U]==0x001fU);
 assert(framebuffer[42U*160U]==0x07deU);
 assert(framebuffer[43U*160U]==0x001fU);
 assert(stats.pixelsDrawn==2U && stats.mode7Pixels==2U);
 /* A non-fire reuses the old opaque mode: masked pixels still untouched. */
 memset(&stats,0,sizeof stats);
 assert(spans(&r,&line,&f,EspNativeFireVisual_mapMode(0,1,91),0,&stats));
 assert(framebuffer[40U*160U]==0xf800U &&
        framebuffer[42U*160U]==0x07e0U);
 assert(framebuffer[41U*160U]==0x001fU &&
        framebuffer[43U*160U]==0x001fU);
 assert(stats.mode7Pixels==0U);
 /* Glow remains its own explicit additive pass over a base pixel. */
 memset(&stats,0,sizeof stats);
 assert(spans(&r,&line,&f,7U,1,&stats));
 assert(stats.glowPixels==2U && stats.mode7Pixels==0U);
 assert(framebuffer[40U*160U]==0xe000U);
 /* The historical RGB565 carry approximation is NOT ideal clamp-to-white.
  * Check exact native bytes, including its overflow behavior. This P1 does
  * not alter the existing glow or other additive blend formula. */
 f.palette[1]=f.palette[2]=0xffffU;
 framebuffer[40U*160U]=framebuffer[42U*160U]=0xffffU;
 memset(&stats,0,sizeof stats);
 assert(spans(&r,&line,&f,7U,0,&stats));
 assert(framebuffer[40U*160U]==0xe79cU &&
        framebuffer[42U*160U]==0xe79cU);
 /* Wall depth rejects the entire column without changing any framebuffer. */
 r.columnScale[0]=0;
 memset(&stats,0,sizeof stats);
 assert(spans(&r,&line,&f,7U,0,&stats));
 assert(stats.wallOccludedColumns==1U && stats.pixelsDrawn==0U);
 assert(framebuffer[41U*160U]==0x001fU);
 r.columnScale[0]=32768;
 /* Bad resources and unsupported modes fail closed at the raster boundary. */
 f.packedBytes=0;
 memset(&stats,0,sizeof stats);
 assert(!spans(&r,&line,&f,7U,0,&stats));
 assert(!spans(&r,&line,&f,9U,0,&stats));
 puts("FIRE-VIS RGB565 real spans: fire/opaque/glow/mask/depth/legacy-carry PASS");
}
/* Same production C compositor, with and without the P2h fast branch.
 * Exercise opaque/add/glow, clipped spans, depth rejection, odd nibble
 * starts, partial padding and intentionally truncated resources. */
static void testSpanParity(void) {
 uint32_t rng=0x9ab81234U;
 static uint16_t pixels[3][160U*80U];
 for(unsigned trial=0;trial<4096U;++trial) {
   Frame f;
   Render_t r;
   Line_t line;
   EspNativeSpriteStats stats[3];
   memset(&f,0,sizeof f);memset(&r,0,sizeof r);
   memset(&line,0,sizeof line);memset(stats,0,sizeof stats);
   f.width=1+(int)(trial%8U);f.height=1+(int)(trial%64U);
   f.pitch=(f.height+7)/8;f.yMin=(int)(trial%7U);
   unsigned count=0;
   for(int col=0;col<f.width;++col) {
     for(int y=0;y<f.height;++y) {
       rng=rng*1664525U+1013904223U;
       if(rng&0x80000000U) {
         f.mask[col*f.pitch+y/8]|=(uint8_t)(1U<<(y&7));++count;
       }
     }
     f.prefix[col+1]=(uint16_t)count;
   }
   f.packedBytes=(count+1U)/2U;
   if(trial%7U==0U && f.packedBytes) --f.packedBytes;
   for(unsigned i=0;i<sizeof f.texels;++i) {rng=rng*1664525U+1013904223U;f.texels[i]=(uint8_t)(rng>>24);}
   for(unsigned i=0;i<16U;++i) f.palette[i]=(uint16_t)(i*4331U);
   line.vert1.x=-(int)(trial%3U)*65536;
   line.vert2.x=(int)(1U+trial%17U)*65536;
   line.vert1.y=line.vert2.y=(int)(65536U*(1U+trial%8U));
   if(trial%3U==1U) line.vert2.y+=32768;
   if(trial%3U==2U) line.vert2.y-=32768;
   line.vert2.z=(f.width-1)*65536;
   r.pitch=320;r.screenHeight=80;r.screenLeft=0;r.screenRight=160;
   r.screenTop=(int)(trial%13U);r.screenBottom=80-(int)(trial%17U);
   r.viewZ=30+(int)(trial%61U);
   for(unsigned x=0;x<160U;++x) r.columnScale[x]=(trial+x)%9U?INT_MAX:0;
   for(unsigned i=0;i<160U*80U;++i) pixels[0][i]=(uint16_t)(i*31U+trial);
   memcpy(pixels[1],pixels[0],sizeof pixels[0]);
   memcpy(pixels[2],pixels[0],sizeof pixels[0]);
   uint8_t mode=trial%5U?0U:7U;int glow=trial%11U==0U;
   Line_t refLine=line;
   r.pixels=(short*)pixels[0];
   int result=spans(&r,&line,&f,mode,glow,&stats[0]);
   r.pixels=(short*)pixels[1];
   int ref=spansReference(&r,&refLine,&f,mode,glow,&stats[1]);
   assert(result==ref);
   assert(!memcmp(pixels[0],pixels[1],sizeof pixels[0]));
   assert(!memcmp(&stats[0],&stats[1],sizeof stats[0]));
   refLine=line;
   r.pixels=(short*)pixels[2];
   int prior=spansP2h(&r,&refLine,&f,mode,glow,&stats[2]);
   assert(result==prior);
   assert(!memcmp(pixels[0],pixels[2],sizeof pixels[0]));
   assert(!memcmp(&stats[0],&stats[2],sizeof stats[0]));
 }
 puts("P2i actual C multi-column opaque/add/glow / P2h / original span parity PASS");
}
static void wallProof(void) {
    Render_t r={0};
    Line_t line={0};
    Frame f={0};
    EspNativeSpriteStats stats={0};
    r.screenWidth=160;r.screenLeft=0;r.screenRight=160;
    line.vert1.x=2<<16;line.vert2.x=7<<16;
    line.vert1.y=line.vert2.y=1<<17;
    const int scale=(0x40000000/line.vert1.y)<<2;
    for(int c=2;c<7;++c) r.columnScale[c]=scale-1;
    assert(fullyOccludedColumns(&r,&line)==5U);
    assert(spans(&r,&line,&f,0U,0,&stats));
    assert(stats.wallOccludedColumns==5U && stats.pixelsDrawn==0U);
    r.columnScale[4]=scale;
    assert(fullyOccludedColumns(&r,&line)==0U);
    r.columnScale[4]=scale-1;r.screenLeft=3;r.screenRight=6;
    assert(fullyOccludedColumns(&r,&line)==3U);
    ++line.vert2.y;
    assert(fullyOccludedColumns(&r,&line)==0U);
    line.vert1.y=line.vert2.y=0;
    assert(fullyOccludedColumns(&r,&line)==0U);
}
int main(void) {
 wallProof();
 testSpanParity();
 testAssetReader();
 testModesAndRaster();
 puts("native_fire_visual_host: PASS");
 return 0;
}
'''
# Remove an unused no-op expression if the host shims ever grow.
host = host.replace(" r.pitch=320;r.screenWidth; /* none: viewport is 160x80 */", " r.pitch=320; /* viewport is 160x80 */")
with tempfile.TemporaryDirectory(prefix="doomrpg-fire-vis-") as d:
    path = Path(d) / "fire_visual_test.c"
    binary = Path(d) / "fire_visual_test"
    path.write_text(host[:host.index("static void testAssetReader")] + "\n" + functions +
                    "\n" + host[host.index("static void testAssetReader"):])
    cmd = shlex.split(os.environ.get("CC", "cc")) + [
        "-O2", "-std=c11", "-Wall", "-Wextra", "-Werror",
        "-IESP32/include", str(path), "-o", str(binary)]
    subprocess.run(cmd, cwd=ROOT, check=True)
    subprocess.run([str(binary)], cwd=ROOT, check=True)
