#include <SDL.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "DoomRPG.h"
#include "Render.h"

#include "esp_asset_pack.h"
#include "esp_native_gameplay_weapon.h"

#define MAP_HEADER 16U
#define PALETTE_HEADER 4U
#define TEXEL_HEADER 4U
#define PAIR_BYTES 8U
#define SHAPE_HEADER 12U
#define MAX_DIM 64
#define MAX_MASK 512U
#define MAX_TEXELS 2048U
#define PLAYER_WEAPON_COUNT 12U
#define WEAPON_SPRITE_BASE 240U
#define WEAPON_FRAME_IDLE 0U
#define WEAPON_FRAME_ATTACK 1U

/* Combat.c wpinfo fields FLD_WP_IDLEX / FLD_WP_IDLEY and
 * FLD_WP_ATKX / FLD_WP_ATKY. This is presentation data only. */
static const int8_t idleOffsets[PLAYER_WEAPON_COUNT][2] = {
    {20, 25}, {0, 15}, {0, 15}, {0, 12},
    {0, 15},  {0, 15}, {0, 15}, {0, 15},
    {0, 13},  {20, 15}, {20, 15}, {20, 15}
};

static const int8_t attackOffsets[PLAYER_WEAPON_COUNT][2] = {
    {20, 0}, {0, 0}, {0, 0}, {0, 0},
    {0, 0},  {0, 0}, {0, 0}, {0, 0},
    {0, 5},  {20, 8}, {20, 8}, {20, 8}
};

typedef struct WeaponSources_s {
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
} WeaponSources;

typedef struct WeaponFrame_s {
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
    uint8_t mask[MAX_MASK];
    uint8_t texels[MAX_TEXELS];
} WeaponFrame;

typedef struct WeaponWorkspace_s {
    WeaponFrame frame;
    uint8_t busy;
    uint8_t cachedValid;
    uint8_t cachedWeapon;
    uint8_t cachedFrame;
} WeaponWorkspace;

/* The intro has a tighter contiguous-heap requirement than resident gameplay.
 * Do not charge this 2.5 KiB decode workspace to BSS from boot: acquire it once
 * on the first real weapon frame, after the legacy prologue has been disposed,
 * then keep/reuse that one bounded owner for the rest of the session.
 *
 * The same allocation caches exactly one decoded pose (idle or attack). There
 * is no second pixel/mask owner; normal same-weapon idle redraws remain zero
 * weapon-layer PAK reads. */
static WeaponWorkspace* weaponWorkspace;
static uint8_t weaponAttackArmed;
static uint8_t weaponAttackWeapon;

int EspNativeGameplayWeapon_armAttack(uint8_t weapon) {
    if (weapon >= PLAYER_WEAPON_COUNT) return 0;
    weaponAttackWeapon = weapon;
    weaponAttackArmed = 1U;
    return 1;
}

void EspNativeGameplayWeapon_cancelAttack(void) {
    weaponAttackArmed = 0U;
    weaponAttackWeapon = 0U;
}

static uint16_t le16(const uint8_t* p) {
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static uint32_t le32(const uint8_t* p) {
    return (uint32_t)p[0] |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

/* Match the source-palette conversion already hardware-proven by the native
 * wall/base sprite renderer. */
static uint16_t source565(uint16_t c) {
    return (uint16_t)(((c & 0x001fU) << 11) |
                      (c & 0x07e0U) |
                      ((c & 0xf800U) >> 11));
}

static int readRange(const EspAssetPackEntry* entry,
                     uint32_t offset,
                     void* destination,
                     uint32_t bytes,
                     EspNativeGameplayWeaponStats* stats) {
    if (entry == NULL || destination == NULL || stats == NULL ||
        offset > entry->size || bytes > entry->size - offset ||
        !EspAssetPack_readRange(entry, offset, destination, bytes)) {
        return 0;
    }
    ++stats->packReads;
    return 1;
}

static int initSources(WeaponSources* sources,
                       EspNativeGameplayWeaponStats* stats) {
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

static int loadFrame(const WeaponSources* sources,
                     uint16_t logical,
                     uint8_t animationFrame,
                     WeaponFrame* frame,
                     EspNativeGameplayWeaponStats* stats) {
    uint8_t idBytes[2];
    uint8_t pair[8];
    uint8_t header[12];
    uint8_t paletteBytes[32];
    uint32_t actual;
    uint32_t baseActual;
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

    if (sources == NULL || frame == NULL || stats == NULL ||
        animationFrame > WEAPON_FRAME_ATTACK ||
        logical >= sources->spriteIds || logical >= 256U) {
        return 0;
    }

    memset(frame, 0, sizeof(*frame));
    if (!readRange(&sources->mappings,
                   sources->spriteIdBase + (uint32_t)logical * 2U,
                   idBytes, sizeof(idBytes), stats)) {
        return 0;
    }

    /* mediaSpriteIds[logical] is the base shape and Render_draw2DSprite adds
     * the animation frame index. Render_addMapSprites follows the same rule. */
    baseActual = (uint32_t)le16(idBytes);
    actual = baseActual + (uint32_t)animationFrame;
    if (actual < baseActual || actual >= sources->bitShapePairs ||
        actual > UINT16_MAX ||
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
                   paletteBytes, sizeof(paletteBytes), stats)) {
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

    for (x = 0U; x < (uint32_t)frame->width; ++x) {
        const uint8_t* column = frame->mask + x * (uint32_t)frame->pitch;
        int y;
        for (y = 0; y < frame->height; ++y) {
            if ((column[y / 8] & (1U << (y & 7))) != 0U) ++active;
        }
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
        frame->palette[p] = source565(le16(paletteBytes + p * 2U));
    }

    stats->logicalSprite = logical;
    stats->actualSprite = frame->actual;
    stats->activePixels = frame->active;
    stats->frameBytes = frame->maskBytes + frame->packedBytes;
    return 1;
}

static int legacyAnchorTerm(int value, int scale) {
    return ((((value << 8) * scale) + 0xff00) >> 16);
}

static int rasterizeFrame(Render_t* render,
                          const WeaponFrame* frame,
                          int anchorX,
                          int anchorY,
                          EspNativeGameplayWeaponStats* stats) {
    uint16_t* framebuffer;
    int pitchPixels;
    int sampleScale;
    uint32_t cursor = 0U;
    int x;

    if (render == NULL || frame == NULL || stats == NULL ||
        render->framebuffer == NULL || render->pitch <= 0 ||
        render->screenWidth <= 0 || render->screenHeight <= 0) {
        return 0;
    }

    /* Render_draw2DSprite uses these two reciprocal fixed-point steps. The
     * destination rectangles below are the mask-backed equivalent for the
     * already-decoded native frame. */
    sampleScale = 65536 / (32768 / render->screenWidth);
    framebuffer = (uint16_t*)render->framebuffer;
    pitchPixels = render->pitch >> 1;

    for (x = 0; x < frame->width; ++x) {
        const uint8_t* column = frame->mask +
                                (uint32_t)x * (uint32_t)frame->pitch;
        int y;
        for (y = 0; y < frame->height; ++y) {
            if ((column[y / 8] & (1U << (y & 7))) != 0U) {
                uint32_t packedIndex;
                uint8_t packed;
                uint16_t color;
                int sourceX;
                int sourceY;
                int x0;
                int x1;
                int y0;
                int y1;
                int dy;

                if (cursor >= frame->active) return 0;
                packedIndex = cursor >> 1;
                if (packedIndex >= frame->packedBytes) return 0;
                packed = frame->texels[packedIndex];
                color = frame->palette[(cursor & 1U) != 0U
                                           ? (packed >> 4)
                                           : (packed & 0x0fU)];
                ++cursor;

                sourceX = frame->xMin + x;
                sourceY = frame->yMin + y;
                x0 = anchorX + ((sourceX * sampleScale) >> 8);
                x1 = anchorX + (((sourceX + 1) * sampleScale) >> 8);
                y0 = anchorY + ((sourceY * sampleScale) >> 8);
                y1 = anchorY + (((sourceY + 1) * sampleScale) >> 8);
                if (x1 <= x0) x1 = x0 + 1;
                if (y1 <= y0) y1 = y0 + 1;

                if (x0 < render->screenLeft) x0 = render->screenLeft;
                if (x1 > render->screenRight) x1 = render->screenRight;
                if (y0 < render->screenTop) y0 = render->screenTop;
                if (y1 > render->screenBottom) y1 = render->screenBottom;
                if (x0 >= x1 || y0 >= y1) continue;

                for (dy = y0; dy < y1; ++dy) {
                    uint16_t* row = framebuffer +
                        (render->screenY + dy) * pitchPixels +
                        render->screenX;
                    int dx;
                    for (dx = x0; dx < x1; ++dx) {
                        row[dx] = color;
                        ++stats->pixelsWritten;
                    }
                }
            }
        }
    }

    return cursor == frame->active;
}

int EspNativeGameplayWeapon_render(
    struct Render_s* renderBase,
    uint8_t weapon,
    uint8_t weaponsPresent,
    EspNativeGameplayWeaponStats* outStats) {
    Render_t* render = (Render_t*)renderBase;
    EspNativeGameplayWeaponStats stats;
    WeaponSources sources;
    WeaponWorkspace* workspace = weaponWorkspace;
    WeaponFrame* frame = NULL;
    const int8_t* offsets;
    uint16_t logical;
    uint8_t animationFrame;
    int scale;
    int anchorX;
    int anchorY;
    int savedScreenLeft = 0;
    int savedScreenTop = 0;
    int savedScreenRight = 0;
    int savedScreenBottom = 0;
    int clipSaved = 0;
    int opened = 0;
    int cacheHit = 0;
    int ok = 0;

    memset(&stats, 0, sizeof(stats));
    stats.weapon = weapon;
    if (outStats != NULL) memset(outStats, 0, sizeof(*outStats));

    if ((workspace != NULL && workspace->busy) ||
        render == NULL || outStats == NULL ||
        render->framebuffer == NULL || render->screenX != 0 ||
        render->screenY != 20 || render->screenWidth != 160 ||
        render->screenHeight != 80 || render->shapeData != NULL ||
        render->mediaTexels != NULL || EspAssetPack_isOpen()) {
        printf("[WEAPON] FAILED contract weapon=%u screen=%d,%d/%dx%d clip=%d,%d..%d,%d shapeData=%p mediaTexels=%p pack=%u\n",
               (unsigned int)weapon,
               render != NULL ? render->screenX : -1,
               render != NULL ? render->screenY : -1,
               render != NULL ? render->screenWidth : -1,
               render != NULL ? render->screenHeight : -1,
               render != NULL ? render->screenLeft : -1,
               render != NULL ? render->screenTop : -1,
               render != NULL ? render->screenRight : -1,
               render != NULL ? render->screenBottom : -1,
               render != NULL ? (void*)render->shapeData : NULL,
               render != NULL ? (void*)render->mediaTexels : NULL,
               (unsigned int)EspAssetPack_isOpen());
        return 0;
    }

    if (weaponsPresent == 0U) {
        stats.skipped = 1U;
        *outStats = stats;
        return 1;
    }
    if (weapon >= PLAYER_WEAPON_COUNT) {
        printf("[WEAPON] FAILED unsupported weapon=%u present=%02x\n",
               (unsigned int)weapon, (unsigned int)weaponsPresent);
        return 0;
    }

    if (weaponAttackArmed != 0U && weaponAttackWeapon != weapon) {
        EspNativeGameplayWeapon_cancelAttack();
    }
    animationFrame = (uint8_t)(weaponAttackArmed != 0U &&
                               weaponAttackWeapon == weapon
                                   ? WEAPON_FRAME_ATTACK
                                   : WEAPON_FRAME_IDLE);
    stats.animationFrame = animationFrame;
    offsets = animationFrame == WEAPON_FRAME_ATTACK
                  ? attackOffsets[weapon]
                  : idleOffsets[weapon];

    if (workspace == NULL) {
        workspace = (WeaponWorkspace*)SDL_calloc(1, sizeof(*workspace));
        if (workspace == NULL) {
            printf("[WEAPON] FAILED workspace-allocation ownerBytes=%u\n",
                   (unsigned int)sizeof(*workspace));
            return 0;
        }
        weaponWorkspace = workspace;
        printf("[WEAPON] WORKSPACE ownerBytes=%u allocation=lazy-gameplay cache=one-decoded-pose\n",
               (unsigned int)sizeof(*workspace));
    }

    frame = &workspace->frame;
    workspace->busy = 1U;
    logical = (uint16_t)(WEAPON_SPRITE_BASE + weapon);

    if (workspace->cachedValid != 0U &&
        workspace->cachedWeapon == weapon &&
        workspace->cachedFrame == animationFrame &&
        frame->logical == logical) {
        stats.logicalSprite = logical;
        stats.actualSprite = frame->actual;
        stats.activePixels = frame->active;
        stats.frameBytes = frame->maskBytes + frame->packedBytes;
        cacheHit = 1;
    }
    else {
        workspace->cachedValid = 0U;
        if (!EspAssetPack_open(ESP_ASSET_PACK_DEFAULT_PATH)) {
            printf("[WEAPON] FAILED pack-open weapon=%u logical=%u frame=%u\n",
                   (unsigned int)weapon,
                   (unsigned int)logical,
                   (unsigned int)animationFrame);
            goto done;
        }
        opened = 1;

        if (!initSources(&sources, &stats) ||
            !loadFrame(&sources, logical, animationFrame, frame, &stats)) {
            printf("[WEAPON] FAILED resource weapon=%u logical=%u frame=%u reads=%u\n",
                   (unsigned int)weapon,
                   (unsigned int)logical,
                   (unsigned int)animationFrame,
                   (unsigned int)stats.packReads);
            goto done;
        }
        workspace->cachedWeapon = weapon;
        workspace->cachedFrame = animationFrame;
        workspace->cachedValid = 1U;
    }

    scale = (render->screenWidth << 16) / 0x8000;
    anchorX = (render->screenWidth / 2) +
              legacyAnchorTerm((int)offsets[0] - 32, scale);
    anchorY = render->screenHeight -
              legacyAnchorTerm(64 - (int)offsets[1], scale);
    stats.anchorX = (int16_t)anchorX;
    stats.anchorY = (int16_t)anchorY;

    /* World/sprite owners restore the caller's Render_t scratch exactly. The
     * caller's clip is therefore not a stable gameplay-frame invariant. Own a
     * full viewport clip only while drawing this screen-space layer, then put
     * every field back before the compositor checks Render_t byte stability. */
    savedScreenLeft = render->screenLeft;
    savedScreenTop = render->screenTop;
    savedScreenRight = render->screenRight;
    savedScreenBottom = render->screenBottom;
    render->screenLeft = 0;
    render->screenTop = 0;
    render->screenRight = render->screenWidth;
    render->screenBottom = render->screenHeight;
    clipSaved = 1;

    if (!rasterizeFrame(render, frame, anchorX, anchorY, &stats)) {
        printf("[WEAPON] FAILED raster weapon=%u logical=%u actual=%u frame=%u\n",
               (unsigned int)weapon,
               (unsigned int)logical,
               (unsigned int)frame->actual,
               (unsigned int)animationFrame);
        goto done;
    }

    stats.drawn = 1U;
    ok = 1;
    if (animationFrame == WEAPON_FRAME_ATTACK) {
        EspNativeGameplayWeapon_cancelAttack();
    }

done:
    if (clipSaved) {
        render->screenLeft = savedScreenLeft;
        render->screenTop = savedScreenTop;
        render->screenRight = savedScreenRight;
        render->screenBottom = savedScreenBottom;
    }
    if (opened) EspAssetPack_close();
    if (ok) {
        printf("[WEAPON] DRAW weapon=%u logical=%u actual=%u frame=%u pose=%s offset=%d,%d anchor=%d,%d bounds=%d..%d,%d..%d active=%u pixels=%u cache=%s reads=%u frameBytes=%u ownerBytes=%u packClosed=%s\n",
               (unsigned int)weapon,
               (unsigned int)stats.logicalSprite,
               (unsigned int)stats.actualSprite,
               (unsigned int)animationFrame,
               animationFrame == WEAPON_FRAME_ATTACK ? "attack" : "idle",
               (int)offsets[0],
               (int)offsets[1],
               anchorX, anchorY,
               frame->xMin, frame->xMax, frame->yMin, frame->yMax,
               (unsigned int)stats.activePixels,
               (unsigned int)stats.pixelsWritten,
               cacheHit ? "hit" : "miss",
               (unsigned int)stats.packReads,
               (unsigned int)stats.frameBytes,
               (unsigned int)sizeof(*workspace),
               EspAssetPack_isOpen() ? "no" : "yes");
    }
    *outStats = stats;
    workspace->busy = 0U;
    return ok;
}
