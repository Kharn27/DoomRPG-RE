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
    int active;
} EspNativeProjectedWallState;

static EspNativeProjectedWallState projectedWall;
static EspNativeProjectedWallStats projectedStats;

void EspNativeProjectedWall_resetStats(void) {
    if (projectedWall.active) {
        EspNativeGraphics_releaseWallFrame(&projectedWall.frame);
    }
    memset(&projectedWall, 0, sizeof(projectedWall));
    memset(&projectedStats, 0, sizeof(projectedStats));
    projectedStats.lastTextureIndex = -1;
}

void EspNativeProjectedWall_getStats(EspNativeProjectedWallStats* outStats) {
    if (outStats != NULL) {
        *outStats = projectedStats;
    }
}

int EspNativeProjectedWall_begin(struct Render_s* renderBase, int textureIndex) {
    Render_t* render = (Render_t*)renderBase;

    if (render == NULL || render->mediaPalettes == NULL ||
        render->mediaTexels != NULL || projectedWall.active) {
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

    projectedWall.active = 1;
    projectedStats.beginCalls++;
    projectedStats.lastTextureIndex = textureIndex;
    projectedStats.lastTexelHash = projectedWall.frame.texelHash;

    render->spanPalettes =
        (unsigned short*)(render->mediaPalettes + projectedWall.frame.paletteOffset);

    printf("[PROJWALL] ACQUIRE texture=%d palette=%d packed=%uB hash=%08x sourceOffset=%u pack=closed\n",
           textureIndex,
           projectedWall.frame.paletteOffset,
           (unsigned int)projectedWall.frame.packedBytes,
           (unsigned int)projectedWall.frame.texelHash,
           (unsigned int)projectedWall.frame.sourceTexelOffset);
    return 1;
}

void EspNativeProjectedWall_end(void) {
    if (!projectedWall.active) {
        return;
    }

    printf("[PROJWALL] RELEASE texture=%d spans=%u pixels=%u rangeErrors=%u\n",
           projectedWall.frame.textureIndex,
           (unsigned int)projectedStats.spanCalls,
           (unsigned int)projectedStats.pixelsDrawn,
           (unsigned int)projectedStats.outOfRangeReads);

    EspNativeGraphics_releaseWallFrame(&projectedWall.frame);
    memset(&projectedWall.frame, 0, sizeof(projectedWall.frame));
    projectedWall.active = 0;
    projectedStats.endCalls++;
}

void EspNativeProjectedWall_spanMode0(struct Render_s* renderBase,
                                      int x,
                                      int y,
                                      int texelPosition,
                                      int texelStep,
                                      int pixelCount) {
    Render_t* render = (Render_t*)renderBase;
    unsigned short* pixels;
    int pitch;
    int remaining;

    if (render == NULL || render->pixels == NULL ||
        render->spanPalettes == NULL || !projectedWall.active ||
        projectedWall.frame.texels == NULL || pixelCount <= 0) {
        return;
    }

    pitch = render->pitch >> 1;
    pixels = render->pixels + pitch * y + x;
    remaining = pixelCount;
    projectedStats.spanCalls++;

    while (remaining-- > 0) {
        uint32_t packedIndex;
        uint8_t packed;
        int paletteIndex;

        if (texelPosition < 0) {
            projectedStats.outOfRangeReads++;
            return;
        }

        packedIndex = ((uint32_t)texelPosition) >> 13;
        if (packedIndex >= projectedWall.frame.packedBytes) {
            projectedStats.outOfRangeReads++;
            return;
        }

        packed = projectedWall.frame.texels[packedIndex];
        paletteIndex = (packed >> ((texelPosition >> 10) & 4)) & 0x0f;
        *pixels = render->spanPalettes[paletteIndex];

        pixels += pitch;
        texelPosition += texelStep;
        projectedStats.pixelsDrawn++;
    }
}
