#include <SDL.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "DoomRPG.h"
#include "Render.h"

#include "esp_asset_pack.h"
#include "esp_map_runtime.h"
#include "esp_map_sprite_topology.h"
#include "esp_native_bsp_visibility.h"
#include "esp_native_graphics_catalog.h"
#include "esp_native_junction_sprite_renderer.h"
#include "esp_player_view_state.h"

#define MAP_HEADER 16U
#define PALETTE_HEADER 4U
#define TEXEL_HEADER 4U
#define PAIR_BYTES 8U
#define SHAPE_HEADER 12U
#define MAX_VISIBLE_SPRITES 64U
#define MAX_DIM 64
#define MAX_MASK 512U
#define MAX_TEXELS 2048U
#define SCREEN_W 160
#define SCREEN_H 80

#define VISUAL_MASK 0x0001fe00UL
#define HIDDEN 0x00010000UL
#define TILE 0x00040000UL
#define CROSS 0x04000000UL
#define SKIP_RESOURCE 0x20000000UL
#define FIXED_ANIM 0x80000000UL
#define SORT_BIAS 0x01000000UL
#define FLIP_HORIZONTAL 0x02000000UL
#define TWO_SIDED 0x08000000UL
#define ORIENT_NORTH 0x00080000UL
#define ORIENT_SOUTH 0x00100000UL
#define ORIENT_EAST 0x00200000UL
#define ORIENT_WEST 0x00400000UL
#define ORIENT_MASK 0x00780000UL
#define ENEMY_TYPE 1U
#define RENDER_MODE_NORMAL 0U
#define RENDER_MODE_ADD 7U

typedef struct Sources_s {
    EspAssetPackEntry mappings;
    EspAssetPackEntry palettes;
    EspAssetPackEntry bitshapes;
    EspAssetPackEntry wtexels;
    EspAssetPackEntry stexels;
    uint32_t texelPairs;
    uint32_t bitShapePairs;
    uint32_t textureIds;
    uint32_t spriteIds;
    uint32_t spritePairBase;
    uint32_t spriteIdBase;
    uint32_t paletteEntries;
    uint32_t wallBytes;
    uint32_t spriteBytes;
} Sources;

typedef struct Frame_s {
    uint16_t logical;
    uint16_t actual;
    uint32_t texelOffset;
    int xMin;
    int xMax;
    int yMin;
    int yMax;
    int width;
    int height;
    int pitch;
    uint32_t maskBytes;
    uint32_t active;
    uint32_t packedBytes;
    uint16_t palette[16];
    uint16_t prefix[MAX_DIM + 1];
    uint8_t mask[MAX_MASK];
    uint8_t texels[MAX_TEXELS];
} Frame;

typedef struct Order_s {
    uint16_t index;
    uint16_t logical;
    uint32_t info;
    int32_t sortZ;
} Order;

typedef struct Scratch_s {
    int viewCos_;
    int viewSin_;
    int viewTransX;
    int viewSin;
    int viewCos;
    int viewTransY;
    int viewX;
    int viewY;
    int viewZ;
    int viewAngle;
    int lineCount;
    int lineRasterCount;
    int nodeCount;
    int nodeRasterCount;
    int spriteCount;
    int spriteRasterCount;
    int screenLeft;
    int screenTop;
    int screenRight;
    int screenBottom;
    int numLines;
    byte spanMode;
    short* pixels;
    Line_t tmpLine;
    int columnScale[SCREEN_W];
} Scratch;

typedef struct SpriteWorkspace_s {
    Frame frame;
    Order order[MAX_VISIBLE_SPRITES];
    uint32_t seenLogical[8];
    EspNativeBspVisibilityState visibility;
} SpriteWorkspace;

static uint16_t le16(const uint8_t* p) {
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static uint32_t le32(const uint8_t* p) {
    return (uint32_t)p[0] |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static uint32_t fnv(uint32_t h, const void* data, uint32_t n) {
    const uint8_t* p = (const uint8_t*)data;
    uint32_t i;
    for (i = 0U; i < n; ++i) {
        h ^= p[i];
        h *= 16777619U;
    }
    return h;
}

/* Keep the same source-palette conversion as the hardware-proven wall/base
 * sprite path. This milestone changes dependency ownership/order only. */
static uint16_t source565(uint16_t c) {
    return (uint16_t)(((c & 0x001fU) << 11) |
                      (c & 0x07e0U) |
                      ((c & 0xf800U) >> 11));
}

static uint8_t spriteRenderMode(uint16_t logical) {
    return (logical == 136U || logical == 137U || logical == 144U)
               ? RENDER_MODE_ADD
               : RENDER_MODE_NORMAL;
}

static uint16_t glowFor(uint16_t logical) {
    if (logical == 135U || logical == 140U) return 136U;
    if (logical == 131U) return 144U;
    return 0U;
}

static int isLegacyCrossLogical(uint16_t logical) {
    return logical >= 82U && logical <= 90U && (logical & 1U) == 0U;
}

static int readRange(const EspAssetPackEntry* entry,
                     uint32_t offset,
                     void* destination,
                     uint32_t bytes,
                     EspNativeJunctionSpriteStats* stats) {
    if (entry == NULL || destination == NULL || stats == NULL ||
        offset > entry->size || bytes > entry->size - offset ||
        !EspAssetPack_readRange(entry, offset, destination, bytes)) {
        return 0;
    }
    ++stats->packReads;
    return 1;
}

static void saveScratch(Render_t* render, Scratch* scratch) {
    memset(scratch, 0, sizeof(*scratch));
    scratch->viewCos_ = render->viewCos_;
    scratch->viewSin_ = render->viewSin_;
    scratch->viewTransX = render->viewTransX;
    scratch->viewSin = render->viewSin;
    scratch->viewCos = render->viewCos;
    scratch->viewTransY = render->viewTransY;
    scratch->viewX = render->viewX;
    scratch->viewY = render->viewY;
    scratch->viewZ = render->viewZ;
    scratch->viewAngle = render->viewAngle;
    scratch->lineCount = render->lineCount;
    scratch->lineRasterCount = render->lineRasterCount;
    scratch->nodeCount = render->nodeCount;
    scratch->nodeRasterCount = render->nodeRasterCount;
    scratch->spriteCount = render->spriteCount;
    scratch->spriteRasterCount = render->spriteRasterCount;
    scratch->screenLeft = render->screenLeft;
    scratch->screenTop = render->screenTop;
    scratch->screenRight = render->screenRight;
    scratch->screenBottom = render->screenBottom;
    scratch->numLines = render->numLines;
    scratch->spanMode = render->spanMode;
    scratch->pixels = render->pixels;
    scratch->tmpLine = render->tmpLine;
    memcpy(scratch->columnScale, render->columnScale,
           sizeof(scratch->columnScale));
}

static void restoreScratch(Render_t* render, const Scratch* scratch) {
    render->viewCos_ = scratch->viewCos_;
    render->viewSin_ = scratch->viewSin_;
    render->viewTransX = scratch->viewTransX;
    render->viewSin = scratch->viewSin;
    render->viewCos = scratch->viewCos;
    render->viewTransY = scratch->viewTransY;
    render->viewX = scratch->viewX;
    render->viewY = scratch->viewY;
    render->viewZ = scratch->viewZ;
    render->viewAngle = scratch->viewAngle;
    render->lineCount = scratch->lineCount;
    render->lineRasterCount = scratch->lineRasterCount;
    render->nodeCount = scratch->nodeCount;
    render->nodeRasterCount = scratch->nodeRasterCount;
    render->spriteCount = scratch->spriteCount;
    render->spriteRasterCount = scratch->spriteRasterCount;
    render->screenLeft = scratch->screenLeft;
    render->screenTop = scratch->screenTop;
    render->screenRight = scratch->screenRight;
    render->screenBottom = scratch->screenBottom;
    render->numLines = scratch->numLines;
    render->spanMode = scratch->spanMode;
    render->pixels = scratch->pixels;
    render->tmpLine = scratch->tmpLine;
    memcpy(render->columnScale, scratch->columnScale,
           sizeof(scratch->columnScale));
}

static int setupDrawView(Render_t* render,
                         const EspPlayerViewState* view,
                         const EspNativeBspVisibilityState* visibility) {
    int sin_;
    int cos_;
    int viewX;
    int viewY;

    if (render == NULL || view == NULL || visibility == NULL ||
        render->framebuffer == NULL || render->columnScale == NULL ||
        render->screenWidth != SCREEN_W || render->screenHeight != SCREEN_H ||
        render->screenX != 0 || render->screenY != 20) {
        return 0;
    }

    sin_ = render->sinTable[view->viewAngle & 255];
    cos_ = render->sinTable[(view->viewAngle + 64) & 255];
    viewX = view->viewX - ((16 * cos_) >> 16);
    viewY = view->viewY + ((16 * sin_) >> 16);

    render->viewX = viewX;
    render->viewY = viewY;
    render->viewZ = view->viewZ;
    render->viewCos_ = cos_;
    render->viewSin_ = -sin_;
    render->viewTransX = -((viewX * render->viewCos_) +
                           (viewY * render->viewSin_));
    render->viewSin = sin_;
    render->viewCos = cos_;
    render->viewTransY = -((viewX * render->viewSin) +
                           (viewY * render->viewCos));
    render->viewAngle = view->viewAngle;
    render->pixels = (short*)&render->framebuffer[render->pitch * render->screenY];
    render->screenLeft = 0;
    render->screenTop = 0;
    render->screenRight = SCREEN_W;
    render->screenBottom = SCREEN_H;
    render->lineCount = 0;
    render->lineRasterCount = 0;
    render->nodeCount = 0;
    render->nodeRasterCount = 0;
    render->spriteCount = 0;
    render->spriteRasterCount = 0;
    render->spanMode = 0;
    memcpy(render->columnScale, visibility->columnScale,
           sizeof(visibility->columnScale));
    return 1;
}

static int initSources(Sources* sources, EspNativeJunctionSpriteStats* stats) {
    uint8_t mappingHeader[16];
    uint8_t paletteHeader[4];
    uint8_t wallHeader[4];
    uint8_t spriteHeader[4];
    uint32_t paletteBytes;
    uint32_t textureIdBase;
    uint64_t expected;

    memset(sources, 0, sizeof(*sources));
    if (!EspAssetPack_findEntry("mappings.bin", &sources->mappings) ||
        !EspAssetPack_findEntry("palettes.bin", &sources->palettes) ||
        !EspAssetPack_findEntry("bitshapes.bin", &sources->bitshapes) ||
        !EspAssetPack_findEntry("wtexels.bin", &sources->wtexels) ||
        !EspAssetPack_findEntry("stexels.bin", &sources->stexels) ||
        !readRange(&sources->mappings, 0U, mappingHeader,
                   sizeof(mappingHeader), stats) ||
        !readRange(&sources->palettes, 0U, paletteHeader,
                   sizeof(paletteHeader), stats) ||
        !readRange(&sources->wtexels, 0U, wallHeader,
                   sizeof(wallHeader), stats) ||
        !readRange(&sources->stexels, 0U, spriteHeader,
                   sizeof(spriteHeader), stats)) {
        return 0;
    }

    sources->texelPairs = le32(mappingHeader);
    sources->bitShapePairs = le32(mappingHeader + 4U);
    sources->textureIds = le32(mappingHeader + 8U);
    sources->spriteIds = le32(mappingHeader + 12U);
    paletteBytes = le32(paletteHeader);
    sources->wallBytes = le32(wallHeader);
    sources->spriteBytes = le32(spriteHeader);
    sources->spritePairBase = MAP_HEADER + sources->texelPairs * PAIR_BYTES;
    textureIdBase = sources->spritePairBase +
                    sources->bitShapePairs * PAIR_BYTES;
    sources->spriteIdBase = textureIdBase + sources->textureIds * 2U;
    expected = (uint64_t)sources->spriteIdBase +
               (uint64_t)sources->spriteIds * 2U;

    if (sources->texelPairs == 0U || sources->bitShapePairs == 0U ||
        sources->spriteIds == 0U || sources->texelPairs > 4096U ||
        sources->bitShapePairs > 4096U || sources->spriteIds > 4096U ||
        expected != sources->mappings.size || (paletteBytes & 1U) != 0U ||
        paletteBytes + PALETTE_HEADER != sources->palettes.size ||
        sources->wallBytes + TEXEL_HEADER != sources->wtexels.size ||
        sources->spriteBytes + TEXEL_HEADER != sources->stexels.size ||
        sources->wallBytes > UINT32_MAX / 2U) {
        return 0;
    }

    sources->paletteEntries = paletteBytes / 2U;
    return 1;
}

static int loadFrame(const Sources* sources,
                     uint16_t logical,
                     uint32_t animation,
                     int glow,
                     Frame* frame,
                     uint32_t seenLogical[8],
                     EspNativeJunctionSpriteStats* stats) {
    uint8_t idBytes[2];
    uint8_t pair[8];
    uint8_t header[12];
    uint8_t palette[32];
    uint32_t actual;
    uint32_t shapeOffset;
    uint32_t maskOffset;
    int32_t sourceOffset;
    int32_t paletteOffset;
    uint32_t x;
    uint32_t active = 0U;
    uint32_t base;
    uint32_t relative;
    uint32_t spriteTexelOffset;
    uint32_t p;
    uint32_t loadedBytes;

    if (logical >= sources->spriteIds || logical >= 256U ||
        EspNativeGraphicsCatalog_findSprite(logical) == NULL) {
        return 0;
    }

    memset(frame, 0, sizeof(*frame));
    if (!readRange(&sources->mappings,
                   sources->spriteIdBase + (uint32_t)logical * 2U,
                   idBytes, sizeof(idBytes), stats)) {
        return 0;
    }

    actual = (uint32_t)le16(idBytes) + animation;
    if (actual >= sources->bitShapePairs || actual > UINT16_MAX ||
        !readRange(&sources->mappings,
                   sources->spritePairBase + actual * PAIR_BYTES,
                   pair, sizeof(pair), stats)) {
        return 0;
    }

    sourceOffset = (int32_t)le32(pair);
    paletteOffset = (int32_t)le32(pair + 4U);
    if (sourceOffset < 0 || paletteOffset < 0 ||
        (uint32_t)paletteOffset > sources->paletteEntries ||
        16U > sources->paletteEntries - (uint32_t)paletteOffset) {
        return 0;
    }

    shapeOffset = TEXEL_HEADER + (uint32_t)sourceOffset;
    if (!readRange(&sources->bitshapes, shapeOffset, header,
                   sizeof(header), stats) ||
        !readRange(&sources->palettes,
                   PALETTE_HEADER + (uint32_t)paletteOffset * 2U,
                   palette, sizeof(palette), stats)) {
        return 0;
    }

    frame->logical = logical;
    frame->actual = (uint16_t)actual;
    frame->texelOffset = le32(header);
    frame->xMin = header[8];
    frame->xMax = header[9];
    frame->yMin = header[10];
    frame->yMax = header[11];
    if (frame->xMax < frame->xMin || frame->yMax < frame->yMin) return 0;

    frame->width = frame->xMax - frame->xMin + 1;
    frame->height = frame->yMax - frame->yMin + 1;
    frame->pitch = (frame->height + 7) / 8;
    if (frame->width <= 0 || frame->width > MAX_DIM ||
        frame->height <= 0 || frame->height > MAX_DIM) {
        return 0;
    }

    frame->maskBytes = (uint32_t)frame->width * (uint32_t)frame->pitch;
    if (frame->maskBytes == 0U || frame->maskBytes > MAX_MASK) return 0;
    maskOffset = shapeOffset + SHAPE_HEADER;
    if (!readRange(&sources->bitshapes, maskOffset, frame->mask,
                   frame->maskBytes, stats)) {
        return 0;
    }

    frame->prefix[0] = 0U;
    for (x = 0U; x < (uint32_t)frame->width; ++x) {
        const uint8_t* column = frame->mask + x * (uint32_t)frame->pitch;
        int y;
        for (y = 0; y < frame->height; ++y) {
            if ((column[y / 8] & (1U << (y & 7))) != 0U) ++active;
        }
        frame->prefix[x + 1U] = (uint16_t)active;
    }

    frame->active = active;
    frame->packedBytes = ((active + 1U) & ~1U) / 2U;
    if (frame->packedBytes == 0U || frame->packedBytes > MAX_TEXELS) return 0;

    base = sources->wallBytes * 2U;
    if (frame->texelOffset < base ||
        ((frame->texelOffset - base) & 1U) != 0U) {
        return 0;
    }
    relative = frame->texelOffset - base;
    spriteTexelOffset = TEXEL_HEADER + relative / 2U;
    if (!readRange(&sources->stexels, spriteTexelOffset, frame->texels,
                   frame->packedBytes, stats)) {
        return 0;
    }

    for (p = 0U; p < 16U; ++p) {
        frame->palette[p] = source565(le16(palette + p * 2U));
    }

    loadedBytes = frame->maskBytes + frame->packedBytes;
    if (glow) {
        ++stats->glowFrameLoads;
        stats->glowFrameBytes += loadedBytes;
        if (loadedBytes > stats->glowMaxFrameBytes) {
            stats->glowMaxFrameBytes = loadedBytes;
        }
    }
    else {
        uint32_t word;
        uint32_t bit;
        ++stats->frameLoads;
        stats->frameBytes += loadedBytes;
        if (loadedBytes > stats->maxFrameBytes) stats->maxFrameBytes = loadedBytes;
        word = (uint32_t)logical >> 5;
        bit = 1U << ((uint32_t)logical & 31U);
        if ((seenLogical[word] & bit) == 0U) {
            seenLogical[word] |= bit;
            ++stats->uniqueLogical;
        }
    }
    return 1;
}

static int buildOrder(Render_t* render,
                      const EspMapRuntimeView* runtime,
                      const EspNativeBspVisibilityState* visibility,
                      Order order[MAX_VISIBLE_SPRITES],
                      EspNativeJunctionSpriteStats* stats,
                      uint32_t* outCount) {
    uint32_t i;
    uint32_t count = 0U;
    uint32_t hash = 2166136261U;

    if (runtime == NULL || visibility == NULL || stats == NULL ||
        outCount == NULL) {
        return 0;
    }

    for (i = 0U; i < runtime->mapSpriteCount; ++i) {
        EspMapSprite sprite;
        uint8_t visual;
        uint8_t type;
        uint8_t subtype;
        uint16_t link;
        uint16_t ordinal;
        uint32_t info;
        uint32_t id;
        uint16_t resourceLogical;
        int recoveredCross;
        uint32_t leaf = UINT32_MAX;
        uint32_t position;
        int visible;
        int32_t sortZ;

        if (!EspMapRuntime_getMapSprite(i, &sprite) ||
            !EspMapSpriteTopology_getVisualState(i, &visual) ||
            !EspMapSpriteTopology_getEntity(i, &type, &subtype, &link, &ordinal)) {
            return 0;
        }
        (void)subtype;
        (void)link;
        (void)ordinal;
        ++stats->objects;

        info = (sprite.info & ~VISUAL_MASK) | ((uint32_t)visual << 9);
        id = info & 511U;
        if ((info & HIDDEN) != 0U) {
            ++stats->hidden;
            continue;
        }

        visible = EspNativeBspVisibility_mapSpriteVisible(visibility, i, &leaf);
        if (!visible) {
            if (leaf == UINT32_MAX) return 0;
            ++stats->bspRejected;
            continue;
        }
        ++stats->bspCandidates;

        recoveredCross = isLegacyCrossLogical((uint16_t)id);
        if (recoveredCross) info |= CROSS;
        resourceLogical = recoveredCross ? (uint16_t)(id - 1U) : (uint16_t)id;
        if ((info & (TILE | SKIP_RESOURCE)) != 0U ||
            ((info & CROSS) != 0U && !recoveredCross) ||
            EspNativeGraphicsCatalog_findSprite(resourceLogical) == NULL) {
            ++stats->unsupported;
            printf("[NATIVESPRITE] UNSUPPORTED index=%u id=%u resource=%u info=%08x leaf=%u tile=%u cross=%u orient=%06x special=%u catalog=%u\n",
                   (unsigned int)i,
                   (unsigned int)id,
                   (unsigned int)resourceLogical,
                   (unsigned int)info,
                   (unsigned int)leaf,
                   (unsigned int)((info & TILE) != 0U),
                   (unsigned int)((info & CROSS) != 0U),
                   (unsigned int)(info & ORIENT_MASK),
                   (unsigned int)((info & SKIP_RESOURCE) != 0U),
                   (unsigned int)(EspNativeGraphicsCatalog_findSprite(resourceLogical) != NULL));
            return 0;
        }
        if (count >= MAX_VISIBLE_SPRITES) {
            ++stats->unsupported;
            printf("[NATIVESPRITE] OVERFLOW visibleCandidates>%u mapSprites=%u index=%u\n",
                   (unsigned int)MAX_VISIBLE_SPRITES,
                   (unsigned int)runtime->mapSpriteCount,
                   (unsigned int)i);
            return 0;
        }

        if (spriteRenderMode(resourceLogical) == RENDER_MODE_ADD) {
            ++stats->mode7Objects;
        }
        else {
            ++stats->mode0Objects;
        }

        sortZ = (int32_t)((sprite.x * render->viewCos_) +
                          (sprite.y * render->viewSin_) + render->viewTransX);
        if ((info & SORT_BIAS) != 0U) ++sortZ;
        else if (type == ENEMY_TYPE) --sortZ;
        else if (id >= 180U && id <= 191U) sortZ -= 2;

        position = count;
        while (position > 0U && sortZ >= order[position - 1U].sortZ) {
            order[position] = order[position - 1U];
            --position;
        }
        order[position].index = (uint16_t)i;
        order[position].logical = (uint16_t)id;
        order[position].info = info;
        order[position].sortZ = sortZ;
        ++count;
    }

    for (i = 0U; i < count; ++i) {
        hash = fnv(hash, &order[i].index, 2U);
        hash = fnv(hash, &order[i].sortZ, 4U);
    }
    stats->orderFNV1a = hash;
    *outCount = count;
    return 1;
}

static int spans(Render_t* render,
                 Line_t* line,
                 const Frame* frame,
                 uint8_t renderMode,
                 int glow,
                 EspNativeJunctionSpriteStats* stats) {
    int dx = line->vert2.x - line->vert1.x;
    int step;
    int dSide;
    int dTex;
    int x;
    int x2;
    int tex;
    int depth;

    if (renderMode != RENDER_MODE_NORMAL && renderMode != RENDER_MODE_ADD) {
        return 0;
    }
    if (dx <= 0) return 1;
    step = (MAXINT / dx) << 1;
    dSide = (int)DoomRPG_FixedMul(line->vert2.y - line->vert1.y, step);
    dTex = (int)DoomRPG_FixedMul(line->vert2.z - line->vert1.z, step);
    x = (line->vert1.x + 65535) >> 16;
    x2 = (line->vert2.x + 65535) >> 16;
    if (x < render->screenLeft) x = render->screenLeft;
    if (x2 > render->screenRight) x2 = render->screenRight;

    {
        int offset = (x << 16) - line->vert1.x;
        tex = line->vert1.z + DoomRPG_FixedMul(offset, dTex);
        depth = line->vert1.y + DoomRPG_FixedMul(offset, dSide);
    }

    while (x < x2) {
        int scale;
        int column;
        int texelStep;

        if (depth <= 0) return 0;
        scale = (0x40000000 / depth) << 2;
        column = (int)(DoomRPG_FixedMul(tex, scale) >> 16);
        depth += dSide;
        tex += dTex;

        if (render->columnScale[x] >= scale) {
            const uint8_t* bits;
            uint32_t cursor;
            int y = 0;

            if (column < 0 || column >= frame->width) return 0;
            texelStep = scale >> 3;
            bits = frame->mask + (uint32_t)column * (uint32_t)frame->pitch;
            cursor = frame->prefix[column];

            while (y < frame->height) {
                int start;
                int length;
                int screenY;
                int pixels;
                int pitch;
                int remaining;
                int worldY;
                uint32_t texelBase;
                int64_t position;
                uint16_t* destination;

                while (y < frame->height &&
                       (bits[y / 8] & (1U << (y & 7))) == 0U) {
                    ++y;
                }
                if (y >= frame->height) break;

                start = y;
                texelBase = cursor;
                while (y < frame->height &&
                       (bits[y / 8] & (1U << (y & 7))) != 0U) {
                    ++cursor;
                    ++y;
                }

                length = y - start;
                worldY = (64 - (frame->yMin + start)) - render->viewZ;
                pixels = (length * depth) >> 17;
                screenY = render->halfScreenHeight - ((worldY * depth) >> 17);
                position = ((int64_t)texelBase) << 12;

                if (screenY < render->screenTop) {
                    int cut = render->screenTop - screenY;
                    position += (int64_t)texelStep * cut;
                    pixels -= cut;
                    screenY = render->screenTop;
                }
                if (screenY + pixels > render->screenBottom) {
                    pixels = render->screenBottom - screenY;
                }
                if (pixels <= 0) continue;

                pitch = render->pitch >> 1;
                destination = (uint16_t*)render->pixels + pitch * screenY + x;
                remaining = pixels;
                if (glow) ++stats->glowSpanRuns;
                else ++stats->spanRuns;

                while (remaining-- > 0) {
                    uint32_t packedIndex;
                    uint8_t packed;
                    int shift;
                    uint16_t source;

                    if (position < 0) return 0;
                    packedIndex = (uint32_t)(position >> 13);
                    if (packedIndex >= frame->packedBytes) return 0;
                    packed = frame->texels[packedIndex];
                    shift = (int)((position >> 10) & 4);
                    source = frame->palette[(packed >> shift) & 15U];

                    if (renderMode == RENDER_MODE_NORMAL) {
                        *destination = source;
                    }
                    else {
                        uint32_t color = (uint32_t)(source & 0xf7deU) +
                                         ((uint32_t)(*destination) & 0xf7deU);
                        uint32_t carry = color & 0x10820U;
                        *destination = (uint16_t)((color & 0xf7deU) |
                                                  (carry >> 1) |
                                                  (carry >> 2) |
                                                  (carry >> 3));
                        if (!glow) ++stats->mode7Pixels;
                    }

                    destination += pitch;
                    position += texelStep;
                    if (glow) ++stats->glowPixels;
                    else ++stats->pixelsDrawn;
                }
            }

            if (cursor != frame->prefix[column + 1]) return 0;
        }
        else {
            if (glow) ++stats->glowWallOccludedColumns;
            else ++stats->wallOccludedColumns;
        }
        ++x;
    }
    return 1;
}

static int drawAt(Render_t* render,
                  const Sources* sources,
                  const Order* parent,
                  uint16_t logical,
                  uint8_t renderMode,
                  int glow,
                  int offsetX,
                  int offsetY,
                  Frame* frame,
                  uint32_t seenLogical[8],
                  EspNativeJunctionSpriteStats* stats) {
    EspMapSprite sprite;
    Vertex_t center;
    Vertex_t swap;
    Line_t line;
    uint32_t animation;
    int minimum;
    int maximum;
    int spriteX;
    int spriteY;

    if (!EspMapRuntime_getMapSprite(parent->index, &sprite)) return 0;
    spriteX = sprite.x + offsetX;
    spriteY = sprite.y + offsetY;

    animation = (parent->info & FIXED_ANIM) != 0U
                    ? ((parent->info & 0x1e00U) >> 9)
                    : 0U;
    if (!loadFrame(sources, logical, animation, glow,
                   frame, seenLogical, stats)) {
        return 0;
    }

    minimum = frame->xMin - 32;
    maximum = frame->xMax - 32;
    memset(&line, 0, sizeof(line));
    line.vert1.z = 0;
    line.vert2.z = maximum - minimum;

    if ((parent->info & ORIENT_MASK) == 0U) {
        memset(&center, 0, sizeof(center));
        center.x = spriteX;
        center.y = spriteY;
        Render_transform2DVerts(render, &center);
        center.x -= 0x100000;
        if (center.x < 0x40000) {
            if (glow) ++stats->glowNearCulled;
            else ++stats->nearCulled;
            return 1;
        }

        line.vert1 = center;
        line.vert2.x = center.x;
        line.vert2.y = center.y + (maximum << 16);
        line.vert2.z = maximum - minimum;
        line.vert1.y += minimum << 16;
    }
    else {
        const int flip = (parent->info & FLIP_HORIZONTAL) != 0U;

        if ((parent->info & ORIENT_NORTH) != 0U) {
            line.vert1.x = flip ? spriteX - minimum : spriteX + minimum;
            line.vert2.x = flip ? spriteX - maximum : spriteX + maximum;
            line.vert1.y = spriteY;
            line.vert2.y = spriteY;
        }
        else if ((parent->info & ORIENT_SOUTH) != 0U) {
            line.vert1.x = flip ? spriteX + minimum : spriteX - minimum;
            line.vert2.x = flip ? spriteX + maximum : spriteX - maximum;
            line.vert1.y = spriteY;
            line.vert2.y = spriteY;
        }
        else if ((parent->info & ORIENT_WEST) != 0U) {
            line.vert1.y = flip ? spriteY - minimum : spriteY + minimum;
            line.vert2.y = flip ? spriteY - maximum : spriteY + maximum;
            line.vert1.x = spriteX;
            line.vert2.x = spriteX;
        }
        else if ((parent->info & ORIENT_EAST) != 0U) {
            line.vert1.y = flip ? spriteY + minimum : spriteY - minimum;
            line.vert2.y = flip ? spriteY + maximum : spriteY - maximum;
            line.vert1.x = spriteX;
            line.vert2.x = spriteX;
        }
        else {
            ++stats->unsupported;
            printf("[NATIVESPRITE] UNSUPPORTED orient-combination index=%u id=%u info=%08x\n",
                   (unsigned int)parent->index,
                   (unsigned int)logical,
                   (unsigned int)parent->info);
            return 0;
        }

        if (((line.vert1.x - render->viewX) *
             (line.vert2.y - line.vert1.y)) +
            ((line.vert1.y - render->viewY) *
             (-(line.vert2.x - line.vert1.x))) <= 0) {
            if ((parent->info & TWO_SIDED) == 0U) {
                if (glow) ++stats->glowClipCulled;
                else ++stats->clipCulled;
                return 1;
            }
            swap = line.vert1;
            line.vert1 = line.vert2;
            line.vert2 = swap;
        }

        Render_transform2DVerts(render, &line.vert1);
        Render_transform2DVerts(render, &line.vert2);
    }

    if (!Render_clipLine(render, &line)) {
        if (glow) ++stats->glowClipCulled;
        else ++stats->clipCulled;
        return 1;
    }
    Render_projectVertex(render, &line.vert1);
    Render_projectVertex(render, &line.vert2);
    if (!spans(render, &line, frame, renderMode, glow, stats)) return 0;
    if (glow) ++stats->glowDraws;
    else ++stats->draws;
    return 1;
}

static int drawLegacyCross(Render_t* render,
                           const Sources* sources,
                           const Order* parent,
                           Frame* frame,
                           uint32_t seenLogical[8],
                           EspNativeJunctionSpriteStats* stats) {
    static const int8_t offsets[4][2] = {
        {16, 0}, {-16, 0}, {0, 16}, {0, -16}
    };
    uint16_t resourceLogical;
    uint32_t drawsBefore;
    uint32_t nearBefore;
    uint32_t clipBefore;
    uint32_t subDraws;
    uint32_t subNear;
    uint32_t subClip;
    uint32_t i;

    if (parent == NULL || !isLegacyCrossLogical(parent->logical) ||
        (parent->info & CROSS) == 0U ||
        (parent->info & ORIENT_MASK) != 0U) {
        ++stats->unsupported;
        return 0;
    }

    resourceLogical = (uint16_t)(parent->logical - 1U);
    if (spriteRenderMode(resourceLogical) != RENDER_MODE_NORMAL ||
        glowFor(resourceLogical) != 0U ||
        EspNativeGraphicsCatalog_findSprite(resourceLogical) == NULL) {
        ++stats->unsupported;
        return 0;
    }

    drawsBefore = stats->draws;
    nearBefore = stats->nearCulled;
    clipBefore = stats->clipCulled;
    for (i = 0U; i < 4U; ++i) {
        if (!drawAt(render, sources, parent, resourceLogical,
                    RENDER_MODE_NORMAL, 0,
                    offsets[i][0], offsets[i][1],
                    frame, seenLogical, stats)) {
            return 0;
        }
    }

    subDraws = stats->draws - drawsBefore;
    subNear = stats->nearCulled - nearBefore;
    subClip = stats->clipCulled - clipBefore;

    /* Four physical billboards are one map-sprite candidate. Collapse only the
     * object accounting counters back to one logical result; pixels/spans/frame
     * loads intentionally retain the real work performed by all four copies. */
    stats->draws = drawsBefore;
    stats->nearCulled = nearBefore;
    stats->clipCulled = clipBefore;
    if (subDraws != 0U) ++stats->draws;
    else if (subNear != 0U) ++stats->nearCulled;
    else ++stats->clipCulled;

    printf("[NATIVESPRITE] CROSS index=%u id=%u resource=%u copies=4 draws=%u near=%u clip=%u accounting=1\n",
           (unsigned int)parent->index,
           (unsigned int)parent->logical,
           (unsigned int)resourceLogical,
           (unsigned int)subDraws,
           (unsigned int)subNear,
           (unsigned int)subClip);
    return 1;
}

static int drawParentAndGlow(Render_t* render,
                             const Sources* sources,
                             const Order* parent,
                             Frame* frame,
                             uint32_t seenLogical[8],
                             EspNativeJunctionSpriteStats* stats) {
    uint16_t glowLogical;

    if ((parent->info & CROSS) != 0U) {
        return drawLegacyCross(render, sources, parent,
                               frame, seenLogical, stats);
    }

    if (!drawAt(render, sources, parent, parent->logical,
                spriteRenderMode(parent->logical), 0, 0, 0,
                frame, seenLogical, stats)) {
        return 0;
    }

    glowLogical = glowFor(parent->logical);
    if (glowLogical == 0U) return 1;
    ++stats->glowCompanions;
    if (EspNativeGraphicsCatalog_findSprite(glowLogical) == NULL) {
        ++stats->glowDeferred;
        return 0;
    }
    return drawAt(render, sources, parent, glowLogical,
                  RENDER_MODE_ADD, 1, 0, 0,
                  frame, seenLogical, stats);
}

int EspNativeJunctionSprite_render(struct Render_s* renderBase,
                                   EspNativeJunctionSpriteStats* outStats) {
    Render_t* render = (Render_t*)renderBase;
    const EspMapRuntimeView* runtime = EspMapRuntime_view();
    const EspPlayerViewState* view = EspPlayerView_view();
    Scratch saved;
    Scratch after;
    Sources sources;
    SpriteWorkspace* workspace = NULL;
    EspNativeJunctionSpriteStats stats;
    uint32_t orderCount = 0U;
    uint32_t i;
    int opened = 0;
    int ok = 0;

    if (outStats != NULL) memset(outStats, 0, sizeof(*outStats));
    if (render == NULL || outStats == NULL || runtime == NULL || view == NULL ||
        !EspMapSpriteTopology_isReady() ||
        !EspNativeGraphicsCatalog_isReady() || EspAssetPack_isOpen() ||
        render->screenWidth != SCREEN_W || render->columnScale == NULL ||
        render->framebuffer == NULL || runtime->nodeCount == 0U ||
        runtime->lineCount == 0U) {
        return 0;
    }

    workspace = (SpriteWorkspace*)malloc(sizeof(*workspace));
    if (workspace == NULL) return 0;
    memset(workspace, 0, sizeof(*workspace));
    memset(&stats, 0, sizeof(stats));
    saveScratch(render, &saved);

    if (!EspNativeBspVisibility_build(render, &workspace->visibility) ||
        !setupDrawView(render, view, &workspace->visibility)) {
        goto done;
    }

    stats.depthNodes = workspace->visibility.nodes;
    stats.depthLeaves = workspace->visibility.leaves;
    stats.depthNodeCulled = workspace->visibility.nodeCull;
    stats.depthLines = workspace->visibility.lines;
    stats.depthBackfaceCulled = workspace->visibility.backfaceCull;
    stats.depthClipCulled = workspace->visibility.clipCull;
    stats.depthOccluders = workspace->visibility.occluders;
    stats.depthSpriteSpans = workspace->visibility.spriteSpans;

    if (!buildOrder(render, runtime, &workspace->visibility,
                    workspace->order, &stats, &orderCount)) {
        goto done;
    }
    if (!EspAssetPack_open(ESP_ASSET_PACK_DEFAULT_PATH)) goto done;
    opened = 1;
    if (!initSources(&sources, &stats)) goto done;

    for (i = 0U; i < orderCount; ++i) {
        if (!drawParentAndGlow(render, &sources, &workspace->order[i],
                               &workspace->frame, workspace->seenLogical,
                               &stats)) {
            goto done;
        }
    }

    ok = stats.bspCandidates > 0U && stats.draws > 0U &&
         stats.pixelsDrawn > 0U && stats.mode0Objects > 0U &&
         stats.mode7Objects > 0U && stats.mode7Pixels > 0U &&
         stats.glowCompanions > 0U && stats.glowDraws > 0U &&
         stats.glowPixels > 0U && stats.glowDeferred == 0U;

done:
    if (opened || EspAssetPack_isOpen()) EspAssetPack_close();
    restoreScratch(render, &saved);
    saveScratch(render, &after);
    if (memcmp(&saved, &after, sizeof(saved)) != 0) ok = 0;
    free(workspace);
    *outStats = stats;
    return ok;
}
