#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "esp_asset_pack.h"
#include "esp_native_gameplay_hud_direction.h"
#include "esp_native_gameplay_status_message.h"
#include "esp_native_indexed_bmp.h"
#include "platform_video_c_bridge.h"
#include "platform_video_config.h"

#define HUD_BOTTOM_Y 100
#define HUD_ARROW_RIGHT 144
#define HUD_ARROW_Y 102
#define HUD_DIR_X 135
#define HUD_DIR_Y 107
#define HUD_FONT_WIDTH 9U
#define HUD_FONT_HEIGHT 12U
#define HUD_TRANSPARENT 1U
#define HUD_OPAQUE 0U

/* The initial hardware-proven HUD places pistol-ammo text immediately to the
 * left of the compass. Do not clear a broad fixed panel here: the old x112..143
 * range erased part of that text on every TURN. Reconstruct only the exact
 * horizontal union occupied by o.bmp and the direction glyph. */
#define HUD_COMPASS_RIGHT (HUD_DIR_X + (int)HUD_FONT_WIDTH - 1)

typedef struct HudDirectionScratch_s {
    EspNativeIndexedBmp bar;
    EspNativeIndexedBmp arrow;
    EspNativeIndexedBmp font;
    EspNativeGameplayHudDirectionStats stats;
} HudDirectionScratch;

static void mergeStats(EspNativeGameplayHudDirectionStats* out,
                       const EspNativeIndexedBmpStats* in) {
    if (out == NULL || in == NULL) return;
    out->packReads += in->packReads;
    out->bytesRead += in->bytesRead;
    out->rowsRead += in->rowsRead;
    out->pixelsWritten += in->pixelsWritten;
}

static int openBmp(const char* name,
                   EspNativeIndexedBmp* bmp,
                   EspNativeGameplayHudDirectionStats* stats) {
    EspNativeIndexedBmpStats local;
    memset(&local, 0, sizeof(local));
    if (EspNativeIndexedBmp_open(name, bmp, &local) != ESP_NATIVE_INDEXED_BMP_OK) {
        mergeStats(stats, &local);
        return 0;
    }
    mergeStats(stats, &local);
    return 1;
}

static int drawBmp(const EspNativeIndexedBmp* bmp,
                   uint16_t* framebuffer,
                   uint16_t sourceX,
                   uint16_t sourceY,
                   uint16_t width,
                   uint16_t height,
                   int destinationX,
                   int destinationY,
                   uint8_t transparent,
                   EspNativeGameplayHudDirectionStats* stats) {
    EspNativeIndexedBmpStats local;
    memset(&local, 0, sizeof(local));
    if (EspNativeIndexedBmp_blit(
            bmp, framebuffer,
            DOOMRPG_LOGICAL_WIDTH, DOOMRPG_LOGICAL_HEIGHT,
            sourceX, sourceY, width, height,
            (int16_t)destinationX, (int16_t)destinationY,
            transparent, &local) != ESP_NATIVE_INDEXED_BMP_OK) {
        mergeStats(stats, &local);
        return 0;
    }
    mergeStats(stats, &local);
    return 1;
}

static char directionChar(uint8_t angle) {
    switch (angle) {
    case 0U: return 'E';
    case 64U: return 'N';
    case 128U: return 'W';
    case 192U: return 'S';
    default: return '\0';
    }
}

static int clearCompass(const EspNativeIndexedBmp* bar,
                        const EspNativeIndexedBmp* arrow,
                        uint16_t* framebuffer,
                        EspNativeGameplayHudDirectionStats* stats) {
    int left;
    int right = HUD_COMPASS_RIGHT;
    int y;

    if (bar == NULL || arrow == NULL ||
        bar->width != 20U || bar->height != 20U ||
        arrow->width == 0U || arrow->width > 32U) {
        return 0;
    }

    left = HUD_ARROW_RIGHT - (int)arrow->width;
    if (HUD_DIR_X < left) left = HUD_DIR_X;
    if (HUD_ARROW_RIGHT - 1 > right) right = HUD_ARROW_RIGHT - 1;
    if (left < 0 || right >= DOOMRPG_LOGICAL_WIDTH || left > right) return 0;

    for (y = 0; y < 20; ++y) {
        int x = left;
        while (x <= right) {
            const uint16_t sourceX = (uint16_t)(x % 20);
            int run = 20 - (int)sourceX;
            const int remaining = right - x + 1;
            if (run > remaining) run = remaining;
            if (!drawBmp(bar, framebuffer,
                         sourceX, (uint16_t)y,
                         (uint16_t)run, 1U,
                         x, HUD_BOTTOM_Y + y,
                         HUD_OPAQUE, stats)) {
                return 0;
            }
            x += run;
        }
    }
    return 1;
}

int EspNativeGameplayHudDirection_render(
    uint8_t angle,
    EspNativeGameplayHudDirectionStats* outStats) {
    HudDirectionScratch* scratch = NULL;
    EspNativeGameplayHudDirectionStats* stats;
    EspNativeIndexedBmp* bar;
    EspNativeIndexedBmp* arrow;
    EspNativeIndexedBmp* font;
    uint16_t* framebuffer;
    size_t framebufferBytes;
    char c;
    uint8_t glyph;
    int ok = 0;

    if (outStats != NULL) memset(outStats, 0, sizeof(*outStats));
    if (outStats == NULL) return 0;
    c = directionChar(angle);
    if (c == '\0' || EspAssetPack_isOpen()) return 0;

    framebuffer = (uint16_t*)Esp32PlatformVideo_framebuffer();
    framebufferBytes = Esp32PlatformVideo_framebufferSizeBytes();
    if (framebuffer == NULL ||
        framebufferBytes != (size_t)DOOMRPG_LOGICAL_WIDTH *
                                (size_t)DOOMRPG_LOGICAL_HEIGHT *
                                sizeof(uint16_t)) {
        return 0;
    }

    /* FORCE_MESSAGE is a top-bar fallback, not a separate popup. Repaint its
     * dirty owner before the final HUD direction pass; this function still
     * performs no present, so the enclosing gameplay frame owns the one screen
     * presentation and can roll the semantic owner back on frame failure. */
    if (!EspNativeGameplayStatusMessage_paintIfDirty()) return 0;

    /* Each EspNativeIndexedBmp carries a 256-entry RGB565 palette. Keep the
     * three metadata objects in one bounded transient heap block instead of
     * stacking ~1.6 KiB on top of the gameplay compositor/render stack. */
    scratch = (HudDirectionScratch*)malloc(sizeof(*scratch));
    if (scratch == NULL) return 0;
    memset(scratch, 0, sizeof(*scratch));
    stats = &scratch->stats;
    bar = &scratch->bar;
    arrow = &scratch->arrow;
    font = &scratch->font;

    if (!EspAssetPack_open(ESP_ASSET_PACK_DEFAULT_PATH)) goto done;
    if (!openBmp("k.bmp", bar, stats) ||
        bar->width != 20U || bar->height != 20U ||
        !openBmp("o.bmp", arrow, stats) ||
        arrow->width == 0U || arrow->width > 32U ||
        arrow->height == 0U || arrow->height > 20U ||
        !openBmp("a.bmp", font, stats) ||
        font->width != 144U || font->height != 72U) {
        goto done;
    }
    stats->resourcesValidated = 3U;

    if (!clearCompass(bar, arrow, framebuffer, stats)) goto done;
    if (!drawBmp(arrow, framebuffer, 0U, 0U, arrow->width, arrow->height,
                 HUD_ARROW_RIGHT - (int)arrow->width, HUD_ARROW_Y,
                 HUD_TRANSPARENT, stats)) {
        goto done;
    }

    glyph = (uint8_t)c - 33U;
    if (!drawBmp(font, framebuffer,
                 (uint16_t)(HUD_FONT_WIDTH * (glyph & 0x0fU)),
                 (uint16_t)(HUD_FONT_HEIGHT * (glyph >> 4)),
                 HUD_FONT_WIDTH, HUD_FONT_HEIGHT,
                 HUD_DIR_X, HUD_DIR_Y, HUD_TRANSPARENT, stats)) {
        goto done;
    }

    stats->angle = angle;
    stats->rendered = 1U;
    ok = 1;

done:
    if (EspAssetPack_isOpen()) EspAssetPack_close();
    if (ok) *outStats = *stats;
    free(scratch);
    return ok;
}
