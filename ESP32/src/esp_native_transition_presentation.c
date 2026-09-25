#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <esp_timer.h>

#include "esp_asset_pack.h"
#include "esp_map_catalog.h"
#include "esp_native_gameplay_transition.h"
#include "esp_native_indexed_bmp.h"
#include "esp_native_transition_presentation.h"
#include "platform_video_c_bridge.h"
#include "platform_video_config.h"

#if DOOMRPG_LOGICAL_WIDTH != 160 || DOOMRPG_LOGICAL_HEIGHT != 120
#error "Transition presentation is defined for the 160x120 logical framebuffer"
#endif

#define TRANSITION_STAR_NAME "c.bmp"
#define TRANSITION_FONT_NAME "a.bmp"

#define TRANSITION_FONT_WIDTH 9U
#define TRANSITION_FONT_HEIGHT 12U
#define TRANSITION_FONT_ADVANCE 7
#define TRANSITION_FONT_SOURCE_WIDTH 144U
#define TRANSITION_FONT_SOURCE_HEIGHT 72U
#define TRANSITION_TRANSPARENT 1U

#define TRANSITION_PROGRESS_LEFT 19
#define TRANSITION_PROGRESS_TOP 84
#define TRANSITION_PROGRESS_WIDTH 122
#define TRANSITION_PROGRESS_HEIGHT 8
#define TRANSITION_PROGRESS_STEP 5U

#define COLOR_BLACK      0x0000U
#define COLOR_BG         0x0841U
#define COLOR_PANEL      0x10a2U
#define COLOR_STEEL_DARK 0x3186U
#define COLOR_STEEL      0x6b4dU
#define COLOR_AMBER      0xfd20U

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
    EspNativeIndexedBmp font;
    EspNativeIndexedBmpStats stats;
} EspNativeTransitionPaintScratch;

static EspNativeTransitionPresentationState presentation;

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

static int gameTextWidth(const char* text) {
    const size_t length = text != NULL ? strlen(text) : 0U;
    return length == 0U
               ? 0
               : (int)((length - 1U) * TRANSITION_FONT_ADVANCE +
                       TRANSITION_FONT_WIDTH);
}

static int drawGlyph(const EspNativeIndexedBmp* font,
                     uint8_t c,
                     int x,
                     int y,
                     EspNativeIndexedBmpStats* stats) {
    uint8_t glyph;
    if (font == NULL || c < 33U || c > 127U) return 0;
    glyph = (uint8_t)(c - 33U);
    return EspNativeIndexedBmp_blit(
               font, framebuffer(),
               DOOMRPG_LOGICAL_WIDTH, DOOMRPG_LOGICAL_HEIGHT,
               (uint16_t)(TRANSITION_FONT_WIDTH * (glyph & 0x0fU)),
               (uint16_t)(TRANSITION_FONT_HEIGHT * (glyph >> 4)),
               TRANSITION_FONT_WIDTH, TRANSITION_FONT_HEIGHT,
               (int16_t)x, (int16_t)y,
               TRANSITION_TRANSPARENT, stats) == ESP_NATIVE_INDEXED_BMP_OK;
}

static int drawGameText(const EspNativeIndexedBmp* font,
                        const char* text,
                        int x,
                        int y,
                        EspNativeIndexedBmpStats* stats) {
    const unsigned char* p = (const unsigned char*)text;
    if (font == NULL || text == NULL || stats == NULL) return 0;
    while (*p != '\0') {
        const uint8_t c = *p++;
        if (c != ' ' && !drawGlyph(font, c, x, y, stats)) return 0;
        x += TRANSITION_FONT_ADVANCE;
    }
    return 1;
}

static int drawGameTextCentered(const EspNativeIndexedBmp* font,
                                const char* text,
                                int y,
                                EspNativeIndexedBmpStats* stats) {
    return drawGameText(font, text,
                        (DOOMRPG_LOGICAL_WIDTH - gameTextWidth(text)) / 2,
                        y, stats);
}

static int openFont(EspNativeIndexedBmp* font,
                    EspNativeIndexedBmpStats* stats) {
    return font != NULL && stats != NULL &&
           EspNativeIndexedBmp_open(TRANSITION_FONT_NAME, font, stats) ==
               ESP_NATIVE_INDEXED_BMP_OK &&
           font->width == TRANSITION_FONT_SOURCE_WIDTH &&
           font->height == TRANSITION_FONT_SOURCE_HEIGHT;
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

    /* One centered crop of the exact intro starfield. It is intentionally
     * frozen after this call so map-flash staging never competes with SD reads
     * for presentation bandwidth. */
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

    /* Approximate the historical cost distribution rather than pretending
     * byte counts map linearly to wall-clock time. */
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

static void paintProgressBar(uint8_t percent) {
    const int innerLeft = TRANSITION_PROGRESS_LEFT + 2;
    const int innerTop = TRANSITION_PROGRESS_TOP + 2;
    const int innerWidth = TRANSITION_PROGRESS_WIDTH - 4;
    const int innerHeight = TRANSITION_PROGRESS_HEIGHT - 4;
    int fillWidth;

    if (percent > 100U) percent = 100U;
    fillWidth = (innerWidth * (int)percent) / 100;

    fillRect(innerLeft, innerTop,
             innerLeft + innerWidth - 1,
             innerTop + innerHeight - 1,
             COLOR_BLACK);
    if (fillWidth > 0) {
        fillRect(innerLeft, innerTop,
                 innerLeft + fillWidth - 1,
                 innerTop + innerHeight - 1,
                 COLOR_AMBER);
    }
}

int EspNativeTransitionPresentation_showStats(
    const struct EspNativeGameplayTransitionState_s* transitionBase) {
    const EspNativeGameplayTransitionState* transition =
        (const EspNativeGameplayTransitionState*)transitionBase;
    EspNativeTransitionPaintScratch scratch;
    char source[24];
    char value[32];
    uint32_t fnv;
    int openedHere = 0;
    int ok = 0;

    if (transition == NULL || transition->active != 1U ||
        transition->waitingStats != 1U ||
        transition->levelStats.showStats != 1U || !framebufferReady()) {
        return 0;
    }

    memset(&scratch, 0, sizeof(scratch));
    if (!EspAssetPack_isOpen()) {
        if (!EspAssetPack_open(ESP_ASSET_PACK_DEFAULT_PATH)) return 0;
        openedHere = 1;
    }
    if (!openFont(&scratch.font, &scratch.stats)) goto done;

    /* Full opaque takeover: no gameplay top-bar pixels survive this frame. */
    fillRect(0, 0, DOOMRPG_LOGICAL_WIDTH - 1,
             DOOMRPG_LOGICAL_HEIGHT - 1, COLOR_BLACK);
    fillRect(3, 3, 156, 116, COLOR_BG);
    rect(3, 3, 156, 116, COLOR_STEEL_DARK);

    formatMapLabel(transition->committed.sourceMapId, source, sizeof(source));

    if (!drawGameTextCentered(&scratch.font, "LEVEL COMPLETE", 5,
                              &scratch.stats) ||
        !drawGameTextCentered(&scratch.font, source, 20, &scratch.stats)) {
        goto done;
    }

    fillRect(15, 36, 144, 36, COLOR_STEEL_DARK);

    if (!drawGameTextCentered(&scratch.font, "SECRETS", 42,
                              &scratch.stats)) {
        goto done;
    }
    snprintf(value, sizeof(value), "%u / %u",
             (unsigned int)transition->levelStats.secretsFound,
             (unsigned int)transition->levelStats.secretsTotal);
    if (!drawGameTextCentered(&scratch.font, value, 55, &scratch.stats) ||
        !drawGameTextCentered(&scratch.font, "MONSTERS", 72,
                              &scratch.stats)) {
        goto done;
    }
    snprintf(value, sizeof(value), "%u / %u",
             (unsigned int)transition->levelStats.monstersDead,
             (unsigned int)transition->levelStats.monstersTotal);
    if (!drawGameTextCentered(&scratch.font, value, 85, &scratch.stats) ||
        !drawGameTextCentered(&scratch.font, "TAP TO CONTINUE", 103,
                              &scratch.stats)) {
        goto done;
    }

    if (!Esp32PlatformVideo_present()) goto done;
    fnv = frameFNV();
    printf("[LEVELSTATS] PRESENT sourceMap=%u targetMap=%u source=%s secrets=%u/%u monsters=%u/%u extended=time+moves+xp-deferred font=game-a.bmp reads=%u bytes=%u fullScreen=yes frame=%08x input=one-tap\n",
           (unsigned int)transition->committed.sourceMapId,
           (unsigned int)transition->committed.targetMapId,
           source,
           (unsigned int)transition->levelStats.secretsFound,
           (unsigned int)transition->levelStats.secretsTotal,
           (unsigned int)transition->levelStats.monstersDead,
           (unsigned int)transition->levelStats.monstersTotal,
           (unsigned int)scratch.stats.packReads,
           (unsigned int)scratch.stats.bytesRead,
           (unsigned int)fnv);
    ok = 1;

done:
    if (openedHere && EspAssetPack_isOpen()) EspAssetPack_close();
    return ok;
}

int EspNativeTransitionPresentation_beginLoading(uint8_t targetMapId) {
    EspNativeTransitionPaintScratch scratch;
    char target[24];
    char entering[48];
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

    if (!drawFixedStarfield(&scratch.star, &scratch.stats) ||
        !openFont(&scratch.font, &scratch.stats)) {
        goto done;
    }

    formatMapLabel(targetMapId, target, sizeof(target));
    snprintf(entering, sizeof(entering), "ENTERING %s", target);

    /* Minimal readable treatment: no giant NOW, no decorative orange title.
     * The original starfield is the background; text uses the game's own font. */
    fillRect(13, 35, 146, 75, COLOR_PANEL);
    rect(13, 35, 146, 75, COLOR_STEEL_DARK);
    if (!drawGameTextCentered(&scratch.font, "LOADING...", 43,
                              &scratch.stats) ||
        !drawGameTextCentered(&scratch.font, entering, 59,
                              &scratch.stats)) {
        goto done;
    }

    rect(TRANSITION_PROGRESS_LEFT, TRANSITION_PROGRESS_TOP,
         TRANSITION_PROGRESS_LEFT + TRANSITION_PROGRESS_WIDTH - 1,
         TRANSITION_PROGRESS_TOP + TRANSITION_PROGRESS_HEIGHT - 1,
         COLOR_STEEL);
    paintProgressBar(0U);

    if (!Esp32PlatformVideo_present()) goto done;

    presentation.targetMapId = targetMapId;
    presentation.lastPercent = 0U;
    presentation.lastPhase = 0U;
    presentation.loadingActive = 1U;
    presentation.loadingStartMs = nowMs();
    presentation.frames = 1U;

    printf("[TRANSITIONLOAD] BEGIN targetMap=%u background=%s mode=fixed font=%s entering=\"%s\" progress=0%% reads=%u bytes=%u frame=%08x\n",
           (unsigned int)targetMapId,
           TRANSITION_STAR_NAME,
           TRANSITION_FONT_NAME,
           entering,
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
    uint32_t fnv;

    if (!presentation.loadingActive) return;

    percent = overallPercent(phase, completed, total);
    phaseChanged = phase != presentation.lastPhase ? 1U : 0U;

    if (!phaseChanged && percent < 100U &&
        percent < (uint8_t)(presentation.lastPercent +
                            TRANSITION_PROGRESS_STEP)) {
        return;
    }
    if (percent < presentation.lastPercent) percent = presentation.lastPercent;

    paintProgressBar(percent);
    if (!Esp32PlatformVideo_present()) {
        printf("[TRANSITIONLOAD] FRAME-DEFER phase=%s overall=%u reason=present-failed\n",
               phaseName(phase), (unsigned int)percent);
        return;
    }

    presentation.lastPhase = phase;
    presentation.lastPercent = percent;
    ++presentation.frames;
    fnv = frameFNV();

    printf("[TRANSITIONLOAD] FRAME n=%u phase=%s progress=%u/%u overall=%u background=fixed assetReads=0 frame=%08x\n",
           (unsigned int)presentation.frames,
           phaseName(phase),
           (unsigned int)completed,
           (unsigned int)total,
           (unsigned int)percent,
           (unsigned int)fnv);
}

void EspNativeTransitionPresentation_endLoading(void) {
    uint32_t elapsed;
    if (!presentation.loadingActive) {
        memset(&presentation, 0, sizeof(presentation));
        return;
    }
    if (presentation.lastPercent < 100U) {
        paintProgressBar(100U);
        if (Esp32PlatformVideo_present()) {
            ++presentation.frames;
            presentation.lastPercent = 100U;
        }
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
