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
    byte* originalMediaTexels;
    int originalTexelOffset;
    int textureIndex;
    int active;
} EspNativeProjectedWallState;

static EspNativeProjectedWallState projectedWall;
static EspNativeProjectedWallStats projectedStats;

void EspNativeProjectedWall_resetStats(void) {
    if (projectedWall.active) {
        EspNativeProjectedWall_end();
    }
    memset(&projectedWall, 0, sizeof(projectedWall));
    memset(&projectedStats, 0, sizeof(projectedStats));
    projectedStats.lastTextureIndex = -1;
    projectedStats.lastPaletteOffset = -1;
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

    mappingIndex = textureIndex * 2;
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

    projectedWall.render = render;
    projectedWall.originalMediaTexels = render->mediaTexels;
    projectedWall.originalTexelOffset = render->mediaTexelOffsets[mappingIndex];
    projectedWall.textureIndex = textureIndex;
    projectedWall.active = 1;

    render->mediaTexelOffsets[mappingIndex] = 0;
    render->mediaTexels = projectedWall.frame.texels;

    projectedStats.beginCalls++;
    projectedStats.boundBytes = projectedWall.frame.packedBytes;
    projectedStats.lastTextureIndex = textureIndex;
    projectedStats.lastPaletteOffset = projectedWall.frame.paletteOffset;
    projectedStats.originalTexelOffset = projectedWall.originalTexelOffset;
    projectedStats.lastTexelHash = projectedWall.frame.texelHash;

    printf("[PROJWALL] BIND texture=%d palette=%d sourceOffset=%d -> localOffset=0 boundedMediaTexels=%uB hash=%08x pack=closed\n",
           textureIndex,
           projectedWall.frame.paletteOffset,
           projectedWall.originalTexelOffset,
           (unsigned int)projectedWall.frame.packedBytes,
           (unsigned int)projectedWall.frame.texelHash);
    printf("[PROJWALL] COMPAT legacy mediaTexels field temporarily aliases one bounded wall frame only\n");
    return 1;
}

void EspNativeProjectedWall_end(void) {
    int mappingIndex;

    if (!projectedWall.active || projectedWall.render == NULL) {
        return;
    }

    mappingIndex = projectedWall.textureIndex * 2;
    projectedWall.render->mediaTexels = projectedWall.originalMediaTexels;
    projectedWall.render->mediaTexelOffsets[mappingIndex] =
        projectedWall.originalTexelOffset;

    printf("[PROJWALL] UNBIND texture=%d restoredOffset=%d mediaTexels=%p\n",
           projectedWall.textureIndex,
           projectedWall.originalTexelOffset,
           (void*)projectedWall.render->mediaTexels);

    EspNativeGraphics_releaseWallFrame(&projectedWall.frame);
    memset(&projectedWall.frame, 0, sizeof(projectedWall.frame));
    projectedWall.render = NULL;
    projectedWall.originalMediaTexels = NULL;
    projectedWall.originalTexelOffset = 0;
    projectedWall.textureIndex = 0;
    projectedWall.active = 0;
    projectedStats.endCalls++;
}
