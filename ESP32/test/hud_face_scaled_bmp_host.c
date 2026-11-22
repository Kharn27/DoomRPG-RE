/* Host oracle for the actual bounded indexed-BMP HUD face scaler.
 * No external DoomRPG assets or SDL required; real decoder C is linked.
 * The fake file is an on-disk-format 8bpp pixel region, bottom-up. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "esp_native_indexed_bmp.h"

static uint8_t imageRows[9U * 4U * 4U];
static uint32_t packReads;

int EspAssetPack_isOpen(void) { return 1; }

int EspAssetPack_findEntry(const char* name, EspAssetPackEntry* outEntry) {
    (void)name;
    (void)outEntry;
    return 0;
}

int EspAssetPack_readRange(const EspAssetPackEntry* entry,
                           uint32_t offset, void* destination, size_t length) {
    if (entry == NULL || destination == NULL ||
        offset > sizeof(imageRows) ||
        length > sizeof(imageRows) - offset) return 0;
    memcpy(destination, imageRows + offset, length);
    ++packReads;
    return 1;
}

static void buildNineFaces(EspNativeIndexedBmp* bmp) {
    uint16_t y;
    memset(bmp, 0, sizeof(*bmp));
    bmp->entry.size = sizeof(imageRows);
    bmp->width = 4U;
    bmp->height = 36U;
    bmp->filePitch = 4U;
    bmp->packedPitch = 4U;
    bmp->bitsPerPixel = 8U;
    bmp->paletteCount = 4U;
    bmp->paletteRgb565[0] = 0x0000U;
    bmp->paletteRgb565[1] = 0xf800U;
    bmp->paletteRgb565[2] = 0x07e0U;
    bmp->paletteRgb565[3] = 0x001fU;
    for (y = 0U; y < bmp->height; ++y) {
        const uint8_t index = (uint8_t)(1U + ((y / 4U) % 3U));
        memset(imageRows + ((uint32_t)bmp->height - y - 1U) * 4U,
               index, 4U);
    }
}

int main(void) {
    EspNativeIndexedBmp bmp;
    EspNativeIndexedBmpStats stats;
    uint16_t framebuffer[8U * 8U];
    uint32_t i;
    buildNineFaces(&bmp);

    /* Frame 8 selects original sheet rows 32..35: all pixels are blue. */
    for (i = 0U; i < 64U; ++i) framebuffer[i] = 0xbeefU;
    memset(&stats, 0, sizeof(stats));
    packReads = 0U;
    assert(EspNativeIndexedBmp_blitScaledNearest(
        &bmp, framebuffer, 8U, 8U, 0U, 32U, 4U, 4U,
        2, 3, 2U, 2U, 0U, &stats) == ESP_NATIVE_INDEXED_BMP_OK);
    assert(framebuffer[3U * 8U + 2U] == 0x001fU);
    assert(framebuffer[3U * 8U + 3U] == 0x001fU);
    assert(framebuffer[4U * 8U + 2U] == 0x001fU);
    assert(framebuffer[4U * 8U + 3U] == 0x001fU);
    assert(framebuffer[2U * 8U + 2U] == 0xbeefU);
    assert(framebuffer[4U * 8U + 4U] == 0xbeefU);
    assert(stats.packReads == 2U && packReads == 2U);
    assert(stats.rowsRead == 2U && stats.bytesRead == 8U);
    assert(stats.pixelsWritten == 4U);

    /* Opaque source with magenta-as-transparent must preserve the underlay. */
    bmp.paletteRgb565[3] = 0xf81fU;
    for (i = 0U; i < 64U; ++i) framebuffer[i] = 0xbeefU;
    assert(EspNativeIndexedBmp_blitScaledNearest(
        &bmp, framebuffer, 8U, 8U, 0U, 32U, 4U, 4U,
        2, 3, 2U, 2U, 1U, NULL) == ESP_NATIVE_INDEXED_BMP_OK);
    assert(framebuffer[3U * 8U + 2U] == 0xbeefU);
    bmp.paletteRgb565[3] = 0x001fU;

    /* Never sample outside a face, or permit an unbounded destination. */
    assert(EspNativeIndexedBmp_blitScaledNearest(
        &bmp, framebuffer, 8U, 8U, 3U, 32U, 2U, 4U,
        2, 3, 2U, 2U, 0U, NULL) == ESP_NATIVE_INDEXED_BMP_UNSUPPORTED);
    assert(EspNativeIndexedBmp_blitScaledNearest(
        &bmp, framebuffer, 8U, 8U, 0U, 32U, 4U, 4U,
        2, 3, 33U, 2U, 0U, NULL) == ESP_NATIVE_INDEXED_BMP_INVALID);

    /* An invalid palette index fails closed rather than drawing garbage. */
    imageRows[(36U - 33U - 1U) * 4U + 1U] = 99U;
    assert(EspNativeIndexedBmp_blitScaledNearest(
        &bmp, framebuffer, 8U, 8U, 0U, 32U, 4U, 4U,
        2, 3, 2U, 2U, 0U, NULL) == ESP_NATIVE_INDEXED_BMP_UNSUPPORTED);

    puts("HUD-FACE host indexed nearest-fit: PASS (9-frame offset, bounds, palette, 1 KiB shared scratch)");
    return 0;
}
