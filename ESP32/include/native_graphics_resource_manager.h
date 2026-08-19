#ifndef DOOMRPG_ESP32_NATIVE_GRAPHICS_RESOURCE_MANAGER_H
#define DOOMRPG_ESP32_NATIVE_GRAPHICS_RESOURCE_MANAGER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct Render_s;

typedef struct EspNativeSpriteFrame_s {
    int spriteIndex;
    int paletteOffset;
    int xMin;
    int xMax;
    int yMin;
    int yMax;
    int width;
    int height;
    int pitch;
    uint32_t sourceOffset;
    uint32_t texelOffset;
    uint32_t stexelsReadOffset;
    uint32_t maskBytes;
    uint32_t activePixels;
    uint32_t packedBytes;
    uint32_t storageBytes;
    uint32_t texelHash;
    uint8_t* storage;
    uint8_t* mask;
    uint8_t* texels;
} EspNativeSpriteFrame;

typedef struct EspNativeWallFrame_s {
    int textureIndex;
    int paletteOffset;
    int width;
    int height;
    uint32_t sourceTexelOffset;
    uint32_t wtexelsReadOffset;
    uint32_t packedBytes;
    uint32_t texelHash;
    uint8_t* texels;
} EspNativeWallFrame;

typedef struct EspNativeGraphicsStats_s {
    uint32_t spriteLoads;
    uint32_t wallLoads;
    uint32_t packOpenCycles;
    uint32_t logicalBytesLoaded;
    uint32_t peakFrameBytes;
} EspNativeGraphicsStats;

void EspNativeGraphics_resetStats(void);
void EspNativeGraphics_getStats(EspNativeGraphicsStats* outStats);

int EspNativeGraphics_loadSpriteFrame(struct Render_s* render,
                                      int spriteIndex,
                                      EspNativeSpriteFrame* outFrame);
void EspNativeGraphics_releaseSpriteFrame(EspNativeSpriteFrame* frame);

int EspNativeGraphics_loadWallFrame(struct Render_s* render,
                                    int textureIndex,
                                    EspNativeWallFrame* outFrame);
void EspNativeGraphics_releaseWallFrame(EspNativeWallFrame* frame);

#ifdef __cplusplus
}
#endif

#endif
