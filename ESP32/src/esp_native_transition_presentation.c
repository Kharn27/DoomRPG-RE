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
#define TRANSITION_STAR_STEP_MS 157U
#define TRANSITION_PRESENT_INTERVAL_MS 314U

#define COLOR_BLACK      0x0000U
#define COLOR_BG         0x0841U
#define COLOR_PANEL      0x18c3U
#define COLOR_STEEL_DARK 0x3186U
#define COLOR_STEEL      0x6b4dU
#define COLOR_IVORY      0xef5cU
#define COLOR_AMBER      0xfd20U
#define COLOR_GREEN      0x4d8bU

typedef struct EspNativeTransitionPresentationState_s {
    EspNativeIndexedBmp star;
    uint32_t loadingStartMs;
    uint32_t lastPresentMs;
    uint32_t frames;
    uint8_t targetMapId;
    uint8_t starReady;
    uint8_t loadingActive;
    uint8_t reserved;
} EspNativeTransitionPresentationState;

static EspNativeTransitionPresentationState presentation;

/* Compact 5x7 uppercase font. Rows are five low bits, left to right. Program
 * data only: transition UI adds no second font image or framebuffer owner. */
static const uint8_t tinyFont[59][7] = {
    ['0' - 32] = {0x0e,0x11,0x13,0x15,0x19,0x11,0x0e},
    ['1' - 32] = {0x04,0x0c,0x04,0x04,0x04,0x04,0x0e},
    ['2' - 32] = {0x0e,0x11,0x01,0x02,0x04,0x08,0x1f},
    ['3' - 32] = {0x1e,0x01,0x01,0x0e,0x01,0x01,0x1e},
    ['4' - 32] = {0x02,0x06,0x0a,0x12,0x1f,0x02,0x02},
    ['5' - 32] = {0x1f,0x10,0x10,0x1e,0x01,0x01,0x1e},
    ['6' - 32] = {0x0e,0x10,0x10,0x1e,0x11,0x11,0x0e},
    ['7' - 32] = {0x1f,0x01,0x02,0x04,0x08,0x08,0x08},
    ['8' - 32] = {0x0e,0x11,0x11,0x0e,0x11,0x11,0x0e},
    ['9' - 32] = {0x0e,0x11,0x11,0x0f,0x01,0x01,0x0e},
    ['A' - 32] = {0x0e,0x11,0x11,0x1f,0x11,0x11,0x11},
    ['B' - 32] = {0x1e,0x11,0x11,0x1e,0x11,0x11,0x1e},
    ['C' - 32] = {0x0f,0x10,0x10,0x10,0x10,0x10,0x0f},
    ['D' - 32] = {0x1e,0x11,0x11,0x11,0x11,0x11,0x1e},
    ['E' - 32] = {0x1f,0x10,0x10,0x1e,0x10,0x10,0x1f},
    ['F' - 32] = {0x1f,0x10,0x10,0x1e,0x10,0x10,0x10},
    ['G' - 32] = {0x0f,0x10,0x10,0x17,0x11,0x11,0x0f},
    ['H' - 32] = {0x11,0x11,0x11,0x1f,0x11,0x11,0x11},
    ['I' - 32] = {0x0e,0x04,0x04,0x04,0x04,0x04,0x0e},
    ['J' - 32] = {0x01,0x01,0x01,0x01,0x11,0x11,0x0e},
    ['K' - 32] = {0x11,0x12,0x14,0x18,0x14,0x12,0x11},
    ['L' - 32] = {0x10,0x10,0x10,0x10,0x10,0x10,0x1f},
    ['M' - 32] = {0x11,0x1b,0x15,0x15,0x11,0x11,0x11},
    ['N' - 32] = {0x11,0x19,0x15,0x13,0x11,0x11,0x11},
    ['O' - 32] = {0x0e,0x11,0x11,0x11,0x11,0x11,0x0e},
    ['P' - 32] = {0x1e,0x11,0x11,0x1e,0x10,0x10,0x10},
    ['Q' - 32] = {0x0e,0x11,0x11,0x11,0x15,0x12,0x0d},
    ['R' - 32] = {0x1e,0x11,0x11,0x1e,0x14,0x12,0x11},
    ['S' - 32] = {0x0f,0x10,0x10,0x0e,0x01,0x01,0x1e},
    ['T' - 32] = {0x1f,0x04,0x04,0x04,0x04,0x04,0x04},
    ['U' - 32] = {0x11,0x11,0x11,0x11,0x11,0x11,0x0e},
    ['V' - 32] = {0x11,0x11,0x11,0x11,0x11,0x0a,0x04},
    ['W' - 32] = {0x11,0x11,0x11,0x15,0x15,0x15,0x0a},
    ['X' - 32] = {0x11,0x11,0x0a,0x04,0x0a,0x11,0x11},
    ['Y' - 32] = {0x11,0x11,0x0a,0x04,0x04,0x04,0x04},
    ['Z' - 32] = {0x1f,0x01,0x02,0x04,0x08,0x10,0x1f},
    ['.' - 32] = {0x00,0x00,0x00,0x00,0x00,0x06,0x06},
    ['/' - 32] = {0x01,0x02,0x02,0x04,0x08,0x08,0x10},
    ['-' - 32] = {0x00,0x00,0x00,0x1f,0x00,0x00,0x00},
    [':' - 32] = {0x00,0x06,0x06,0x00,0x06,0x06,0x00}
};

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
        y < 0 || y >= DOOMRPG_LOGICAL_HEIGHT) return;
    fb[(uint32_t)y * DOOMRPG_LOGICAL_WIDTH + (uint32_t)x] = color;
}

static void fillRect(int left, int top, int right, int bottom, uint16_t color) {
    int x;
    int y;
    if (left < 0) left = 0;
    if (top < 0) top = 0;
    if (right >= DOOMRPG_LOGICAL_WIDTH) right = DOOMRPG_LOGICAL_WIDTH - 1;
    if (bottom >= DOOMRPG_LOGICAL_HEIGHT) bottom = DOOMRPG_LOGICAL_HEIGHT - 1;
    for (y = top; y <= bottom; ++y) {
        for (x = left; x <= right; ++x) pixel(x, y, color);
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

static const uint8_t* glyph(char c) {
    unsigned int index;
    if (c >= 'a' && c <= 'z') c = (char)(c - ('a' - 'A'));
    if (c < 32 || c > 'Z') return NULL;
    index = (unsigned int)(c - 32);
    if (index >= (sizeof(tinyFont) / sizeof(tinyFont[0]))) return NULL;
    return tinyFont[index];
}

static int textWidth(const char* text, int scale) {
    size_t length;
    if (text == NULL || scale <= 0) return 0;
    length = strlen(text);
    return length == 0U ? 0 : (int)(length * (size_t)(6 * scale) - scale);
}

static void drawText(const char* text, int x, int y, int scale, uint16_t color) {
    const unsigned char* p = (const unsigned char*)text;
    if (text == NULL || scale <= 0) return;
    while (*p != '\0') {
        char c = (char)*p++;
        const uint8_t* rows = glyph(c);
        int row;
        if (c != ' ' && rows != NULL) {
            for (row = 0; row < 7; ++row) {
                int col;
                for (col = 0; col < 5; ++col) {
                    if ((rows[row] & (uint8_t)(1U << (4 - col))) != 0U) {
                        int sy;
                        for (sy = 0; sy < scale; ++sy) {
                            int sx;
                            for (sx = 0; sx < scale; ++sx) {
                                pixel(x + col * scale + sx,
                                      y + row * scale + sy, color);
                            }
                        }
                    }
                }
            }
        }
        x += 6 * scale;
    }
}

static void drawCentered(const char* text, int y, int scale, uint16_t color) {
    drawText(text, (DOOMRPG_LOGICAL_WIDTH - textWidth(text, scale)) / 2,
             y, scale, color);
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

static const char* phaseName(uint8_t phase) {
    switch (phase) {
    case ESP_ASSET_PACK_MAP_FLASH_PROGRESS_ERASE: return "ERASE";
    case ESP_ASSET_PACK_MAP_FLASH_PROGRESS_COPY: return "COPY";
    case ESP_ASSET_PACK_MAP_FLASH_PROGRESS_VERIFY: return "VERIFY";
    default: return "PREP";
    }
}

static int paintStarfield(uint32_t elapsedMs,
                          EspNativeIndexedBmpStats* stats) {
    uint16_t* fb = framebuffer();
    uint16_t sourceY = 0U;
    uint16_t destY = 0U;
    uint16_t phase;
    if (fb == NULL || !presentation.starReady ||
        presentation.star.width == 0U || presentation.star.height == 0U ||
        !EspAssetPack_isOpen()) return 0;

    phase = (uint16_t)((elapsedMs / TRANSITION_STAR_STEP_MS) %
                       presentation.star.width);
    while (destY < DOOMRPG_LOGICAL_HEIGHT) {
        uint16_t h = presentation.star.height;
        uint16_t x = 0U;
        uint16_t sx =
            (uint16_t)((presentation.star.width - phase) %
                       presentation.star.width);
        if ((uint32_t)destY + h > DOOMRPG_LOGICAL_HEIGHT) {
            h = (uint16_t)(DOOMRPG_LOGICAL_HEIGHT - destY);
        }
        while (x < DOOMRPG_LOGICAL_WIDTH) {
            uint16_t run =
                (uint16_t)(presentation.star.width - sx);
            if ((uint32_t)x + run > DOOMRPG_LOGICAL_WIDTH) {
                run = (uint16_t)(DOOMRPG_LOGICAL_WIDTH - x);
            }
            if (EspNativeIndexedBmp_blit(
                    &presentation.star, fb,
                    DOOMRPG_LOGICAL_WIDTH, DOOMRPG_LOGICAL_HEIGHT,
                    sx, sourceY, run, h,
                    (int16_t)x, (int16_t)destY, 0U, stats) !=
                ESP_NATIVE_INDEXED_BMP_OK) {
                return 0;
            }
            x = (uint16_t)(x + run);
            sx = 0U;
        }
        destY = (uint16_t)(destY + h);
        sourceY = 0U;
    }
    return 1;
}

static int paintLoadingFrame(uint8_t phase,
                             uint32_t completed,
                             uint32_t total,
                             int force) {
    EspNativeIndexedBmpStats stats;
    char target[24];
    uint32_t now;
    uint32_t elapsed;
    uint32_t fnv;
    if (!presentation.loadingActive || !framebufferReady()) return 0;
    now = nowMs();
    if (!force && (uint32_t)(now - presentation.lastPresentMs) <
                      TRANSITION_PRESENT_INTERVAL_MS) {
        return 1;
    }
    if (!EspAssetPack_isOpen()) return force ? 0 : 1;

    memset(&stats, 0, sizeof(stats));
    elapsed = (uint32_t)(now - presentation.loadingStartMs);
    fillRect(0, 0, DOOMRPG_LOGICAL_WIDTH - 1,
             DOOMRPG_LOGICAL_HEIGHT - 1, COLOR_BLACK);
    if (!paintStarfield(elapsed, &stats)) return 0;

    fillRect(17, 35, 142, 86, COLOR_BG);
    rect(16, 34, 143, 87, COLOR_STEEL);
    rect(18, 36, 141, 85, COLOR_STEEL_DARK);
    drawCentered("NOW", 42, 2, COLOR_IVORY);
    drawCentered("LOADING...", 59, 1, COLOR_AMBER);
    formatMapLabel(presentation.targetMapId, target, sizeof(target));
    drawCentered(target, 75, 1, COLOR_IVORY);

    if (!Esp32PlatformVideo_present()) return 0;
    presentation.lastPresentMs = now;
    ++presentation.frames;
    fnv = frameFNV();
    printf("[TRANSITIONLOAD] FRAME n=%u phase=%s progress=%u/%u starStep=%u reads=%u bytes=%u frame=%08x\n",
           (unsigned int)presentation.frames,
           phaseName(phase),
           (unsigned int)completed,
           (unsigned int)total,
           (unsigned int)(elapsed / TRANSITION_STAR_STEP_MS),
           (unsigned int)stats.packReads,
           (unsigned int)stats.bytesRead,
           (unsigned int)fnv);
    return 1;
}

int EspNativeTransitionPresentation_showStats(
    const struct EspNativeGameplayTransitionState_s* transitionBase) {
    const EspNativeGameplayTransitionState* transition =
        (const EspNativeGameplayTransitionState*)transitionBase;
    char source[24];
    char value[32];
    uint32_t fnv;

    if (transition == NULL || transition->active != 1U ||
        transition->waitingStats != 1U ||
        transition->levelStats.showStats != 1U || !framebufferReady()) {
        return 0;
    }

    fillRect(0, 0, DOOMRPG_LOGICAL_WIDTH - 1,
             DOOMRPG_LOGICAL_HEIGHT - 1, COLOR_BG);
    rect(2, 2, 157, 117, COLOR_STEEL);
    rect(4, 4, 155, 115, COLOR_STEEL_DARK);

    drawCentered("LEVEL COMPLETE", 7, 1, COLOR_IVORY);
    formatMapLabel(transition->committed.sourceMapId, source, sizeof(source));
    drawCentered(source, 18, 2, COLOR_AMBER);

    fillRect(12, 37, 147, 38, COLOR_STEEL_DARK);
    drawCentered("SECRETS", 43, 1, COLOR_IVORY);
    snprintf(value, sizeof(value), "%u / %u",
             (unsigned int)transition->levelStats.secretsFound,
             (unsigned int)transition->levelStats.secretsTotal);
    drawCentered(value, 53, 2,
                 transition->levelStats.secretsFound ==
                         transition->levelStats.secretsTotal
                     ? COLOR_GREEN : COLOR_IVORY);

    drawCentered("MONSTERS", 72, 1, COLOR_IVORY);
    snprintf(value, sizeof(value), "%u / %u",
             (unsigned int)transition->levelStats.monstersDead,
             (unsigned int)transition->levelStats.monstersTotal);
    drawCentered(value, 82, 2,
                 transition->levelStats.monstersDead ==
                         transition->levelStats.monstersTotal
                     ? COLOR_GREEN : COLOR_IVORY);

    drawCentered("TAP TO CONTINUE", 105, 1, COLOR_AMBER);

    if (!Esp32PlatformVideo_present()) return 0;
    fnv = frameFNV();
    printf("[LEVELSTATS] PRESENT sourceMap=%u targetMap=%u source=%s secrets=%u/%u monsters=%u/%u extended=time+moves+xp-deferred frame=%08x input=one-tap\n",
           (unsigned int)transition->committed.sourceMapId,
           (unsigned int)transition->committed.targetMapId,
           source,
           (unsigned int)transition->levelStats.secretsFound,
           (unsigned int)transition->levelStats.secretsTotal,
           (unsigned int)transition->levelStats.monstersDead,
           (unsigned int)transition->levelStats.monstersTotal,
           (unsigned int)fnv);
    return 1;
}

int EspNativeTransitionPresentation_beginLoading(uint8_t targetMapId) {
    EspNativeIndexedBmpStats stats;
    int openedHere = 0;
    if (!EspMapCatalog_isValidId(targetMapId) || !framebufferReady() ||
        presentation.loadingActive) return 0;

    memset(&presentation, 0, sizeof(presentation));
    memset(&stats, 0, sizeof(stats));

    if (!EspAssetPack_isOpen()) {
        if (!EspAssetPack_open(ESP_ASSET_PACK_DEFAULT_PATH)) return 0;
        openedHere = 1;
    }
    if (EspNativeIndexedBmp_open(TRANSITION_STAR_NAME,
                                 &presentation.star, &stats) !=
            ESP_NATIVE_INDEXED_BMP_OK ||
        presentation.star.width == 0U || presentation.star.height == 0U) {
        if (openedHere) EspAssetPack_close();
        memset(&presentation, 0, sizeof(presentation));
        return 0;
    }

    presentation.targetMapId = targetMapId;
    presentation.starReady = 1U;
    presentation.loadingActive = 1U;
    presentation.loadingStartMs = nowMs();
    presentation.lastPresentMs = 0U;

    if (!paintLoadingFrame(0U, 0U, 0U, 1)) {
        if (openedHere) EspAssetPack_close();
        memset(&presentation, 0, sizeof(presentation));
        return 0;
    }
    if (openedHere) EspAssetPack_close();

    printf("[TRANSITIONLOAD] BEGIN targetMap=%u star=%s size=%ux%u cadence=%ums presentInterval=%ums ownerBytes=%u fullFrameExtra=0\n",
           (unsigned int)targetMapId,
           TRANSITION_STAR_NAME,
           (unsigned int)presentation.star.width,
           (unsigned int)presentation.star.height,
           (unsigned int)TRANSITION_STAR_STEP_MS,
           (unsigned int)TRANSITION_PRESENT_INTERVAL_MS,
           (unsigned int)sizeof(presentation));
    return 1;
}

void EspNativeTransitionPresentation_progress(uint8_t phase,
                                              uint32_t completed,
                                              uint32_t total) {
    if (!presentation.loadingActive) return;
    if (!paintLoadingFrame(phase, completed, total, 0)) {
        printf("[TRANSITIONLOAD] FRAME-DEFER phase=%s progress=%u/%u reason=paint-failed\n",
               phaseName(phase),
               (unsigned int)completed,
               (unsigned int)total);
    }
}

void EspNativeTransitionPresentation_endLoading(void) {
    uint32_t elapsed;
    if (!presentation.loadingActive) {
        memset(&presentation, 0, sizeof(presentation));
        return;
    }
    elapsed = (uint32_t)(nowMs() - presentation.loadingStartMs);
    printf("[TRANSITIONLOAD] END targetMap=%u frames=%u elapsedMs=%u framebuffer=retained-until-target-frame\n",
           (unsigned int)presentation.targetMapId,
           (unsigned int)presentation.frames,
           (unsigned int)elapsed);
    memset(&presentation, 0, sizeof(presentation));
}

void EspNativeTransitionPresentation_reset(void) {
    memset(&presentation, 0, sizeof(presentation));
}
