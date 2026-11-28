#!/usr/bin/env python3
"""P3g: compile actual native row + LRU against the frozen P3f scalar bodies."""
from pathlib import Path
import os
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
src = (root / "ESP32/src/esp_native_plane_renderer.c").read_text()

def body(signature):
    start = src.index(signature)
    brace = src.index("{", start)
    depth = 0
    for i in range(brace, len(src)):
        if src[i] == "{":
            depth += 1
        elif src[i] == "}":
            depth -= 1
            if depth == 0:
                return src[start:i+1]
    raise AssertionError(signature)

layout = src[src.index("typedef struct PlaneTextureDesc_s"):src.index("static EspNativePlaneRenderStats planeStats;")]
real = "\n".join(body(sig) for sig in ("static int findTextureIndex(", "static int acquireTexture(", "static inline void flushPlaneHits(", "static int spanPlane(", "static int collectTextures(", "static int32_t planeRowStep("))
reference = r"""
static int refCollectTextures(PlaneWork* work) {
    uint32_t i;
    if (work == NULL || work->runtime == NULL ||
        work->runtime->planeMap == NULL ||
        work->runtime->planeMapBytes != EXPECTED_PLANE_MAP_BYTES) return 0;

    work->textureCount = 0U;
    for (i = 0U; i < EXPECTED_PLANE_MAP_BYTES; ++i) {
        const uint8_t id = work->runtime->planeMap[i];
        if (findTextureIndex(work, id) >= 0) continue;
        if (work->textureCount >= MAX_PLANE_TEXTURES) return 0;
        memset(&work->textures[work->textureCount], 0,
               sizeof(work->textures[work->textureCount]));
        work->textures[work->textureCount].logicalId = id;
        ++work->textureCount;
    }
    return work->textureCount > 0U;
}
static int32_t refPlaneRowStep(int32_t projected, int width) {
    return (int32_t)((((int64_t)projected << 19) /
                      (32 * width)) >> 8);
}
static int refAcquireTexture(PlaneWork* work,
                          const PlaneTextureDesc* source,
                          const uint8_t** outTexels) {
    PlaneCacheSlot* target = NULL;
    uint32_t oldest = UINT32_MAX;
    uint32_t i;
    uint32_t readOffset;

    if (outTexels != NULL) *outTexels = NULL;
    if (work == NULL || source == NULL || outTexels == NULL) return 0;

    ++work->cacheClock;
    if (work->cacheClock == 0U) work->cacheClock = 1U;

    for (i = 0U; i < PLANE_CACHE_SLOTS; ++i) {
        PlaneCacheSlot* slot = &work->cache[i];
        if (slot->texels == NULL) break;
        if (slot->valid && slot->source != NULL &&
            slot->source->actualId == source->actualId &&
            slot->source->sourceTexelOffset == source->sourceTexelOffset) {
            slot->lastUse = work->cacheClock;
            ++work->stats.cacheHits;
            *outTexels = slot->texels;
            return 1;
        }
    }

    ++work->stats.cacheMisses;
    for (i = 0U; i < PLANE_CACHE_SLOTS; ++i) {
        PlaneCacheSlot* slot = &work->cache[i];
        if (slot->texels == NULL) break;
        if (!slot->valid) {
            target = slot;
            break;
        }
        if (slot->lastUse < oldest) {
            oldest = slot->lastUse;
            target = slot;
        }
    }
    if (target == NULL) return 0;
    if (target->valid) ++work->stats.cacheEvictions;

    readOffset = WALL_TEXEL_HEADER_BYTES + (source->sourceTexelOffset >> 1);
    if (readOffset > work->wallTexels.size ||
        PLANE_TEXTURE_BYTES > work->wallTexels.size - readOffset ||
        !EspAssetPack_readRange(&work->wallTexels, readOffset,
                                target->texels, PLANE_TEXTURE_BYTES)) return 0;

    target->source = source;
    target->lastUse = work->cacheClock;
    target->valid = 1U;
    work->stats.texelReadBytes += PLANE_TEXTURE_BYTES;
    *outTexels = target->texels;
    return 1;
}
static int refSpanPlane(PlaneWork* work,
                     int x, int y,
                     const uint8_t* planeCells,
                     int32_t param5, int32_t param6,
                     int32_t param7, int32_t param8,
                     int count) {
    uint16_t* pixels;
    int pitch;

    if (work == NULL || work->render == NULL || planeCells == NULL ||
        count <= 0) return 0;

    pitch = work->render->pitch >> 1;
    pixels = (uint16_t*)work->render->pixels + pitch * y + x;

    while (--count >= 0) {
        const uint32_t u5 = (uint32_t)param5;
        const uint32_t u6 = (uint32_t)param6;
        const uint32_t cellIndex =
            ((u6 >> 17) & 0x3e0U) + ((u5 << 5) >> 27);
        const uint32_t texelIndex =
            ((u6 >> 10) & 0x0fc0U) + ((u5 << 10) >> 26);
        const uint8_t logicalId = planeCells[cellIndex];
        const int descIndex = findTextureIndex(work, logicalId);
        const PlaneTextureDesc* desc;
        const uint8_t* texels;
        uint8_t packed;
        uint8_t paletteIndex;

        if (cellIndex >= ESP_MAP_PLANE_CELL_COUNT || texelIndex >= 4096U ||
            descIndex < 0) return 0;
        desc = &work->textures[descIndex];
        if (!refAcquireTexture(work, desc, &texels)) return 0;

        packed = texels[texelIndex >> 1];
        paletteIndex = (uint8_t)((texelIndex & 1U)
                                     ? ((packed >> 4) & 0x0fU)
                                     : (packed & 0x0fU));
        *pixels++ = desc->paletteRgb565[paletteIndex];
        ++work->stats.pixelsRendered;

        param5 = (int32_t)((uint32_t)param5 + (uint32_t)param7);
        param6 = (int32_t)((uint32_t)param6 + (uint32_t)param8);
    }
    return 1;
}
"""
harness = r"""
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "esp_native_plane_renderer.h"
#define PLANE_TEXTURE_BYTES 2048U
#define PLANE_PALETTE_COLORS 16U
#define MAX_PLANE_TEXTURES 24U
#define PLANE_CACHE_SLOTS 6U
#define WALL_TEXEL_HEADER_BYTES 4U
#define ESP_MAP_PLANE_CELL_COUNT 1024U
typedef struct Render_s { int pitch; void* pixels; } Render_t;
#define EXPECTED_PLANE_MAP_BYTES 2048U
typedef struct EspMapRuntimeView_s {
    const uint8_t* planeMap;
    uint32_t planeMapBytes;
} EspMapRuntimeView;
typedef struct EspAssetPackEntry_s { uint32_t size; } EspAssetPackEntry;
static uint32_t reads, failRead;
static uint32_t offsets[1024];
static int EspAssetPack_readRange(const EspAssetPackEntry* e, uint32_t off,
                                 void* dst, uint32_t n) {
    assert(reads<1024 && off+n<=e->size);
    offsets[reads++]=off;
    if(reads==failRead) return 0;
    for(uint32_t i=0;i<n;++i) ((uint8_t*)dst)[i]=(uint8_t)((off+i*13U)^((off+i)>>8));
    return 1;
}
""" + layout + real + reference + r"""
static PlaneWork fast, scalar;
static uint8_t storage[2][6][2048], cells[1024];
static uint16_t pixels[2][2*176];
static uint32_t expectedOffsets[1024];
static uint32_t rng=0xabc81234U;
static uint32_t next(void) {rng=rng*1664525U+1013904223U;return rng;}
static void init(PlaneWork* w,Render_t* r,unsigned side,unsigned width,uint32_t clock) {
    memset(w,0,sizeof(*w));
    memset(storage[side],0x37,sizeof(storage[side]));
    memset(pixels[side],0xa5,sizeof(pixels[side]));
    r->pitch=176*2;r->pixels=pixels[side];w->render=r;
    w->wallTexels.size=2048*24+4;w->textureCount=12;w->cacheClock=clock;
    for(unsigned i=0;i<12;++i) {
        PlaneTextureDesc* d=&w->textures[i];
        d->logicalId=(uint8_t)(i*7);
        d->actualId=(uint16_t)(i==11?3:i); // alias payload, distinct palette
        d->sourceTexelOffset=(uint32_t)d->actualId*4096;
        for(unsigned j=0;j<16;++j)d->paletteRgb565[j]=(uint16_t)(i*7919U+j*211U);
    }
    for(unsigned i=0;i<width;++i)w->cache[i].texels=storage[side][i];
}
static void compare(void) {
    assert(fast.cacheClock==scalar.cacheClock);
    assert(!memcmp(&fast.stats,&scalar.stats,sizeof(fast.stats)));
    assert(!memcmp(pixels[0],pixels[1],sizeof(pixels[0])));
    for(unsigned i=0;i<6;++i) {
        PlaneCacheSlot *a=&fast.cache[i],*b=&scalar.cache[i];
        assert(a->valid==b->valid && a->lastUse==b->lastUse);
        assert((a->texels==NULL)==(b->texels==NULL));
        assert((a->source==NULL)==(b->source==NULL));
        if(a->source)assert(a->source-fast.textures==b->source-scalar.textures);
        assert(!memcmp(storage[0][i],storage[1][i],2048));
    }
}
static void checkCollectionAndStep(void) {
    uint8_t map[2048];
    EspMapRuntimeView runtime = {map, sizeof(map)};
    for (unsigned trial=0; trial<512; ++trial) {
        memset(&fast, 0xa5, sizeof(fast));
        memset(&scalar, 0xa5, sizeof(scalar));
        fast.runtime=scalar.runtime=&runtime;
        unsigned unique=1U+trial%32U;
        for (unsigned i=0;i<2048;++i)
            map[i]=(uint8_t)((next()%unique)*7U + trial);
        runtime.planeMapBytes=trial%29U==0U?2047U:2048U;
        assert(collectTextures(&fast)==refCollectTextures(&scalar));
        assert(fast.textureCount==scalar.textureCount);
        assert(!memcmp(fast.textures,scalar.textures,sizeof(fast.textures)));
    }
    /* P4m: unaligned source, repeated mixed groups, all-byte groups,
     * changed groups with new IDs in every lane, overflow at every lane. */
    {
        uint8_t storageMap[2048+4];
        for(unsigned offset=0;offset<4U;++offset) {
            uint8_t* data=storageMap+offset;
            runtime.planeMap=data;
            runtime.planeMapBytes=2048U;
            for(unsigned trial=0;trial<128U;++trial) {
                for(unsigned i=0;i<2048U;++i) {
                    unsigned lane=i%4U;
                    unsigned group=i/4U;
                    data[i]=(uint8_t)(trial%4U==0U ? trial :
                        trial%4U==1U ? lane+trial :
                        trial%4U==2U ? ((group/17U)%27U)+lane :
                        ((group/32U)%24U)+(lane==trial%4U));
                }
                if(trial%8U==7U) {
                    /* Force exactly 24 admitted IDs, then a 25th at a
                     * selected lane of a changed group near the end. */
                    for(unsigned i=0;i<2048U;++i)data[i]=(uint8_t)(i%24U);
                    data[2044U+((trial/8U)%4U)]=255U;
                }
                memset(&fast,0xa5,sizeof(fast));
                memset(&scalar,0xa5,sizeof(scalar));
                fast.runtime=scalar.runtime=&runtime;
                assert(collectTextures(&fast)==refCollectTextures(&scalar));
                assert(fast.textureCount==scalar.textureCount);
                assert(!memcmp(fast.textures,scalar.textures,sizeof(fast.textures)));
            }
        }
        runtime.planeMap=map;
    }
    puts("PASS P4m 512 grouped/unaligned/overflow texture collection cases");
    for(int32_t p=0;p<=131072;++p)
        assert(planeRowStep(p,160)==refPlaneRowStep(p,160));
    const int32_t edge[]={INT32_MAX/2-1,INT32_MAX/2,INT32_MAX/2+1,INT32_MAX};
    for(unsigned i=0;i<sizeof(edge)/sizeof(edge[0]);++i)
        assert(planeRowStep(edge[i],160)==refPlaneRowStep(edge[i],160));
    for(int width=1;width<=320;++width)
        for(int32_t p=0;p<3000;p+=17)
            assert(planeRowStep(p,width)==refPlaneRowStep(p,width));
    puts("PASS P3h collection order/capacity/failure and exact row step");
}
/* Explicit same-texture run crossing clock wrap before a missing
 * descriptor or a failing read; compare partial output, LRU and stats. */
static void checkBatchedHits(void) {
    for (unsigned tail=0; tail<3; ++tail) {
        for (unsigned offset=0; offset<32; ++offset) {
            Render_t f,r;
            init(&fast,&f,0,2,UINT32_MAX-offset);
            init(&scalar,&r,1,2,fast.cacheClock);
            fast.stats.pixelsRendered = scalar.stats.pixelsRendered = UINT32_MAX-offset;
            fast.stats.cacheHits = scalar.stats.cacheHits = UINT32_MAX-offset;
            memset(cells,21,sizeof(cells));
            if(tail==1) cells[16]=255;
            if(tail==2) cells[16]=28;
            failRead=tail==2?2:0;
            reads=0;
            int a=refSpanPlane(&scalar,4,0,cells,0,0,1<<22,0,32);
            uint32_t expected=reads;
            memcpy(expectedOffsets,offsets,reads*sizeof(offsets[0]));
            reads=0;
            int b=spanPlane(&fast,4,0,cells,0,0,1<<22,0,32);
            assert(a==b && a==(tail==0) && reads==expected);
            assert(!memcmp(expectedOffsets,offsets,reads*sizeof(offsets[0])));
            compare();
        }
    }
    puts("PASS P4e/P4f 96 batched LRU/stat wrap/partial-failure spans");
}
int main(void) {
    checkCollectionAndStep();
    checkBatchedHits();
    /* P4f: sweep all texel coordinates in both nibble positions. */
    for (unsigned row=0; row<64; ++row) {
        Render_t f,r;
        init(&fast,&f,0,1,0);
        init(&scalar,&r,1,1,0);
        memset(cells,21,sizeof(cells));
        failRead=0; reads=0;
        assert(refSpanPlane(&scalar,4,0,cells,0,(int32_t)(row<<16),1<<16,0,64));
        reads=0;
        assert(spanPlane(&fast,4,0,cells,0,(int32_t)(row<<16),1<<16,0,64));
        compare();
    }
    puts("PASS P4f all 4096 plane texel positions");
    unsigned cases=0;
    for(unsigned width=1;width<=6;++width) {
        for(unsigned trial=0;trial<240;++trial) {
            Render_t f,r;
            init(&fast,&f,0,width,trial%7==0?UINT32_MAX-8U:0U);
            init(&scalar,&r,1,width,fast.cacheClock);
            for(unsigned i=0;i<1024;++i) {
                unsigned id=trial%4==0?3:trial%4==1?(i/32)%12:next()%12;
                cells[i]=(uint8_t)(id*7);
            }
            if(trial%13==0)cells[next()%1024]=255; // missing descriptor
            for(unsigned row=0;row<2;++row) {
                int32_t x=(int32_t)next(),y=(int32_t)next();
                int32_t dx=trial%5==0?0:(int32_t)next();
                int32_t dy=trial%5==0?0:(int32_t)next();
                int n=trial%11==0?1:160;
                failRead=trial%17==0?1+trial%5:0;
                reads=0;
                int a=refSpanPlane(&scalar,4,(int)row,cells,x,y,dx,dy,n);
                uint32_t expected=reads;
                memcpy(expectedOffsets,offsets,reads*sizeof(offsets[0]));
                reads=0;
                int b=spanPlane(&fast,4,(int)row,cells,x,y,dx,dy,n);
                assert(a==b && reads==expected);
                assert(!memcmp(expectedOffsets,offsets,reads*sizeof(offsets[0])));
                compare();++cases;
                if(!a)break;
            }
        }
    }
    printf("PASS P3g real plane row vs frozen P3f: %u spans, pixels/LRU/reads/failure/wrap exact\n",cases);
    return 0;
}
"""
with tempfile.TemporaryDirectory(prefix="doomrpg-p3g-") as tmp:
    c = Path(tmp) / "oracle.c"
    exe = Path(tmp) / "oracle"
    c.write_text(harness)
    subprocess.run(shlex.split(os.environ.get("CC", "cc")) + [
        "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror", "-I",
        str(root / "ESP32/include"), str(c), "-o", str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
