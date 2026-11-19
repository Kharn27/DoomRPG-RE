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
/* Host-only mock of removable SD, 128x640 BMP V5. */
enum { SD_OFFSET = 138, SD_BYTES = SD_OFFSET + 128 * 640 * 3 };
static uint8_t sdBmp[SD_BYTES];
static int sdPresent, sdOpened, sdFailRead;
static unsigned int sdReads;

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
    sdPresent = 0;
    sdFailRead = 0;
    sdReads = 0U;
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

int EspNativeBootLegalsSd_open(uint32_t* outSize) {
    assert(!opened); /* No simultaneous SD override and PAK owner. */
    if (outSize != NULL) *outSize = 0U;
    if (!sdPresent) return 0;
    assert(!sdOpened);
    sdOpened = 1;
    if (outSize != NULL) *outSize = sizeof(sdBmp);
    return 1;
}
int EspNativeBootLegalsSd_read(uint32_t offset, void* dest, size_t len) {
    if (!sdOpened || sdFailRead || dest == NULL ||
        offset > sizeof(sdBmp) || len > sizeof(sdBmp) - offset)
        return 0;
    memcpy(dest, sdBmp + offset, len);
    ++sdReads;
    return 1;
}
void EspNativeBootLegalsSd_close(void) { sdOpened = 0; }

static void sdFixture(void) {
    static const uint8_t colors[5][3] = {
        {248U, 0U, 0U}, {0U, 252U, 0U}, {0U, 0U, 248U},
        {248U, 252U, 248U}, {248U, 0U, 248U}
    };
    memset(sdBmp, 0, sizeof(sdBmp));
    sdBmp[0] = 'B'; sdBmp[1] = 'M';
    for (int i = 0; i < 4; ++i) {
        sdBmp[2+i] = (uint8_t)((uint32_t)SD_BYTES >> (i*8));
        sdBmp[10+i] = (uint8_t)((uint32_t)SD_OFFSET >> (i*8));
        sdBmp[14+i] = (uint8_t)(124U >> (i*8));
        sdBmp[18+i] = (uint8_t)(128U >> (i*8));
        sdBmp[22+i] = (uint8_t)(640U >> (i*8));
        sdBmp[34+i] = (uint8_t)((128U*640U*3U) >> (i*8));
    }
    sdBmp[26] = 1U;
    sdBmp[28] = 24U;
    for (unsigned int y = 0; y < 640U; ++y) {
        const unsigned int panel = (639U - y) / 128U;
        for (unsigned int x = 0; x < 128U; ++x) {
            const size_t pos = SD_OFFSET + y*384U + x*3U;
            sdBmp[pos+0] = colors[panel][2]; /* BGR */
            sdBmp[pos+1] = colors[panel][1];
            sdBmp[pos+2] = colors[panel][0];
        }
    }
    sdPresent = 1;
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
    assert(EspNativeBootLegals_advance(3099U, 0) == 0); /* 2999ms */
    assert(EspNativeBootLegals_advance(3100U, 0) == 1); /* 3000ms */
    checkPanel(1);
    assert(EspNativeBootLegals_advance(3101U, 1) == 1); /* touch skips */
    checkPanel(2);
    assert(EspNativeBootLegals_advance(6100U, 0) == 0); /* 2999ms after touch */
    assert(EspNativeBootLegals_advance(6101U, 0) == 1); /* 3000ms after touch */
    checkPanel(3);
    assert(EspNativeBootLegals_advance(9100U, 0) == 0);
    assert(EspNativeBootLegals_advance(9101U, 0) == 2); /* fourth is last */
    assert(!EspNativeBootLegals_isActive()); /* no fifth credit panel */
    assert(EspNativeBootLegals_currentPanel() == 0U);
    assert(!opened);
    assert(reads == 2U + 4U * 16U); /* only the four g.bmp frames */

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
    for (int i = 0; i < 3; ++i)
        assert(EspNativeBootLegals_advance(1U, 1) == 1);
    assert(EspNativeBootLegals_currentPanel() == 4U);
    assert(EspNativeBootLegals_advance(2U, 1) == 2); /* skip fourth */
    assert(!opened && !EspNativeBootLegals_isActive());
    assert(EspNativeBootLegals_currentPanel() == 0U);
    assert(reads == 2U); /* four touches need no g.bmp pixel reads */

    /* Valid SD BMP: five panels replace the four g.bmp panels, no PAK pixel reads. */
    fixture(); sdFixture();
    assert(EspNativeBootLegals_begin(0U) && !opened && !sdOpened);
    assert(reads == 2U && sdReads == 1U); /* Only header preflights. */
    for (unsigned int panel = 0; panel < 5U; ++panel) {
        static const uint16_t expected[] = {
            0xf800U, 0x07e0U, 0x001fU, 0xffffU, 0xf81fU
        };
        assert(EspNativeBootLegals_currentPanel() == panel + 1U);
        memset(fb, 0x7f, sizeof(fb));
        assert(EspNativeBootLegals_draw(fb, 160U*120U));
        assert(!opened && !sdOpened);
        assert(fb[60U*160U+15U] == 0U);
        assert(fb[60U*160U+16U] == expected[panel]);
        assert(fb[60U*160U+143U] == expected[panel]);
        assert(fb[60U*160U+144U] == 0U);
        if (panel != 4U) assert(EspNativeBootLegals_advance(1U, 1) == 1);
    }
    assert(reads == 2U);
    assert(EspNativeBootLegals_advance(1U, 1) == 2);
    assert(!sdOpened && !opened);

    /* Compatibility: classic DIB40 is valid as well, including padded
     * headers before the pixel array. */
    fixture(); sdFixture();
    sdBmp[14] = 40U;
    assert(EspNativeBootLegals_begin(0U));
    assert(EspNativeBootLegals_draw(fb, 160U*120U));
    assert(fb[60U*160U+16U] == 0xf800U);
    assert(!sdOpened && !opened);

    /* Invalid format: fall back to original four frames. */
    fixture(); sdFixture();
    sdBmp[28] = 8U; /* Not 24bpp. */
    assert(EspNativeBootLegals_begin(0U));
    checkPanel(0);
    assert(sdReads == 1U);
    assert(!sdOpened && !opened);

    /* A valid override that starts failing must repaint the same panel from
     * original g.bmp, then stay on original for every later panel. */
    fixture(); sdFixture();
    assert(EspNativeBootLegals_begin(0U));
    sdFailRead = 1;
    checkPanel(0);
    assert(!sdOpened && !opened);
    sdFailRead = 0;
    assert(EspNativeBootLegals_advance(1U, 1) == 1);
    checkPanel(1);
    assert(reads == 2U + 2U*16U);

    /* V5 DIB file offset must actually follow the 124-byte DIB header. */
    fixture(); sdFixture();
    sdBmp[10] = 54U;
    assert(EspNativeBootLegals_begin(0U));
    checkPanel(0);

    /* Truncated payload is rejected by the strict file-size/pixel bounds. */
    fixture(); sdFixture();
    sdBmp[2] -= 1U; /* BMP file-size header no longer matches SD size. */
    assert(EspNativeBootLegals_begin(0U));
    checkPanel(0);

    /* Valid file disappearing after rendering the 5th panel: no original
     * fifth panel exists, so abandon the stage safely without PAK overread. */
    fixture(); sdFixture();
    assert(EspNativeBootLegals_begin(0U));
    for (int i = 0; i < 4; ++i)
        assert(EspNativeBootLegals_advance(1U, 1) == 1);
    assert(EspNativeBootLegals_currentPanel() == 5U);
    sdFailRead = 1;
    assert(!EspNativeBootLegals_draw(fb, 160U*120U));
    assert(!EspNativeBootLegals_isActive());
    assert(!opened && !sdOpened);
    assert(reads == 2U);
    
    /* Missing custom BMP must stop after fourth original panel, not revive
     * the removed 'ESP32 CYD PORT' credit screen. */
    fixture();
    assert(EspNativeBootLegals_begin(0U));
    for (int i = 0; i < 3; ++i)
        assert(EspNativeBootLegals_advance(1U, 1) == 1);
    assert(EspNativeBootLegals_advance(2U, 1) == 2);
    assert(!EspNativeBootLegals_isActive());

    printf("BOOTLEGALS HOST PASS originals=4 custom=5 durationMs=3000 panels=4 credit=none touch=skip crop=160x120 pack=closed ranges=16/frame failure=closed\\n");
    return 0;
}
