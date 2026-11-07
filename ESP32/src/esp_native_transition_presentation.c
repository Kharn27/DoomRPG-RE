#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <esp_timer.h>

#include "esp_asset_pack.h"
#include "esp_map_catalog.h"
#include "esp_main_menu_skin.h"
#include "esp_native_gameplay_hub_theme.h"
#include "esp_native_gameplay_hub_touch_ui.h"
#include "esp_native_gameplay_player_state.h"
#include "esp_player_fresh_map_state.h"
#include "esp_native_gameplay_transition.h"
#include "esp_native_indexed_bmp.h"
#include "esp_native_transition_presentation.h"
#include "platform_video_c_bridge.h"
#include "platform_video_config.h"

#if DOOMRPG_LOGICAL_WIDTH != 160 || DOOMRPG_LOGICAL_HEIGHT != 120
#error "Transition presentation is defined for the 160x120 logical framebuffer"
#endif

#define TRANSITION_STAR_NAME "c.bmp"
/* Paired industrial plates, vertically balanced around the starfield center. */
#define TRANSITION_LOADING_CARD_LEFT 16
#define TRANSITION_LOADING_CARD_TOP 26
#define TRANSITION_LOADING_CARD_RIGHT 143
#define TRANSITION_LOADING_CARD_BOTTOM 69
#define TRANSITION_PROGRESS_LEFT 16
#define TRANSITION_PROGRESS_TOP 82
#define TRANSITION_PROGRESS_WIDTH 128
#define TRANSITION_PROGRESS_HEIGHT 18
#define TRANSITION_PROGRESS_STEP 5U

#define COLOR_BLACK      ESP_HUB_COLOR_BLACK
#define COLOR_BG         ESP_HUB_COLOR_BG
#define COLOR_PANEL      ESP_HUB_COLOR_PANEL
#define COLOR_PANEL_ALT  ESP_HUB_COLOR_PANEL_ALT
#define COLOR_STEEL_DARK ESP_HUB_COLOR_STEEL_DARK
#define COLOR_STEEL      ESP_HUB_COLOR_STEEL
#define COLOR_IVORY      ESP_HUB_COLOR_IVORY
#define COLOR_AMBER_DIM  ESP_HUB_COLOR_AMBER_DIM
#define COLOR_AMBER      ESP_HUB_COLOR_AMBER
#define COLOR_GREEN      ESP_HUB_COLOR_GREEN

typedef struct EspNativeTransitionPresentationState_s {
    uint32_t loadingStartMs;
    uint32_t frames;
    uint8_t targetMapId;
    uint8_t lastPercent;
    uint8_t lastPhase;
    uint8_t loadingActive;
} EspNativeTransitionPresentationState;

typedef struct EspNativeTransitionPaintScratch_s {
    EspNativeIndexedBmp star;
    EspNativeIndexedBmpStats stats;
} EspNativeTransitionPaintScratch;

static EspNativeTransitionPresentationState presentation;

/* This module is itself the full-frame presentation owner. Bypass the global
 * gameplay compositor wrapper so loading/stats frames cannot acquire a stale
 * top-bar feedback or viewport flash while they are on screen. */
int __real_Esp32PlatformVideo_present(void);

static uint32_t nowMs(void) {
    return (uint32_t)(esp_timer_get_time() / 1000LL);
}

static int framebufferReady(void) {
    return Esp32PlatformVideo_framebuffer() != NULL &&
           Esp32PlatformVideo_framebufferSizeBytes() ==
               (size_t)DOOMRPG_LOGICAL_WIDTH * DOOMRPG_LOGICAL_HEIGHT *
                   sizeof(uint16_t);
}

static uint16_t* framebuffer(void) {
    return (uint16_t*)Esp32PlatformVideo_framebuffer();
}

static void pixel(int x, int y, uint16_t color) {
    uint16_t* fb = framebuffer();
    if (fb == NULL || x < 0 || x >= DOOMRPG_LOGICAL_WIDTH ||
        y < 0 || y >= DOOMRPG_LOGICAL_HEIGHT) {
        return;
    }
    fb[(uint32_t)y * DOOMRPG_LOGICAL_WIDTH + (uint32_t)x] = color;
}

static void fillRect(int left, int top, int right, int bottom, uint16_t color) {
    int x;
    int y;
    if (left < 0) left = 0;
    if (top < 0) top = 0;
    if (right >= DOOMRPG_LOGICAL_WIDTH) right = DOOMRPG_LOGICAL_WIDTH - 1;
    if (bottom >= DOOMRPG_LOGICAL_HEIGHT) bottom = DOOMRPG_LOGICAL_HEIGHT - 1;
    if (right < left || bottom < top) return;
    for (y = top; y <= bottom; ++y) {
        for (x = left; x <= right; ++x) {
            pixel(x, y, color);
        }
    }
}

static void rect(int left, int top, int right, int bottom, uint16_t color) {
    int x;
    int y;
    for (x = left; x <= right; ++x) {
        pixel(x, top, color);
        pixel(x, bottom, color);
    }
    for (y = top + 1; y < bottom; ++y) {
        pixel(left, y, color);
        pixel(right, y, color);
    }
}

static uint32_t frameFNV(void) {
    const uint8_t* data = (const uint8_t*)Esp32PlatformVideo_framebuffer();
    size_t bytes = Esp32PlatformVideo_framebufferSizeBytes();
    uint32_t hash = 2166136261U;
    size_t i;
    if (data == NULL) return 0U;
    for (i = 0U; i < bytes; ++i) {
        hash ^= data[i];
        hash *= 16777619U;
    }
    return hash;
}

static void formatMapLabel(uint8_t mapId, char* out, size_t capacity) {
    const char* resource;
    size_t i = 0U;
    if (out == NULL || capacity == 0U) return;
    out[0] = '\0';
    if (mapId == ESP_MAP_ID_INTRO) {
        snprintf(out, capacity, "ENTRANCE");
        return;
    }
    if (mapId == ESP_MAP_ID_JUNCTION) {
        snprintf(out, capacity, "JUNCTION");
        return;
    }
    if (mapId >= 2U && mapId <= 8U) {
        snprintf(out, capacity, "SECTOR %u", (unsigned int)(mapId - 1U));
        return;
    }
    if (mapId == 10U) {
        snprintf(out, capacity, "JUNCTION RUINS");
        return;
    }
    resource = EspMapCatalog_nameForId(mapId);
    if (resource == NULL) {
        snprintf(out, capacity, "MAP %u", (unsigned int)mapId);
        return;
    }
    while (*resource == '/' || *resource == '\\') ++resource;
    while (*resource != '\0' && *resource != '.' && i + 1U < capacity) {
        char c = *resource++;
        if (c >= 'a' && c <= 'z') c = (char)(c - ('a' - 'A'));
        out[i++] = c;
    }
    out[i] = '\0';
}

static int miniRows(char c, uint8_t rows[5]) {
    static const uint8_t digits[10][5] = {
        {7U,5U,5U,5U,7U}, {2U,6U,2U,2U,7U},
        {7U,1U,7U,4U,7U}, {7U,1U,7U,1U,7U},
        {5U,5U,7U,1U,1U}, {7U,4U,7U,1U,7U},
        {7U,4U,7U,5U,7U}, {7U,1U,2U,2U,2U},
        {7U,5U,7U,5U,7U}, {7U,5U,7U,1U,7U}
    };
    static const uint8_t letters[26][5] = {
        {2U,5U,7U,5U,5U}, {6U,5U,6U,5U,6U},
        {7U,4U,4U,4U,7U}, {6U,5U,5U,5U,6U},
        {7U,4U,6U,4U,7U}, {7U,4U,6U,4U,4U},
        {7U,4U,5U,5U,7U}, {5U,5U,7U,5U,5U},
        {7U,2U,2U,2U,7U}, {1U,1U,1U,5U,7U},
        {5U,5U,6U,5U,5U}, {4U,4U,4U,4U,7U},
        {5U,7U,7U,5U,5U}, {5U,7U,7U,7U,5U},
        {7U,5U,5U,5U,7U}, {6U,5U,6U,4U,4U},
        {7U,5U,5U,7U,1U}, {6U,5U,6U,5U,5U},
        {7U,4U,7U,1U,7U}, {7U,2U,2U,2U,2U},
        {5U,5U,5U,5U,7U}, {5U,5U,5U,5U,2U},
        {5U,5U,7U,7U,5U}, {5U,5U,2U,5U,5U},
        {5U,5U,2U,2U,2U}, {7U,1U,2U,4U,7U}
    };
    static const uint8_t slash[5] = {1U,1U,2U,4U,4U};
    static const uint8_t colon[5] = {0U,2U,0U,2U,0U};
    static const uint8_t plus[5] = {0U,2U,7U,2U,0U};
    static const uint8_t dash[5] = {0U,0U,7U,0U,0U};
    const uint8_t* source;
    if (c >= '0' && c <= '9') source = digits[c - '0'];
    else if (c >= 'A' && c <= 'Z') source = letters[c - 'A'];
    else if (c == '/') source = slash;
    else if (c == ':') source = colon;
    else if (c == '+') source = plus;
    else if (c == '-') source = dash;
    else return 0;
    memcpy(rows, source, 5U);
    return 1;
}

static int miniTextWidth(const char* text, int scale) {
    int count = 0;
    if (text == NULL || scale <= 0) return 0;
    while (*text++ != '\0') ++count;
    return count == 0 ? 0 : count * (3 * scale) + (count - 1) * scale;
}

static void drawMiniTextAt(const char* text,
                           int x,
                           int top,
                           int scale,
                           uint16_t color,
                           int idleGradient) {
    if (text == NULL || scale <= 0) return;
    while (*text != '\0') {
        uint8_t rows[5];
        int row;
        if (miniRows(*text, rows)) {
            for (row = 0; row < 5; ++row) {
                int column;
                for (column = 0; column < 3; ++column) {
                    if ((rows[row] & (uint8_t)(1U << (2 - column))) != 0U) {
                        /* Only the original glyph mask is recolored.
                         * One tone per source row; the 2x blocks never shift. */
                        const uint16_t tone = idleGradient
                            ? EspMainMenuSkin_letterTone(row, 5) : color;
                        fillRect(x + column * scale,
                                 top + row * scale,
                                 x + column * scale + scale - 1,
                                 top + row * scale + scale - 1,
                                 tone);
                    }
                }
            }
        }
        x += 4 * scale;
        ++text;
    }
}

static void drawMiniTextCentered(const char* text,
                                 int centerX,
                                 int top,
                                 int scale,
                                 uint16_t color) {
    drawMiniTextAt(text, centerX - miniTextWidth(text, scale) / 2,
                   top, scale, color, 0);
}

/* Same LOADING glyphs, horizontal center=80, top=34 and scale=2. */
static void drawLoadingTitle(void) {
    const char* title = "LOADING";
    drawMiniTextAt(title, 80 - miniTextWidth(title, 2) / 2,
                   34, 2, 0U, 1);
}

static void drawMetricCard(int left,
                           int right,
                           const char* label,
                           uint16_t value,
                           uint16_t maximum,
                           uint16_t accent) {
    char number[16];
    uint32_t fill = 0U;
    const int innerLeft = left + 4;
    const int innerRight = right - 4;
    const int width = innerRight - innerLeft + 1;

    fillRect(left, 33, right, 67, COLOR_PANEL);
    rect(left, 33, right, 67, COLOR_STEEL_DARK);
    fillRect(left + 1, 34, left + 2, 66, accent);
    EspNativeGameplayHubTouchUi_drawCrispText(framebuffer(), label,
        (left + right) / 2 + 1, 38, COLOR_STEEL);

    snprintf(number, sizeof(number), "%u/%u",
             (unsigned int)value, (unsigned int)maximum);
    if (strlen(number) * 6U - 1U <= (size_t)width) {
        EspNativeGameplayHubTouchUi_drawCrispText(framebuffer(), number,
            (left + right) / 2 + 1, 50, COLOR_IVORY);
    } else {
        drawMiniTextCentered(number, (left + right) / 2 + 1, 51, 1, COLOR_IVORY);
    }

    fillRect(innerLeft, 61, innerRight, 63, COLOR_BLACK);
    if (maximum != 0U) {
        uint32_t bounded = value > maximum ? maximum : value;
        fill = (uint32_t)(((uint64_t)bounded * (uint64_t)width) / maximum);
    }
    if (fill != 0U) {
        fillRect(innerLeft, 61, innerLeft + (int)fill - 1, 63, accent);
    }
}

static void drawReportValue(int left, const char* label, const char* value,
                            uint16_t accent) {
    const int right = left + 43;
    fillRect(left, 72, right, 97, COLOR_PANEL);
    drawMiniTextCentered(label, left + 21, 76, 1, COLOR_STEEL);
    if (strlen(value) * 6U - 1U <= 39U) {
        EspNativeGameplayHubTouchUi_drawCrispText(framebuffer(), value,
            left + 21, 86, accent);
    } else {
        drawMiniTextCentered(value, left + 21, 87, 1, accent);
    }
}

static int drawFixedStarfield(EspNativeIndexedBmp* star,
                              EspNativeIndexedBmpStats* stats) {
    if (star == NULL || stats == NULL ||
        EspNativeIndexedBmp_open(TRANSITION_STAR_NAME, star, stats) !=
            ESP_NATIVE_INDEXED_BMP_OK ||
        star->width < DOOMRPG_LOGICAL_WIDTH ||
        star->height < DOOMRPG_LOGICAL_HEIGHT) {
        return 0;
    }

    return EspNativeIndexedBmp_blit(
               star, framebuffer(),
               DOOMRPG_LOGICAL_WIDTH, DOOMRPG_LOGICAL_HEIGHT,
               (uint16_t)((star->width - DOOMRPG_LOGICAL_WIDTH) / 2U),
               (uint16_t)((star->height - DOOMRPG_LOGICAL_HEIGHT) / 2U),
               DOOMRPG_LOGICAL_WIDTH, DOOMRPG_LOGICAL_HEIGHT,
               0, 0, 0U, stats) == ESP_NATIVE_INDEXED_BMP_OK;
}

static const char* phaseName(uint8_t phase) {
    switch (phase) {
    case ESP_ASSET_PACK_MAP_FLASH_PROGRESS_ERASE: return "ERASE";
    case ESP_ASSET_PACK_MAP_FLASH_PROGRESS_COPY: return "COPY";
    case ESP_ASSET_PACK_MAP_FLASH_PROGRESS_VERIFY: return "VERIFY";
    default: return "PREP";
    }
}

static uint8_t overallPercent(uint8_t phase,
                              uint32_t completed,
                              uint32_t total) {
    uint32_t local;
    if (total == 0U) return 0U;
    if (completed > total) completed = total;
    local = (completed * 100U) / total;
    switch (phase) {
    case ESP_ASSET_PACK_MAP_FLASH_PROGRESS_ERASE:
        return (uint8_t)((local * 20U) / 100U);
    case ESP_ASSET_PACK_MAP_FLASH_PROGRESS_COPY:
        return (uint8_t)(20U + (local * 60U) / 100U);
    case ESP_ASSET_PACK_MAP_FLASH_PROGRESS_VERIFY:
        return (uint8_t)(80U + (local * 20U) / 100U);
    default:
        return 0U;
    }
}

/* MENU_MAIN's exact procedural industrial chassis, enlarged to 128x44.
 * The compact loading text stays centered and needs no font asset.
 * Redrawing the whole plate prevents stale gameplay pixels during progress. */
static int paintLoadingCard(const char* target) {
    if (target == NULL ||
        !EspMainMenuSkin_paintPlate(framebuffer(),
                                   DOOMRPG_LOGICAL_WIDTH,
                                   DOOMRPG_LOGICAL_WIDTH,
                                   DOOMRPG_LOGICAL_HEIGHT,
                                   TRANSITION_LOADING_CARD_LEFT,
                                   TRANSITION_LOADING_CARD_TOP,
                                   TRANSITION_LOADING_CARD_RIGHT,
                                   TRANSITION_LOADING_CARD_BOTTOM,
                                   ESP_MAIN_MENU_SKIN_IDLE)) return 0;
    drawLoadingTitle();
    drawMiniTextCentered("ENTERING", 80, 49, 1, COLOR_STEEL);
    drawMiniTextCentered(target, 80, 57, 1, COLOR_IVORY);
    return 1;
}

/* Same MENU_MAIN chassis, now a thin progress module: engraved black
 * track and warm metal fill. The entire bounded plate is repainted for each
 * progress step; no accumulated pixels and no extra buffering. */
static int paintProgressBar(uint8_t percent) {
    /* Inner track begins right after the 3px steel rim/1px groove. A 10px
     * inset left two dark rectangular end caps visible beside a full gauge. */
    const int trackLeft = TRANSITION_PROGRESS_LEFT + 4;
    const int trackTop = TRANSITION_PROGRESS_TOP + 6;
    const int trackWidth = TRANSITION_PROGRESS_WIDTH - 8;
    const int trackHeight = TRANSITION_PROGRESS_HEIGHT - 12;
    int fillWidth;
    int row;

    if (!EspMainMenuSkin_paintPlate(framebuffer(),
                                   DOOMRPG_LOGICAL_WIDTH,
                                   DOOMRPG_LOGICAL_WIDTH,
                                   DOOMRPG_LOGICAL_HEIGHT,
                                   TRANSITION_PROGRESS_LEFT,
                                   TRANSITION_PROGRESS_TOP,
                                   TRANSITION_PROGRESS_LEFT +
                                       TRANSITION_PROGRESS_WIDTH - 1,
                                   TRANSITION_PROGRESS_TOP +
                                       TRANSITION_PROGRESS_HEIGHT - 1,
                                   ESP_MAIN_MENU_SKIN_IDLE)) return 0;

    if (percent > 100U) percent = 100U;
    fillWidth = (trackWidth * (int)percent) / 100;

    /* One-pixel engraved border; at 100% the 120px fill reaches both ends
     * of the track. No black side pockets survive outside the track. */
    rect(trackLeft - 1, trackTop - 1,
         trackLeft + trackWidth, trackTop + trackHeight,
         0x3123U);
    fillRect(trackLeft, trackTop,
             trackLeft + trackWidth - 1,
             trackTop + trackHeight - 1, COLOR_BLACK);

    /* Color only the six real track rows, matching the saved menu theme.
     * CLASSIC preserves every existing gauge pixel; no offset fill or extra
     * side-cap cavity even at 100 percent. */
    for (row = 0; row < trackHeight && fillWidth > 0; ++row) {
        const uint16_t tone = EspMainMenuSkin_progressTone(row, trackHeight);
        fillRect(trackLeft, trackTop + row,
                 trackLeft + fillWidth - 1, trackTop + row, tone);
    }
    return 1;
}

static void presentOverall(uint8_t percent,
                           const char* stage,
                           const char* source) {
    uint32_t fnv;

    if (!presentation.loadingActive) return;
    if (percent > 100U) percent = 100U;
    if (percent < presentation.lastPercent) percent = presentation.lastPercent;

    /*
     * The logical framebuffer is shared with gameplay.  A suppressed gameplay
     * present can still have writers that touched that buffer before reaching
     * the present gate.  Never rely on the previous loading frame remaining
     * byte-identical: reconstruct the complete loading frame immediately
     * before every physical progress present.
     */
    {
        EspNativeTransitionPaintScratch scratch;
        char target[24];
        int openedHere = 0;

        memset(&scratch, 0, sizeof(scratch));
        if (!EspAssetPack_isOpen()) {
            if (!EspAssetPack_open(ESP_ASSET_PACK_DEFAULT_PATH)) {
                printf("[TRANSITIONLOAD] FRAME-DEFER stage=%s overall=%u reason=pack-open\n",
                       stage != NULL ? stage : "LOAD",
                       (unsigned int)percent);
                return;
            }
            openedHere = 1;
        }

        if (!drawFixedStarfield(&scratch.star, &scratch.stats)) {
            if (openedHere && EspAssetPack_isOpen()) EspAssetPack_close();
            printf("[TRANSITIONLOAD] FRAME-DEFER stage=%s overall=%u reason=background-redraw\n",
                   stage != NULL ? stage : "LOAD",
                   (unsigned int)percent);
            return;
        }

        formatMapLabel(presentation.targetMapId, target, sizeof(target));
        if (!paintLoadingCard(target)) {
            if (openedHere && EspAssetPack_isOpen()) EspAssetPack_close();
            return;
        }
        if (!paintProgressBar(percent)) {
            if (openedHere && EspAssetPack_isOpen()) EspAssetPack_close();
            return;
        }

        if (!__real_Esp32PlatformVideo_present()) {
            if (openedHere && EspAssetPack_isOpen()) EspAssetPack_close();
            printf("[TRANSITIONLOAD] FRAME-DEFER stage=%s overall=%u reason=present-failed\n",
                   stage != NULL ? stage : "LOAD",
                   (unsigned int)percent);
            return;
        }

        if (openedHere && EspAssetPack_isOpen()) EspAssetPack_close();

        presentation.lastPercent = percent;
        ++presentation.frames;
        fnv = frameFNV();

        printf("[TRANSITIONLOAD] FRAME n=%u stage=%s overall=%u background=repainted assetReads=%u source=%s frame=%08x\n",
               (unsigned int)presentation.frames,
               stage != NULL ? stage : "LOAD",
               (unsigned int)percent,
               (unsigned int)scratch.stats.packReads,
               source != NULL ? source : "generic",
               (unsigned int)fnv);
    }
}

int EspNativeTransitionPresentation_showStats(
    const struct EspNativeGameplayTransitionState_s* transitionBase) {
    const EspNativeGameplayTransitionState* transition =
        (const EspNativeGameplayTransitionState*)transitionBase;
    char source[24];
    char duration[16];
    char moves[16];
    char experience[16];
    EspPlayerLevelProgress progress;
    const EspNativeGameplayPlayerState* player;
    uint32_t gained = 0U;
    uint32_t seconds;
    int hasProgress;
    uint32_t fnv;
    uint16_t secretAccent;
    uint16_t monsterAccent;

    if (transition == NULL || transition->active != 1U ||
        transition->waitingStats != 1U ||
        transition->levelStats.showStats != 1U || !framebufferReady()) {
        return 0;
    }

    player = EspNativeGameplayPlayerState_view();
    hasProgress = EspPlayerFreshMap_snapshotProgress(nowMs(), &progress) &&
        progress.targetMapId == transition->committed.sourceMapId &&
        player != NULL && progress.xpBaseline <= player->xpGained;
    if (hasProgress) {
        gained = player->xpGained - progress.xpBaseline;
        seconds = progress.elapsedMs / 1000U;
        if (seconds < 3600U) {
            snprintf(duration, sizeof(duration), "%02lu:%02lu",
                (unsigned long)(seconds / 60U), (unsigned long)(seconds % 60U));
        } else {
            snprintf(duration, sizeof(duration), "%lu:%02lu:%02lu",
                (unsigned long)(seconds / 3600U),
                (unsigned long)((seconds / 60U) % 60U),
                (unsigned long)(seconds % 60U));
        }
        snprintf(moves, sizeof(moves), "%lu", (unsigned long)progress.moves);
        snprintf(experience, sizeof(experience), "+%lu", (unsigned long)gained);
    } else {
        snprintf(duration, sizeof(duration), "--");
        snprintf(moves, sizeof(moves), "--");
        snprintf(experience, sizeof(experience), "--");
    }

    fillRect(0, 0, DOOMRPG_LOGICAL_WIDTH - 1,
             DOOMRPG_LOGICAL_HEIGHT - 1, COLOR_BLACK);
    fillRect(3, 3, 156, 116, COLOR_BG);
    rect(3, 3, 156, 116, COLOR_STEEL_DARK);

    /* Same death-menu-inspired unframed stencil as LEVEL UP. The long
     * MISSION COMPLETE glyphs have reserved space between the side hatches.
     * WAIT_STATS remains the exclusive full-screen owner until ACK. */
    if (!EspMainMenuSkin_paintModalTitleStencil(
            framebuffer(), DOOMRPG_LOGICAL_WIDTH,
            DOOMRPG_LOGICAL_WIDTH, DOOMRPG_LOGICAL_HEIGHT)) return 0;
    formatMapLabel(transition->committed.sourceMapId, source, sizeof(source));

    EspNativeGameplayHubTouchUi_drawCrispText(framebuffer(), "MISSION COMPLETE",
        80, 11, EspMainMenuSkin_uiAccent());
    drawMiniTextCentered(source, 80, 20, 1, COLOR_IVORY);

    secretAccent =
        transition->levelStats.secretsTotal != 0U &&
        transition->levelStats.secretsFound >= transition->levelStats.secretsTotal
            ? COLOR_GREEN : EspMainMenuSkin_uiAccent();
    monsterAccent =
        transition->levelStats.monstersTotal != 0U &&
        transition->levelStats.monstersDead >= transition->levelStats.monstersTotal
            ? COLOR_GREEN : EspMainMenuSkin_uiAccent();

    drawMetricCard(10, 76, "SECRETS",
                   transition->levelStats.secretsFound,
                   transition->levelStats.secretsTotal,
                   secretAccent);
    drawMetricCard(83, 149, "MONSTERS",
                   transition->levelStats.monstersDead,
                   transition->levelStats.monstersTotal,
                   monsterAccent);

    drawReportValue(10, "TIME", duration, ESP_HUB_COLOR_BLUE);
    drawReportValue(58, "MOVES", moves, COLOR_IVORY);
    drawReportValue(106, "XP GAINED", experience, COLOR_GREEN);
    drawMiniTextCentered(hasProgress && !progress.complete ? "SINCE LOAD" :
        "SECTOR REPORT", 80, 100, 1, COLOR_STEEL);
    EspNativeGameplayHubTouchUi_drawCrispText(framebuffer(), "TAP TO CONTINUE",
        80, 108, COLOR_IVORY);

    if (!__real_Esp32PlatformVideo_present()) return 0;
    /* The stats screen is presentation-only. PR #249 commits the SOURCE
     * completion bit exactly once, after the destination spawn/session are
     * configured successfully. Painting this modal before acknowledgement
     * must not persist gameplay progress if a later handoff fails closed. */
    fnv = frameFNV();
    printf("[LEVELSTATS] PRESENT sourceMap=%u targetMap=%u source=%s secrets=%u/%u monsters=%u/%u elapsedMs=%lu moves=%lu xp=%lu counters=%s style=hub-sector-report+industrial-theme font=crisp5x7+mini3x5 reads=0 fullScreen=yes frame=%08x input=one-tap theme=%u\n",
           (unsigned int)transition->committed.sourceMapId,
           (unsigned int)transition->committed.targetMapId,
           source,
           (unsigned int)transition->levelStats.secretsFound,
           (unsigned int)transition->levelStats.secretsTotal,
           (unsigned int)transition->levelStats.monstersDead,
           (unsigned int)transition->levelStats.monstersTotal,
           (unsigned long)(hasProgress ? progress.elapsedMs : 0U),
           (unsigned long)(hasProgress ? progress.moves : 0U),
           (unsigned long)gained,
           !hasProgress ? "unavailable" : (progress.complete ? "full-level" : "since-load"),
           (unsigned int)fnv,
           (unsigned int)EspMainMenuSkin_getTheme());
    return 1;
}

int EspNativeTransitionPresentation_beginLoading(uint8_t targetMapId) {
    EspNativeTransitionPaintScratch scratch;
    char target[24];
    int openedHere = 0;
    int ok = 0;

    if (!EspMapCatalog_isValidId(targetMapId) || !framebufferReady() ||
        presentation.loadingActive) {
        return 0;
    }

    memset(&presentation, 0, sizeof(presentation));
    memset(&scratch, 0, sizeof(scratch));

    if (!EspAssetPack_isOpen()) {
        if (!EspAssetPack_open(ESP_ASSET_PACK_DEFAULT_PATH)) return 0;
        openedHere = 1;
    }

    if (!drawFixedStarfield(&scratch.star, &scratch.stats)) goto done;

    formatMapLabel(targetMapId, target, sizeof(target));

    /* First frame and all progress redraws share the same bounded panel. */
    if (!paintLoadingCard(target)) goto done;

    if (!paintProgressBar(0U)) goto done;

    presentation.targetMapId = targetMapId;
    presentation.lastPercent = 0U;
    presentation.lastPhase = 0U;
    presentation.loadingActive = 1U;
    presentation.loadingStartMs = nowMs();
    presentation.frames = 1U;

    if (!__real_Esp32PlatformVideo_present()) goto done;

    printf("[TRANSITIONLOAD] BEGIN targetMap=%u background=%s mode=fixed font=mini-hub entering=\"ENTERING %s\" progress=0%% reads=%u bytes=%u frame=%08x\n",
           (unsigned int)targetMapId,
           TRANSITION_STAR_NAME,
           target,
           (unsigned int)scratch.stats.packReads,
           (unsigned int)scratch.stats.bytesRead,
           (unsigned int)frameFNV());
    ok = 1;

done:
    if (openedHere && EspAssetPack_isOpen()) EspAssetPack_close();
    if (!ok) memset(&presentation, 0, sizeof(presentation));
    return ok;
}

void EspNativeTransitionPresentation_progress(uint8_t phase,
                                              uint32_t completed,
                                              uint32_t total) {
    uint8_t percent;
    uint8_t phaseChanged;

    if (!presentation.loadingActive) return;
    percent = overallPercent(phase, completed, total);
    phaseChanged = phase != presentation.lastPhase ? 1U : 0U;

    if (!phaseChanged && percent < 100U &&
        percent < (uint8_t)(presentation.lastPercent +
                            TRANSITION_PROGRESS_STEP)) {
        return;
    }
    presentation.lastPhase = phase;
    presentOverall(percent, phaseName(phase), "map-flash");
}

void EspNativeTransitionPresentation_checkpointProgress(uint8_t percent,
                                                        const char* stage) {
    if (!presentation.loadingActive) return;
    if (percent < 100U &&
        percent < (uint8_t)(presentation.lastPercent +
                            TRANSITION_PROGRESS_STEP)) {
        return;
    }
    presentOverall(percent, stage != NULL ? stage : "CHECKPOINT", "checkpoint");
}

int EspNativeTransitionPresentation_isLoadingActive(void) {
    return presentation.loadingActive != 0U ? 1 : 0;
}

void EspNativeTransitionPresentation_abortLoading(const char* reason) {
    if (presentation.loadingActive) {
        printf("[TRANSITIONLOAD] ABORT targetMap=%u frames=%u progress=%u reason=%s framebuffer=caller-owned\n",
               (unsigned int)presentation.targetMapId,
               (unsigned int)presentation.frames,
               (unsigned int)presentation.lastPercent,
               reason != NULL ? reason : "load-failed");
    }
    memset(&presentation, 0, sizeof(presentation));
}

void EspNativeTransitionPresentation_releaseLoading(const char* reason) {
    uint32_t elapsed;
    if (!presentation.loadingActive) return;
    elapsed = (uint32_t)(nowMs() - presentation.loadingStartMs);
    printf("[TRANSITIONLOAD] RELEASE targetMap=%u frames=%u elapsedMs=%u progress=%u background=fixed owner=gameplay-next-present reason=%s\n",
           (unsigned int)presentation.targetMapId,
           (unsigned int)presentation.frames,
           (unsigned int)elapsed,
           (unsigned int)presentation.lastPercent,
           reason != NULL ? reason : "session-ready");
    memset(&presentation, 0, sizeof(presentation));
}

void EspNativeTransitionPresentation_endLoading(void) {
    uint32_t elapsed;
    if (!presentation.loadingActive) {
        memset(&presentation, 0, sizeof(presentation));
        return;
    }
    if (presentation.lastPercent < 100U) {
        presentOverall(100U, "READY", "completion");
    }
    elapsed = (uint32_t)(nowMs() - presentation.loadingStartMs);
    printf("[TRANSITIONLOAD] END targetMap=%u frames=%u elapsedMs=%u progress=%u background=fixed assetReadsDuringProgress=0 framebuffer=retained-until-target-frame\n",
           (unsigned int)presentation.targetMapId,
           (unsigned int)presentation.frames,
           (unsigned int)elapsed,
           (unsigned int)presentation.lastPercent);
    memset(&presentation, 0, sizeof(presentation));
}

void EspNativeTransitionPresentation_reset(void) {
    memset(&presentation, 0, sizeof(presentation));
}
