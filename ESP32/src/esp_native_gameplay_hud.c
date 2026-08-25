#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "esp_asset_pack.h"
#include "esp_hud_post_load_clear_state.h"
#include "esp_hud_refresh_state.h"
#include "esp_map_catalog.h"
#include "esp_native_gameplay_hud.h"
#include "esp_native_indexed_bmp.h"
#include "platform_video_c_bridge.h"
#include "platform_video_config.h"

#if DOOMRPG_LOGICAL_WIDTH != 160 || DOOMRPG_LOGICAL_HEIGHT != 120
#error "The first native gameplay HUD contract is defined only for 160x120"
#endif

#define HUD_TOP_HEIGHT 20U
#define HUD_BOTTOM_HEIGHT 20U
#define HUD_BOTTOM_Y (DOOMRPG_LOGICAL_HEIGHT - HUD_BOTTOM_HEIGHT)
#define HUD_CENTER_X 80
#define HUD_NORMAL_CENTER_OFFSET 64
#define HUD_CENTER_BASE (HUD_CENTER_X - HUD_NORMAL_CENTER_OFFSET)
#define HUD_NUMBER_Y 105
#define HUD_MID_Y 110
#define HUD_FONT_WIDTH 9U
#define HUD_FONT_HEIGHT 12U
#define HUD_FONT_ADVANCE 7
#define HUD_FONT_SRC_WIDTH 144U
#define HUD_FONT_SRC_HEIGHT 72U
#define HUD_TRANSPARENT 1U
#define HUD_OPAQUE 0U

#define HUD_HEALTH_X 2
#define HUD_ARMOR_X 35
#define HUD_FACE_X 66
#define HUD_AMMO_X 85
#define HUD_ORIENTATION_X 126
#define HUD_ORIENTATION_ARROW_X 128
#define HUD_LINE1_X 33
#define HUD_LINE2_X 155

static EspNativeGameplayHudState hudState;

static uint16_t rgb565(uint32_t rgb888) {
    uint8_t red = (uint8_t)((rgb888 >> 16) & 0xffU);
    uint8_t green = (uint8_t)((rgb888 >> 8) & 0xffU);
    uint8_t blue = (uint8_t)(rgb888 & 0xffU);
    return (uint16_t)((((uint16_t)red & 0xf8U) << 8) |
                      (((uint16_t)green & 0xfcU) << 3) |
                      ((uint16_t)blue >> 3));
}

static void mergeBmpStats(EspNativeGameplayHudStats* out,
                          const EspNativeIndexedBmpStats* in) {
    if (out == NULL || in == NULL) return;
    out->packReads += in->packReads;
    out->bytesRead += in->bytesRead;
    out->rowsRead += in->rowsRead;
    out->pixelsWritten += in->pixelsWritten;
}

static uint8_t chooseFace(const EspNativeGameplayHudModel* model) {
    if (model->health > 0U && model->gotFace != 0U) return 8U;
    if (model->health == 0U || model->damageActive == 0U) {
        if (model->health <= model->maxHealth / 4U) return 3U;
        if (model->health <= model->maxHealth / 3U) return 2U;
        if (model->health <= model->maxHealth / 2U) return 1U;
        return 0U;
    }
    if (model->damageDir == 2U) return 7U;
    if (model->damageDir == 1U) return 6U;
    if (model->damageDir == 3U) return 5U;
    return 4U;
}

static int directionSupported(uint8_t angle) {
    return angle == 0U || angle == 64U || angle == 128U || angle == 192U;
}

static char directionChar(uint8_t angle) {
    if (angle == 0U) return 'E';
    if (angle == 128U) return 'W';
    if (angle == 192U) return 'S';
    return 'N';
}

static void drawVerticalLine(uint16_t* framebuffer,
                             int x,
                             int y0,
                             int y1,
                             uint16_t color,
                             EspNativeGameplayHudStats* stats) {
    int y;
    if (framebuffer == NULL || x < 0 || x >= DOOMRPG_LOGICAL_WIDTH) return;
    if (y0 < 0) y0 = 0;
    if (y1 >= DOOMRPG_LOGICAL_HEIGHT) y1 = DOOMRPG_LOGICAL_HEIGHT - 1;
    if (y1 < y0) return;
    for (y = y0; y <= y1; ++y) {
        framebuffer[y * DOOMRPG_LOGICAL_WIDTH + x] = color;
        if (stats != NULL) ++stats->pixelsWritten;
    }
}

static EspNativeGameplayHudStatus openAsset(
    const char* name,
    EspNativeIndexedBmp* bmp,
    EspNativeGameplayHudStats* stats) {
    EspNativeIndexedBmpStats local;
    EspNativeIndexedBmpStatus status;
    memset(&local, 0, sizeof(local));
    status = EspNativeIndexedBmp_open(name, bmp, &local);
    mergeBmpStats(stats, &local);
    return status == ESP_NATIVE_INDEXED_BMP_OK
               ? ESP_NATIVE_GAMEPLAY_HUD_OK
               : ESP_NATIVE_GAMEPLAY_HUD_RESOURCE_FAILED;
}

static int validateStatusBar(const EspNativeIndexedBmp* bmp) {
    return bmp != NULL && bmp->width > 0U && bmp->height == HUD_BOTTOM_HEIGHT;
}

static int validateIcons(const EspNativeIndexedBmp* bmp) {
    return bmp != NULL && bmp->width > 0U && bmp->width <= 32U &&
           bmp->height >= 9U && (bmp->height % 9U) == 0U &&
           (bmp->height / 9U) <= HUD_BOTTOM_HEIGHT;
}

static int validateFaces(const EspNativeIndexedBmp* bmp) {
    return bmp != NULL && bmp->width > 0U && bmp->width <= 32U &&
           bmp->height >= 9U && (bmp->height % 9U) == 0U &&
           (bmp->height / 9U) <= HUD_BOTTOM_HEIGHT;
}

static int validateArrow(const EspNativeIndexedBmp* bmp) {
    return bmp != NULL && bmp->width > 0U && bmp->width <= 32U &&
           bmp->height > 0U && bmp->height <= HUD_BOTTOM_HEIGHT;
}

static int validateFont(const EspNativeIndexedBmp* bmp) {
    return bmp != NULL && bmp->width == HUD_FONT_SRC_WIDTH &&
           bmp->height == HUD_FONT_SRC_HEIGHT;
}

static EspNativeGameplayHudStatus preflightResources(
    EspNativeGameplayHudStats* stats) {
    EspNativeIndexedBmp bmp;

    if (openAsset("k.bmp", &bmp, stats) != ESP_NATIVE_GAMEPLAY_HUD_OK ||
        !validateStatusBar(&bmp)) return ESP_NATIVE_GAMEPLAY_HUD_RESOURCE_FAILED;
    if (openAsset("m.bmp", &bmp, stats) != ESP_NATIVE_GAMEPLAY_HUD_OK ||
        !validateIcons(&bmp)) return ESP_NATIVE_GAMEPLAY_HUD_RESOURCE_FAILED;
    if (openAsset("l.bmp", &bmp, stats) != ESP_NATIVE_GAMEPLAY_HUD_OK ||
        !validateFaces(&bmp)) return ESP_NATIVE_GAMEPLAY_HUD_RESOURCE_FAILED;
    if (openAsset("o.bmp", &bmp, stats) != ESP_NATIVE_GAMEPLAY_HUD_OK ||
        !validateArrow(&bmp)) return ESP_NATIVE_GAMEPLAY_HUD_RESOURCE_FAILED;
    if (openAsset("a.bmp", &bmp, stats) != ESP_NATIVE_GAMEPLAY_HUD_OK ||
        !validateFont(&bmp)) return ESP_NATIVE_GAMEPLAY_HUD_RESOURCE_FAILED;

    if (stats != NULL) stats->resourcesValidated = 5U;
    return ESP_NATIVE_GAMEPLAY_HUD_OK;
}

static EspNativeGameplayHudStatus drawBmp(
    const EspNativeIndexedBmp* bmp,
    uint16_t* framebuffer,
    uint16_t sourceX,
    uint16_t sourceY,
    uint16_t width,
    uint16_t height,
    int destinationX,
    int destinationY,
    uint8_t transparent,
    EspNativeGameplayHudStats* stats) {
    EspNativeIndexedBmpStats local;
    EspNativeIndexedBmpStatus status;
    memset(&local, 0, sizeof(local));
    status = EspNativeIndexedBmp_blit(
        bmp, framebuffer, DOOMRPG_LOGICAL_WIDTH, DOOMRPG_LOGICAL_HEIGHT,
        sourceX, sourceY, width, height,
        (int16_t)destinationX, (int16_t)destinationY,
        transparent, &local);
    mergeBmpStats(stats, &local);
    return status == ESP_NATIVE_INDEXED_BMP_OK
               ? ESP_NATIVE_GAMEPLAY_HUD_OK
               : ESP_NATIVE_GAMEPLAY_HUD_RESOURCE_FAILED;
}

static EspNativeGameplayHudStatus drawSmallText(
    const EspNativeIndexedBmp* font,
    uint16_t* framebuffer,
    const char* text,
    int anchorX,
    int y,
    uint8_t rightAligned,
    uint8_t maxChars,
    EspNativeGameplayHudStats* stats) {
    size_t length;
    size_t i;
    int x;

    if (font == NULL || text == NULL) return ESP_NATIVE_GAMEPLAY_HUD_INVALID;
    length = strlen(text);
    if (length > maxChars) length = maxChars;
    x = rightAligned ? anchorX - (int)length * HUD_FONT_ADVANCE : anchorX;

    for (i = 0U; i < length; ++i) {
        uint8_t c = (uint8_t)text[i];
        if (c == ' ') {
            x += HUD_FONT_ADVANCE;
            continue;
        }
        if (c < 33U || c > 127U) return ESP_NATIVE_GAMEPLAY_HUD_UNSUPPORTED_CONTEXT;
        {
            uint8_t glyph = (uint8_t)(c - 33U);
            uint16_t sourceX = (uint16_t)(HUD_FONT_WIDTH * (glyph & 0x0fU));
            uint16_t sourceY = (uint16_t)(HUD_FONT_HEIGHT * (glyph >> 4));
            EspNativeGameplayHudStatus status = drawBmp(
                font, framebuffer, sourceX, sourceY,
                HUD_FONT_WIDTH, HUD_FONT_HEIGHT,
                x, y, HUD_TRANSPARENT, stats);
            if (status != ESP_NATIVE_GAMEPLAY_HUD_OK) return status;
        }
        x += HUD_FONT_ADVANCE;
    }
    return ESP_NATIVE_GAMEPLAY_HUD_OK;
}

static EspNativeGameplayHudStatus paintPrepared(
    const EspNativeGameplayHudState* prepared,
    uint16_t* framebuffer,
    EspNativeGameplayHudStats* stats) {
    EspNativeIndexedBmp bmp;
    EspNativeIndexedBmpStats local;
    EspNativeGameplayHudStatus status;
    uint16_t iconWidth;
    uint16_t iconHeight;
    uint16_t faceWidth;
    uint16_t faceHeight;
    int cx = HUD_CENTER_BASE;
    int y = HUD_NUMBER_Y;
    int dy = HUD_MID_Y;
    int faceX = HUD_FACE_X + HUD_CENTER_BASE;
    char healthNum[4];
    char armorNum[4];
    char ammoNum[4];
    char dir[2];

    status = openAsset("k.bmp", &bmp, stats);
    if (status != ESP_NATIVE_GAMEPLAY_HUD_OK || !validateStatusBar(&bmp)) return ESP_NATIVE_GAMEPLAY_HUD_RESOURCE_FAILED;
    if (stats != NULL) {
        stats->statusBarWidth = bmp.width;
        stats->statusBarHeight = bmp.height;
    }
    memset(&local, 0, sizeof(local));
    if (EspNativeIndexedBmp_tile(&bmp, framebuffer,
                                 DOOMRPG_LOGICAL_WIDTH, DOOMRPG_LOGICAL_HEIGHT,
                                 0, 0, DOOMRPG_LOGICAL_WIDTH, HUD_TOP_HEIGHT,
                                 HUD_OPAQUE, &local) != ESP_NATIVE_INDEXED_BMP_OK) {
        mergeBmpStats(stats, &local);
        return ESP_NATIVE_GAMEPLAY_HUD_RESOURCE_FAILED;
    }
    mergeBmpStats(stats, &local);
    memset(&local, 0, sizeof(local));
    if (EspNativeIndexedBmp_tile(&bmp, framebuffer,
                                 DOOMRPG_LOGICAL_WIDTH, DOOMRPG_LOGICAL_HEIGHT,
                                 0, HUD_BOTTOM_Y,
                                 DOOMRPG_LOGICAL_WIDTH, HUD_BOTTOM_HEIGHT,
                                 HUD_OPAQUE, &local) != ESP_NATIVE_INDEXED_BMP_OK) {
        mergeBmpStats(stats, &local);
        return ESP_NATIVE_GAMEPLAY_HUD_RESOURCE_FAILED;
    }
    mergeBmpStats(stats, &local);

    drawVerticalLine(framebuffer, HUD_LINE1_X + cx, HUD_BOTTOM_Y,
                     DOOMRPG_LOGICAL_HEIGHT - 1, rgb565(0x313131U), stats);
    drawVerticalLine(framebuffer, HUD_LINE2_X + cx, HUD_BOTTOM_Y,
                     DOOMRPG_LOGICAL_HEIGHT - 1, rgb565(0x313131U), stats);
    drawVerticalLine(framebuffer, HUD_LINE1_X + cx + 1, HUD_BOTTOM_Y,
                     DOOMRPG_LOGICAL_HEIGHT - 1, rgb565(0x808591U), stats);
    drawVerticalLine(framebuffer, HUD_LINE2_X + cx + 1, HUD_BOTTOM_Y,
                     DOOMRPG_LOGICAL_HEIGHT - 1, rgb565(0x808591U), stats);

    status = openAsset("m.bmp", &bmp, stats);
    if (status != ESP_NATIVE_GAMEPLAY_HUD_OK || !validateIcons(&bmp)) return ESP_NATIVE_GAMEPLAY_HUD_RESOURCE_FAILED;
    iconWidth = bmp.width;
    iconHeight = (uint16_t)(bmp.height / 9U);
    if (stats != NULL) {
        stats->iconWidth = iconWidth;
        stats->iconHeight = iconHeight;
    }
    status = drawBmp(&bmp, framebuffer, 0U, 0U, iconWidth, iconHeight,
                     HUD_HEALTH_X + cx, dy - (int)(iconHeight >> 1),
                     HUD_TRANSPARENT, stats);
    if (status != ESP_NATIVE_GAMEPLAY_HUD_OK) return status;
    status = drawBmp(&bmp, framebuffer, 0U, iconHeight, iconWidth, iconHeight,
                     HUD_ARMOR_X + cx, dy - (int)(iconHeight >> 1),
                     HUD_TRANSPARENT, stats);
    if (status != ESP_NATIVE_GAMEPLAY_HUD_OK) return status;
    if (prepared->model.weaponsPresent != 0U) {
        uint16_t row = prepared->model.weapon == 0U
                           ? 2U
                           : (uint16_t)(prepared->model.ammoType + 3U);
        status = drawBmp(&bmp, framebuffer, 0U,
                         (uint16_t)(iconHeight * row), iconWidth, iconHeight,
                         HUD_AMMO_X + cx, dy - (int)(iconHeight >> 1),
                         HUD_TRANSPARENT, stats);
        if (status != ESP_NATIVE_GAMEPLAY_HUD_OK) return status;
    }

    status = openAsset("l.bmp", &bmp, stats);
    if (status != ESP_NATIVE_GAMEPLAY_HUD_OK || !validateFaces(&bmp)) return ESP_NATIVE_GAMEPLAY_HUD_RESOURCE_FAILED;
    faceWidth = bmp.width;
    faceHeight = (uint16_t)(bmp.height / 9U);
    if (stats != NULL) {
        stats->faceWidth = faceWidth;
        stats->faceHeight = faceHeight;
        stats->faceState = prepared->faceState;
    }
    drawVerticalLine(framebuffer, faceX - 1, HUD_BOTTOM_Y,
                     DOOMRPG_LOGICAL_HEIGHT - 1, rgb565(0x323232U), stats);
    status = drawBmp(&bmp, framebuffer, 0U,
                     (uint16_t)(prepared->faceState * faceHeight),
                     faceWidth, faceHeight,
                     faceX, dy - (int)(faceHeight >> 1), HUD_OPAQUE, stats);
    if (status != ESP_NATIVE_GAMEPLAY_HUD_OK) return status;
    drawVerticalLine(framebuffer, faceX + faceWidth, HUD_BOTTOM_Y,
                     DOOMRPG_LOGICAL_HEIGHT - 1, rgb565(0x828282U), stats);

    status = openAsset("o.bmp", &bmp, stats);
    if (status != ESP_NATIVE_GAMEPLAY_HUD_OK || !validateArrow(&bmp)) return ESP_NATIVE_GAMEPLAY_HUD_RESOURCE_FAILED;
    status = drawBmp(&bmp, framebuffer, 0U, 0U, bmp.width, bmp.height,
                     HUD_ORIENTATION_ARROW_X + cx - bmp.width,
                     y - 3, HUD_TRANSPARENT, stats);
    if (status != ESP_NATIVE_GAMEPLAY_HUD_OK) return status;

    status = openAsset("a.bmp", &bmp, stats);
    if (status != ESP_NATIVE_GAMEPLAY_HUD_OK || !validateFont(&bmp)) return ESP_NATIVE_GAMEPLAY_HUD_RESOURCE_FAILED;

    snprintf(healthNum, sizeof(healthNum), "%u", (unsigned int)prepared->model.health);
    snprintf(armorNum, sizeof(armorNum), "%u", (unsigned int)prepared->model.armor);
    if (prepared->model.weapon == 0U) {
        strncpy(ammoNum, "--", sizeof(ammoNum));
        ammoNum[sizeof(ammoNum) - 1U] = '\0';
    }
    else {
        snprintf(ammoNum, sizeof(ammoNum), "%u", (unsigned int)prepared->model.ammo);
    }
    dir[0] = directionChar(prepared->model.destAngle);
    dir[1] = '\0';

    status = drawSmallText(&bmp, framebuffer, healthNum,
                           HUD_HEALTH_X + cx + 14 + iconWidth + 1,
                           y, 1U, 3U, stats);
    if (status != ESP_NATIVE_GAMEPLAY_HUD_OK) return status;
    status = drawSmallText(&bmp, framebuffer, armorNum,
                           HUD_ARMOR_X + cx + 14 + iconWidth,
                           y, 1U, 3U, stats);
    if (status != ESP_NATIVE_GAMEPLAY_HUD_OK) return status;
    if (prepared->model.weaponsPresent != 0U) {
        status = drawSmallText(&bmp, framebuffer, ammoNum,
                               HUD_AMMO_X + cx + iconWidth + 14,
                               y, 1U, 2U, stats);
        if (status != ESP_NATIVE_GAMEPLAY_HUD_OK) return status;
    }
    status = drawSmallText(&bmp, framebuffer, dir,
                           HUD_ORIENTATION_X + cx,
                           y + 2, 1U, 1U, stats);
    return status;
}

void EspNativeGameplayHud_reset(void) {
    memset(&hudState, 0, sizeof(hudState));
}

int EspNativeGameplayHud_isReady(void) {
    return hudState.active == 1U && hudState.painted == 1U;
}

const EspNativeGameplayHudState* EspNativeGameplayHud_view(void) {
    return EspNativeGameplayHud_isReady() ? &hudState : NULL;
}

EspNativeGameplayHudStatus EspNativeGameplayHud_prepareInitial(
    const EspNativeGameplayHudModel* model,
    EspNativeGameplayHudState* outState) {
    EspNativeGameplayHudState next;

    if (outState != NULL) memset(outState, 0, sizeof(*outState));
    if (model == NULL || outState == NULL) return ESP_NATIVE_GAMEPLAY_HUD_INVALID;
    if (!EspMapCatalog_isValidId(model->targetMapId) ||
        model->gameplayLoadMapId == 0U || model->gameplayLoadMapId > 32U ||
        model->loadType != 0U || model->maxHealth == 0U ||
        model->health > model->maxHealth || model->armor > model->maxArmor ||
        model->ammo > 99U || model->weapon > 11U || model->ammoType > 5U ||
        model->weaponsPresent > 1U || model->damageActive > 1U ||
        model->gotFace > 1U || model->damageDir > 3U ||
        !directionSupported(model->destAngle)) {
        return ESP_NATIVE_GAMEPLAY_HUD_UNSUPPORTED_CONTEXT;
    }
    if (model->messageCount != 0U || model->statBarMessagePresent != 0U ||
        model->logMessageLength != 0U) {
        return ESP_NATIVE_GAMEPLAY_HUD_UNSUPPORTED_CONTEXT;
    }

    memset(&next, 0, sizeof(next));
    next.model = *model;
    next.faceState = chooseFace(model);
    next.painted = 1U;
    next.active = 1U;
    *outState = next;
    return ESP_NATIVE_GAMEPLAY_HUD_OK;
}

EspNativeGameplayHudStatus EspNativeGameplayHud_routeInitial(
    const EspNativeGameplayHudModel* model,
    EspNativeGameplayHudStats* outStats) {
    EspNativeGameplayHudState prepared;
    const EspHudRefreshState* dirty;
    const EspHudPostLoadClearState* clear;
    uint16_t* framebuffer;
    size_t framebufferBytes;
    EspNativeGameplayHudStatus status;

    if (outStats != NULL) memset(outStats, 0, sizeof(*outStats));
    if (EspNativeGameplayHud_isReady()) return ESP_NATIVE_GAMEPLAY_HUD_ALREADY_ACTIVE;

    status = EspNativeGameplayHud_prepareInitial(model, &prepared);
    if (status != ESP_NATIVE_GAMEPLAY_HUD_OK) return status;

    dirty = EspHudRefresh_view();
    if (dirty == NULL || dirty->targetMapId != model->targetMapId ||
        dirty->gameplayLoadMapId != model->gameplayLoadMapId ||
        dirty->loadType != model->loadType) {
        return ESP_NATIVE_GAMEPLAY_HUD_DIRTY_NOT_READY;
    }
    clear = EspHudPostLoadClear_view();
    if (clear == NULL || clear->targetMapId != model->targetMapId ||
        clear->gameplayLoadMapId != model->gameplayLoadMapId ||
        clear->loadType != model->loadType || clear->messageCount != 0U ||
        clear->statBarMessagePresent != 0U || clear->logMessageLength != 0U ||
        clear->cleared != 1U || clear->active != 1U) {
        return ESP_NATIVE_GAMEPLAY_HUD_CLEAR_NOT_READY;
    }

    framebuffer = (uint16_t*)Esp32PlatformVideo_framebuffer();
    framebufferBytes = Esp32PlatformVideo_framebufferSizeBytes();
    if (framebuffer == NULL ||
        framebufferBytes != (size_t)DOOMRPG_LOGICAL_WIDTH *
                                (size_t)DOOMRPG_LOGICAL_HEIGHT *
                                sizeof(uint16_t)) {
        return ESP_NATIVE_GAMEPLAY_HUD_FRAMEBUFFER_INVALID;
    }
    if (EspAssetPack_isOpen()) return ESP_NATIVE_GAMEPLAY_HUD_PACK_BUSY;
    if (!EspAssetPack_open(ESP_ASSET_PACK_DEFAULT_PATH)) {
        return ESP_NATIVE_GAMEPLAY_HUD_PACK_OPEN_FAILED;
    }

    status = preflightResources(outStats);
    if (status == ESP_NATIVE_GAMEPLAY_HUD_OK) {
        status = paintPrepared(&prepared, framebuffer, outStats);
    }
    EspAssetPack_close();
    if (status != ESP_NATIVE_GAMEPLAY_HUD_OK) return status;

    if (!EspHudRefresh_consumePaint(model->targetMapId,
                                    model->gameplayLoadMapId,
                                    model->loadType)) {
        return ESP_NATIVE_GAMEPLAY_HUD_PAINT_CONSUME_FAILED;
    }

    hudState = prepared;
    return ESP_NATIVE_GAMEPLAY_HUD_OK;
}
