#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "esp_asset_pack.h"
#include "esp_entity_def_type_catalog.h"
#include "esp_native_gameplay_hub_weapon_grid.h"
#include "platform_video_config.h"

#define GRID_LEFT 2
#define GRID_TOP 35
#define GRID_CELL_WIDTH 39
#define GRID_CELL_HEIGHT 21
#define GRID_RIGHT 157
#define GRID_BOTTOM 97
#define GRID_ICON_MAX_WIDTH 33
#define GRID_ICON_MAX_HEIGHT 17

#define GRID_BLACK 0x0000U
#define GRID_DIM_BLUE 0x0010U
#define GRID_BLUE 0x001fU
#define GRID_WHITE 0xffffU
#define GRID_EQUIPPED 0xffe0U

#define GRID_ENTITY_TYPE_WEAPON 5U
#define GRID_PISTOL_WEAPON_ID 2U
#define GRID_MAX_SOURCE_DIMENSION 64U
#define GRID_MAX_MASK_BYTES 512U
#define GRID_MAX_TEXEL_BYTES 2048U
#define GRID_PALETTE_COLORS 16U

#define MAPPINGS_HEADER_BYTES 16U
#define MAPPING_PAIR_BYTES 8U
#define PALETTES_HEADER_BYTES 4U
#define BITSHAPE_FILE_HEADER_BYTES 4U
#define BITSHAPE_FIXED_HEADER_BYTES 12U
#define TEXEL_FILE_HEADER_BYTES 4U

typedef struct HubWeaponIconFrame_s {
    uint16_t palette[GRID_PALETTE_COLORS];
    uint32_t sourceOffset;
    uint32_t texelOffset;
    uint32_t stexelsReadOffset;
    uint32_t maskBytes;
    uint32_t activePixels;
    uint32_t packedBytes;
    uint32_t texelHash;
    uint16_t paletteOffset;
    uint8_t width;
    uint8_t height;
    uint8_t pitch;
} HubWeaponIconFrame;

typedef struct HubWeaponIconWorkspace_s {
    uint8_t mask[GRID_MAX_MASK_BYTES];
    uint8_t texels[GRID_MAX_TEXEL_BYTES];
    uint16_t prefix[GRID_MAX_SOURCE_DIMENSION + 1U];
} HubWeaponIconWorkspace;

static uint16_t readLe16(const uint8_t* p) {
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static uint32_t readLe32(const uint8_t* p) {
    return (uint32_t)p[0] |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static uint32_t fnvUpdate(uint32_t hash, const void* data, uint32_t bytes) {
    const uint8_t* p = (const uint8_t*)data;
    uint32_t i;
    if (p == NULL && bytes != 0U) return 0U;
    for (i = 0U; i < bytes; ++i) {
        hash ^= p[i];
        hash *= 16777619U;
    }
    return hash;
}

static uint32_t fnv32(const uint8_t* data, uint32_t bytes) {
    return fnvUpdate(2166136261U, data, bytes);
}

static void putPixel(uint16_t* framebuffer, int x, int y, uint16_t color) {
    if (framebuffer == NULL || x < GRID_LEFT || x > GRID_RIGHT ||
        y < GRID_TOP || y > GRID_BOTTOM) return;
    framebuffer[y * DOOMRPG_LOGICAL_WIDTH + x] = color;
}

static void fillRect(uint16_t* framebuffer,
                     int left,
                     int top,
                     int right,
                     int bottom,
                     uint16_t color) {
    int x;
    int y;
    for (y = top; y <= bottom; ++y) {
        for (x = left; x <= right; ++x) putPixel(framebuffer, x, y, color);
    }
}

static void drawRect(uint16_t* framebuffer,
                     int left,
                     int top,
                     int right,
                     int bottom,
                     uint16_t color) {
    int x;
    int y;
    for (x = left; x <= right; ++x) {
        putPixel(framebuffer, x, top, color);
        putPixel(framebuffer, x, bottom, color);
    }
    for (y = top + 1; y < bottom; ++y) {
        putPixel(framebuffer, left, y, color);
        putPixel(framebuffer, right, y, color);
    }
}

void EspNativeGameplayHubWeaponGrid_cellBounds(uint8_t weaponId,
                                                uint8_t* outLeft,
                                                uint8_t* outTop,
                                                uint8_t* outRight,
                                                uint8_t* outBottom) {
    uint8_t column;
    uint8_t row;
    int left;
    int top;
    if (weaponId >= ESP_NATIVE_GAMEPLAY_HUB_WEAPON_GRID_COUNT) return;
    column = (uint8_t)(weaponId % ESP_NATIVE_GAMEPLAY_HUB_WEAPON_GRID_COLUMNS);
    row = (uint8_t)(weaponId / ESP_NATIVE_GAMEPLAY_HUB_WEAPON_GRID_COLUMNS);
    left = GRID_LEFT + (int)column * GRID_CELL_WIDTH;
    top = GRID_TOP + (int)row * GRID_CELL_HEIGHT;
    if (outLeft != NULL) *outLeft = (uint8_t)left;
    if (outTop != NULL) *outTop = (uint8_t)top;
    if (outRight != NULL) *outRight = (uint8_t)(left + GRID_CELL_WIDTH - 1);
    if (outBottom != NULL) *outBottom = (uint8_t)(top + GRID_CELL_HEIGHT - 1);
}

int EspNativeGameplayHubWeaponGrid_hitTest(int logicalX,
                                           int logicalY,
                                           uint8_t* outWeaponId) {
    int column;
    int row;
    int id;
    if (outWeaponId == NULL || logicalX < GRID_LEFT || logicalX > GRID_RIGHT ||
        logicalY < GRID_TOP || logicalY > GRID_BOTTOM) return 0;
    column = (logicalX - GRID_LEFT) / GRID_CELL_WIDTH;
    row = (logicalY - GRID_TOP) / GRID_CELL_HEIGHT;
    if (column < 0 || column >= (int)ESP_NATIVE_GAMEPLAY_HUB_WEAPON_GRID_COLUMNS ||
        row < 0 || row >= (int)ESP_NATIVE_GAMEPLAY_HUB_WEAPON_GRID_ROWS) return 0;
    id = row * (int)ESP_NATIVE_GAMEPLAY_HUB_WEAPON_GRID_COLUMNS + column;
    if (id < 0 || id >= (int)ESP_NATIVE_GAMEPLAY_HUB_WEAPON_GRID_COUNT) return 0;
    *outWeaponId = (uint8_t)id;
    return 1;
}

static uint16_t grey565(uint16_t color) {
    uint32_t r = (color >> 11) & 31U;
    uint32_t g5 = ((color >> 5) & 63U) >> 1;
    uint32_t b = color & 31U;
    uint32_t y = (r * 77U + g5 * 150U + b * 29U) >> 8;
    if (y > 31U) y = 31U;
    return (uint16_t)((y << 11) | ((y << 1) << 5) | y);
}

static int findBulletsTile(uint16_t* outTile) {
    uint16_t tile;
    char name[17];
    uint8_t type;
    uint8_t subtype;
    int32_t parm;
    if (outTile == NULL || !EspAssetPack_isOpen()) return 0;
    for (tile = 0U; tile < ESP_ENTITY_DEF_TYPE_CATALOG_LIMIT; ++tile) {
        if (!EspEntityDefTypeCatalog_getMetadata(tile, &type, &subtype, &parm)) {
            continue;
        }
        (void)type;
        (void)subtype;
        (void)parm;
        memset(name, 0, sizeof(name));
        if (!EspEntityDefTypeCatalog_readNameFromOpenPack(tile, name, sizeof(name))) {
            return 0;
        }
        if (strcmp(name, "Bullets") == 0 || strcmp(name, "Bullet") == 0) {
            *outTile = tile;
            return 1;
        }
    }
    return 0;
}

static int loadFrame(const EspAssetPackEntry* mappings,
                     const EspAssetPackEntry* palettes,
                     const EspAssetPackEntry* bitshapes,
                     const EspAssetPackEntry* stexels,
                     uint32_t spritePairBase,
                     uint32_t bitShapePairs,
                     uint32_t paletteEntries,
                     uint32_t spriteBaseTexelOffset,
                     uint32_t spriteDataSize,
                     uint16_t logicalSprite,
                     HubWeaponIconWorkspace* workspace,
                     HubWeaponIconFrame* frame) {
    uint8_t pair[MAPPING_PAIR_BYTES];
    uint8_t shapeHeader[BITSHAPE_FIXED_HEADER_BYTES];
    uint8_t paletteBytes[GRID_PALETTE_COLORS * 2U];
    uint32_t pairOffset;
    uint32_t maskOffset;
    uint32_t relativeTexelOffset;
    int32_t sourceOffset;
    int32_t paletteOffset;
    uint32_t active = 0U;
    uint32_t x;
    uint32_t i;
    int width;
    int height;
    int pitch;

    if (mappings == NULL || palettes == NULL || bitshapes == NULL ||
        stexels == NULL || workspace == NULL || frame == NULL ||
        (uint32_t)logicalSprite >= bitShapePairs) return 0;

    pairOffset = spritePairBase + (uint32_t)logicalSprite * MAPPING_PAIR_BYTES;
    if (pairOffset > mappings->size || MAPPING_PAIR_BYTES > mappings->size - pairOffset ||
        !EspAssetPack_readRange(mappings, pairOffset, pair, sizeof(pair))) return 0;
    sourceOffset = (int32_t)readLe32(pair);
    paletteOffset = (int32_t)readLe32(pair + 4U);
    if (sourceOffset < 0 || paletteOffset < 0 ||
        (uint32_t)paletteOffset > paletteEntries ||
        GRID_PALETTE_COLORS > paletteEntries - (uint32_t)paletteOffset) return 0;

    if (!EspAssetPack_readRange(
            palettes,
            PALETTES_HEADER_BYTES + (uint32_t)paletteOffset * 2U,
            paletteBytes,
            sizeof(paletteBytes))) return 0;

    if (BITSHAPE_FILE_HEADER_BYTES + (uint32_t)sourceOffset > bitshapes->size ||
        BITSHAPE_FIXED_HEADER_BYTES >
            bitshapes->size - (BITSHAPE_FILE_HEADER_BYTES + (uint32_t)sourceOffset) ||
        !EspAssetPack_readRange(bitshapes,
                                BITSHAPE_FILE_HEADER_BYTES + (uint32_t)sourceOffset,
                                shapeHeader,
                                sizeof(shapeHeader))) return 0;

    width = (int)shapeHeader[9] - (int)shapeHeader[8] + 1;
    height = (int)shapeHeader[11] - (int)shapeHeader[10] + 1;
    pitch = (height + 7) / 8;
    if (width <= 0 || height <= 0 ||
        width > (int)GRID_MAX_SOURCE_DIMENSION ||
        height > (int)GRID_MAX_SOURCE_DIMENSION || pitch <= 0 || pitch > 8) {
        return 0;
    }

    memset(frame, 0, sizeof(*frame));
    frame->width = (uint8_t)width;
    frame->height = (uint8_t)height;
    frame->pitch = (uint8_t)pitch;
    frame->sourceOffset = (uint32_t)sourceOffset;
    frame->paletteOffset = (uint16_t)paletteOffset;
    frame->texelOffset = readLe32(shapeHeader);
    frame->maskBytes = (uint32_t)width * (uint32_t)pitch;
    if (frame->maskBytes == 0U || frame->maskBytes > sizeof(workspace->mask)) return 0;
    maskOffset = BITSHAPE_FILE_HEADER_BYTES + frame->sourceOffset +
                 BITSHAPE_FIXED_HEADER_BYTES;
    if (maskOffset > bitshapes->size || frame->maskBytes > bitshapes->size - maskOffset ||
        !EspAssetPack_readRange(bitshapes, maskOffset,
                                workspace->mask, frame->maskBytes)) return 0;

    workspace->prefix[0] = 0U;
    for (x = 0U; x < (uint32_t)width; ++x) {
        const uint8_t* column = workspace->mask + x * (uint32_t)pitch;
        int y;
        for (y = 0; y < height; ++y) {
            if ((column[y / 8] & (uint8_t)(1U << (y & 7))) != 0U) ++active;
        }
        if (active > 4096U) return 0;
        workspace->prefix[x + 1U] = (uint16_t)active;
    }
    if (active == 0U) return 0;
    frame->activePixels = active;
    frame->packedBytes = ((active + 1U) & ~1U) / 2U;
    if (frame->packedBytes == 0U || frame->packedBytes > sizeof(workspace->texels)) return 0;

    if (frame->texelOffset < spriteBaseTexelOffset ||
        ((frame->texelOffset - spriteBaseTexelOffset) & 1U) != 0U) return 0;
    relativeTexelOffset = frame->texelOffset - spriteBaseTexelOffset;
    frame->stexelsReadOffset = TEXEL_FILE_HEADER_BYTES + relativeTexelOffset / 2U;
    if (spriteDataSize + TEXEL_FILE_HEADER_BYTES != stexels->size ||
        frame->stexelsReadOffset > stexels->size ||
        frame->packedBytes > stexels->size - frame->stexelsReadOffset ||
        !EspAssetPack_readRange(stexels, frame->stexelsReadOffset,
                                workspace->texels, frame->packedBytes)) return 0;

    for (i = 0U; i < GRID_PALETTE_COLORS; ++i) {
        frame->palette[i] = readLe16(&paletteBytes[i * 2U]);
    }
    frame->texelHash = fnv32(workspace->texels, frame->packedBytes);
    return frame->texelHash != 0U;
}

static int activeIndexAt(const HubWeaponIconFrame* frame,
                         const HubWeaponIconWorkspace* workspace,
                         int x,
                         int y,
                         uint32_t* outIndex) {
    const uint8_t* column;
    uint32_t active;
    int yy;
    if (frame == NULL || workspace == NULL || outIndex == NULL ||
        x < 0 || x >= frame->width || y < 0 || y >= frame->height) return 0;
    column = workspace->mask + (uint32_t)x * frame->pitch;
    if ((column[y / 8] & (uint8_t)(1U << (y & 7))) == 0U) return 0;
    active = workspace->prefix[x];
    for (yy = 0; yy < y; ++yy) {
        if ((column[yy / 8] & (uint8_t)(1U << (yy & 7))) != 0U) ++active;
    }
    if (active >= frame->activePixels) return 0;
    *outIndex = active;
    return 1;
}

static int drawFrame(uint16_t* framebuffer,
                     uint8_t weaponId,
                     int owned,
                     const HubWeaponIconFrame* frame,
                     const HubWeaponIconWorkspace* workspace) {
    uint8_t left8;
    uint8_t top8;
    uint8_t right8;
    uint8_t bottom8;
    int left;
    int top;
    int right;
    int bottom;
    int targetWidth;
    int targetHeight;
    int startX;
    int startY;
    int tx;
    int ty;

    if (framebuffer == NULL || frame == NULL || workspace == NULL) return 0;
    EspNativeGameplayHubWeaponGrid_cellBounds(
        weaponId, &left8, &top8, &right8, &bottom8);
    left = left8;
    top = top8;
    right = right8;
    bottom = bottom8;

    if ((int)frame->width * GRID_ICON_MAX_HEIGHT >
        (int)frame->height * GRID_ICON_MAX_WIDTH) {
        targetWidth = GRID_ICON_MAX_WIDTH;
        targetHeight = ((int)frame->height * GRID_ICON_MAX_WIDTH +
                        (int)frame->width / 2) / (int)frame->width;
    }
    else {
        targetHeight = GRID_ICON_MAX_HEIGHT;
        targetWidth = ((int)frame->width * GRID_ICON_MAX_HEIGHT +
                       (int)frame->height / 2) / (int)frame->height;
    }
    if (targetWidth < 1) targetWidth = 1;
    if (targetHeight < 1) targetHeight = 1;
    if (targetWidth > GRID_ICON_MAX_WIDTH) targetWidth = GRID_ICON_MAX_WIDTH;
    if (targetHeight > GRID_ICON_MAX_HEIGHT) targetHeight = GRID_ICON_MAX_HEIGHT;

    startX = (left + right - targetWidth + 1) / 2;
    startY = (top + bottom - targetHeight + 1) / 2;
    for (ty = 0; ty < targetHeight; ++ty) {
        int sy = (ty * (int)frame->height) / targetHeight;
        for (tx = 0; tx < targetWidth; ++tx) {
            int sx = (tx * (int)frame->width) / targetWidth;
            uint32_t activeIndex;
            uint8_t packed;
            uint8_t paletteIndex;
            uint16_t color;
            if (!activeIndexAt(frame, workspace, sx, sy, &activeIndex)) continue;
            packed = workspace->texels[activeIndex >> 1];
            paletteIndex = (uint8_t)((packed >> ((activeIndex & 1U) ? 4U : 0U)) & 0x0fU);
            color = frame->palette[paletteIndex];
            if (!owned) color = grey565(color);
            putPixel(framebuffer, startX + tx, startY + ty, color);
        }
    }
    return 1;
}

int EspNativeGameplayHubWeaponGrid_paint(
    uint16_t* framebuffer,
    const EspNativeGameplayPlayerState* player,
    uint8_t selectedWeapon) {
    EspAssetPackEntry mappings;
    EspAssetPackEntry palettes;
    EspAssetPackEntry bitshapes;
    EspAssetPackEntry wtexels;
    EspAssetPackEntry stexels;
    /* Bounded render scratch is an explicit static owner: keeping these roughly
     * 2.7 KiB off the Arduino loopTask stack prevents nested HUB paints from
     * tripping the ESP32 stack canary. No icon or map-wide texel cache is kept. */
    static HubWeaponIconWorkspace workspace;
    static HubWeaponIconFrame frame;
    uint8_t mappingHeader[MAPPINGS_HEADER_BYTES];
    uint8_t paletteHeader[PALETTES_HEADER_BYTES];
    uint8_t wallHeader[TEXEL_FILE_HEADER_BYTES];
    uint8_t spriteHeader[TEXEL_FILE_HEADER_BYTES];
    uint32_t texelPairs;
    uint32_t bitShapePairs;
    uint32_t paletteBytes;
    uint32_t paletteEntries;
    uint32_t spritePairBase;
    uint32_t wallDataSize;
    uint32_t spriteDataSize;
    uint32_t spriteBaseTexelOffset;
    uint32_t assetFNV = 2166136261U;
    uint32_t totalMaskBytes = 0U;
    uint32_t totalTexelBytes = 0U;
    uint16_t pistolSource = 0U;
    uint8_t ownedCount = 0U;
    uint8_t pistolFallback = 0U;
    uint8_t weapon;

    if (framebuffer == NULL || player == NULL || player->active != 1U ||
        player->weapon >= ESP_NATIVE_GAMEPLAY_HUB_WEAPON_GRID_COUNT ||
        selectedWeapon >= ESP_NATIVE_GAMEPLAY_HUB_WEAPON_GRID_COUNT ||
        !EspAssetPack_isOpen() || !EspEntityDefTypeCatalog_isReady()) return 0;

    memset(&workspace, 0, sizeof(workspace));
    if (!EspAssetPack_findEntry("mappings.bin", &mappings) ||
        !EspAssetPack_findEntry("palettes.bin", &palettes) ||
        !EspAssetPack_findEntry("bitshapes.bin", &bitshapes) ||
        !EspAssetPack_findEntry("wtexels.bin", &wtexels) ||
        !EspAssetPack_findEntry("stexels.bin", &stexels) ||
        !EspAssetPack_readRange(&mappings, 0U, mappingHeader, sizeof(mappingHeader)) ||
        !EspAssetPack_readRange(&palettes, 0U, paletteHeader, sizeof(paletteHeader)) ||
        !EspAssetPack_readRange(&wtexels, 0U, wallHeader, sizeof(wallHeader)) ||
        !EspAssetPack_readRange(&stexels, 0U, spriteHeader, sizeof(spriteHeader))) return 0;

    texelPairs = readLe32(mappingHeader);
    bitShapePairs = readLe32(mappingHeader + 4U);
    paletteBytes = readLe32(paletteHeader);
    wallDataSize = readLe32(wallHeader);
    spriteDataSize = readLe32(spriteHeader);
    if (texelPairs == 0U || bitShapePairs == 0U || bitShapePairs > 4096U ||
        (paletteBytes & 1U) != 0U || paletteBytes < 32U ||
        paletteBytes + PALETTES_HEADER_BYTES != palettes.size ||
        wallDataSize > UINT32_MAX / 2U ||
        spriteDataSize + TEXEL_FILE_HEADER_BYTES != stexels.size) return 0;
    paletteEntries = paletteBytes / 2U;
    spritePairBase = MAPPINGS_HEADER_BYTES + texelPairs * MAPPING_PAIR_BYTES;
    spriteBaseTexelOffset = wallDataSize * 2U;

    for (weapon = 0U; weapon < ESP_NATIVE_GAMEPLAY_HUB_WEAPON_GRID_COUNT; ++weapon) {
        uint16_t weaponTile;
        uint16_t iconTile;
        uint8_t fallback = 0U;
        uint8_t left;
        uint8_t top;
        uint8_t right;
        uint8_t bottom;
        uint16_t border;
        int owned;
        char name[17];

        memset(&frame, 0, sizeof(frame));
        memset(name, 0, sizeof(name));
        if (!EspEntityDefTypeCatalog_findTileIndex(
                GRID_ENTITY_TYPE_WEAPON, weapon, &weaponTile) ||
            !EspEntityDefTypeCatalog_readNameFromOpenPack(
                weaponTile, name, sizeof(name))) return 0;
        iconTile = weaponTile;
        if (!loadFrame(&mappings, &palettes, &bitshapes, &stexels,
                       spritePairBase, bitShapePairs, paletteEntries,
                       spriteBaseTexelOffset, spriteDataSize,
                       iconTile, &workspace, &frame)) {
            if (weapon != GRID_PISTOL_WEAPON_ID ||
                !findBulletsTile(&iconTile) ||
                !loadFrame(&mappings, &palettes, &bitshapes, &stexels,
                           spritePairBase, bitShapePairs, paletteEntries,
                           spriteBaseTexelOffset, spriteDataSize,
                           iconTile, &workspace, &frame)) {
                printf("[HUBWGRID] DEFER weapon=%u tile=%u name=\"%s\" reason=icon-source\n",
                       (unsigned int)weapon,
                       (unsigned int)weaponTile,
                       name);
                return 0;
            }
            fallback = 1U;
            pistolFallback = 1U;
        }
        if (weapon == GRID_PISTOL_WEAPON_ID) pistolSource = iconTile;

        owned = (player->weapons & (uint16_t)(1U << weapon)) != 0U;
        if (owned) ++ownedCount;
        EspNativeGameplayHubWeaponGrid_cellBounds(
            weapon, &left, &top, &right, &bottom);
        fillRect(framebuffer, left, top, right, bottom, GRID_BLACK);
        border = weapon == player->weapon
                     ? GRID_EQUIPPED
                     : (owned ? GRID_BLUE : GRID_DIM_BLUE);
        drawRect(framebuffer, left, top, right, bottom, border);
        if (weapon == selectedWeapon && weapon != player->weapon) {
            drawRect(framebuffer, left + 1, top + 1, right - 1, bottom - 1,
                     GRID_WHITE);
        }
        if (!drawFrame(framebuffer, weapon, owned, &frame, &workspace)) return 0;

        assetFNV = fnvUpdate(assetFNV, &weapon, sizeof(weapon));
        assetFNV = fnvUpdate(assetFNV, &weaponTile, sizeof(weaponTile));
        assetFNV = fnvUpdate(assetFNV, &iconTile, sizeof(iconTile));
        assetFNV = fnvUpdate(assetFNV, &fallback, sizeof(fallback));
        assetFNV = fnvUpdate(assetFNV, &frame.width, sizeof(frame.width));
        assetFNV = fnvUpdate(assetFNV, &frame.height, sizeof(frame.height));
        assetFNV = fnvUpdate(assetFNV, &frame.paletteOffset, sizeof(frame.paletteOffset));
        assetFNV = fnvUpdate(assetFNV, &frame.texelHash, sizeof(frame.texelHash));
        totalMaskBytes += frame.maskBytes;
        totalTexelBytes += frame.packedBytes;

        printf("[HUBWGRID] ICON weapon=%u tile=%u icon=%u name=\"%s\" size=%ux%u mask=%u texels=%u palette=%u owned=%s equipped=%s render=%s fallback=%s\n",
               (unsigned int)weapon,
               (unsigned int)weaponTile,
               (unsigned int)iconTile,
               name,
               (unsigned int)frame.width,
               (unsigned int)frame.height,
               (unsigned int)frame.maskBytes,
               (unsigned int)frame.packedBytes,
               (unsigned int)frame.paletteOffset,
               owned ? "yes" : "no",
               weapon == player->weapon ? "yes" : "no",
               owned ? "color" : "gray",
               fallback ? "bullets" : "weapon");
    }

    if (assetFNV == 0U || !EspAssetPack_isOpen()) return 0;
    printf("[HUBWGRID] FRAME weapons=12/12 owned=%u equipped=%u selected=%u ownedRender=color unavailableRender=gray equippedBorder=ffe0 pistolIcon=%s/%u persistentIconBytes=0 scratchBytes=%u scratchOwner=static sourceMaskBytes=%u sourceTexelBytes=%u assetFNV=%08x packOwnership=preserved-open mutation=no turn=no\n",
           (unsigned int)ownedCount,
           (unsigned int)player->weapon,
           (unsigned int)selectedWeapon,
           pistolFallback ? "bullets" : "weapon",
           (unsigned int)pistolSource,
           (unsigned int)sizeof(workspace),
           (unsigned int)totalMaskBytes,
           (unsigned int)totalTexelBytes,
           (unsigned int)assetFNV);
    return 1;
}
