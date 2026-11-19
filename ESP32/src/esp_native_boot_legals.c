#include "esp_native_boot_legals.h"
#include "esp_native_boot_legals_sd.h"
#include "esp_asset_pack.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum {
    BOOT_FRAME_W = 128,
    BOOT_FRAME_H = 128,
    BOOT_FRAME_COUNT = 4, /* original g.bmp panels, unchanged */
    BOOT_SHEET_H = BOOT_FRAME_H * BOOT_FRAME_COUNT,
    BOOT_ROW_BYTES = BOOT_FRAME_W / 2,
    BOOT_ROW_BLOCK = 8,
    BOOT_SD_FRAME_COUNT = 5,
    BOOT_SD_SHEET_H = BOOT_FRAME_H * BOOT_SD_FRAME_COUNT,
    BOOT_SD_ROW_BYTES = BOOT_FRAME_W * 3,
    BOOT_SD_ROW_BLOCK = 4,
    BOOT_SD_PIXEL_BYTES = BOOT_SD_ROW_BYTES * BOOT_SD_SHEET_H,
    BOOT_LOGICAL_W = 160,
    BOOT_LOGICAL_H = 120,
    BOOT_FRAME_X = (BOOT_LOGICAL_W - BOOT_FRAME_W) / 2,
    BOOT_FRAME_Y = (BOOT_LOGICAL_H - BOOT_FRAME_H) / 2,
    /* CYD touch-to-skip: all four original panels share the same 3s slot.
     * Original legacy 1501ms timing is intentionally superseded here. */
    BOOT_SCREEN_MILLIS = 3000
};

typedef struct EspBootLegalsState_s {
    EspAssetPackEntry entry;
    uint16_t palette[16];
    uint32_t pixelOffset;
    uint32_t sdPixelOffset;
    uint32_t frameStartedMs;
    uint8_t panel;
    uint8_t active;
    uint8_t sdCustom;
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
    EspNativeBootLegalsSd_close();
    memset(&legals, 0, sizeof(legals));
}

int EspNativeBootLegals_isActive(void) {
    return legals.active != 0;
}

unsigned int EspNativeBootLegals_currentPanel(void) {
    return legals.active ? (unsigned int)legals.panel + 1U : 0U;
}

/* A PNG decoder would need tens of KiB of temporary RAM on this no-PSRAM
 * board. The user-facing PNG stays as the artwork source; the removable SD
 * override uses an equivalent uncompressed RGB24 BMP with just 1.5 KiB of
 * stack scratch. This is strictly an optional boot-time file. */
static int preflightSdOverride(uint32_t* outOffset) {
    uint8_t header[54];
    uint32_t size = 0U;
    int ok = 0;
    if (outOffset != NULL) *outOffset = 0U;
    if (EspAssetPack_isOpen() || !EspNativeBootLegalsSd_open(&size))
        return 0;

    if (size >= sizeof(header) && EspNativeBootLegalsSd_read(0U, header, sizeof(header))) {
        const uint32_t pixelOffset = read32(header + 10);
        const uint32_t imageBytes = read32(header + 34);
        const uint32_t dibSize = read32(header + 14);
        if (header[0] == 'B' && header[1] == 'M' &&
            read32(header + 2) == size &&
            (dibSize == 40U || dibSize == 124U) &&
            read32(header + 18) == BOOT_FRAME_W &&
            read32(header + 22) == BOOT_SD_SHEET_H &&
            read16(header + 26) == 1U &&
            read16(header + 28) == 24U &&
            read32(header + 30) == 0U &&
            (imageBytes == 0U || imageBytes == BOOT_SD_PIXEL_BYTES) &&
            pixelOffset >= 14U + dibSize && pixelOffset <= size &&
            BOOT_SD_PIXEL_BYTES <= size - pixelOffset) {
            if (outOffset != NULL) *outOffset = pixelOffset;
            ok = 1;
        }
    }
    EspNativeBootLegalsSd_close();
    return ok;
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
    legals.sdCustom = (uint8_t)preflightSdOverride(&legals.sdPixelOffset);
    printf("[BOOTLEGALS] OVERRIDE path=%s status=%s fallback=g.bmp panels=4 format=bmp24 pack=closed\n",
           ESP_NATIVE_BOOT_LEGALS_SD_PATH,
           legals.sdCustom ? "valid" : "missing-or-invalid");
    printf("[BOOTLEGALS] BEGIN asset=g.bmp sheet=128x512 bpp=4 palette=%u panels=4 durationMs=3000 pack=closed\n",
           (unsigned int)colorsUsed);
    return 1;

fail:
    if (opened) EspAssetPack_close();
    printf("[BOOTLEGALS] SKIP asset=g.bmp missing-or-invalid pack=closed fallback=main-menu\n");
    EspNativeBootLegals_cancel();
    return 0;
}

/* Every panel is independent: open SD, seek/read four bottom-up rows,
 * paint into the borrowed framebuffer, close SD. No resident image buffer.
 * On ANY SD read/open failure the caller repaints this same panel from the
 * original PAK (or ROM author-credit) and keeps the boot sequence alive. */
static int drawSdPanel(uint16_t* framebuffer, size_t pixelCount) {
    uint8_t packedRows[BOOT_SD_ROW_BYTES * BOOT_SD_ROW_BLOCK];
    uint32_t size = 0U;
    unsigned int blockY;
    int ok = 0;

    if (EspAssetPack_isOpen() || !EspNativeBootLegalsSd_open(&size))
        return 0;
    if (legals.sdPixelOffset > size ||
        BOOT_SD_PIXEL_BYTES > size - legals.sdPixelOffset)
        goto close;

    memset(framebuffer, 0, pixelCount * sizeof(uint16_t));
    for (blockY = 0U; blockY < BOOT_FRAME_H; blockY += BOOT_SD_ROW_BLOCK) {
        const uint32_t fileY = BOOT_SD_SHEET_H -
            ((uint32_t)legals.panel * BOOT_FRAME_H + blockY +
             BOOT_SD_ROW_BLOCK);
        const uint32_t offset = legals.sdPixelOffset +
            fileY * BOOT_SD_ROW_BYTES;
        unsigned int y;
        if (!EspNativeBootLegalsSd_read(offset, packedRows,
                                         sizeof(packedRows))) goto close;
        for (y = 0U; y < BOOT_SD_ROW_BLOCK; ++y) {
            const int outY = (int)(blockY + y) + BOOT_FRAME_Y;
            const uint8_t* src = packedRows +
                (BOOT_SD_ROW_BLOCK - 1U - y) * BOOT_SD_ROW_BYTES;
            uint16_t* dst;
            unsigned int x;
            if (outY < 0 || outY >= BOOT_LOGICAL_H) continue;
            dst = framebuffer + outY * BOOT_LOGICAL_W + BOOT_FRAME_X;
            for (x = 0U; x < BOOT_FRAME_W; ++x) {
                const uint8_t* bgr = src + x * 3U;
                dst[x] = rgb565(bgr[2], bgr[1], bgr[0]);
            }
        }
    }
    ok = 1;

close:
    EspNativeBootLegalsSd_close();
    if (ok) {
        printf("[BOOTLEGALS] FRAME %u/5 source=sd-bmp24 pack=closed sd=closed scratch=1536 framebuffer=borrowed\n",
               (unsigned int)legals.panel + 1U);
    }
    return ok;
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
    if (legals.sdCustom) {
        if (drawSdPanel(framebuffer, pixelCount)) return 1;
        legals.sdCustom = 0U;
        printf("[BOOTLEGALS] OVERRIDE read-failed panel=%u fallback=g.bmp pack=closed sd=closed\n",
               (unsigned int)legals.panel + 1U);
        if (legals.panel >= BOOT_FRAME_COUNT) {
            /* No fifth ROM panel on current main: hand off to main menu.
             * Do not read past the end of the original g.bmp sheet. */
            EspNativeBootLegals_cancel();
            return 0;
        }
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
    const unsigned int count = legals.sdCustom ? BOOT_SD_FRAME_COUNT : BOOT_FRAME_COUNT;
    if (++legals.panel == count) {
        printf("[BOOTLEGALS] COMPLETE reason=%s panels=%u source=%s pack=closed\n",
               touchPressedEdge ? "touch" : "timer", count,
               legals.sdCustom ? "sd-bmp24" : "g.bmp");
        EspNativeBootLegals_cancel();
        return 2;
    }
    printf("[BOOTLEGALS] NEXT panel=%u reason=%s\n",
           (unsigned int)legals.panel + 1U,
           touchPressedEdge ? "touch" : "timer");
    return 1;
}
