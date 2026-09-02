#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_asset_pack.h"
#include "esp_map_sprite_topology.h"
#include "esp_native_gameplay_action_engine.h"
#include "esp_native_gameplay_hud.h"
#include "esp_native_gameplay_hud_direction.h"
#include "esp_native_gameplay_player_resources.h"
#include "esp_native_gameplay_player_state.h"
#include "esp_native_indexed_bmp.h"
#include "platform_video_c_bridge.h"
#include "platform_video_config.h"

#define PROJECTION_VISUAL_HIDDEN 0x80U

#define HUD_BOTTOM_Y 100
#define HUD_CENTER_BASE 16
#define HUD_NUMBER_Y 105
#define HUD_MID_Y 110
#define HUD_FONT_WIDTH 9U
#define HUD_FONT_HEIGHT 12U
#define HUD_FONT_ADVANCE 7
#define HUD_HEALTH_X 2
#define HUD_ARMOR_X 35
#define HUD_FACE_X 66
#define HUD_AMMO_X 85
#define HUD_ORIENTATION_X 126
#define HUD_ORIENTATION_ARROW_X 128
#define HUD_LINE1_X 33
#define HUD_LINE2_X 155
#define HUD_TRANSPARENT 1U
#define HUD_OPAQUE 0U

static uint32_t lastPaintedPlayerFNV;
static uint8_t lastPaintedMapId;

int __real_EspNativeGameplayActionEngine_getVisualState(
    uint32_t spriteIndex,
    uint8_t* outVisualState);
int __real_EspNativeGameplayActionEngine_getEntity(
    uint32_t spriteIndex,
    uint8_t* outType,
    uint8_t* outSubType,
    uint16_t* outLinkState,
    uint16_t* outLinkOrder);
int __real_EspNativeGameplayHudDirection_render(
    uint8_t angle,
    EspNativeGameplayHudDirectionStats* outStats);

static uint16_t rgb565(uint32_t rgb888) {
    uint8_t red = (uint8_t)((rgb888 >> 16) & 0xffU);
    uint8_t green = (uint8_t)((rgb888 >> 8) & 0xffU);
    uint8_t blue = (uint8_t)(rgb888 & 0xffU);
    return (uint16_t)((((uint16_t)red & 0xf8U) << 8) |
                      (((uint16_t)green & 0xfcU) << 3) |
                      ((uint16_t)blue >> 3));
}

static void mergeBmpStats(EspNativeGameplayHudDirectionStats* out,
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
    if (EspNativeIndexedBmp_open(name, bmp, &local) !=
        ESP_NATIVE_INDEXED_BMP_OK) {
        mergeBmpStats(stats, &local);
        return 0;
    }
    mergeBmpStats(stats, &local);
    return 1;
}

static int blit(const EspNativeIndexedBmp* bmp,
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
        mergeBmpStats(stats, &local);
        return 0;
    }
    mergeBmpStats(stats, &local);
    return 1;
}

static int tileBottom(const EspNativeIndexedBmp* bmp,
                      uint16_t* framebuffer,
                      EspNativeGameplayHudDirectionStats* stats) {
    EspNativeIndexedBmpStats local;
    memset(&local, 0, sizeof(local));
    if (bmp == NULL || bmp->width != 20U || bmp->height != 20U ||
        EspNativeIndexedBmp_tile(
            bmp, framebuffer,
            DOOMRPG_LOGICAL_WIDTH, DOOMRPG_LOGICAL_HEIGHT,
            0, HUD_BOTTOM_Y,
            DOOMRPG_LOGICAL_WIDTH, 20U,
            HUD_OPAQUE, &local) != ESP_NATIVE_INDEXED_BMP_OK) {
        mergeBmpStats(stats, &local);
        return 0;
    }
    mergeBmpStats(stats, &local);
    return 1;
}

static void verticalLine(uint16_t* framebuffer,
                         int x,
                         uint16_t color,
                         EspNativeGameplayHudDirectionStats* stats) {
    int y;
    if (framebuffer == NULL || x < 0 || x >= DOOMRPG_LOGICAL_WIDTH) return;
    for (y = HUD_BOTTOM_Y; y < DOOMRPG_LOGICAL_HEIGHT; ++y) {
        framebuffer[y * DOOMRPG_LOGICAL_WIDTH + x] = color;
        if (stats != NULL) ++stats->pixelsWritten;
    }
}

static int drawSmallText(const EspNativeIndexedBmp* font,
                         uint16_t* framebuffer,
                         const char* text,
                         int anchorX,
                         int y,
                         uint8_t rightAligned,
                         uint8_t maxChars,
                         EspNativeGameplayHudDirectionStats* stats) {
    size_t length;
    size_t i;
    int x;

    if (font == NULL || text == NULL || font->width != 144U ||
        font->height != 72U) return 0;
    length = strlen(text);
    if (length > maxChars) length = maxChars;
    x = rightAligned ? anchorX - (int)length * HUD_FONT_ADVANCE : anchorX;

    for (i = 0U; i < length; ++i) {
        uint8_t c = (uint8_t)text[i];
        uint8_t glyph;
        if (c == ' ') {
            x += HUD_FONT_ADVANCE;
            continue;
        }
        if (c < 33U || c > 127U) return 0;
        glyph = (uint8_t)(c - 33U);
        if (!blit(font, framebuffer,
                  (uint16_t)(HUD_FONT_WIDTH * (glyph & 0x0fU)),
                  (uint16_t)(HUD_FONT_HEIGHT * (glyph >> 4)),
                  HUD_FONT_WIDTH, HUD_FONT_HEIGHT,
                  x, y, HUD_TRANSPARENT, stats)) {
            return 0;
        }
        x += HUD_FONT_ADVANCE;
    }
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

static int repaintPlayerHud(uint8_t angle,
                            EspNativeGameplayHudDirectionStats* outStats) {
    const EspNativeGameplayHudState* hud = EspNativeGameplayHud_view();
    uint32_t playerFNV;
    uint16_t* framebuffer;
    size_t framebufferBytes;
    EspNativeIndexedBmp* bmp = NULL;
    EspNativeGameplayHudDirectionStats localStats;
    uint16_t iconWidth;
    uint16_t iconHeight;
    uint16_t faceWidth;
    uint16_t faceHeight;
    int cx = HUD_CENTER_BASE;
    int faceX = HUD_FACE_X + HUD_CENTER_BASE;
    char healthNum[4];
    char armorNum[4];
    char ammoNum[4];
    char dir[2];
    int ok = 0;

    if (hud == NULL || hud->active != 1U || hud->painted != 1U ||
        directionChar(angle) == '\0') {
        return 1;
    }

    playerFNV = EspNativeGameplayPlayerState_fingerprint();
    if (playerFNV == 0U) return 1;
    if (lastPaintedPlayerFNV == playerFNV &&
        lastPaintedMapId == hud->model.targetMapId) {
        return 1;
    }

    framebuffer = (uint16_t*)Esp32PlatformVideo_framebuffer();
    framebufferBytes = Esp32PlatformVideo_framebufferSizeBytes();
    if (framebuffer == NULL ||
        framebufferBytes != (size_t)DOOMRPG_LOGICAL_WIDTH *
                                (size_t)DOOMRPG_LOGICAL_HEIGHT *
                                sizeof(uint16_t) ||
        EspAssetPack_isOpen()) {
        return 0;
    }

    bmp = (EspNativeIndexedBmp*)malloc(sizeof(*bmp));
    if (bmp == NULL) return 0;
    memset(&localStats, 0, sizeof(localStats));

    if (!EspAssetPack_open(ESP_ASSET_PACK_DEFAULT_PATH)) goto done;

    if (!openBmp("k.bmp", bmp, &localStats) ||
        !tileBottom(bmp, framebuffer, &localStats)) {
        goto done;
    }

    verticalLine(framebuffer, HUD_LINE1_X + cx,
                 rgb565(0x313131U), &localStats);
    verticalLine(framebuffer, HUD_LINE2_X + cx,
                 rgb565(0x313131U), &localStats);
    verticalLine(framebuffer, HUD_LINE1_X + cx + 1,
                 rgb565(0x808591U), &localStats);
    verticalLine(framebuffer, HUD_LINE2_X + cx + 1,
                 rgb565(0x808591U), &localStats);

    if (!openBmp("m.bmp", bmp, &localStats) || bmp->width == 0U ||
        bmp->width > 32U || bmp->height < 9U ||
        (bmp->height % 9U) != 0U || (bmp->height / 9U) > 20U) {
        goto done;
    }
    iconWidth = bmp->width;
    iconHeight = (uint16_t)(bmp->height / 9U);
    if (!blit(bmp, framebuffer, 0U, 0U, iconWidth, iconHeight,
              HUD_HEALTH_X + cx, HUD_MID_Y - (int)(iconHeight >> 1),
              HUD_TRANSPARENT, &localStats) ||
        !blit(bmp, framebuffer, 0U, iconHeight, iconWidth, iconHeight,
              HUD_ARMOR_X + cx, HUD_MID_Y - (int)(iconHeight >> 1),
              HUD_TRANSPARENT, &localStats)) {
        goto done;
    }
    if (hud->model.weaponsPresent != 0U) {
        uint16_t row = hud->model.weapon == 0U
                           ? 2U
                           : (uint16_t)(hud->model.ammoType + 3U);
        if (row >= 9U ||
            !blit(bmp, framebuffer, 0U,
                  (uint16_t)(iconHeight * row), iconWidth, iconHeight,
                  HUD_AMMO_X + cx, HUD_MID_Y - (int)(iconHeight >> 1),
                  HUD_TRANSPARENT, &localStats)) {
            goto done;
        }
    }

    if (!openBmp("l.bmp", bmp, &localStats) || bmp->width == 0U ||
        bmp->width > 32U || bmp->height < 9U ||
        (bmp->height % 9U) != 0U || (bmp->height / 9U) > 20U) {
        goto done;
    }
    faceWidth = bmp->width;
    faceHeight = (uint16_t)(bmp->height / 9U);
    if (hud->faceState >= 9U) goto done;
    verticalLine(framebuffer, faceX - 1, rgb565(0x323232U), &localStats);
    if (!blit(bmp, framebuffer, 0U,
              (uint16_t)(hud->faceState * faceHeight),
              faceWidth, faceHeight,
              faceX, HUD_MID_Y - (int)(faceHeight >> 1),
              HUD_OPAQUE, &localStats)) {
        goto done;
    }
    verticalLine(framebuffer, faceX + faceWidth,
                 rgb565(0x828282U), &localStats);

    if (!openBmp("o.bmp", bmp, &localStats) || bmp->width == 0U ||
        bmp->width > 32U || bmp->height == 0U || bmp->height > 20U ||
        !blit(bmp, framebuffer, 0U, 0U, bmp->width, bmp->height,
              HUD_ORIENTATION_ARROW_X + cx - (int)bmp->width,
              HUD_NUMBER_Y - 3,
              HUD_TRANSPARENT, &localStats)) {
        goto done;
    }

    if (!openBmp("a.bmp", bmp, &localStats) ||
        bmp->width != 144U || bmp->height != 72U) {
        goto done;
    }

    snprintf(healthNum, sizeof(healthNum), "%u",
             (unsigned int)hud->model.health);
    snprintf(armorNum, sizeof(armorNum), "%u",
             (unsigned int)hud->model.armor);
    if (hud->model.weapon == 0U) {
        strncpy(ammoNum, "--", sizeof(ammoNum));
        ammoNum[sizeof(ammoNum) - 1U] = '\0';
    }
    else {
        snprintf(ammoNum, sizeof(ammoNum), "%u",
                 (unsigned int)hud->model.ammo);
    }
    dir[0] = directionChar(angle);
    dir[1] = '\0';

    if (!drawSmallText(bmp, framebuffer, healthNum,
                       HUD_HEALTH_X + cx + 14 + iconWidth + 1,
                       HUD_NUMBER_Y, 1U, 3U, &localStats) ||
        !drawSmallText(bmp, framebuffer, armorNum,
                       HUD_ARMOR_X + cx + 14 + iconWidth,
                       HUD_NUMBER_Y, 1U, 3U, &localStats)) {
        goto done;
    }
    if (hud->model.weaponsPresent != 0U &&
        !drawSmallText(bmp, framebuffer, ammoNum,
                       HUD_AMMO_X + cx + iconWidth + 14,
                       HUD_NUMBER_Y, 1U, 2U, &localStats)) {
        goto done;
    }
    if (!drawSmallText(bmp, framebuffer, dir,
                       HUD_ORIENTATION_X + cx,
                       HUD_NUMBER_Y + 2, 1U, 1U, &localStats)) {
        goto done;
    }

    lastPaintedPlayerFNV = playerFNV;
    lastPaintedMapId = hud->model.targetMapId;
    localStats.resourcesValidated = 5U;
    ok = 1;

done:
    if (EspAssetPack_isOpen()) EspAssetPack_close();
    if (ok && outStats != NULL) {
        outStats->packReads += localStats.packReads;
        outStats->bytesRead += localStats.bytesRead;
        outStats->rowsRead += localStats.rowsRead;
        outStats->pixelsWritten += localStats.pixelsWritten;
        if (outStats->resourcesValidated < localStats.resourcesValidated) {
            outStats->resourcesValidated = localStats.resourcesValidated;
        }
    }
    free(bmp);
    return ok;
}

int __wrap_EspNativeGameplayActionEngine_getVisualState(
    uint32_t spriteIndex,
    uint8_t* outVisualState) {
    if (!__real_EspNativeGameplayActionEngine_getVisualState(spriteIndex,
                                                              outVisualState)) {
        return 0;
    }
    if (outVisualState != NULL &&
        EspNativeGameplayPlayerResources_isConsumed(spriteIndex)) {
        *outVisualState |= PROJECTION_VISUAL_HIDDEN;
    }
    return 1;
}

int __wrap_EspNativeGameplayActionEngine_getEntity(
    uint32_t spriteIndex,
    uint8_t* outType,
    uint8_t* outSubType,
    uint16_t* outLinkState,
    uint16_t* outLinkOrder) {
    if (!__real_EspNativeGameplayActionEngine_getEntity(
            spriteIndex, outType, outSubType, outLinkState, outLinkOrder)) {
        return 0;
    }
    if (outLinkState != NULL &&
        EspNativeGameplayPlayerResources_isConsumed(spriteIndex)) {
        *outLinkState &= (uint16_t)~(ESP_MAP_SPRITE_TOPOLOGY_LINKED |
                                     ESP_MAP_SPRITE_TOPOLOGY_ALIVE);
        if (outLinkOrder != NULL) *outLinkOrder = 0U;
    }
    return 1;
}

int __wrap_EspNativeGameplayHudDirection_render(
    uint8_t angle,
    EspNativeGameplayHudDirectionStats* outStats) {
    if (!__real_EspNativeGameplayHudDirection_render(angle, outStats)) return 0;
    if (!repaintPlayerHud(angle, outStats)) {
        printf("[PLAYERHUD] REFRESH-FAILED angle=%u playerFNV=%08x mutation=no\n",
               (unsigned int)angle,
               (unsigned int)EspNativeGameplayPlayerState_fingerprint());
        return 0;
    }
    return 1;
}
