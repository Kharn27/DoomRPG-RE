#include <SDL.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "DoomRPG.h"
#include "Render.h"

#include "native_graphics_resource_manager.h"
#include "native_projected_wall_bridge.h"

#define EXPECTED_WALL_WIDTH 64
#define EXPECTED_WALL_HEIGHT 64
#define EXPECTED_WALL_PACKED_BYTES 2048U

typedef struct EspNativeProjectedWallState_s {
    EspNativeWallFrame frame;
    Render_t* render;
    int active;
} EspNativeProjectedWallState;

static EspNativeProjectedWallState projectedWall;
static EspNativeProjectedWallStats projectedStats;

static int sampleSpanMode0(Render_t* render,
                           int x,
                           int y,
                           int texelPosition,
                           int texelStep,
                           int pixelCount) {
    unsigned short* pixels;
    const unsigned short* palette;
    int pitch;
    int remaining;
    int64_t localPosition;
    int64_t basePosition;
    int mappingIndex;

    projectedStats.spanCalls++;

    if (render == NULL || render->pixels == NULL ||
        !projectedWall.active || projectedWall.frame.texels == NULL ||
        projectedWall.render != render) {
        projectedStats.rangeErrors++;
        return 0;
    }

    if (render->mediaTexels != NULL) {
        projectedStats.legacyPointerViolations++;
    }

    mappingIndex = projectedWall.frame.textureIndex * 2;
    if (render->mediaTexelOffsets == NULL ||
        render->mediaTexelOffsets[mappingIndex] !=
            (int)projectedWall.frame.sourceTexelOffset) {
        projectedStats.mappingOffsetViolations++;
    }

    if (pixelCount <= 0) {
        return 1;
    }

    pitch = render->pitch >> 1;
    pixels = (unsigned short*)render->pixels + pitch * y + x;
    palette = (const unsigned short*)(
        render->mediaPalettes + projectedWall.frame.paletteOffset);

    basePosition = ((int64_t)projectedWall.frame.sourceTexelOffset) << 12;
    localPosition = (int64_t)texelPosition - basePosition;
    remaining = pixelCount;

    while (remaining-- > 0) {
        uint32_t packedIndex;
        uint8_t packed;
        int paletteIndex;
        int nibbleShift;

        if (localPosition < 0) {
            projectedStats.rangeErrors++;
            return 0;
        }

        packedIndex = (uint32_t)(localPosition >> 13);
        if (packedIndex >= projectedWall.frame.packedBytes) {
            projectedStats.rangeErrors++;
            return 0;
        }

        packed = projectedWall.frame.texels[packedIndex];
        nibbleShift = (int)((localPosition >> 10) & 4);
        paletteIndex = (packed >> nibbleShift) & 0x0f;
        *pixels = palette[paletteIndex];

        pixels += pitch;
        localPosition += texelStep;
        projectedStats.pixelsDrawn++;
    }

    return 1;
}

void EspNativeProjectedWall_resetStats(void) {
    if (projectedWall.active) {
        EspNativeProjectedWall_end();
    }
    memset(&projectedWall, 0, sizeof(projectedWall));
    memset(&projectedStats, 0, sizeof(projectedStats));
    projectedStats.lastTextureIndex = -1;
    projectedStats.lastPaletteOffset = -1;
    projectedStats.sourceTexelOffset = -1;
}

void EspNativeProjectedWall_getStats(EspNativeProjectedWallStats* outStats) {
    if (outStats != NULL) {
        *outStats = projectedStats;
    }
}

int EspNativeProjectedWall_begin(struct Render_s* renderBase, int textureIndex) {
    Render_t* render = (Render_t*)renderBase;
    int mappingIndex;

    if (render == NULL || render->mediaPalettes == NULL ||
        render->mediaTexelOffsets == NULL || render->mediaTexels != NULL ||
        projectedWall.active || textureIndex < 0) {
        printf("[PROJWALL] FAILED begin texture=%d render=%p mediaTexels=%p active=%d\n",
               textureIndex,
               (void*)render,
               render != NULL ? (void*)render->mediaTexels : NULL,
               projectedWall.active);
        return 0;
    }

    memset(&projectedWall.frame, 0, sizeof(projectedWall.frame));
    if (!EspNativeGraphics_loadWallFrame(render, textureIndex,
                                         &projectedWall.frame)) {
        printf("[PROJWALL] FAILED GFXRM wall acquire texture=%d\n",
               textureIndex);
        return 0;
    }

    if (projectedWall.frame.width != EXPECTED_WALL_WIDTH ||
        projectedWall.frame.height != EXPECTED_WALL_HEIGHT ||
        projectedWall.frame.packedBytes != EXPECTED_WALL_PACKED_BYTES ||
        projectedWall.frame.paletteOffset < 0 ||
        projectedWall.frame.paletteOffset + 15 >= render->mediaPalettesLength) {
        printf("[PROJWALL] FAILED unsupported wall frame texture=%d size=%dx%d packed=%u palette=%d\n",
               textureIndex,
               projectedWall.frame.width,
               projectedWall.frame.height,
               (unsigned int)projectedWall.frame.packedBytes,
               projectedWall.frame.paletteOffset);
        EspNativeGraphics_releaseWallFrame(&projectedWall.frame);
        memset(&projectedWall.frame, 0, sizeof(projectedWall.frame));
        return 0;
    }

    mappingIndex = textureIndex * 2;
    if (render->mediaTexelOffsets[mappingIndex] !=
        (int)projectedWall.frame.sourceTexelOffset) {
        printf("[PROJWALL] FAILED mapping/source mismatch texture=%d mapping=%d frame=%u\n",
               textureIndex,
               render->mediaTexelOffsets[mappingIndex],
               (unsigned int)projectedWall.frame.sourceTexelOffset);
        EspNativeGraphics_releaseWallFrame(&projectedWall.frame);
        memset(&projectedWall.frame, 0, sizeof(projectedWall.frame));
        return 0;
    }

    projectedWall.render = render;
    projectedWall.active = 1;

    projectedStats.beginCalls++;
    projectedStats.boundBytes = projectedWall.frame.packedBytes;
    projectedStats.lastTextureIndex = textureIndex;
    projectedStats.lastPaletteOffset = projectedWall.frame.paletteOffset;
    projectedStats.sourceTexelOffset =
        (int)projectedWall.frame.sourceTexelOffset;
    projectedStats.lastTexelHash = projectedWall.frame.texelHash;

    printf("[PROJWALL] ACQUIRE texture=%d palette=%d sourceOffset=%u packed=%uB hash=%08x mediaTexels=%p mappingOffset=%d pack=closed\n",
           textureIndex,
           projectedWall.frame.paletteOffset,
           (unsigned int)projectedWall.frame.sourceTexelOffset,
           (unsigned int)projectedWall.frame.packedBytes,
           (unsigned int)projectedWall.frame.texelHash,
           (void*)render->mediaTexels,
           render->mediaTexelOffsets[mappingIndex]);
    printf("[PROJWALL] NATIVE source active; no mediaTexels alias and no mapping rewrite\n");
    return 1;
}

void EspNativeProjectedWall_end(void) {
    if (!projectedWall.active) {
        return;
    }

    printf("[PROJWALL] RELEASE texture=%d spans=%u pixels=%u rangeErrors=%u legacyPtrViolations=%u mappingViolations=%u mediaTexels=%p\n",
           projectedWall.frame.textureIndex,
           (unsigned int)projectedStats.spanCalls,
           (unsigned int)projectedStats.pixelsDrawn,
           (unsigned int)projectedStats.rangeErrors,
           (unsigned int)projectedStats.legacyPointerViolations,
           (unsigned int)projectedStats.mappingOffsetViolations,
           projectedWall.render != NULL ? (void*)projectedWall.render->mediaTexels : NULL);

    EspNativeGraphics_releaseWallFrame(&projectedWall.frame);
    memset(&projectedWall.frame, 0, sizeof(projectedWall.frame));
    projectedWall.render = NULL;
    projectedWall.active = 0;
    projectedStats.endCalls++;
}

int EspNativeProjectedWall_isActive(void) {
    return projectedWall.active;
}

int EspNativeProjectedWall_drawWallSpans(struct Render_s* renderBase,
                                         struct Line_s* projectedLine) {
    Render_t* render = (Render_t*)renderBase;
    Line_t* line = (Line_t*)projectedLine;
    int i, i2, i3, i4, i5, i6, i7, i8, i9;
    int i12, i13, i14, i15, i16, i17, zPos;

    if (render == NULL || line == NULL || !projectedWall.active ||
        projectedWall.render != render || render->mediaTexels != NULL ||
        render->spanMode != 0 ||
        line->texture != projectedWall.frame.textureIndex) {
        printf("[PROJWALL] FAILED native wall-span precondition active=%d spanMode=%d lineTexture=%d frameTexture=%d mediaTexels=%p\n",
               projectedWall.active,
               render != NULL ? render->spanMode : -1,
               line != NULL ? line->texture : -1,
               projectedWall.frame.textureIndex,
               render != NULL ? (void*)render->mediaTexels : NULL);
        return 0;
    }

    i = line->vert2.x - line->vert1.x;
    if (i <= 0) {
        return 1;
    }

    render->lineRasterCount++;

#if FIXED_VERSION == 1
    i2 = (MAXINT / i) << 1;
    i3 = (int)DoomRPG_FixedMul((line->vert2.y - line->vert1.y), i2);
    i4 = (int)DoomRPG_FixedMul((line->vert2.z - line->vert1.z), i2);
#else
    i2 = (MAXINT / i) << 1;
    i3 = (int)((((int)(line->vert2.y - line->vert1.y)) * ((int64_t)i2)) >> 16);
    i4 = (int)((((int)(line->vert2.z - line->vert1.z)) * ((int64_t)i2)) >> 16);
#endif

    i5 = (line->vert1.x + 65535) >> 16;
    i6 = (line->vert2.x + 65535) >> 16;

    if (render->screenLeft > i5) {
        i5 = render->screenLeft;
    }
    if (render->screenRight < i6) {
        i6 = render->screenRight;
    }

#if FIXED_VERSION == 1
    {
        int j = ((i5 << 16) - line->vert1.x);
        i7 = line->vert1.z + DoomRPG_FixedMul(j, i4);
        i8 = line->vert1.y + DoomRPG_FixedMul(j, i3);
    }
#else
    {
        int64_t j = (int64_t)((i5 << 16) - line->vert1.x);
        i7 = line->vert1.z + ((int)((j * ((int64_t)i4)) >> 16));
        i8 = line->vert1.y + ((int)((j * ((int64_t)i3)) >> 16));
    }
#endif

    i9 = render->mediaTexelOffsets[line->texture * 2];
    if (i9 != (int)projectedWall.frame.sourceTexelOffset) {
        projectedStats.mappingOffsetViolations++;
        return 0;
    }

    while (i5 < i6) {
        i12 = (0x40000000 / i8) << 2;

#if FIXED_VERSION == 1
        i13 = ((int)(DoomRPG_FixedMul(i7, i12) >> 16)) & 63;
#else
        i13 = ((int)((((int64_t)i7) * ((int64_t)i12)) >> 32)) & 63;
#endif

        i8 += i3;
        i7 += i4;
        if (render->columnScale[i5] >= i12) {
            render->columnScale[i5] = i12;
            i14 = i12 >> 3;
            i15 = (64 * i8) >> 17;

            if (line->flags & 0xC0010000) {
                if (!(line->flags & 0xC0000000)) {
                    i15 *= 2;
                }
                zPos = 128;
            }
            else {
                zPos = 64;
            }
            zPos = 64;

            i16 = render->halfScreenHeight -
                  (((zPos - render->viewZ) * i8) >> 17);
            i17 = (i9 + (i13 << 6)) << 12;

            if (render->screenTop > i16) {
                i17 -= (i14 * (i16 - render->screenTop));
                i15 += (i16 - render->screenTop);
                i16 = render->screenTop;
            }

            if (i16 + i15 > render->screenBottom) {
                i15 = render->screenBottom - i16;
            }

            if (!sampleSpanMode0(render, i5, i16, i17, i14, i15)) {
                return 0;
            }
        }
        i5++;
    }

    return 1;
}
