#include "esp_native_boot_legals.h"
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
    printf("[BOOTLEGALS] BEGIN asset=g.bmp sheet=128x512 bpp=4 palette=%u panels=4 durationMs=3000 pack=closed\n",
           (unsigned int)colorsUsed);
    return 1;

fail:
    if (opened) EspAssetPack_close();
    printf("[BOOTLEGALS] SKIP asset=g.bmp missing-or-invalid pack=closed fallback=main-menu\n");
    EspNativeBootLegals_cancel();
    return 0;
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
    if (++legals.panel == BOOT_FRAME_COUNT) {
        printf("[BOOTLEGALS] COMPLETE reason=%s panels=4 originals=4 pack=closed\n",
               touchPressedEdge ? "touch" : "timer");
        EspNativeBootLegals_cancel();
        return 2;
    }
    printf("[BOOTLEGALS] NEXT panel=%u reason=%s\n",
           (unsigned int)legals.panel + 1U,
           touchPressedEdge ? "touch" : "timer");
    return 1;
}
