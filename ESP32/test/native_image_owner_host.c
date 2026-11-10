#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_native_indexed_image.h"
#include "esp_asset_pack.h"

/* In-memory fixture stands in for immutable PAK ranges. The actual native
 * image owner (not a mock decoder) is compiled into this test. */
static uint8_t bmp[512];
static uint32_t bmpSize;
static int packOpen, opens, closes, packReads, readFails;
static uint32_t bytesRead;
static int allocCalls, allocLive, failAllocation;

static void le16(uint8_t* p, uint16_t v) {
    p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8);
}
static void le32(uint8_t* p, uint32_t v) {
    le16(p, (uint16_t)v); le16(p + 2, (uint16_t)(v >> 16));
}
static uint8_t indexFor(uint32_t x, uint32_t y, uint8_t bpp) {
    return (uint8_t)((x + y) % (bpp == 1 ? 2U : 4U));
}
static void fixture(uint32_t w, uint32_t h, uint8_t bpp, int topDown) {
    uint32_t n = bpp == 1 ? 2U : 4U;
    uint32_t pitch = (w * bpp + 7U) / 8U;
    uint32_t filePitch = ((w * bpp + 31U) / 32U) * 4U;
    uint32_t offset = 54U + 4U * n;
    uint32_t fy, x;
    assert(offset + filePitch * h <= sizeof(bmp));
    memset(bmp, 0xEA, sizeof(bmp));
    bmp[0] = 'B'; bmp[1] = 'M';
    le32(bmp + 2, offset + filePitch * h);
    le32(bmp + 10, offset);
    le32(bmp + 14, 40U);
    le32(bmp + 18, w);
    le32(bmp + 22, topDown ? (uint32_t)(-(int32_t)h) : h);
    le16(bmp + 26, 1U);
    le16(bmp + 28, bpp);
    le32(bmp + 30, 0U);
    le32(bmp + 46, n);
    /* magenta, unquantized red, green, blue */
    bmp[54] = 255; bmp[55] = 0; bmp[56] = 255; bmp[57] = 0;
    if (n > 1) { bmp[58] = 11; bmp[59] = 26; bmp[60] = 253; bmp[61] = 0; }
    if (n > 2) { bmp[62] = 25; bmp[63] = 252; bmp[64] = 9; bmp[65] = 0; }
    if (n > 3) { bmp[66] = 253; bmp[67] = 14; bmp[68] = 9; bmp[69] = 0; }
    for (fy = 0; fy < h; ++fy) {
        uint32_t y = topDown ? fy : h - 1U - fy;
        uint8_t* row = bmp + offset + filePitch * fy;
        memset(row, 0, pitch);
        for (x = 0; x < w; ++x) {
            uint8_t index = indexFor(x, y, bpp);
            if (bpp == 8) row[x] = index;
            else if (bpp == 4) row[x >> 1] |= (uint8_t)(index << (x & 1U ? 0U : 4U));
            else row[x >> 3] |= (uint8_t)(index << (7U - (x & 7U)));
        }
    }
    bmpSize = offset + filePitch * h;
}
static int readBytes(void* ctx, uint32_t offset, void* dst, uint32_t size) {
    (void)ctx;
    ++packReads;
    if (readFails || offset > bmpSize || size > bmpSize - offset) return 0;
    memcpy(dst, bmp + offset, size);
    bytesRead += size;
    return 1;
}
int EspAssetPack_isOpen(void) { return packOpen; }
int EspAssetPack_open(const char* path) {
    assert(strcmp(path, ESP_ASSET_PACK_DEFAULT_PATH) == 0);
    assert(!packOpen); packOpen = 1; ++opens; return 1;
}
void EspAssetPack_close(void) { assert(packOpen); packOpen = 0; ++closes; }
int EspAssetPack_findEntry(const char* name, EspAssetPackEntry* entry) {
    if (strcmp(name, "fixture.bmp") != 0) return 0;
    memset(entry, 0, sizeof(*entry)); entry->size = bmpSize;
    return 1;
}
int EspAssetPack_readRange(const EspAssetPackEntry* e,
                           uint32_t offset, void* out, size_t bytes) {
    assert(e->size == bmpSize);
    assert(bytes <= UINT32_MAX);
    return readBytes(NULL, offset, out, (uint32_t)bytes);
}
static void* failAlloc(size_t bytes) {
    ++allocCalls;
    if (allocCalls == failAllocation) return NULL;
    void* p = malloc(bytes);
    if (p) ++allocLive;
    return p;
}
static void trackedFree(void* p) { if (p) { --allocLive; free(p); } }

static void expectPixels(const EspNativeIndexedImage* image, uint8_t bpp) {
    int x, y;
    for (y = 0; y < image->height; ++y) {
        const uint8_t* row = image->pixels + y * image->pitch;
        for (x = 0; x < image->width; ++x) {
            uint8_t actual;
            if (bpp == 8) actual = row[x];
            else if (bpp == 4) actual = (uint8_t)((x & 1) ? (row[x >> 1] & 15) : (row[x >> 1] >> 4));
            else actual = (uint8_t)((row[x >> 3] >> (7 - (x & 7))) & 1);
            assert(actual == indexFor((uint32_t)x, (uint32_t)y, bpp));
        }
    }
}
static void testPack(int bpp, int topDown) {
    EspNativeIndexedImage image;
    const int oldOpen = opens, oldClose = closes;
    fixture(bpp == 1 ? 9U : (bpp == 4 ? 5U : 3U), 3U, (uint8_t)bpp, topDown);
    packReads = 0; bytesRead = 0;
    assert(EspNativeImage_loadPack("fixture.bmp", 1, &image) == ESP_NATIVE_IMAGE_OK);
    assert(!packOpen && opens == oldOpen + 1 && closes == oldClose + 1);
    assert(image.bitsPerPixel == bpp && image.hasColorKey == 1U);
    assert(image.paletteSize == (bpp == 1 ? 2 : 4));
    assert(image.pitch == (image.width * bpp + 7) / 8);
    assert(image.palette[0].r == 255 && image.palette[0].g == 0 &&
           image.palette[0].b == 255 && image.palette[0].a == 255);
    assert(image.palette[1].r == 255 && image.palette[1].g == 24 &&
           image.palette[1].b == 8);
    expectPixels(&image, (uint8_t)bpp);
    assert(bytesRead < bmpSize); /* packed rows, no padded BMP copy */
    EspNativeImage_dispose(&image);
    assert(image.pixels == NULL && image.palette == NULL);
}

static void testFailure(void) {
    EspNativeIndexedImage out;
    EspNativeImageInput input;
    fixture(5, 3, 4, 0);
    memset(&input, 0, sizeof(input));
    input.size = bmpSize; input.readRange = readBytes;
    input.allocate = failAlloc; input.release = trackedFree;
    for (failAllocation = 1; failAllocation <= 2; ++failAllocation) {
        allocCalls = allocLive = 0;
        assert(EspNativeImage_decode(&input, 0, &out) == ESP_NATIVE_IMAGE_OOM);
        assert(!out.pixels && !out.palette && allocLive == 0);
    }
    failAllocation = 0;
    readFails = 1;
    assert(EspNativeImage_decode(&input, 0, &out) == ESP_NATIVE_IMAGE_TRUNCATED);
    assert(!out.pixels && !out.palette);
    readFails = 0;
    bmp[28] = 2;
    assert(EspNativeImage_decode(&input, 0, &out) == ESP_NATIVE_IMAGE_UNSUPPORTED);
    bmp[28] = 4;
    bmp[26] = 2;
    assert(EspNativeImage_decode(&input, 0, &out) == ESP_NATIVE_IMAGE_UNSUPPORTED);
    bmp[26] = 1;
    le32(bmp + 30, 1);
    assert(EspNativeImage_decode(&input, 0, &out) == ESP_NATIVE_IMAGE_UNSUPPORTED);
    le32(bmp + 30, 0);
    le32(bmp + 10, bmpSize + 1);
    assert(EspNativeImage_decode(&input, 0, &out) == ESP_NATIVE_IMAGE_TRUNCATED);
    fixture(5, 3, 4, 0);
    input.size = bmpSize - 1U;
    assert(EspNativeImage_decode(&input, 0, &out) == ESP_NATIVE_IMAGE_TRUNCATED);
    input.size = bmpSize;
    bmp[0] = 'Z';
    assert(EspNativeImage_decode(&input, 0, &out) == ESP_NATIVE_IMAGE_UNSUPPORTED);
    fixture(5, 3, 4, 0);
    packOpen = 1; /* exclusive PAK lease must not be stolen */
    assert(EspNativeImage_loadPack("fixture.bmp", 0, &out) == ESP_NATIVE_IMAGE_PACK_UNAVAILABLE);
    assert(packOpen); packOpen = 0;
    assert(EspNativeImage_loadPack("missing.bmp", 0, &out) == ESP_NATIVE_IMAGE_PACK_UNAVAILABLE);
    assert(!packOpen);
    readFails = 1;
    assert(EspNativeImage_loadPack("fixture.bmp", 0, &out) == ESP_NATIVE_IMAGE_TRUNCATED);
    assert(!packOpen);
    readFails = 0;
    assert(allocLive == 0);
}

int main(void) {
    testPack(1, 0); testPack(1, 1);
    testPack(4, 0); testPack(4, 1);
    testPack(8, 0); testPack(8, 1);
    testFailure();
    puts("SDL-C1 PASS real indexed owner: PAK leases, 1/4/8 packing, orientation,"
         " row padding, RGB565 palette parity, transparency, errors/OOM/rollback");
    return 0;
}
