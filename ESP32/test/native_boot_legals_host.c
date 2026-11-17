/* Real native g.bmp renderer with a synthetic 4x128x128 bottom-up BMP.
 * Exercises time, touch, geometry, pack lease, and corrupt-input fail close. */
#include "esp_native_boot_legals.h"
#include "esp_asset_pack.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum { BMP_OFFSET = 86, BMP_SIZE = 86 + 512 * 64 };
static uint8_t bmp[BMP_SIZE];
static int opened;
static int busy;
static int missing;
static int failRead;
static unsigned int reads;

static void put16(int at, uint16_t v) {
    bmp[at] = (uint8_t)v;
    bmp[at + 1] = (uint8_t)(v >> 8);
}
static void put32(int at, uint32_t v) {
    for (int i = 0; i < 4; ++i) bmp[at + i] = (uint8_t)(v >> (i * 8));
}
static void fixture(void) {
    static const uint8_t rgb[4][3] = {
        {248, 0, 0}, {0, 252, 0}, {0, 0, 248}, {248, 252, 248}
    };
    memset(bmp, 0, sizeof(bmp));
    bmp[0] = 'B'; bmp[1] = 'M';
    put32(2, BMP_SIZE);
    put32(10, BMP_OFFSET);
    put32(14, 40);
    put32(18, 128);
    put32(22, 512);
    put16(26, 1);
    put16(28, 4);
    put32(34, 512 * 64);
    put32(46, 8); /* Exactly the uploaded real g.bmp palette size. */
    for (int i = 0; i < 4; ++i) {
        bmp[54 + (i + 1) * 4 + 0] = rgb[i][2];
        bmp[54 + (i + 1) * 4 + 1] = rgb[i][1];
        bmp[54 + (i + 1) * 4 + 2] = rgb[i][0];
    }
    for (int fileY = 0; fileY < 512; ++fileY) {
        const int topY = 511 - fileY;
        const uint8_t pixel = (uint8_t)(1 + topY / 128);
        memset(bmp + BMP_OFFSET + fileY * 64,
               (int)((pixel << 4) | pixel), 64);
    }
    opened = 0;
    busy = 0;
    missing = 0;
    failRead = 0;
    reads = 0;
    EspNativeBootLegals_cancel();
}

int EspAssetPack_isOpen(void) { return opened || busy; }
int EspAssetPack_open(const char* path) {
    if (opened || busy || strcmp(path, ESP_ASSET_PACK_DEFAULT_PATH) != 0)
        return 0;
    opened = 1;
    return 1;
}
void EspAssetPack_close(void) { assert(opened); opened = 0; }
int EspAssetPack_findEntry(const char* name, EspAssetPackEntry* entry) {
    if (!opened || missing || strcmp(name, "g.bmp") != 0) return 0;
    *entry = (EspAssetPackEntry){
        .nameHash = 0x12345678U, .offset = 250U, .size = sizeof(bmp),
        .crc32 = 0xabcdef01U, .flags = 0U
    };
    return 1;
}
int EspAssetPack_readRange(const EspAssetPackEntry* entry, uint32_t offset,
                           void* dest, size_t length) {
    if (!opened || failRead || entry->size != sizeof(bmp) ||
        offset > sizeof(bmp) || length > sizeof(bmp) - offset)
        return 0;
    memcpy(dest, bmp + offset, length);
    ++reads;
    return 1;
}

static uint16_t fb[160 * 120];
static const uint16_t ink[4] = {0xf800U, 0x07e0U, 0x001fU, 0xffffU};

static void checkPanel(unsigned int index) {
    assert(EspNativeBootLegals_isActive());
    assert(EspNativeBootLegals_currentPanel() == index + 1U);
    memset(fb, 0x7f, sizeof(fb));
    assert(EspNativeBootLegals_draw(fb, 160U * 120U));
    assert(!opened);
    /* Original crop: 128px graphic centered x16, vertically clipped -4. */
    for (int y = 0; y < 120; ++y) {
        assert(fb[y * 160 + 0] == 0);
        assert(fb[y * 160 + 15] == 0);
        assert(fb[y * 160 + 16] == ink[index]);
        assert(fb[y * 160 + 79] == ink[index]);
        assert(fb[y * 160 + 143] == ink[index]);
        assert(fb[y * 160 + 144] == 0);
        assert(fb[y * 160 + 159] == 0);
    }
}

int main(void) {
    fixture();
    assert(EspNativeBootLegals_begin(100U));
    assert(!opened);
    checkPanel(0);
    assert(EspNativeBootLegals_advance(1599U, 0) == 0);
    assert(EspNativeBootLegals_advance(1600U, 0) == 0);
    assert(EspNativeBootLegals_advance(1601U, 0) == 1);
    checkPanel(1);
    assert(EspNativeBootLegals_advance(1602U, 1) == 1);
    checkPanel(2);
    assert(EspNativeBootLegals_advance(3102U, 0) == 0);
    assert(EspNativeBootLegals_advance(3103U, 0) == 1);
    checkPanel(3);
    assert(EspNativeBootLegals_advance(4604U, 0) == 1);
    assert(EspNativeBootLegals_currentPanel() == 5U);
    {
        const unsigned int before = reads;
        memset(fb, 0x7f, sizeof(fb));
        assert(EspNativeBootLegals_draw(fb, 160U * 120U));
        assert(!opened);
        assert(reads == before); /* credit never opens or reads PAK */
        /* Independent exact character, heart, and copyright raster probes. */
        assert(fb[0] == 0U);
        assert(fb[17 * 160 + 80] == 0x7324U);
        assert(fb[28 * 160 + 38] == 0xe6a6U); /* E of ESP32 */
        assert(fb[68 * 160 + 77] == 0xd964U); /* heart */
        assert(fb[80 * 160 + 38] == 0xff2cU); /* K of Kharn27 */
        assert(fb[105 * 160 + 55] == 0x8410U); /* (C) circle */
        assert(fb[119 * 160 + 159] == 0U);
    }
    assert(EspNativeBootLegals_advance(6603U, 0) == 0); /* 1999ms */
    assert(EspNativeBootLegals_advance(6604U, 0) == 2); /* 2000ms */
    assert(!EspNativeBootLegals_isActive());
    assert(EspNativeBootLegals_currentPanel() == 0);
    assert(!opened);
    assert(reads == 2U + 4U * 16U); /* credit adds no PAK IO */

    fixture();
    assert(EspNativeBootLegals_begin(0U));
    assert(EspNativeBootLegals_draw(fb, 160U * 120U - 1U) == 0);
    assert(!EspNativeBootLegals_isActive());
    assert(!opened);

    fixture();
    busy = 1;
    assert(!EspNativeBootLegals_begin(0U));
    assert(!EspNativeBootLegals_isActive());
    busy = 0;

    fixture();
    missing = 1;
    assert(!EspNativeBootLegals_begin(0U));
    assert(!opened);

    fixture();
    bmp[28] = 8; /* Not the original 4bpp sprite sheet. */
    assert(!EspNativeBootLegals_begin(0U));
    assert(!opened);

    fixture();
    put32(22, 511); /* Four 128px panels required. */
    assert(!EspNativeBootLegals_begin(0U));
    assert(!opened);

    fixture();
    put32(10, BMP_SIZE - 16U); /* Cannot fit pixel sheet. */
    assert(!EspNativeBootLegals_begin(0U));
    assert(!opened);

    fixture();
    assert(EspNativeBootLegals_begin(0U));
    failRead = 1;
    assert(!EspNativeBootLegals_draw(fb, 160U * 120U));
    assert(!EspNativeBootLegals_isActive());
    assert(!opened);

    fixture();
    assert(EspNativeBootLegals_begin(0U));
    for (int i = 0; i < 4; ++i)
        assert(EspNativeBootLegals_advance(1U, 1) == 1);
    assert(EspNativeBootLegals_currentPanel() == 5U);
    assert(EspNativeBootLegals_draw(fb, 160U * 120U));
    assert(EspNativeBootLegals_advance(2U, 1) == 2); /* skip credit */
    assert(!opened && !EspNativeBootLegals_isActive());
    assert(reads == 2U); /* five touches did not read g.bmp pixels */

    printf("BOOTLEGALS HOST PASS originals=4 credit=Kharn27 creditMs=2000 touch=skip crop=160x120 pack=closed ranges=16/frame failure=closed\\n");
    return 0;
}
