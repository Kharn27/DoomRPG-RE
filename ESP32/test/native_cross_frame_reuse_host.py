#!/usr/bin/env python3
"""Actual native CROSS draw-path parity: one frame decode for four copies.

Extracts drawAt/drawLegacyCross from the shipped renderer, reconstructs its
pre-change calls from the same bodies, then compiles/runs both paths. The
raster shim records deterministic per-copy inputs; exact on-device framebuffer
parity and timings remain a hardware acceptance gate.
"""
from pathlib import Path
import os
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
            if not depth:
                return src[start:pos + 1]
    raise AssertionError("unclosed " + signature)

draw = extract("static int drawAt(Render_t* render,")
cross = extract("static int drawLegacyCross(Render_t* render,")
# Compile the actual pure native fixed-width helper alongside the extracted
# sprite bodies: no duplicated formula and no legacy Render_t mirror.
geom_header = (ROOT / "ESP32/include/esp_render_geometry_primitives.h").read_text()
scalar_signature = "static inline int EspNativeRenderGeometry_fracHalfScreenWidth(int screenWidth)"
assert geom_header.count(scalar_signature) == 1
scalar_begin = geom_header.index(scalar_signature)
scalar_end = geom_header.index("\n}\n", scalar_begin) + len("\n}")
scalar_owner = geom_header[scalar_begin:scalar_end]

assert "uint32_t* crossAnimation" in draw
assert "crossAnimation != NULL && *crossAnimation != UINT32_MAX" in draw
assert "uint32_t crossAnimation = UINT32_MAX;" in cross
assert "frame, seenLogical, stats, &crossAnimation)" in cross

reference_draw = draw.replace("static int drawAt(", "static int drawAtReference(", 1)
reference_draw = reference_draw.replace(
    "EspNativeSpriteStats* stats,\n                  uint32_t* crossAnimation) {",
    "EspNativeSpriteStats* stats) {", 1)
start = reference_draw.index("    /* All four CROSS billboards use")
stop = reference_draw.index("    minimum = frame->xMin - 32;", start)
reference_draw = (
    reference_draw[:start]
    + """    if (!loadFrame(sources, logical, animation, glow,
                   frame, seenLogical, stats)) {
        return 0;
    }

"""
    + reference_draw[stop:]
)
assert "crossAnimation" not in reference_draw
reference_cross = cross.replace(
    "static int drawLegacyCross(", "static int drawLegacyCrossReference(", 1)
reference_cross = reference_cross.replace(
    "    uint32_t crossAnimation = UINT32_MAX;\n", "", 1)
reference_cross = reference_cross.replace(
    "drawAt(render, sources, parent, resourceLogical,",
    "drawAtReference(render, sources, parent, resourceLogical,", 1)
reference_cross = reference_cross.replace(
    "frame, seenLogical, stats, &crossAnimation))",
    "frame, seenLogical, stats))", 1)
assert "crossAnimation" not in reference_cross

shim = r'''
#include <assert.h>
#include <inttypes.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "esp_native_sprite_visual_legacy.h"

#define CROSS 0x04000000UL
#define ORIENT_MASK 0x00780000UL
#define FLIP_HORIZONTAL 0x02000000UL
#define TWO_SIDED 0x08000000UL
#define ORIENT_NORTH 0x00080000UL
#define ORIENT_SOUTH 0x00100000UL
#define ORIENT_EAST  0x00200000UL
#define ORIENT_WEST  0x00400000UL
#define RENDER_MODE_NORMAL 0U
#define RENDER_MODE_ADD 7U
#define ENEMY_TYPE 1U
#if defined(DOOMRPG_ESP32_TRACE_SPRITE)
#define TRACE_SPRITE(index, ...) do { if (0) printf(__VA_ARGS__); } while (0)
#else
#define TRACE_SPRITE(index, ...) do {} while (0)
#endif

typedef struct { int x,y,z; } Vertex_t;
typedef struct { Vertex_t vert1, vert2; } Line_t;
typedef struct { uint16_t x,y; uint32_t info; } EspMapSprite;
typedef struct { int dummy; } Sources;
typedef struct {uint16_t index, logical;uint32_t info;int32_t sortZ;} Order;
typedef struct {uint16_t logical,actual; int xMin,xMax;uint16_t palette[16];uint8_t texels[4];} Frame;
typedef struct {
    int viewCos_,viewSin_,viewSin,viewCos,viewTransX,viewTransY;
    int viewX,viewY,screenWidth;
    uint32_t rasterHash;
} Render_t;
typedef struct {
    uint32_t pixelsDrawn,glowPixels,draws,glowDraws;
    uint32_t nearCulled,clipCulled,glowNearCulled,glowClipCulled;
    uint32_t ambientCandidates,ambientFastCandidates,ambientRapidCandidates;
    uint32_t unsupported,frameLoads,frameBytes,packReads;
} EspNativeSpriteStats;
static unsigned reads, rasters, entities, mapReads, failRead, corruptFrame, animationDrift;
static uint32_t clockFrame, lastOffsets[16];
static int EspMapRuntime_getMapSprite(uint32_t index, EspMapSprite *out) {
    ++mapReads;
    *out=(EspMapSprite){(uint16_t)(512+index),928U,0};
    return 1;
}
static int EspMapSpriteTopology_getEntity(uint32_t index, uint8_t *type,
                                          uint8_t *subtype, uint16_t *link,
                                          uint16_t *order) {
    ++entities;*type=1;*subtype=(uint8_t)(index&3U);*link=7;*order=1;
    return 1;
}
static uint32_t EspNativeAmbientSprite_frame(
    uint32_t info,uint8_t type,uint8_t subtype,uint32_t index,uint32_t time) {
    (void)info;(void)type;(void)subtype;(void)index;(void)time;
    return animationDrift && ++clockFrame > 1U ? 1U : 0U;
}
static uint32_t ambientSampleMs=0;
/* The real native CROSS painter now decorates pain palettes only on actual
 * monster hit leases. This frame-reuse test covers ordinary CROSS billboards
 * without a damage lease. Do not mock away the production branch: compile it
 * against the same Frame.palette[16] contract, forced inactive here. */
static int EspNativeGameplayMonsterCombat_damageBlend(uint16_t sprite,
                                                       uint32_t now) {
    (void)sprite;(void)now;return 0;
}
static unsigned int DoomRPG_GetUpTimeMS(void) {return 0U;}
static uint16_t EspNativePlayerPainVisual_weaponColor(uint16_t c) {return c;}
static int loadFrame(const Sources* src, uint16_t logical,uint32_t animation,
                     int glow,Frame* frame,uint32_t seen[8],
                     EspNativeSpriteStats* stats) {
    (void)src;(void)seen;(void)glow;
    ++reads;
    if (failRead && reads==failRead) return 0;
    memset(frame,0,sizeof *frame);
    frame->logical=logical;
    frame->actual=(uint16_t)(logical*2U+animation);
    frame->xMin=28;frame->xMax=35;
    frame->texels[0]=(uint8_t)(frame->actual^0x5aU);
    ++stats->frameLoads;
    stats->frameBytes+=5;
    stats->packReads+=7;
    return 1;
}
static void EspNativeRenderGeometry_transformPoint(
    int viewCos_,int viewSin_,int viewSin,int viewCos,int transX,int transY,
    int* x,int* y) {
    (void)viewCos_;(void)viewSin_;(void)viewSin;(void)viewCos;
    (void)transX;(void)transY;
    *x=0x200000+(*x-512)*65536;
    *y=(*y-900)*65536;
}
static int EspNativeDynamicLineRender_clipLine(Line_t *line) {
    (void)line;return 1;
}
static void EspNativeRenderGeometry_projectVertex(
    int width,int half,int* x,int* y,int* z) {
    (void)width;(void)half;(void)z;
    *x=(*x)/8;*y=(*y)/8;
}
static int spans(Render_t* render,Line_t* line,const Frame* frame,
                 uint8_t mode,int glow,EspNativeSpriteStats* stats) {
    uint32_t numbers[6]={
       (uint32_t)line->vert1.x,(uint32_t)line->vert2.y,
       (uint32_t)line->vert1.y,(uint32_t)line->vert2.z,
       (uint32_t)frame->actual,(uint32_t)frame->texels[0]
    };
    size_t i;
    assert(rasters<16U);
    lastOffsets[rasters++]=(uint32_t)line->vert1.y;
    for(i=0;i<6;++i) {
        render->rasterHash^=numbers[i];
        render->rasterHash*=16777619U;
    }
    render->rasterHash^=(uint32_t)(mode+glow);
    if(glow) stats->glowPixels+=3;
    else stats->pixelsDrawn+=3;
    return 1;
}
static int EspNativeAmbientSprite_rasterCandidate(
    uint32_t info,uint8_t type,uint32_t before,uint32_t after) {
    (void)info;(void)type;return after>before;
}
static int EspNativeAmbientSprite_fast(uint8_t type) {
    (void)type;return 0;
}
static int EspNativeAmbientSprite_rapid(uint8_t type,uint8_t subtype) {
    (void)type;(void)subtype;return 0;
}
static int isLegacyCrossLogical(uint16_t logical) {
    return logical>=82U && logical<=90U && (logical&1U)==0U;
}
static uint8_t spriteRenderMode(uint16_t logical) {
    (void)logical;return RENDER_MODE_NORMAL;
}
static uint16_t glowFor(uint16_t logical) {
    (void)logical;return 0U;
}
static const int* EspNativeGraphicsCatalog_findSprite(uint16_t logical) {
    static const int match=1;
    return logical == 83U || logical == 81U ? &match : NULL;
}
'''
cases = r'''
static void clearInput(void) {
    reads=rasters=entities=mapReads=failRead=corruptFrame=animationDrift=0U;
    clockFrame=0;
    memset(lastOffsets,0,sizeof lastOffsets);
}
static int runGroup(int optimized,int corrupt,int vary,int fail,
                    uint32_t* hash,EspNativeSpriteStats* stats,
                    uint32_t* readCount) {
    Render_t render={0};
    Sources sources={0};
    Frame frame={0};
    Order parent={7U,84U,CROSS|84U,123};
    uint32_t seen[8]={0};
    int ok;
    clearInput();
    render.rasterHash=2166136261U;
    failRead=(unsigned)fail;
    corruptFrame=(unsigned)corrupt;
    animationDrift=(unsigned)vary;
    memset(stats,0,sizeof *stats);
    ok=optimized
        ? drawLegacyCross(&render,&sources,&parent,&frame,seen,stats)
        : drawLegacyCrossReference(&render,&sources,&parent,&frame,seen,stats);
    *hash=render.rasterHash;
    *readCount=reads;
    if (!fail && !vary && !corrupt) {
        assert(ok);
        assert(rasters==4U);
        assert(stats->draws==1U);
        assert(stats->pixelsDrawn==12U);
        assert(mapReads==4U && entities==4U);
        assert(reads==(optimized ? 1U:4U));
    }
    return ok;
}
int main(void) {
    EspNativeSpriteStats oldStats,newStats;
    uint32_t oldHash,newHash,oldReads,newReads;
    int oldOk,newOk;
    oldOk=runGroup(0,0,0,0,&oldHash,&oldStats,&oldReads);
    newOk=runGroup(1,0,0,0,&newHash,&newStats,&newReads);
    assert(oldOk && newOk && oldHash==newHash);
    assert(oldStats.pixelsDrawn==newStats.pixelsDrawn);
    assert(oldStats.draws==newStats.draws);
    assert(oldStats.nearCulled==newStats.nearCulled);
    assert(oldStats.clipCulled==newStats.clipCulled);
    assert(oldStats.ambientCandidates==newStats.ambientCandidates);
    assert(oldStats.frameLoads==4U && newStats.frameLoads==1U);
    assert(oldStats.packReads==28U && newStats.packReads==7U);
    printf("CROSS parity hash=%08x draws=%u beforeFrames=%u afterFrames=%u beforeReads=%u afterReads=%u\n",
           newHash,newStats.draws,oldStats.frameLoads,newStats.frameLoads,
           oldStats.packReads,newStats.packReads);

    assert(!runGroup(0,0,0,1,&oldHash,&oldStats,&oldReads));
    assert(!runGroup(1,0,0,1,&newHash,&newStats,&newReads));
    assert(oldHash==newHash && newReads==1U);

    /* If a mutable animation provider did change within one four-copy
     * group, never reuse a stale frame. The original path would have loaded
     * four independent frames, but our optimized path must FAIL CLOSED. */
    assert(!runGroup(1,0,1,0,&newHash,&newStats,&newReads));
    assert(newReads==1U);

    puts("native_cross_frame_reuse_host: PASS raster witness parity, 4->1 PAK loads, initial I/O fail-closed, animation drift fail-closed");
    return 0;
}
'''
with tempfile.TemporaryDirectory(prefix="doomrpg-cross-reuse-") as td:
    unit=Path(td)/"cross.c"
    program=Path(td)/"cross"
    unit.write_text(shim+"\n"+scalar_owner+"\n"+reference_draw+"\n"+reference_cross+
                    "\n"+draw+"\n"+cross+"\n"+cases)
    for defines in ([], ["-DDOOMRPG_ESP32_TRACE_SPRITE=7"]):
        subprocess.run([
            os.environ.get("CC","cc"),"-std=c11","-O2",
            "-Wall","-Wextra","-Werror","-Wno-unused-parameter",
            "-IESP32/include",*defines,str(unit),"-o",str(program)
        ],cwd=ROOT,check=True)
        subprocess.run([str(program)],cwd=ROOT,check=True)
print("PASS CROSS four-copy renderer frame reuse normal and trace")
