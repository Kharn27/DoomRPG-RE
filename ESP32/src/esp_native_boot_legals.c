#include "esp_native_boot_legals.h"
#include "esp_asset_pack.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum {
    BOOT_FRAME_W = 128,
    BOOT_FRAME_H = 128,
    BOOT_FRAME_COUNT = 4, /* original g.bmp panels, unchanged */
    BOOT_TOTAL_PANELS = BOOT_FRAME_COUNT + 1, /* native port credit */
    BOOT_SHEET_H = BOOT_FRAME_H * BOOT_FRAME_COUNT,
    BOOT_ROW_BYTES = BOOT_FRAME_W / 2,
    BOOT_ROW_BLOCK = 8,
    BOOT_LOGICAL_W = 160,
    BOOT_LOGICAL_H = 120,
    BOOT_FRAME_X = (BOOT_LOGICAL_W - BOOT_FRAME_W) / 2,
    BOOT_FRAME_Y = (BOOT_LOGICAL_H - BOOT_FRAME_H) / 2,
    /* CYD touch-to-skip: all five panels share the same readable 3s slot.
     * Original legacy 1501ms timing is intentionally superseded here. */
    BOOT_SCREEN_MILLIS = 3000
};

typedef struct EspBootLegalsState_s {
    EspAssetPackEntry entry;
    uint16_t palette[16];
    uint32_t pixelOffset;
    uint32_t frameStartedMs;
    uint8_t panel;
    uint8_t active;
} EspBootLegalsState;

static EspBootLegalsState legals;

static uint16_t read16(const uint8_t* p) {
    return (uint16_t)(p[0] | (uint16_t)(p[1] << 8));
}

static uint32_t read32(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
    return (uint16_t)(((uint16_t)(r & 0xf8U) << 8) |
                      ((uint16_t)(g & 0xfcU) << 3) |
                      (uint16_t)(b >> 3));
}

static int sameEntry(const EspAssetPackEntry* a,
                     const EspAssetPackEntry* b) {
    return a->nameHash == b->nameHash &&
           a->offset == b->offset &&
           a->size == b->size &&
           a->crc32 == b->crc32 &&
           a->flags == b->flags;
}

void EspNativeBootLegals_cancel(void) {
    memset(&legals, 0, sizeof(legals));
}

int EspNativeBootLegals_isActive(void) {
    return legals.active != 0;
}

unsigned int EspNativeBootLegals_currentPanel(void) {
    return legals.active ? (unsigned int)legals.panel + 1U : 0U;
}

int EspNativeBootLegals_begin(uint32_t nowMs) {
    EspAssetPackEntry entry;
    uint8_t header[54];
    uint8_t colors[64];
    uint32_t dibBytes, colorsUsed, paletteAt, pixelOffset;
    uint32_t i;
    int opened = 0;

    EspNativeBootLegals_cancel();
    if (EspAssetPack_isOpen()) return 0;
    if (!EspAssetPack_open(ESP_ASSET_PACK_DEFAULT_PATH)) return 0;
    opened = 1;
    if (!EspAssetPack_findEntry("g.bmp", &entry) ||
        (entry.flags & ESP_ASSET_PACK_FLAG_DIRECTORY) != 0U ||
        entry.size < sizeof(header) ||
        !EspAssetPack_readRange(&entry, 0U, header, sizeof(header)))
        goto fail;

    dibBytes = read32(header + 14);
    pixelOffset = read32(header + 10);
    colorsUsed = read32(header + 46);
    if (colorsUsed == 0U) colorsUsed = 16U;

    /* Strict known original sprite sheet. Never allocate 128x512 pixels. */
    if (header[0] != 'B' || header[1] != 'M' ||
        dibBytes != 40U ||
        read32(header + 18) != BOOT_FRAME_W ||
        read32(header + 22) != BOOT_SHEET_H ||
        read16(header + 26) != 1U ||
        read16(header + 28) != 4U ||
        read32(header + 30) != 0U ||
        colorsUsed == 0U || colorsUsed > 16U)
        goto fail;

    paletteAt = 14U + dibBytes;
    if (pixelOffset < paletteAt + colorsUsed * 4U ||
        pixelOffset > entry.size ||
        BOOT_ROW_BYTES * BOOT_SHEET_H > entry.size - pixelOffset ||
        !EspAssetPack_readRange(&entry, paletteAt, colors,
                                colorsUsed * 4U))
        goto fail;

    for (i = 0; i < colorsUsed; ++i) {
        const uint8_t* bgra = colors + i * 4U;
        legals.palette[i] = rgb565(bgra[2], bgra[1], bgra[0]);
    }
    legals.entry = entry;
    legals.pixelOffset = pixelOffset;
    legals.frameStartedMs = nowMs;
    legals.panel = 0;
    legals.active = 1;
    EspAssetPack_close();
    printf("[BOOTLEGALS] BEGIN asset=g.bmp sheet=128x512 bpp=4 palette=%u panels=4 durationMs=3000 credit=Kharn27 creditMs=3000 pack=closed\n",
           (unsigned int)colorsUsed);
    return 1;

fail:
    if (opened) EspAssetPack_close();
    printf("[BOOTLEGALS] SKIP asset=g.bmp missing-or-invalid pack=closed fallback=main-menu\n");
    EspNativeBootLegals_cancel();
    return 0;
}

/* Fifth screen: compact ROM-only 5x7 glyphs for the exact author credit.
 * Only letters used here are compiled; no bitmap asset, second framebuffer,
 * heap allocation, UI state owner, or SD/PAK read is introduced. */
typedef struct BootCreditGlyph_s {
    char ch;
    uint8_t row[7];
} BootCreditGlyph;

static const BootCreditGlyph creditGlyphs[] = {
    { 'E', {31,16,16,30,16,16,31} },
    { 'S', {15,16,16,14, 1, 1,30} },
    { 'P', {30,17,17,30,16,16,16} },
    { '3', {30, 1, 1,14, 1, 1,30} },
    { '2', {14,17, 1, 2, 4, 8,31} },
    { 'C', {15,16,16,16,16,16,15} },
    { 'Y', {17,17,10, 4, 4, 4, 4} },
    { 'D', {30,17,17,17,17,17,30} },
    { 'O', {14,17,17,17,17,17,14} },
    { 'R', {30,17,17,30,20,18,17} },
    { 'T', {31, 4, 4, 4, 4, 4, 4} },
    { 'K', {17,18,20,24,20,18,17} },
    { '7', {31, 1, 2, 4, 8, 8, 8} },
    { 'm', { 0, 0,26,21,21,21,21} },
    { 'a', { 0, 0,14, 1,15,17,15} },
    { 'd', { 1, 1,13,19,17,19,13} },
    { 'e', { 0, 0,14,17,31,16,14} },
    { 'w', { 0, 0,17,17,21,21,10} },
    { 'i', { 4, 0,12, 4, 4, 4,14} },
    { 't', { 4, 4,31, 4, 4, 5, 2} },
    { 'h', {16,16,22,25,17,17,17} },
    { 'l', {12, 4, 4, 4, 4, 4,14} },
    { 'o', { 0, 0,14,17,17,17,14} },
    { 'v', { 0, 0,17,17,17,10, 4} },
    { 'b', {16,16,22,25,17,25,22} },
    { 'y', { 0, 0,17,17,15, 1,14} },
    { 'n', { 0, 0,22,25,17,17,17} },
    { 'r', { 0, 0,22,25,16,16,16} }
};

static void creditPixel(uint16_t* framebuffer, int x, int y,
                        uint16_t color) {
    if (x >= 0 && x < BOOT_LOGICAL_W &&
        y >= 0 && y < BOOT_LOGICAL_H)
        framebuffer[y * BOOT_LOGICAL_W + x] = color;
}

static const uint8_t* creditRows(char ch) {
    size_t i;
    for (i = 0; i < sizeof(creditGlyphs) / sizeof(creditGlyphs[0]); ++i) {
        if (creditGlyphs[i].ch == ch)
            return creditGlyphs[i].row;
    }
    return NULL; /* space, or unsupported: fail visibly blank, no memory IO */
}

static void creditText(uint16_t* framebuffer, const char* text,
                       int top, int scale, uint16_t ink) {
    const int width = (int)strlen(text) * 6 * scale;
    int cursor = (BOOT_LOGICAL_W - width) / 2;
    for (; *text != '\0'; ++text, cursor += 6 * scale) {
        const uint8_t* rows = creditRows(*text);
        int y, x, sy, sx;
        if (rows == NULL) continue;
        for (y = 0; y < 7; ++y) {
            for (x = 0; x < 5; ++x) {
                if (!(rows[y] & (1U << (4 - x)))) continue;
                for (sy = 0; sy < scale; ++sy) {
                    for (sx = 0; sx < scale; ++sx)
                        creditPixel(framebuffer, cursor + x * scale + sx,
                                    top + y * scale + sy, ink);
                }
            }
        }
    }
}

static void creditMark(uint16_t* framebuffer) {
    /* A tiny hand-rastered copyright symbol, not a claim on Doom RPG.
     * The author credit applies solely to this independent CYD port. */
    static const uint16_t circleC[9] = {
        0x7c,0x82,0x139,0x141,0x141,0x141,0x139,0x82,0x7c
    };
    const uint16_t ink = 0x8410U;
    const int left = (BOOT_LOGICAL_W - (9 + 3 + 7 * 6)) / 2;
    int y, x;
    for (y = 0; y < 9; ++y) {
        for (x = 0; x < 9; ++x) {
            if (circleC[y] & (1U << (8 - x)))
                creditPixel(framebuffer, left + x, 90 + y, ink);
        }
    }
    /* Keep the real copyright mark separate from ASCII text raster. */
    {
        const char* tail = "Kharn27";
        int cursor = left + 12;
        for (; *tail; ++tail, cursor += 6) {
            const uint8_t* rows = creditRows(*tail);
            if (rows != NULL) {
                for (y = 0; y < 7; ++y) {
                    for (x = 0; x < 5; ++x) {
                        if (rows[y] & (1U << (4 - x)))
                            creditPixel(framebuffer, cursor + x, 91 + y,
                                        ink);
                    }
                }
            }
        }
    }
}

static void drawPortCredit(uint16_t* framebuffer, size_t pixelCount) {
    static const uint8_t heart[7] = {
        0x66U,0xffU,0xffU,0x7eU,0x3cU,0x18U,0x00U
    };
    int x, y;
    memset(framebuffer, 0, pixelCount * sizeof(uint16_t));
    /* Quiet retro splash: monochrome palette, one warm accent, no animation. */
    for (x = 39; x <= 120; ++x)
        creditPixel(framebuffer, x, 17, 0x7324U);
    creditText(framebuffer, "ESP32 CYD PORT", 28, 1, 0xe6a6U);
    creditText(framebuffer, "made with love", 54, 1, 0xc638U);
    for (y = 0; y < 7; ++y) {
        for (x = 0; x < 8; ++x) {
            if (heart[y] & (1U << (7 - x)))
                creditPixel(framebuffer, 76 + x, 68 + y, 0xd964U);
        }
    }
    creditMark(framebuffer);
}

int EspNativeBootLegals_draw(uint16_t* framebuffer, size_t pixelCount) {
    EspAssetPackEntry entry;
    uint8_t packedRows[BOOT_ROW_BYTES * BOOT_ROW_BLOCK];
    unsigned int blockY;
    int ok = 0;

    if (!legals.active || framebuffer == NULL ||
        pixelCount != (size_t)BOOT_LOGICAL_W * BOOT_LOGICAL_H ||
        EspAssetPack_isOpen())
        goto fail;
    if (legals.panel == BOOT_FRAME_COUNT) {
        drawPortCredit(framebuffer, pixelCount);
        printf("[BOOTLEGALS] FRAME 5/5 credit=Kharn27 ms=3000 pack=closed scratch=0 framebuffer=borrowed\n");
        return 1;
    }
    if (!EspAssetPack_open(ESP_ASSET_PACK_DEFAULT_PATH))
        goto fail;
    if (!EspAssetPack_findEntry("g.bmp", &entry) ||
        !sameEntry(&entry, &legals.entry))
        goto close;

    /* Black full screen; the legacy 128px panel is vertically clipped by
     * 4px at the top and bottom of the original 160x120 viewport. */
    memset(framebuffer, 0, pixelCount * sizeof(uint16_t));
    for (blockY = 0; blockY < BOOT_FRAME_H; blockY += BOOT_ROW_BLOCK) {
        /* Positive-height BMP rows are bottom-up. Request 8 adjacent rows
         * per PAK read, then reverse the row order in our 512B scratch. */
        const uint32_t fileY = BOOT_SHEET_H -
            ((uint32_t)legals.panel * BOOT_FRAME_H + blockY +
             BOOT_ROW_BLOCK);
        const uint32_t offset = legals.pixelOffset +
            fileY * BOOT_ROW_BYTES;
        unsigned int y;
        if (!EspAssetPack_readRange(&entry, offset, packedRows,
                                    sizeof(packedRows)))
            goto close;
        for (y = 0; y < BOOT_ROW_BLOCK; ++y) {
            const int outputY = (int)(blockY + y) + BOOT_FRAME_Y;
            const uint8_t* src =
                packedRows + (BOOT_ROW_BLOCK - 1U - y) * BOOT_ROW_BYTES;
            uint16_t* dst;
            unsigned int x;
            if (outputY < 0 || outputY >= BOOT_LOGICAL_H) continue;
            dst = framebuffer + outputY * BOOT_LOGICAL_W + BOOT_FRAME_X;
            for (x = 0; x < BOOT_ROW_BYTES; ++x) {
                const uint8_t packed = src[x];
                dst[x * 2U] = legals.palette[packed >> 4];
                dst[x * 2U + 1U] = legals.palette[packed & 0x0fU];
            }
        }
    }
    ok = 1;

close:
    EspAssetPack_close();
fail:
    if (!ok) {
        printf("[BOOTLEGALS] FAILED panel=%u; fallback=main-menu\n",
               (unsigned int)legals.panel + 1U);
        EspNativeBootLegals_cancel();
        return 0;
    }
    printf("[BOOTLEGALS] FRAME %u/4 pack=closed scratch=512 framebuffer=borrowed\n",
           (unsigned int)legals.panel + 1U);
    return 1;
}

int EspNativeBootLegals_advance(uint32_t nowMs, int touchPressedEdge) {
    if (!legals.active) return 0;
    if (!touchPressedEdge &&
        (uint32_t)(nowMs - legals.frameStartedMs) < BOOT_SCREEN_MILLIS)
        return 0;

    legals.frameStartedMs = nowMs;
    if (++legals.panel == BOOT_TOTAL_PANELS) {
        printf("[BOOTLEGALS] COMPLETE reason=%s panels=5 originals=4 credit=Kharn27 pack=closed\n",
               touchPressedEdge ? "touch" : "timer");
        EspNativeBootLegals_cancel();
        return 2;
    }
    printf("[BOOTLEGALS] NEXT panel=%u reason=%s\n",
           (unsigned int)legals.panel + 1U,
           touchPressedEdge ? "touch" : "timer");
    return 1;
}
