/* Real BSP and PAK-backed HELP instructional preview, before game launch.
 * Link the production esp_native_help_scene.c to bounded fake pack entries.
 * Only the resource provider is mocked, never the renderer or breath. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "esp_asset_pack.h"
#include "esp_bsp_reader.h"
#include "esp_map_catalog.h"
#include "esp_native_help_scene.h"
#include "platform_video_config.h"

#define W DOOMRPG_LOGICAL_WIDTH
#define H DOOMRPG_LOGICAL_HEIGHT
static uint16_t before[W * H], after[W * H], rowFrame[W * H];
static uint8_t mappingData[78];
static uint8_t palettesData[164];
static uint8_t wallData[4 + 5 * 2048];
static uint8_t bspData[40 + 256 + 2048];
static int openPack, packOpens, blockReads;

static void le16(uint8_t* p, uint16_t v) {
    p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8);
}
static void le32(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}
int EspMapCatalog_isValidId(uint8_t id) { return id == 1U; }
const char* EspMapCatalog_nameForId(uint8_t id) {
    return id == 1U ? "/intro.bsp" : NULL;
}
int EspAssetPack_isOpen(void) { return openPack; }
int EspAssetPack_open(const char* path) {
    if (openPack || strcmp(path, ESP_ASSET_PACK_DEFAULT_PATH) != 0)
        return 0;
    openPack = 1;
    ++packOpens;
    return 1;
}
void EspAssetPack_close(void) { assert(openPack); openPack = 0; }
int EspAssetPack_findEntry(const char* name, EspAssetPackEntry* out) {
    if (!openPack || out == NULL) return 0;
    memset(out, 0, sizeof(*out));
    if (strcmp(name, "/intro.bsp") == 0) {
        out->offset = 1; out->size = sizeof(bspData);
    }
    else if (strcmp(name, "mappings.bin") == 0) {
        out->offset = 2; out->size = sizeof(mappingData);
    }
    else if (strcmp(name, "palettes.bin") == 0) {
        out->offset = 3; out->size = sizeof(palettesData);
    }
    else if (strcmp(name, "wtexels.bin") == 0) {
        out->offset = 4; out->size = sizeof(wallData);
    }
    else return 0;
    return 1;
}
int EspAssetPack_readRange(const EspAssetPackEntry* entry,
                           uint32_t offset, void* output,
                           size_t bytes) {
    const uint8_t* data;
    size_t size;
    if (!openPack || entry == NULL || output == NULL ||
        offset > entry->size || bytes > entry->size - offset)
        return 0;
    switch (entry->offset) {
        case 1: data = bspData; size = sizeof(bspData); ++blockReads; break;
        case 2: data = mappingData; size = sizeof(mappingData); break;
        case 3: data = palettesData; size = sizeof(palettesData); break;
        case 4: data = wallData; size = sizeof(wallData); break;
        default: return 0;
    }
    if (offset > size || bytes > size - offset) return 0;
    memcpy(output, data + offset, bytes);
    return 1;
}
int EspBspReader_inventoryPackEntry(const char* name,
                                    EspBspInventory* out) {
    if (!openPack || out == NULL || strcmp(name, "/intro.bsp") != 0)
        return 0;
    memset(out, 0, sizeof(*out));
    out->spawnIndex = (uint16_t)(16U * 32U + 16U);
    out->spawnDirection = 0U;
    out->sourceBytes = sizeof(bspData);
    out->sections.blockMapOffset = 40U;
    out->sections.planeTexturesOffset = 40U + 256U;
    out->floorRgb[0] = 55U;
    out->floorRgb[1] = 47U;
    out->floorRgb[2] = 41U;
    out->ceilingRgb[0] = 26U;
    out->ceilingRgb[1] = 36U;
    out->ceilingRgb[2] = 52U;
    for (uint32_t id = 1; id <= 4; ++id)
        out->lineTextureIdBits[id >> 5] |= 1UL << (id & 31U);
    return 1;
}
static void initializePak(void) {
    int x, y, i;
    le32(mappingData, 5U);       /* wall texture pairs */
    le32(mappingData + 4, 1U);   /* bitshape pairs */
    le32(mappingData + 8, 6U);   /* logical wall IDs */
    le32(mappingData + 12, 1U);  /* logical sprite IDs */
    for (i = 0; i < 5; ++i) {
        le32(mappingData + 16 + 8 * i, (uint32_t)i * 2048U * 2U);
        le32(mappingData + 16 + 8 * i + 4, (uint32_t)i * 16U);
    }
    for (i = 0; i < 6; ++i)
        le16(mappingData + 64 + 2 * i, (uint16_t)(i ? i - 1 : 0));
    le32(palettesData, 5U * 32U);
    le32(wallData, 5U * 2048U);
    for (i = 0; i < 5 * 16; ++i)
        le16(palettesData + 4 + 2 * i,
             (uint16_t)(0x0210U + (i * 0x0033U)));
    for (i = 0; i < 5 * 2048; ++i)
        wallData[4 + i] = (uint8_t)((i % 16) |
                                    (((i / 23 + 3) % 16) << 4));
    /* Real 32x32 compact 2-bit wall/secret map, including a nonrectangular
     * corridor around the spawn. The source file is never loaded as a world. */
    for (y = 0; y < 32; ++y)
        for (x = 0; x < 32; ++x) {
            const int blocked =
                x == 0 || y == 0 || x == 31 || y == 31 ||
                (x == 19 && y >= 12 && y <= 21) ||
                (y == 13 && x >= 10 && x <= 23) ||
                (x == 12 && y >= 16 && y <= 20);
            const int tile = y * 32 + x;
            if (blocked) bspData[40 + tile / 4] |=
                (uint8_t)(1U << ((tile % 4) * 2));
            /* The true BSP has 1024 floor cells then 1024 ceiling cells.
             * Include distinct map PAK material IDs, not fake flat bands. */
            bspData[40 + 256 + tile] = (uint8_t)(1U + (x / 5 + y / 7) % 3);
            bspData[40 + 256 + 1024 + tile] =
                (uint8_t)(2U + ((x / 6 + y / 5) % 2));
        }
}
static int diversePixels(const uint16_t* data, int top, int height) {
    const uint16_t first = data[top * W + 12];
    int x, y, different = 0;
    for (y = top; y < top + height; ++y)
        for (x = 12; x < 148; ++x)
            different += (data[y * W + x] != first);
    return different;
}
static int pulseAllowed(int x, int y) {
    const int left[3] = {8, 56, 104};
    const int top[3] = {29, 51, 74};
    const int right[3] = {55, 103, 151};
    const int bottom[3] = {50, 73, 96};
    int row, col;
    for (row = 0; row < 3; ++row) {
        if (y < top[row] || y > bottom[row]) continue;
        for (col = 0; col < 3; ++col) {
            const int cx = (left[col] + right[col]) / 2;
            const int cy = (top[row] + bottom[row]) / 2;
            if (x < left[col] || x > right[col]) continue;
            /* The two 1px cell edges and their 1px inner halos. */
            if (y <= top[row] + 1 || y >= bottom[row] - 1 ||
                x <= left[col] + 1 || x >= right[col] - 1) return 1;
            /* The glyph itself (including rounded U-turn and WPN label). */
            if (x >= cx - 15 && x <= cx + 14 &&
                y >= cy - 10 && y <= cy + 10) return 1;
        }
    }
    return 0;
}
static uint32_t hashRect(const uint16_t* data, int x0, int y0,
                         int width, int height) {
    uint32_t h = 2166136261U;
    int x, y;
    for (y = y0; y < y0 + height; ++y)
        for (x = x0; x < x0 + width; ++x) {
            h ^= data[y * W + x];
            h *= 16777619U;
        }
    return h;
}
int main(void) {
    uint32_t rowHash[3];
    int x, y, changed = 0;
    int red, green, yellow;
    initializePak();
    assert(!EspNativeHelpScene_isReady());
    assert(EspNativeHelpScene_begin(0)); /* Valid even before FIRST GAME */
    assert(EspNativeHelpScene_isReady());
    assert(EspNativeHelpScene_previewMapId() == 1);
    assert(EspNativeHelpScene_materialCount() == 3);
    assert(!openPack && packOpens == 1 && blockReads == 2);

    for (x = 0; x <= 16; ++x) {
        const int current = EspNativeHelpScene_breathPower((uint8_t)x);
        if (x == 0) assert(current == 0);
        else assert(current >= EspNativeHelpScene_breathPower((uint8_t)(x - 1)));
        if (x > 0) assert(current -
            EspNativeHelpScene_breathPower((uint8_t)(x - 1)) < 26);
    }
    assert(EspNativeHelpScene_breathStep(2560U) == 0U);
    assert(EspNativeHelpScene_breathLevel(16U) == 16U);

    assert(EspNativeHelpScene_paint(before, 0U));
    assert(EspNativeHelpScene_paint(after, 16U));
    assert(hashRect(before, 8, 29, 144, 68) != 0);
    assert(diversePixels(after, 30, 8) > 160); /* Textured top! */
    assert(diversePixels(after, 88, 8) > 160); /* Textured bottom! */
    for (y = 0; y < H; ++y)
        for (x = 0; x < W; ++x)
            if (before[y * W + x] != after[y * W + x]) {
                assert(x >= 8 && x <= 151 && y >= 29 && y <= 96);
                assert(pulseAllowed(x, y));
                ++changed;
            }
    assert(changed > 300); /* Contours and labels BOTH breathe. */
    assert(changed < 2900); /* Never tint all 144*68 background pixels. */

    /* RGB565 action colors must match gameplay: turn/move GREEN,
     * attack/use RED, PREV/NEXT YELLOW. No blanket BLUE dividers. */
    green = after[29 * W + 30];
    red = after[51 * W + 78];
    yellow = after[74 * W + 30];
    assert(((green >> 5) & 63) > 2 * ((green >> 11) & 31));
    assert(((green >> 5) & 63) > 2 * (green & 31));
    assert(((red >> 11) & 31) > 2 * ((red >> 5) & 63));
    assert(((red >> 11) & 31) > 2 * (red & 31));
    assert(((yellow >> 11) & 31) > 2 * (yellow & 31));
    assert(((yellow >> 5) & 63) > 2 * (yellow & 31));
    /* Sample a non-border MOVE glyph pixel; its neon varies too. */
    assert(before[39 * W + 79] != after[39 * W + 79]);

    for (x = 0; x < 3; ++x) {
        memset(rowFrame, 0, sizeof(rowFrame));
        assert(EspNativeHelpScene_paintRow(rowFrame, (uint8_t)x, 0));
        rowHash[x] = hashRect(rowFrame, 8, 39, 144, 23);
    }
    assert(rowHash[0] != rowHash[1]);
    assert(rowHash[1] != rowHash[2]);
    assert(rowHash[0] != rowHash[2]);
    assert(!openPack); /* Illustration repaint is entirely allocation-free. */
    assert(!EspNativeHelpScene_paintRow(rowFrame, 3U, 0));
    assert(!EspNativeHelpScene_paint(before, 17U));
    EspNativeHelpScene_reset();
    assert(!EspNativeHelpScene_isReady());
    assert(EspNativeHelpScene_begin(1)); /* Re-enter without gameplay. */
    assert(EspNativeHelpScene_isReady());
    assert(packOpens == 2 && !openPack);
    puts("PASS HELP BSP: full-height textured PAK planes, three cameras, RGB action contours+labels breathing");
    return 0;
}
