#include <SDL.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "DoomRPG.h"
#include "Render.h"

#include "native_graphics_resource_manager.h"
#include "native_projected_sprite_renderer.h"
#include "native_projected_wall_bridge.h"

#define MAX_NATIVE_SPRITE_DIMENSION 64
#define TRACKED_SPRITE_FRAMES 1024
#define TRACKED_SPRITE_WORDS (TRACKED_SPRITE_FRAMES / 32)

static EspNativeProjectedSpriteStats spriteStats;
static uint32_t seenSpriteFrames[TRACKED_SPRITE_WORDS];

static uint32_t fnvMixU32(uint32_t hash, uint32_t value) {
    int shift;
    for (shift = 0; shift < 32; shift += 8) {
        hash ^= (value >> shift) & 0xffU;
        hash *= 16777619U;
    }
    return hash;
}

static void noteSpriteFrameRequest(int objectIndex, int mediaId, int renderMode) {
    uint32_t word;
    uint32_t bit;

    spriteStats.spriteFrameRequests++;
    spriteStats.requestHash = fnvMixU32(spriteStats.requestHash,
                                        (uint32_t)objectIndex);
    spriteStats.requestHash = fnvMixU32(spriteStats.requestHash,
                                        (uint32_t)mediaId);
    spriteStats.requestHash = fnvMixU32(spriteStats.requestHash,
                                        (uint32_t)renderMode);

    if (mediaId >= 0 && mediaId < TRACKED_SPRITE_FRAMES) {
        word = (uint32_t)mediaId >> 5;
        bit = 1U << ((uint32_t)mediaId & 31U);
        if ((seenSpriteFrames[word] & bit) != 0U) {
            spriteStats.repeatedSpriteFrames++;
        }
        else {
            seenSpriteFrames[word] |= bit;
            spriteStats.uniqueSpriteFrames++;
        }
    }

    printf("[MENUSPRITE] REQUEST n=%u object=%d media=%d mode=%d\n",
           (unsigned int)spriteStats.spriteFrameRequests,
           objectIndex,
           mediaId,
           renderMode);
}

static int prepareColumnPrefixes(const EspNativeSpriteFrame* frame,
                                 uint16_t prefix[MAX_NATIVE_SPRITE_DIMENSION + 1]) {
    uint32_t total = 0;
    int x;

    if (frame == NULL || frame->mask == NULL ||
        frame->width <= 0 || frame->width > MAX_NATIVE_SPRITE_DIMENSION ||
        frame->height <= 0 || frame->height > MAX_NATIVE_SPRITE_DIMENSION ||
        frame->pitch <= 0) {
        return 0;
    }

    prefix[0] = 0;
    for (x = 0; x < frame->width; ++x) {
        const uint8_t* column = frame->mask +
            ((uint32_t)x * (uint32_t)frame->pitch);
        int y;

        for (y = 0; y < frame->height; ++y) {
            if ((column[y / 8] & (1U << (y & 7))) != 0U) {
                total++;
            }
        }

        if (total > UINT16_MAX) {
            return 0;
        }
        prefix[x + 1] = (uint16_t)total;
    }

    return total == frame->activePixels;
}

static int applySpanPixel(Render_t* render,
                          uint16_t* pixel,
                          uint16_t sourceColor,
                          int renderMode) {
    int color;

    switch (renderMode) {
    case 0:
        *pixel = sourceColor;
        break;
    case 1:
        *pixel ^= sourceColor;
        break;
    case 2:
        *pixel |= sourceColor;
        break;
    case 3:
        *pixel &= sourceColor;
        break;
    case 4:
        *pixel = (uint16_t)(((int)(*pixel & 0xF7DFU) >> 1) +
                            ((int)(*pixel & 0xE79FU) >> 2) +
                            sourceColor);
        break;
    case 5:
        *pixel = (uint16_t)(sourceColor + ((*pixel & 0xF7DFU) >> 1));
        break;
    case 6:
        *pixel = (uint16_t)(sourceColor +
                            ((int)(*pixel & 0xE79FU) >> 2));
        break;
    case 7:
        color = sourceColor + (*pixel & 0xF7DEU);
        *pixel = (uint16_t)((color & 0xF7DE) |
                            ((color & 0x10820) >> 1) |
                            ((color & 0x10820) >> 2) |
                            ((color & 0x10820) >> 3));
        break;
    case 8:
        color = (*pixel | 0x10820U) - sourceColor;
        *pixel = (uint16_t)(color & ((color | 0xFFFEF7DF) >> 1) &
                            ((color | 0xFFFEF7DF) >> 2) &
                            ((color | 0xFFFEF7DF) >> 3) & 0xF7DE);
        break;
    case 9: {
        int rnd = DoomRPG_randNextByte(&render->doomRpg->random) >> 5;
        unsigned int b = *pixel & 0x1FU;
        unsigned int g = (*pixel >> 5) & 0x3FU;
        unsigned int r = (*pixel >> 11) & 0x1FU;

        b = (3U * (b >> 2)) + (unsigned int)rnd;
        g = (3U * (g >> 2)) + ((unsigned int)rnd << 1);
        r = (3U * (r >> 2)) + (unsigned int)rnd;
        if (b > 31U) b = 31U;
        if (g > 63U) g = 63U;
        if (r > 31U) r = 31U;
        *pixel = (uint16_t)((r << 11) | (g << 5) | b);
        break;
    }
    default:
        spriteStats.unsupportedRenderModes++;
        return 0;
    }

    return 1;
}

static int sampleNativeSpriteSpan(Render_t* render,
                                  const EspNativeSpriteFrame* frame,
                                  int renderMode,
                                  int x,
                                  int y,
                                  int texelPosition,
                                  int texelStep,
                                  int pixelCount) {
    uint16_t* pixels;
    int pitch;
    int remaining;
    int64_t localPosition;
    int64_t basePosition;

    spriteStats.spanCalls++;

    if (render == NULL || frame == NULL || frame->texels == NULL ||
        render->pixels == NULL || render->mediaPalettes == NULL ||
        frame->paletteOffset < 0 ||
        frame->paletteOffset + 15 >= render->mediaPalettesLength) {
        spriteStats.rangeErrors++;
        return 0;
    }

    if (render->mediaTexels != NULL) {
        spriteStats.legacyPointerViolations++;
    }
    if (render->shapeData != NULL) {
        spriteStats.legacyShapeViolations++;
    }

    if (pixelCount <= 0 || render->skipStretch != 0) {
        return 1;
    }

    pitch = render->pitch >> 1;
    pixels = render->pixels + pitch * y + x;
    basePosition = ((int64_t)frame->texelOffset) << 12;
    localPosition = (int64_t)texelPosition - basePosition;
    remaining = pixelCount;

    while (remaining-- > 0) {
        uint16_t sourceColor = 0;

        if (renderMode != 9) {
            uint32_t packedIndex;
            uint8_t packed;
            int paletteIndex;
            int nibbleShift;

            if (localPosition < 0) {
                spriteStats.rangeErrors++;
                return 0;
            }

            packedIndex = (uint32_t)(localPosition >> 13);
            if (packedIndex >= frame->packedBytes) {
                spriteStats.rangeErrors++;
                return 0;
            }

            packed = frame->texels[packedIndex];
            nibbleShift = (int)((localPosition >> 10) & 4);
            paletteIndex = (packed >> nibbleShift) & 0x0f;
            sourceColor = render->spanPalettes[paletteIndex];
        }

        if (!applySpanPixel(render, pixels, sourceColor, renderMode)) {
            return 0;
        }

        pixels += pitch;
        localPosition += texelStep;
        spriteStats.pixelsDrawn++;
    }

    return 1;
}

static int drawNativeSpriteSpans(Render_t* render,
                                 Line_t* line,
                                 const EspNativeSpriteFrame* frame,
                                 int renderMode) {
    uint16_t prefix[MAX_NATIVE_SPRITE_DIMENSION + 1];
    int i, i2, i3, i4, i5, i6, i7, i8;

    if (!prepareColumnPrefixes(frame, prefix)) {
        printf("[MENUSPRITE] FAILED invalid native column-prefix contract media=%d\n",
               frame != NULL ? frame->spriteIndex : -1);
        spriteStats.rangeErrors++;
        return 0;
    }

    if (render->mediaBitShapeOffsets == NULL ||
        render->mediaBitShapeOffsets[frame->spriteIndex * 2] !=
            (int)frame->sourceOffset ||
        render->mediaBitShapeOffsets[frame->spriteIndex * 2 + 1] !=
            frame->paletteOffset) {
        spriteStats.mappingViolations++;
        return 0;
    }

    Render_getSpanMode(render, frame->paletteOffset, (byte)renderMode);

    i = line->vert2.x - line->vert1.x;
    if (i <= 0) {
        return 1;
    }

    render->spriteRasterCount++;

#if FIXED_VERSION == 1
    i2 = (MAXINT / i) << 1;
    i3 = (int)DoomRPG_FixedMul((line->vert2.y - line->vert1.y), i2);
    i4 = (int)DoomRPG_FixedMul((line->vert2.z - line->vert1.z), i2);
#else
    i2 = (MAXINT / i) << 1;
    i3 = (int)((((int64_t)(line->vert2.y - line->vert1.y)) *
                (int64_t)i2) >> 16);
    i4 = (int)((((int64_t)(line->vert2.z - line->vert1.z)) *
                (int64_t)i2) >> 16);
#endif

    i5 = (line->vert1.x + 65535) >> 16;
    i6 = (line->vert2.x + 65535) >> 16;
    if (render->screenLeft > i5) i5 = render->screenLeft;
    if (render->screenRight < i6) i6 = render->screenRight;

#if FIXED_VERSION == 1
    {
        int j = (i5 << 16) - line->vert1.x;
        i7 = line->vert1.z + DoomRPG_FixedMul(j, i4);
        i8 = line->vert1.y + DoomRPG_FixedMul(j, i3);
    }
#else
    {
        int64_t j = (int64_t)((i5 << 16) - line->vert1.x);
        i7 = line->vert1.z + (int)((j * (int64_t)i4) >> 16);
        i8 = line->vert1.y + (int)((j * (int64_t)i3) >> 16);
    }
#endif

    while (i5 < i6) {
        int i14 = (0x40000000U / i8) << 2;
        int i15;

#if FIXED_VERSION == 1
        i15 = (int)(DoomRPG_FixedMul(i7, i14) >> 16);
#else
        i15 = (int)(((int64_t)i7 * (int64_t)i14) >> 32);
#endif

        i8 += i3;
        i7 += i4;

        if (render->columnScale[i5] >= i14) {
            int i16 = i14 >> 3;
            const uint8_t* column;
            uint32_t activeCursor;
            int y = 0;

            if (i15 < 0 || i15 >= frame->width) {
                spriteStats.rangeErrors++;
                return 0;
            }

            column = frame->mask +
                ((uint32_t)i15 * (uint32_t)frame->pitch);
            activeCursor = prefix[i15];

            while (y < frame->height) {
                int runStart;
                int runLength;
                uint32_t runActiveBase;
                int i19;
                int i22;
                int i23;
                int i24;

                while (y < frame->height &&
                       (column[y / 8] & (1U << (y & 7))) == 0U) {
                    y++;
                }
                if (y >= frame->height) {
                    break;
                }

                runStart = y;
                runActiveBase = activeCursor;
                while (y < frame->height &&
                       (column[y / 8] & (1U << (y & 7))) != 0U) {
                    activeCursor++;
                    y++;
                }
                runLength = y - runStart;

                i19 = (64 - (frame->yMin + runStart)) - render->viewZ;
                i22 = (runLength * i8) >> 17;
                i23 = render->halfScreenHeight - ((i19 * i8) >> 17);
                i24 = (int)((frame->texelOffset + runActiveBase) << 12);

                if (i23 < render->screenTop) {
                    i24 -= i16 * (i23 - render->screenTop);
                    i22 += i23 - render->screenTop;
                    i23 = render->screenTop;
                }
                if (i23 + i22 > render->screenBottom) {
                    i22 = render->screenBottom - i23;
                }

                if (!sampleNativeSpriteSpan(render, frame, renderMode,
                                            i5, i23, i24, i16, i22)) {
                    return 0;
                }
            }

            if (activeCursor != prefix[i15 + 1]) {
                spriteStats.rangeErrors++;
                return 0;
            }
        }

        i5++;
    }

    return 1;
}

/* 0=fatal, 1=valid but culled, 2=projected and ready. */
static int projectWorldLine(Render_t* render, Line_t* line) {
    Vertex_t swap;

    if (((line->vert1.x - render->viewX) *
         (line->vert2.y - line->vert1.y)) +
        ((line->vert1.y - render->viewY) *
         (-(line->vert2.x - line->vert1.x))) <= 0) {
        if ((line->flags & 1) == 0) {
            spriteStats.backfaceCulled++;
            return 1;
        }
        swap = line->vert1;
        line->vert1 = line->vert2;
        line->vert2 = swap;
    }

    Render_transform2DVerts(render, &line->vert1);
    Render_transform2DVerts(render, &line->vert2);

    if (!Render_clipLine(render, line)) {
        spriteStats.clipCulled++;
        return 1;
    }

    Render_projectVertex(render, &line->vert1);
    Render_projectVertex(render, &line->vert2);
    return 2;
}

static int projectTransformedBillboard(Render_t* render, Line_t* line) {
    if (!Render_clipLine(render, line)) {
        spriteStats.clipCulled++;
        return 1;
    }
    Render_projectVertex(render, &line->vert1);
    Render_projectVertex(render, &line->vert2);
    return 2;
}

static int buildOrientedLine(Line_t* line,
                             int x,
                             int y,
                             int minBound,
                             int maxBound,
                             int info) {
    if ((info & 0x80000) != 0) {
        if ((info & 0x2000000) != 0) {
            line->vert1.x = x - minBound;
            line->vert2.x = x - maxBound;
        }
        else {
            line->vert1.x = x + minBound;
            line->vert2.x = x + maxBound;
        }
        line->vert1.y = y;
        line->vert2.y = y;
    }
    else if ((info & 0x100000) != 0) {
        if ((info & 0x2000000) != 0) {
            line->vert1.x = x + minBound;
            line->vert2.x = x + maxBound;
        }
        else {
            line->vert1.x = x - minBound;
            line->vert2.x = x - maxBound;
        }
        line->vert1.y = y;
        line->vert2.y = y;
    }
    else if ((info & 0x400000) != 0) {
        if ((info & 0x2000000) != 0) {
            line->vert1.y = y - minBound;
            line->vert2.y = y - maxBound;
        }
        else {
            line->vert1.y = y + minBound;
            line->vert2.y = y + maxBound;
        }
        line->vert1.x = x;
        line->vert2.x = x;
    }
    else if ((info & 0x200000) != 0) {
        if ((info & 0x2000000) != 0) {
            line->vert1.y = y + minBound;
            line->vert2.y = y + maxBound;
        }
        else {
            line->vert1.y = y - minBound;
            line->vert2.y = y - maxBound;
        }
        line->vert1.x = x;
        line->vert2.x = x;
    }
    else {
        return 0;
    }

    return 1;
}

static int drawBitshapeRequest(Render_t* render,
                               int x,
                               int y,
                               int mediaId,
                               int info,
                               int renderMode,
                               int objectIndex) {
    EspNativeSpriteFrame frame;
    Line_t line;
    Vertex_t center;
    int minBound;
    int maxBound;
    int projectionResult;
    int standardBillboard = (info & 0x780000) == 0;
    int result = 0;

    spriteStats.resolvedDrawCalls++;

    if ((info & 0x60000000) != 0) {
        spriteStats.unsupportedFlagPaths++;
        printf("[MENUSPRITE] FAILED unsupported BREW flag path object=%d info=%08x\n",
               objectIndex, (unsigned int)info);
        return 0;
    }

    memset(&center, 0, sizeof(center));
    if (standardBillboard) {
        center.x = x;
        center.y = y;
        Render_transform2DVerts(render, &center);
        center.x -= 0x100000;
        if (center.x < 0x40000) {
            spriteStats.nearCulled++;
            return 1;
        }
    }

    noteSpriteFrameRequest(objectIndex, mediaId, renderMode);
    memset(&frame, 0, sizeof(frame));
    if (!EspNativeGraphics_loadSpriteFrame(render, mediaId, &frame)) {
        printf("[MENUSPRITE] FAILED GFXRM sprite frame object=%d media=%d\n",
               objectIndex, mediaId);
        return 0;
    }

    if (frame.storageBytes > spriteStats.maxFrameBytes) {
        spriteStats.maxFrameBytes = frame.storageBytes;
    }

    minBound = frame.xMin - 32;
    maxBound = frame.xMax - 32;

    memset(&line, 0, sizeof(line));
    line.texture = (short)mediaId;
    line.flags = 2;
    line.vert1.z = 0;
    line.vert2.z = maxBound - minBound;
    if ((info & 0x8000000) != 0) {
        line.flags |= 1;
    }

    if (standardBillboard) {
        line.vert1 = center;
        line.vert1.z = 0;
        line.vert2.x = center.x;
        line.vert2.y = center.y + (maxBound << 16);
        line.vert2.z = maxBound - minBound;
        line.vert1.y += minBound << 16;
        projectionResult = projectTransformedBillboard(render, &line);
    }
    else {
        if (!buildOrientedLine(&line, x, y, minBound, maxBound, info)) {
            spriteStats.unsupportedFlagPaths++;
            printf("[MENUSPRITE] FAILED unsupported orientation object=%d info=%08x\n",
                   objectIndex, (unsigned int)info);
            goto cleanup;
        }
        projectionResult = projectWorldLine(render, &line);
    }

    if (projectionResult == 0) {
        goto cleanup;
    }
    if (projectionResult == 1) {
        result = 1;
        goto cleanup;
    }

    render->spanMode = (byte)renderMode;
    if (!drawNativeSpriteSpans(render, &line, &frame, renderMode)) {
        printf("[MENUSPRITE] FAILED projected spans object=%d media=%d mode=%d\n",
               objectIndex, mediaId, renderMode);
        goto cleanup;
    }

    result = 1;

cleanup:
    EspNativeGraphics_releaseSpriteFrame(&frame);
    return result;
}

static int drawWallBackedRequest(Render_t* render,
                                 int x,
                                 int y,
                                 int textureIndex,
                                 int info,
                                 int renderMode,
                                 int objectIndex) {
    Line_t line;
    Vertex_t center;
    int projectionResult;
    int standardBillboard = (info & 0x780000) == 0;

    spriteStats.resolvedDrawCalls++;
    if (renderMode != 0) {
        spriteStats.unsupportedRenderModes++;
        printf("[MENUSPRITE] FAILED wall-backed object=%d texture=%d mode=%d unsupported\n",
               objectIndex, textureIndex, renderMode);
        return 0;
    }

    memset(&line, 0, sizeof(line));
    memset(&center, 0, sizeof(center));
    line.texture = (short)textureIndex;
    line.flags = 0;
    line.vert1.z = 0;
    line.vert2.z = 64;
    if ((info & 0x20000000) != 0) line.flags = (int)0x80000000U;
    if ((info & 0x40000000) != 0) line.flags = 0x40000000;
    if ((info & 0x8000000) != 0) line.flags |= 1;

    if (standardBillboard) {
        center.x = x;
        center.y = y;
        Render_transform2DVerts(render, &center);
        center.x -= 0x100000;
        if (center.x < 0x40000) {
            spriteStats.nearCulled++;
            return 1;
        }
        line.vert1 = center;
        line.vert1.z = 0;
        line.vert2.x = center.x;
        line.vert2.y = center.y + (32 << 16);
        line.vert2.z = 64;
        line.vert1.y += -32 << 16;
        projectionResult = projectTransformedBillboard(render, &line);
    }
    else {
        if (!buildOrientedLine(&line, x, y, -32, 32, info)) {
            spriteStats.unsupportedFlagPaths++;
            return 0;
        }
        projectionResult = projectWorldLine(render, &line);
    }

    if (projectionResult != 2) {
        return projectionResult != 0;
    }

    spriteStats.wallBackedRequests++;
    render->spanMode = 0;
    if (!EspNativeProjectedWall_begin(render, textureIndex)) {
        printf("[MENUSPRITE] FAILED wall-backed GFXRM object=%d texture=%d\n",
               objectIndex, textureIndex);
        return 0;
    }
    if (!EspNativeProjectedWall_drawWallSpans(render, &line)) {
        EspNativeProjectedWall_end();
        printf("[MENUSPRITE] FAILED wall-backed spans object=%d texture=%d\n",
               objectIndex, textureIndex);
        return 0;
    }
    EspNativeProjectedWall_end();
    return 1;
}

void EspNativeProjectedSprite_resetStats(void) {
    memset(&spriteStats, 0, sizeof(spriteStats));
    memset(seenSpriteFrames, 0, sizeof(seenSpriteFrames));
    spriteStats.requestHash = 2166136261U;
}

void EspNativeProjectedSprite_getStats(EspNativeProjectedSpriteStats* outStats) {
    if (outStats != NULL) {
        *outStats = spriteStats;
    }
}

int EspNativeProjectedSprite_drawObject(struct Render_s* renderBase,
                                        struct Sprite_s* spriteBase,
                                        int objectIndex,
                                        int renderFloorCeilingTextures) {
    Render_t* render = (Render_t*)renderBase;
    Sprite_t* sprite = (Sprite_t*)spriteBase;
    int logicalId;
    int anim;
    int mediaId;

    if (render == NULL || sprite == NULL ||
        render->mediaSpriteIds == NULL || render->mediaTexturesIds == NULL ||
        render->mediaBitShapeOffsets == NULL || render->mediaPalettes == NULL) {
        return 0;
    }

    spriteStats.objectCalls++;

    if ((sprite->info & SPRITE_FLAG_HIDDEN) != 0) {
        spriteStats.hiddenObjects++;
        return 1;
    }

    if (sprite->ent != NULL) {
        spriteStats.entityObjectsUnsupported++;
        printf("[MENUSPRITE] FAILED deterministic menu probe found linked entity object=%d\n",
               objectIndex);
        return 0;
    }

    logicalId = sprite->info & 511;
    anim = (sprite->info & 0x1E00) >> 9;
    if ((sprite->info & (int)0x80000000U) == 0) {
        anim = (render->animFrameTime / 250) % 4;
    }

    if (logicalId == 137 && !renderFloorCeilingTextures) {
        spriteStats.lightObjectsSkipped++;
        return 1;
    }

    printf("[MENUSPRITE] OBJECT index=%d sortZ=%d logical=%d anim=%d pos=%d,%d info=%08x mode=%u\n",
           objectIndex,
           sprite->sortZ,
           logicalId,
           anim,
           sprite->x,
           sprite->y,
           (unsigned int)sprite->info,
           (unsigned int)sprite->renderMode);

    render->damageBlend = false;

    if ((sprite->info & 0x40000) != 0) {
        if (logicalId < 0 || logicalId >= render->textureCnt) {
            return 0;
        }
        mediaId = render->mediaTexturesIds[logicalId] + anim;
        return drawWallBackedRequest(render,
                                     sprite->x, sprite->y,
                                     mediaId, sprite->info,
                                     sprite->renderMode, objectIndex);
    }

    if (logicalId < 0 || logicalId >= render->spriteCnt) {
        return 0;
    }

    if ((sprite->info & 0x4000000) != 0) {
        if (logicalId == 0) {
            return 0;
        }
        mediaId = render->mediaSpriteIds[logicalId - 1] + anim;
        if (!drawBitshapeRequest(render, sprite->x + 16, sprite->y,
                                 mediaId, sprite->info, sprite->renderMode,
                                 objectIndex) ||
            !drawBitshapeRequest(render, sprite->x - 16, sprite->y,
                                 mediaId, sprite->info, sprite->renderMode,
                                 objectIndex) ||
            !drawBitshapeRequest(render, sprite->x, sprite->y + 16,
                                 mediaId, sprite->info, sprite->renderMode,
                                 objectIndex) ||
            !drawBitshapeRequest(render, sprite->x, sprite->y - 16,
                                 mediaId, sprite->info, sprite->renderMode,
                                 objectIndex)) {
            return 0;
        }
    }
    else {
        mediaId = render->mediaSpriteIds[logicalId] + anim;
        if (!drawBitshapeRequest(render, sprite->x, sprite->y,
                                 mediaId, sprite->info, sprite->renderMode,
                                 objectIndex)) {
            return 0;
        }
    }

    if (logicalId == 135 || logicalId == 140) {
        mediaId = render->mediaSpriteIds[136] + anim;
        if (!drawBitshapeRequest(render, sprite->x, sprite->y,
                                 mediaId, sprite->info, 7, objectIndex)) {
            return 0;
        }
    }
    else if (logicalId == 131) {
        mediaId = render->mediaSpriteIds[144] + anim;
        if (!drawBitshapeRequest(render, sprite->x, sprite->y,
                                 mediaId, sprite->info, 7, objectIndex)) {
            return 0;
        }
    }

    return 1;
}
