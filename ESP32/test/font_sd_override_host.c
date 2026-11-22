/* Real decoder host oracle: SD-over-PAK priority, geometry, indexed bpp,
 * transparent palette, bounded glyph reads and deterministic fallback. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_asset_pack.h"
#include "esp_native_font_sd.h"
#include "esp_native_indexed_bmp.h"
#include "esp_native_indexed_image.h"

static uint8_t pakA[5302], pakLarge[10726], sdA[5302], sdLarge[10726];
static uint8_t sdEnabled[2] = {0U, 0U};
static uint8_t pakOpen = 0U;
static unsigned sdReads, pakReads, rejects, probes;
static void le16(uint8_t* p, uint16_t v) { p[0] = v & 255U; p[1] = v >> 8; }
static void le32(uint8_t* p, uint32_t v) {
    le16(p, (uint16_t)v); le16(p + 2, (uint16_t)(v >> 16));
}

static void fixture(uint8_t* p, size_t capacity,
                    uint16_t width, uint16_t height, uint8_t colors) {
    const uint32_t offset = 54U + colors * 4U;
    const uint32_t pitch = ((uint32_t)width * 4U + 31U) / 32U * 4U;
    assert((size_t)offset + (size_t)pitch * height <= capacity);
    memset(p, 0, capacity);
    p[0] = 'B'; p[1] = 'M';
    le32(p + 2, (uint32_t)capacity);
    le32(p + 10, offset);
    le32(p + 14, 40U);
    le32(p + 18, width);
    le32(p + 22, height);
    le16(p + 26, 1U); le16(p + 28, 4U);
    le32(p + 34, pitch * height);
    le32(p + 46, colors);
    /* Palette index 0 transparent magenta, index 1 white. */
    p[54] = 255U; p[55] = 0U; p[56] = 255U;
    p[58] = 255U; p[59] = 255U; p[60] = 255U;
    memset(p + offset, 0x11, pitch * height);
    p[offset] = 0x01U; /* Top-left half-pixel transparent. */
}

static uint8_t* packData(const char* name, uint32_t* size) {
    if (strcmp(name, "a.bmp") == 0 || strcmp(name, "g.bmp") == 0) {
        *size = sizeof(pakA); return pakA;
    }
    if (strcmp(name, "larger_font.bmp") == 0) {
        *size = sizeof(pakLarge); return pakLarge;
    }
    *size = 0U; return NULL;
}

int EspAssetPack_open(const char* path) {
    assert(strcmp(path, ESP_ASSET_PACK_DEFAULT_PATH) == 0);
    if (pakOpen) return 0;
    pakOpen = 1U; return 1;
}
void EspAssetPack_close(void) { pakOpen = 0U; }
int EspAssetPack_isOpen(void) { return pakOpen; }
int EspAssetPack_findEntry(const char* name, EspAssetPackEntry* entry) {
    uint32_t size = 0U;
    uint8_t* data = packData(name, &size);
    if (!pakOpen || data == NULL) return 0;
    memset(entry, 0, sizeof(*entry));
    entry->size = size;
    entry->nameHash = (data == pakLarge) ? 2U : 1U;
    return 1;
}
int EspAssetPack_readRange(const EspAssetPackEntry* entry, uint32_t offset,
                           void* dest, size_t bytes) {
    const uint8_t* data = entry->nameHash == 2U ? pakLarge : pakA;
    const uint32_t capacity =
        entry->nameHash == 2U ? sizeof(pakLarge) : sizeof(pakA);
    if (!pakOpen || offset > capacity || bytes > capacity - offset) return 0;
    memcpy(dest, data + offset, bytes); ++pakReads; return 1;
}

uint8_t EspNativeFontSd_probe(const char* name, uint32_t* size) {
    unsigned id = strcmp(name, "a.bmp") == 0 ? 1U
                : strcmp(name, "larger_font.bmp") == 0 ? 2U : 0U;
    *size = 0U; ++probes;
    if (id != 0U && sdEnabled[id - 1U]) {
        *size = id == 1U ? sizeof(sdA) : sizeof(sdLarge);
        return (uint8_t)id;
    }
    return 0U;
}
int EspNativeFontSd_read(uint8_t source, uint32_t offset,
                         void* dest, uint32_t bytes) {
    uint32_t size = source == 1U ? sizeof(sdA) : sizeof(sdLarge);
    const uint8_t* data = source == 1U ? sdA : sdLarge;
    if (!source || source > 2U || offset > size || bytes > size - offset) return 0;
    memcpy(dest, data + offset, bytes); ++sdReads; return 1;
}
void EspNativeFontSd_reject(uint8_t source) {
    assert(source == 1U || source == 2U);
    sdEnabled[source - 1U] = 0U; ++rejects;
}

int main(void) {
    EspNativeIndexedBmp bmp = {0};
    EspNativeIndexedBmpStats stats = {0};
    EspNativeIndexedImage image = {0};
    uint16_t framebuffer[160U * 120U] = {0};
    unsigned prior;
    fixture(pakA, sizeof(pakA), 144U, 72U, 3U);
    fixture(sdA, sizeof(sdA), 144U, 72U, 16U);
    fixture(pakLarge, sizeof(pakLarge), 208U, 102U, 3U);
    fixture(sdLarge, sizeof(sdLarge), 208U, 102U, 16U);

    /* UAC 120: the MENU_MAIN PAK is closed while SD soundtrack runs.
     * Standalone loader must select SD larger_font without opening PAK,
     * reject malformed SD, and never grant standalone access to other names. */
    assert(!pakOpen);
    assert(EspNativeIndexedBmp_openSdFontStandalone(
               "larger_font.bmp", &bmp, &stats) ==
           ESP_NATIVE_INDEXED_BMP_NOT_FOUND);
    sdEnabled[1] = 1U;
    prior = pakReads;
    assert(EspNativeIndexedBmp_openSdFontStandalone(
               "larger_font.bmp", &bmp, &stats) ==
           ESP_NATIVE_INDEXED_BMP_OK &&
           bmp.sdFontSource == ESP_NATIVE_FONT_SD_LARGER &&
           bmp.width == 208U && bmp.height == 102U);
    assert(pakReads == prior && sdReads == 2U);
    uint16_t glyph[13U * 17U] = {0};
    assert(EspNativeIndexedBmp_blit(&bmp, glyph, 13U, 17U,
                                   0U, 34U, 13U, 17U,
                                   0, 0, 1U, &stats) ==
           ESP_NATIVE_INDEXED_BMP_OK);
    assert(glyph[0] == 0xffffU);
    assert(pakReads == prior);
    /* The small atlas used by UAC MUST also work with PAK closed.
     * Real a.bmp has palette index 2 = black; it must remain usable. */
    sdEnabled[0] = 1U;
    prior = pakReads;
    unsigned priorSdReads = sdReads;
    assert(EspNativeIndexedBmp_openSdFontStandalone(
               "a.bmp", &bmp, &stats) == ESP_NATIVE_INDEXED_BMP_OK &&
           bmp.sdFontSource == ESP_NATIVE_FONT_SD_STANDARD &&
           bmp.width == 144U && bmp.height == 72U &&
           bmp.bitsPerPixel == 4U && bmp.paletteCount == 16U);
    assert(pakReads == prior && sdReads == priorSdReads + 2U);
    uint16_t smallGlyph[9U * 12U] = {0};
    assert(EspNativeIndexedBmp_blit(&bmp, smallGlyph, 9U, 12U,
                                   0U, 0U, 9U, 12U,
                                   0, 0, 1U, &stats) ==
           ESP_NATIVE_INDEXED_BMP_OK);
    assert(pakReads == prior);
    /* Reject malformed small atlas geometry, without opening PAK. */
    le32(sdA + 18, 143U);
    assert(EspNativeIndexedBmp_openSdFontStandalone(
               "a.bmp", &bmp, &stats) ==
           ESP_NATIVE_INDEXED_BMP_UNSUPPORTED);
    assert(rejects == 1U && !sdEnabled[0]);
    fixture(sdA, sizeof(sdA), 144U, 72U, 16U);
    assert(EspNativeIndexedBmp_openSdFontStandalone(
               "g.bmp", &bmp, &stats) == ESP_NATIVE_INDEXED_BMP_INVALID);
    pakOpen = 1U;
    assert(EspNativeIndexedBmp_openSdFontStandalone(
               "larger_font.bmp", &bmp, &stats) ==
           ESP_NATIVE_INDEXED_BMP_INVALID);
    assert(EspNativeIndexedBmp_openSdFontStandalone(
               "a.bmp", &bmp, &stats) ==
           ESP_NATIVE_INDEXED_BMP_INVALID);
    pakOpen = 0U;
    /* Reset the small-atlas rejection count before the large case. */
    rejects = 0U;
    le32(sdLarge + 18, 207U);
    assert(EspNativeIndexedBmp_openSdFontStandalone(
               "larger_font.bmp", &bmp, &stats) ==
           ESP_NATIVE_INDEXED_BMP_UNSUPPORTED);
    assert(rejects == 1U && !sdEnabled[1]);
    fixture(sdLarge, sizeof(sdLarge), 208U, 102U, 16U);
    rejects = 0U;
    sdReads = pakReads = 0U;

    /* Missing override: exact historical PAK range path, no new SD read. */
    pakOpen = 1U;
    assert(EspNativeIndexedBmp_open("a.bmp", &bmp, &stats) ==
           ESP_NATIVE_INDEXED_BMP_OK && bmp.sdFontSource == 0U);
    assert(sdReads == 0U && pakReads == 2U);
    prior = probes;
    assert(EspNativeIndexedBmp_open("g.bmp", &bmp, &stats) ==
           ESP_NATIVE_INDEXED_BMP_OK && bmp.sdFontSource == 0U);
    assert(probes == prior + 1U);

    /* Present indexed SD font overrides even with a live PAK lease. */
    sdEnabled[0] = 1U;
    assert(EspNativeIndexedBmp_open("a.bmp", &bmp, &stats) ==
           ESP_NATIVE_INDEXED_BMP_OK && bmp.sdFontSource == 1U);
    prior = pakReads;
    assert(EspNativeIndexedBmp_blit(&bmp, framebuffer, 160, 120,
                                   0, 0, 9, 12, 0, 0, 1, &stats) ==
           ESP_NATIVE_INDEXED_BMP_OK);
    assert(pakReads == prior && sdReads >= 3U);
    /* Fixture BMP is bottom-up: the deliberately transparent half-pixel
     * lives on source row 71, not the top rendered row. */
    assert(framebuffer[0] == 0xffffU && framebuffer[1] == 0xffffU);
    /* Erase the destination first: colorkey leaves existing pixels untouched. */
    memset(framebuffer + 160, 0, 9U * sizeof(framebuffer[0]));
    assert(EspNativeIndexedBmp_blit(&bmp, framebuffer, 160, 120,
                                   0, 71, 9, 1, 0, 1, 1, &stats) ==
           ESP_NATIVE_INDEXED_BMP_OK);
    assert(framebuffer[160] == 0U && framebuffer[161] == 0xffffU);
    sdEnabled[1] = 1U;
    assert(EspNativeIndexedBmp_open("larger_font.bmp", &bmp, &stats) ==
           ESP_NATIVE_INDEXED_BMP_OK && bmp.sdFontSource == 2U);
    assert(bmp.width == 208U && bmp.height == 102U);

    /* Wrong atlas geometry must reject the override, retain PAK fallback. */
    le32(sdA + 18, 143U);
    assert(EspNativeIndexedBmp_open("a.bmp", &bmp, &stats) ==
           ESP_NATIVE_INDEXED_BMP_OK && bmp.sdFontSource == 0U);
    assert(rejects == 1U);
    fixture(sdA, sizeof(sdA), 144, 72, 16);
    sdEnabled[0] = 1U;
    le16(sdA + 28, 24U); /* Unsupported RGB24 is NOT silently decoded. */
    assert(EspNativeIndexedBmp_open("a.bmp", &bmp, &stats) ==
           ESP_NATIVE_INDEXED_BMP_OK && bmp.sdFontSource == 0U);
    assert(rejects == 2U);
    pakOpen = 0U;

    /* Full compact texture path: packed bytes come from SD, not PAK. */
    fixture(sdA, sizeof(sdA), 144, 72, 16);
    sdEnabled[0] = 1U;
    prior = pakReads;
    assert(EspNativeImage_loadPack("a.bmp", 1, &image) ==
           ESP_NATIVE_IMAGE_OK);
    assert(image.width == 144 && image.height == 72 &&
           image.bitsPerPixel == 4 && image.paletteSize == 16 &&
           image.sdFont == ESP_NATIVE_FONT_SD_STANDARD);
    assert(pakReads == prior && !pakOpen);
    EspNativeImage_dispose(&image);

    /* Malformed SD before texture import: PAK fallback and exclusive close. */
    le16(sdA + 28, 24U);
    assert(EspNativeImage_loadPack("a.bmp", 1, &image) ==
           ESP_NATIVE_IMAGE_OK);
    assert(image.paletteSize == 3 && image.sdFont == 0U &&
           !pakOpen && rejects == 3U);
    EspNativeImage_dispose(&image);

    /* Large font is also supported as a compact texture. */
    assert(EspNativeImage_loadPack("larger_font.bmp", 1, &image) ==
           ESP_NATIVE_IMAGE_OK);
    assert(image.width == 208 && image.height == 102 &&
           image.sdFont == ESP_NATIVE_FONT_SD_LARGER && !pakOpen);
    EspNativeImage_dispose(&image);

    puts("FONT-SD HOST PASS: missing/valid/corrupt geometry/24bpp, "
         "bounded glyph reads, both PAK and texture owners");
    return 0;
}
