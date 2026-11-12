/* Executable pixel contract for the ORIGINAL PAK wall/door HELP scene.
 * The renderer under test is production esp_native_help_scene.c, mocked
 * only at the bounded backing-store API. No BSP world, game or ZIP runtime. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "esp_asset_pack.h"
#include "esp_native_help_scene.h"
#include "platform_video_config.h"

#define W DOOMRPG_LOGICAL_WIDTH
#define H DOOMRPG_LOGICAL_HEIGHT
#define WALL_LEN 2048
#define NUM_TEXTURES 5
static uint16_t softFrame[W * H], brightFrame[W * H], rowFrame[W * H];
static uint8_t mappingData[78];
static uint8_t paletteData[4 + NUM_TEXTURES * 32];
static uint8_t wallData[4 + NUM_TEXTURES * WALL_LEN];
static int packOpen, packOpens, packReads;

static void le16(uint8_t* p, uint16_t v) {
    p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8);
}
static void le32(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}
int EspAssetPack_isOpen(void) { return packOpen; }
int EspAssetPack_open(const char* path) {
    if (packOpen || strcmp(path, ESP_ASSET_PACK_DEFAULT_PATH) != 0)
        return 0;
    packOpen = 1;
    ++packOpens;
    return 1;
}
void EspAssetPack_close(void) {
    assert(packOpen == 1);
    packOpen = 0;
}
int EspAssetPack_findEntry(const char* path, EspAssetPackEntry* entry) {
    if (!packOpen || entry == NULL) return 0;
    memset(entry, 0, sizeof(*entry));
    if (strcmp(path, "mappings.bin") == 0) {
        entry->offset = 1U; entry->size = sizeof(mappingData);
    } else if (strcmp(path, "palettes.bin") == 0) {
        entry->offset = 2U; entry->size = sizeof(paletteData);
    } else if (strcmp(path, "wtexels.bin") == 0) {
        entry->offset = 3U; entry->size = sizeof(wallData);
    } else return 0;
    return 1;
}
int EspAssetPack_readRange(const EspAssetPackEntry* entry,
                            uint32_t offset, void* dst, size_t bytes) {
    const uint8_t* src;
    size_t length;
    if (!packOpen || entry == NULL || dst == NULL ||
        offset > entry->size || bytes > entry->size - offset) return 0;
    if (entry->offset == 1U) {
        src = mappingData; length = sizeof(mappingData);
    } else if (entry->offset == 2U) {
        src = paletteData; length = sizeof(paletteData);
    } else if (entry->offset == 3U) {
        src = wallData; length = sizeof(wallData);
    } else return 0;
    if (offset > length || bytes > length - offset) return 0;
    memcpy(dst, src + offset, bytes);
    ++packReads;
    return 1;
}
static void fakePakInit(void) {
    int i, texel;
    le32(mappingData + 0, NUM_TEXTURES);
    le32(mappingData + 4, 1U);
    le32(mappingData + 8, 6U);
    le32(mappingData + 12, 1U);
    for (i = 0; i < NUM_TEXTURES; ++i) {
        le32(mappingData + 16 + i * 8, (uint32_t)(i * WALL_LEN * 2));
        le32(mappingData + 16 + i * 8 + 4, (uint32_t)(i * 16));
    }
    for (i = 0; i < 6; ++i)
        le16(mappingData + 64 + i * 2, (uint16_t)(i ? i - 1 : 0));
    le32(paletteData, NUM_TEXTURES * 32);
    le32(wallData, NUM_TEXTURES * WALL_LEN);
    for (i = 0; i < NUM_TEXTURES * 16; ++i) {
        const uint16_t v = (uint16_t)(1 + (i % 16) / 3 + i / 16);
        le16(paletteData + 4 + i * 2,
             (uint16_t)((v << 11) | (v * 2 << 5) | v));
    }
    for (texel = 0; texel < NUM_TEXTURES * WALL_LEN; ++texel) {
        const int pix = texel % WALL_LEN;
        wallData[4 + texel] =
            (uint8_t)(((pix * 3 + texel / WALL_LEN) & 15) |
                      (((pix / 9 + texel / WALL_LEN * 2) & 15) << 4));
    }
}
static uint32_t hashArea(const uint16_t* fb, int left, int top,
                          int width, int height) {
    uint32_t value = 2166136261U;
    int x, y;
    for (y = top; y < top + height; ++y)
        for (x = left; x < left + width; ++x) {
            value ^= fb[y * W + x];
            value *= 16777619U;
        }
    return value;
}
static int breathAllowed(int x, int y) {
    const int left[3] = {8, 56, 104};
    const int right[3] = {55, 103, 151};
    const int top[3] = {29, 51, 74};
    const int bottom[3] = {50, 73, 96};
    int row, col;
    for (row = 0; row < 3; ++row) {
        if (y < top[row] || y > bottom[row]) continue;
        for (col = 0; col < 3; ++col) {
            const int cx = (left[col] + right[col]) / 2;
            const int cy = (top[row] + bottom[row]) / 2;
            if (x < left[col] || x > right[col]) continue;
            if (y <= top[row] + 1 || y >= bottom[row] - 1 ||
                x <= left[col] + 1 || x >= right[col] - 1) return 1;
            if (x >= cx - 15 && x <= cx + 14 &&
                y >= cy - 10 && y <= cy + 10) return 1;
        }
    }
    return 0;
}
/* The real HELP painter must keep every breathing glyph inside its own
 * button. The interior excludes the two-pixel frame; delta bounds isolate
 * the neon glyphs from the static PAK texture and stable reading pads. */
static int glyphDeltaBounds(int left, int top, int right, int bottom,
                            int* minX, int* maxX, int* minY, int* maxY) {
    int x, y, count = 0;
    *minX = W; *maxX = -1;
    *minY = H; *maxY = -1;
    for (y = top + 3; y <= bottom - 3; ++y) {
        for (x = left + 3; x <= right - 3; ++x) {
            if (softFrame[y * W + x] == brightFrame[y * W + x]) continue;
            if (x < *minX) *minX = x;
            if (x > *maxX) *maxX = x;
            if (y < *minY) *minY = y;
            if (y > *maxY) *maxY = y;
            ++count;
        }
    }
    return count;
}

int main(void) {
    uint32_t rowHash[3], original, newMap, newPack;
    uint16_t textureId;
    int i, x, y, altered = 0, readCount;

    fakePakInit();
    assert(!EspNativeHelpScene_isReady());
    assert(EspNativeHelpScene_begin(0U, 13700U)); /* before START! */
    assert(EspNativeHelpScene_isReady());
    assert(EspNativeHelpScene_materialCount() == 1U);
    assert(EspNativeHelpScene_previewMapId() == 1U);
    textureId = EspNativeHelpScene_textureId();
    assert(textureId > 0U && textureId < 6U);
    assert(packOpens == 1 && !packOpen);
    readCount = packReads;

    assert(EspNativeHelpScene_breathStep(2560U) == 0U);
    assert(EspNativeHelpScene_breathLevel(0U) == 0U);
    assert(EspNativeHelpScene_breathLevel(16U) == 16U);
    assert(EspNativeHelpScene_breathPower(16U) == 255U);
    for (i = 1; i <= 16; ++i)
        assert(EspNativeHelpScene_breathPower((uint8_t)i) >=
               EspNativeHelpScene_breathPower((uint8_t)(i - 1)));

    assert(EspNativeHelpScene_paint(softFrame, 0));
    assert(EspNativeHelpScene_paint(brightFrame, 16));
    original = hashArea(brightFrame, 8, 29, 144, 68);
    assert(original != 0U);
    /* The first-generation wall-perspective never loads a raycaster:
     * background is unchanged between breath peaks at every non-UI pixel. */
    for (y = 0; y < H; ++y)
        for (x = 0; x < W; ++x)
            if (softFrame[y * W + x] != brightFrame[y * W + x]) {
                assert(breathAllowed(x, y));
                ++altered;
            }
    assert(altered > 300 && altered < 2900);
    /* TURN symbols retain their connected curves but shift two pixels
     * outward (left x22..35, right x123..136). The original green frames
     * and every gameplay touch target stay exactly where they were. */
    {
        int minX, maxX, minY, maxY;
        assert(glyphDeltaBounds(8, 51, 55, 73,
                               &minX, &maxX, &minY, &maxY) > 20);
        assert(minX == 22 && maxX == 35);
        assert(minY == 56 && maxY == 68);
        assert(glyphDeltaBounds(104, 51, 151, 73,
                               &minX, &maxX, &minY, &maxY) > 20);
        assert(minX == 123 && maxX == 136);
        assert(minY == 56 && maxY == 68);

        /* Each two-line 5x7 WPN caption occupies y78..92: its visual
         * center is y85, matching the center of the 74..96 button. */
        assert(glyphDeltaBounds(8, 74, 55, 96,
                               &minX, &maxX, &minY, &maxY) > 20);
        assert(minY == 78 && maxY == 92);
        assert(minX > 8 + 2 && maxX < 55 - 2);
        assert(glyphDeltaBounds(104, 74, 151, 96,
                               &minX, &maxX, &minY, &maxY) > 20);
        assert(minY == 78 && maxY == 92);
        assert(minX > 104 + 2 && maxX < 151 - 2);
    }
    assert(packReads == readCount);
    assert(!packOpen);

    for (i = 0; i < 3; ++i) {
        memset(rowFrame, 0, sizeof(rowFrame));
        assert(EspNativeHelpScene_paintRow(rowFrame, (uint8_t)i, 0U));
        rowHash[i] = hashArea(rowFrame, 8, 39, 144, 23);
    }
    assert(rowHash[0] != rowHash[1]);
    assert(rowHash[1] != rowHash[2]);
    assert(rowHash[0] != rowHash[2]);
    assert(!EspNativeHelpScene_paintRow(rowFrame, 3U, 0U));
    assert(!EspNativeHelpScene_paint(rowFrame, 17U));

    /* Selecting a different flash-staged level changes the source family. */
    assert(EspNativeHelpScene_begin(3U, 13700U));
    assert(EspNativeHelpScene_previewMapId() == 3U);
    assert(EspNativeHelpScene_textureId() != textureId);
    memset(brightFrame, 0, sizeof(brightFrame));
    assert(EspNativeHelpScene_paint(brightFrame, 16U));
    newMap = hashArea(brightFrame, 8, 29, 144, 68);
    assert(newMap != original);

    /* Replacing assets on the installed PAK changes pixels with same map. */
    for (i = 0; i < (int)(NUM_TEXTURES * WALL_LEN); ++i)
        wallData[4 + i] ^= 0x11U;
    assert(EspNativeHelpScene_begin(3U, 13700U));
    memset(brightFrame, 0, sizeof(brightFrame));
    assert(EspNativeHelpScene_paint(brightFrame, 16U));
    newPack = hashArea(brightFrame, 8, 29, 144, 68);
    assert(newPack != newMap);
    assert(packOpens == 3 && !packOpen);

    EspNativeHelpScene_reset();
    assert(!EspNativeHelpScene_isReady());
    puts("PASS original PAK wall HELP: door perspective, cold-boot, map+PAK source variation, 3 rows, RGB soft breath");
    return 0;
}
