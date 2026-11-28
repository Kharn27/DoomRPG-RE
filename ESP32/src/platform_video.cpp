#include "platform_video.h"

#include <Arduino.h>
#include <TFT_eSPI.h>

#include <algorithm>

#include "platform_video_config.h"
#include "esp_video_nearest.h"
#include "esp_res60_layout.h"
#include "esp_res60_world_probe.h"
#if DOOMRPG_RES60_P17_HEAP_SNAPSHOT
#include "esp_res60_world_preview.h"
#endif
#if DOOMRPG_RES60_P20_COMPACT_SERIAL
#include "esp_res60_serial_policy.h"
#endif
#if DOOMRPG_RES60_P12_INVENTORY_CACHE
#include "esp_res60_inventory_cache.h"
#endif
#if DOOMRPG_RES60_P13_MAP_LEASE
#include "esp_res60_map_lease.h"
#endif
#if DOOMRPG_RES60_P11_PROFILE
#include "esp_res60_profile.h"
extern "C" uint32_t EspRes60Profile_nowUs(void) {
    return micros();
}
#endif
#if DOOMRPG_RES60_P7_NAV
#include "esp_res60_nav.h"
#include "esp_map_runtime.h"
#include "esp_asset_pack.h"
#endif
#if DOOMRPG_RES60_P5_TITLE
#include "esp_res60_title_preview.h"
#endif
#if DOOMRPG_RES60_P6_MENU
#include "esp_res60_menu.h"
#endif
#if DOOMRPG_RES60_P3_WORLD
#include <esp_heap_caps.h>
#endif
#include "esp_native_backlight.h"
#include "esp_native_orientation.h"
#include "esp_uac_menu_preview.h"
#include "doomrpg_log.h"
#include "esp_native_resident_gameplay.h"
#include "esp_turn_lamp.h"

namespace {

TFT_eSPI* platformDisplay = nullptr;
uint16_t* framebuffer = nullptr;
#if DOOMRPG_RES60_P16_HEAP_TOPOLOGY
/* One extra byte of BSS (plus compiler alignment), no large arrays or
 * malloc. The profiler is used only AFTER the P11 time capture. */
static uint8_t res60P16WarmDumped=0U;
static void res60P16_printHeapTopology(const char* phase) {
    multi_heap_info_t before{};
    heap_caps_get_info(&before,MALLOC_CAP_8BIT);
    Serial.printf("[RES60P16] REGIONS phase=%s beforeFree=%u beforeLargest=%u beforeMin=%u freeBlocks=%u allocatedBlocks=%u totalBlocks=%u\n",
                  phase,(unsigned)before.total_free_bytes,
                  (unsigned)before.largest_free_block,
                  (unsigned)before.minimum_free_bytes,
                  (unsigned)before.free_blocks,
                  (unsigned)before.allocated_blocks,
                  (unsigned)before.total_blocks);
    /* Per-capability internal heap-region addresses, NOT a new allocator.
     * Published once after framebuffer allocation and once after first HIT. */
    heap_caps_print_heap_info(MALLOC_CAP_8BIT);
    multi_heap_info_t after{};
    heap_caps_get_info(&after,MALLOC_CAP_8BIT);
    Serial.printf("[RES60P16] OBSERVER phase=%s freeAfter=%u largestAfter=%u freeDelta=%ld largestDelta=%ld neutral=%u\n",
                  phase,(unsigned)after.total_free_bytes,
                  (unsigned)after.largest_free_block,
                  (long)after.total_free_bytes-(long)before.total_free_bytes,
                  (long)after.largest_free_block-(long)before.largest_free_block,
                  (unsigned)(after.total_free_bytes==before.total_free_bytes &&
                             after.largest_free_block==before.largest_free_block));
}
#endif

/* Physical-only overlay: snapshot the exact last presented HUD patch, not
 * the potentially newer shared framebuffer. No owner edits, heap, PAK or
 * world redraw. 6x6 logical pixels inside the AUTOMAP touch zone (128..159). */
constexpr int kLampLeft = 152, kLampTop = 2, kLampSize = 6;
static_assert(kLampLeft + kLampSize <= DOOMRPG_LOGICAL_WIDTH &&
              kLampTop + kLampSize <= 20 && DOOMRPG_INTEGER_SCALE == 2,
              "turn lamp must fit classic CYD top HUD");
uint16_t lampUnderlay[kLampSize * kLampSize];
EspTurnLampState lampState = {};
bool lampWorldFrame = false;

void turnLampWrite(bool on) {
    if (platformDisplay == nullptr) return;
    const int width = kLampSize * DOOMRPG_INTEGER_SCALE;
    uint16_t row[kLampSize * DOOMRPG_INTEGER_SCALE];
    platformDisplay->startWrite();
    platformDisplay->setAddrWindow(kLampLeft * DOOMRPG_INTEGER_SCALE,
                                  kLampTop * DOOMRPG_INTEGER_SCALE,
                                  width, width);
    for (int y = 0; y < kLampSize; ++y) {
        for (int x = 0; x < kLampSize; ++x) {
            const bool edge = x == 0 || y == 0 ||
                              x == kLampSize-1 || y == kLampSize-1;
            const uint16_t color = on ? (edge ? 0x0000U : 0xF800U)
                                      : lampUnderlay[y*kLampSize+x];
            row[x*2] = row[x*2+1] = color;
        }
        for (int repeat = 0; repeat < DOOMRPG_INTEGER_SCALE; ++repeat)
            platformDisplay->pushPixels(row, width);
    }
    platformDisplay->endWrite();
}

/* Dedicated backlight LEDC channel; no RGB565 data or frame presentation is
 * ever modified by this owner. TFT_eSPI initializes active-high GPIO21. */
constexpr uint8_t kBacklightChannel = 7U;
constexpr uint32_t kBacklightFrequencyHz = 5000U;
constexpr uint8_t kBacklightResolutionBits = 8U;
bool backlightReady = false;
uint8_t backlightPercent = 100U;
uint8_t currentOrientation = ESP32_ORIENTATION_NORMAL;

#if !defined(TFT_BL) || TFT_BL != 21 || !defined(TFT_BACKLIGHT_ON)
#error "CYD backlight PWM requires active-high TFT_BL=21"
#endif

/* Two classic CYD ILI9341 batches require opposite LCD inversion.
 * Both release environments share the same native engine and framebuffer.
 * Gamma is calibrated independently for the two physical panel batches.
 * Reapply gamma and polarity after setRotation() on both boards. */
#ifndef DOOMRPG_CYD_ALTERNATE_PANEL_INVERSION
#define DOOMRPG_CYD_ALTERNATE_PANEL_INVERSION 0
#endif
#if DOOMRPG_CYD_ALTERNATE_PANEL_INVERSION != 0 && DOOMRPG_CYD_ALTERNATE_PANEL_INVERSION != 1
#error "CYD alternate panel inversion flag must be 0 or 1"
#endif


constexpr size_t kPixelCount =
    static_cast<size_t>(DOOMRPG_VIDEO_BUFFER_WIDTH) * DOOMRPG_VIDEO_BUFFER_HEIGHT;
constexpr size_t kFramebufferBytes = kPixelCount * sizeof(uint16_t);

/* LCD-C1 on classic CYD FAIL (photo, 15 Nov 2026): copying both gamma
 * tables from the accepted INV-OFF panel into the INV-ON first CYD produced
 * severely washed-out cyan highlights. LCD-C2 isolates only E1 as the trial:
 * restore exact proven classic E0 bytes from merged PR #341, but retain
 * the published ILI9341_DRIVER negative E1. The accepted alternate panel
 * still writes its EXACT previous E0/E1 bytes with INV OFF. No RGB565/FB,
 * renderer, PAK, touch, gameplay, backlight or panel power changes. */
#if !DOOMRPG_CYD_ALTERNATE_PANEL_INVERSION
constexpr uint8_t kCydClassicLegacyE0Gamma[15] = {
    0x00, 0x15, 0x17, 0x07, 0x11, 0x06, 0x2b, 0x56,
    0x3c, 0x05, 0x10, 0x0f, 0x3f, 0x3f, 0x0f,
};
#else
constexpr uint8_t kCydPanelPositiveGamma[15] = {
    0x0f, 0x31, 0x2b, 0x0c, 0x0e, 0x08, 0x4e, 0xf1,
    0x37, 0x07, 0x10, 0x03, 0x0e, 0x09, 0x00,
};
#endif
constexpr uint8_t kCydPanelNegativeGamma[15] = {
    0x00, 0x0e, 0x14, 0x03, 0x11, 0x07, 0x31, 0xc1,
    0x48, 0x08, 0x0f, 0x0c, 0x31, 0x36, 0x0f,
};

#if DOOMRPG_ESP32_TOUCH_HITBOX_OVERLAY
constexpr int kDebugOverlayMaxZones = 8;

struct DebugOverlayZone {
    int16_t left;
    int16_t top;
    int16_t right;
    int16_t bottom;
    bool valid;
};

DebugOverlayZone debugOverlayZones[kDebugOverlayMaxZones]{};
int debugOverlayZoneCount = 0;
bool debugTouchValid = false;
int16_t debugTouchX = 0;
int16_t debugTouchY = 0;
#endif

uint16_t rgb565(uint8_t red, uint8_t green, uint8_t blue) {
    return static_cast<uint16_t>(((red & 0xf8) << 8) |
                                 ((green & 0xfc) << 3) | (blue >> 3));
}

void applyCydInvertedGammaProfile() {
    if (platformDisplay == nullptr) {
        return;
    }

    /* Keep power/VCOM/frame-rate, BGR, selector and the INV polarity.
     * C2 differs from merged main ONLY on classic INVON E1; second-panel
     * INV OFF gamma command stream remains byte-for-byte unchanged. */
    platformDisplay->writecommand(0xf2);
    platformDisplay->writedata(0x00);
    platformDisplay->writecommand(0x26);
    platformDisplay->writedata(0x01);

    platformDisplay->writecommand(0xe0);
#if DOOMRPG_CYD_ALTERNATE_PANEL_INVERSION
    for (size_t i = 0; i < sizeof(kCydPanelPositiveGamma); ++i) {
        platformDisplay->writedata(kCydPanelPositiveGamma[i]);
    }
#else
    for (size_t i = 0; i < sizeof(kCydClassicLegacyE0Gamma); ++i) {
        platformDisplay->writedata(kCydClassicLegacyE0Gamma[i]);
    }
#endif

    platformDisplay->writecommand(0xe1);
    for (size_t i = 0; i < sizeof(kCydPanelNegativeGamma); ++i) {
        platformDisplay->writedata(kCydPanelNegativeGamma[i]);
    }

#if DOOMRPG_CYD_ALTERNATE_PANEL_INVERSION
    platformDisplay->invertDisplay(false); /* alternative LCD INV OFF */
#else
    platformDisplay->invertDisplay(true);  /* classic calibrated LCD INV ON */
#endif
    platformDisplay->setSwapBytes(true);
}

void setPixel(int x, int y, uint16_t color) {
    if (framebuffer == nullptr || x < 0 || y < 0 ||
        x >= DOOMRPG_VIDEO_BUFFER_WIDTH || y >= DOOMRPG_VIDEO_BUFFER_HEIGHT) {
        return;
    }
    framebuffer[y * DOOMRPG_VIDEO_BUFFER_WIDTH + x] = color;
}

void fillRect(int x, int y, int width, int height, uint16_t color) {
    if (framebuffer == nullptr || width <= 0 || height <= 0) {
        return;
    }

    const int x0 = std::max(0, x);
    const int y0 = std::max(0, y);
    const int x1 = std::min(DOOMRPG_VIDEO_BUFFER_WIDTH, x + width);
    const int y1 = std::min(DOOMRPG_VIDEO_BUFFER_HEIGHT, y + height);

    for (int row = y0; row < y1; ++row) {
        std::fill(framebuffer + row * DOOMRPG_VIDEO_BUFFER_WIDTH + x0,
                  framebuffer + row * DOOMRPG_VIDEO_BUFFER_WIDTH + x1, color);
    }
}

void drawLine(int x0, int y0, int x1, int y1, uint16_t color) {
    const int dx = abs(x1 - x0);
    const int sx = x0 < x1 ? 1 : -1;
    const int dy = -abs(y1 - y0);
    const int sy = y0 < y1 ? 1 : -1;
    int error = dx + dy;

    while (true) {
        setPixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) {
            break;
        }
        const int doubled = error * 2;
        if (doubled >= dy) {
            error += dy;
            x0 += sx;
        }
        if (doubled <= dx) {
            error += dx;
            y0 += sy;
        }
    }
}

#if DOOMRPG_ESP32_TOUCH_HITBOX_OVERLAY
void drawDebugOverlay() {
    if (platformDisplay == nullptr) {
        return;
    }

    for (int i = 0; i < debugOverlayZoneCount; ++i) {
        const DebugOverlayZone& zone = debugOverlayZones[i];
        if (!zone.valid) {
            continue;
        }

        const int x0 = zone.left * DOOMRPG_INTEGER_SCALE;
        const int y0 = zone.top * DOOMRPG_INTEGER_SCALE;
        const int x1 = ((zone.right + 1) * DOOMRPG_INTEGER_SCALE) - 1;
        const int y1 = ((zone.bottom + 1) * DOOMRPG_INTEGER_SCALE) - 1;
        const int width = x1 - x0 + 1;
        const int height = y1 - y0 + 1;

        platformDisplay->drawRect(x0, y0, width, height, TFT_RED);
        if (width > 4 && height > 4) {
            platformDisplay->drawRect(x0 + 1, y0 + 1,
                                      width - 2, height - 2, TFT_RED);
        }
    }

    if (debugTouchValid) {
        const int x0 = std::max(0, static_cast<int>(debugTouchX) - 6);
        const int x1 = std::min(DOOMRPG_PHYSICAL_WIDTH - 1,
                                static_cast<int>(debugTouchX) + 6);
        const int y0 = std::max(0, static_cast<int>(debugTouchY) - 6);
        const int y1 = std::min(DOOMRPG_PHYSICAL_HEIGHT - 1,
                                static_cast<int>(debugTouchY) + 6);

        platformDisplay->drawFastHLine(x0, debugTouchY,
                                       x1 - x0 + 1, TFT_CYAN);
        platformDisplay->drawFastVLine(debugTouchX, y0,
                                       y1 - y0 + 1, TFT_CYAN);
        platformDisplay->drawCircle(debugTouchX, debugTouchY, 3, TFT_YELLOW);
    }
}
#endif

}  // namespace

bool PlatformVideo_begin(TFT_eSPI* display) {
    platformDisplay = display;
    if (platformDisplay == nullptr) {
        Serial.println("[VIDEO] No TFT attached");
        return false;
    }

#if DOOMRPG_RES60_P16_HEAP_TOPOLOGY
    /* Capture BEFORE calloc, do not print here: lazy UART/log initialization
     * could otherwise alter the very allocation topology under inspection. */
    multi_heap_info_t p16BeforeFramebuffer{};
    heap_caps_get_info(&p16BeforeFramebuffer,MALLOC_CAP_8BIT);
    const bool p16WasAllocated=framebuffer!=nullptr;
#endif
    if (framebuffer == nullptr) {
        framebuffer = static_cast<uint16_t*>(calloc(kPixelCount, sizeof(uint16_t)));
        if (framebuffer == nullptr) {
            Serial.printf("[VIDEO] Unable to allocate %u-byte framebuffer\n",
                          static_cast<unsigned int>(kFramebufferBytes));
            return false;
        }
    }

#if DOOMRPG_RES60_P16_HEAP_TOPOLOGY
    multi_heap_info_t p16AfterFramebuffer{};
    heap_caps_get_info(&p16AfterFramebuffer,MALLOC_CAP_8BIT);
    Serial.printf("[RES60P16] FRAMEBUFFER wasAllocated=%u bytes=%u addr=%p freeBefore=%u freeAfter=%u largestBefore=%u largestAfter=%u minBefore=%u minAfter=%u freeBlocksBefore=%u freeBlocksAfter=%u\n",
                  (unsigned)p16WasAllocated,(unsigned)kFramebufferBytes,
                  static_cast<void*>(framebuffer),
                  (unsigned)p16BeforeFramebuffer.total_free_bytes,
                  (unsigned)p16AfterFramebuffer.total_free_bytes,
                  (unsigned)p16BeforeFramebuffer.largest_free_block,
                  (unsigned)p16AfterFramebuffer.largest_free_block,
                  (unsigned)p16BeforeFramebuffer.minimum_free_bytes,
                  (unsigned)p16AfterFramebuffer.minimum_free_bytes,
                  (unsigned)p16BeforeFramebuffer.free_blocks,
                  (unsigned)p16AfterFramebuffer.free_blocks);
    res60P16_printHeapTopology("framebuffer-ready");
#endif
    applyCydInvertedGammaProfile();

    if (!backlightReady) {
        if (ledcSetup(kBacklightChannel, kBacklightFrequencyHz,
                      kBacklightResolutionBits) <= 0.0) {
            Serial.println("[BACKLIGHT] FAILED LEDC setup; video remains usable, Luminosite setting unavailable");
        } else {
            ledcAttachPin(TFT_BL, kBacklightChannel);
            ledcWrite(kBacklightChannel, 255U);
            backlightPercent = 100U;
            backlightReady = true;
            Serial.println("[BACKLIGHT] READY pin=21 active=HIGH pwm=5000Hz/8bit channel=7 percent=100 persistence=native-sd-config");
        }
    }

#if DOOMRPG_RES60_P1_DIAGNOSTIC
    Serial.printf("[VIDEO] RES60-P1 VIDEO-ONLY framebuffer %dx%d RGB565, %u bytes\n",
                  DOOMRPG_VIDEO_BUFFER_WIDTH, DOOMRPG_VIDEO_BUFFER_HEIGHT,
                  static_cast<unsigned int>(kFramebufferBytes));
    Serial.printf("[VIDEO] Physical output %dx%d fractional nearest 5/3; gameplay=NOT_STARTED\n",
                  DOOMRPG_PHYSICAL_WIDTH, DOOMRPG_PHYSICAL_HEIGHT);
#else
    Serial.printf("[VIDEO] Logical framebuffer %dx%d RGB565, %u bytes\n",
                  DOOMRPG_LOGICAL_WIDTH, DOOMRPG_LOGICAL_HEIGHT,
                  static_cast<unsigned int>(kFramebufferBytes));
    Serial.printf("[VIDEO] Physical output %dx%d, integer scale %dx\n",
                  DOOMRPG_PHYSICAL_WIDTH, DOOMRPG_PHYSICAL_HEIGHT,
                  DOOMRPG_INTEGER_SCALE);
#endif
#if DOOMRPG_CYD_ALTERNATE_PANEL_INVERSION
    Serial.println("[VIDEO] CYD display profile alternate-panel inversion=off gamma=ILI9341-driver-E0E1-trial BGR=unchanged resampling=nearest framebuffer=untouched");
#else
    Serial.println("[VIDEO] CYD display profile classic inversion=on gamma=classic-E0-driver-E1-trial BGR=unchanged resampling=nearest framebuffer=untouched");
#endif
#if DOOMRPG_ESP32_TOUCH_HITBOX_OVERLAY
    Serial.println("[HITBOX] Physical overlay enabled; framebuffer hashes remain untouched");
#endif
#if DOOMRPG_RES60_P17_HEAP_SNAPSHOT
    /* P17 acquisition happens once only AFTER framebuffer allocation and
     * P16 framebuffer-ready probes. This is an A/B of allocator topology.
     * Fail before showing any geometry and rollback the owned framebuffer. */
    const size_t p17LargestBefore=heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    const size_t p17FreeBefore=heap_caps_get_free_size(MALLOC_CAP_8BIT);
    if (!EspRes60Preview_snapshotBegin()) {
        free(framebuffer);
        framebuffer=nullptr;
        Serial.println("[RES60P17] FAIL CLOSED stage=one-shot-6144B-owner");
        return false;
    }
    Serial.printf("[RES60P17] SNAPSHOT owner=HEAP bytes=6144 addr=%p framebuffer=%p heap8Before=%u heap8After=%u largest8Before=%u largest8After=%u minHeap8=%u lifecycle=once-after-framebuffer\n",
                  EspRes60Preview_snapshotAddress(),(void*)framebuffer,
                  (unsigned)p17FreeBefore,
                  (unsigned)heap_caps_get_free_size(MALLOC_CAP_8BIT),
                  (unsigned)p17LargestBefore,
                  (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT),
                  (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT));
#if DOOMRPG_RES60_P18_LOCAL_SNAPSHOT
    Serial.println("[RES60P18] ALIAS source=P17-heap localPerFrame=1 allocationsPerRay=0 framebuffers=1");
#endif
#endif
    return true;
}

extern "C" int Esp32Orientation_isReady(void) {
    return platformDisplay != nullptr && framebuffer != nullptr &&
           platformDisplay->width() == DOOMRPG_PHYSICAL_WIDTH &&
           platformDisplay->height() == DOOMRPG_PHYSICAL_HEIGHT;
}

extern "C" uint8_t Esp32Orientation_get(void) {
    return currentOrientation;
}

extern "C" int Esp32Orientation_set(uint8_t orientation) {
    if (!Esp32Orientation_valid(orientation) || !Esp32Orientation_isReady()) {
        Serial.printf("[ORIENTATION] REJECT requested=%u current=%u ready=%d\n",
                      (unsigned)orientation, (unsigned)currentOrientation,
                      Esp32Orientation_isReady());
        return 0;
    }
    if (orientation == currentOrientation) return 1;
    const uint8_t before = currentOrientation;
    const uint8_t rotation = orientation == ESP32_ORIENTATION_INVERSE ? 3U : 1U;
    platformDisplay->setRotation(rotation);
    if (platformDisplay->width() != DOOMRPG_PHYSICAL_WIDTH ||
        platformDisplay->height() != DOOMRPG_PHYSICAL_HEIGHT) {
        platformDisplay->setRotation(before == ESP32_ORIENTATION_INVERSE ? 3U : 1U);
        applyCydInvertedGammaProfile();
        Serial.printf("[ORIENTATION] ROLLBACK invalid-dimensions requested=%u restored=%u\n",
                      (unsigned)orientation, (unsigned)before);
        return 0;
    }
    applyCydInvertedGammaProfile(); /* Hardware gamma/inversion remain unchanged. */
    currentOrientation = orientation; /* Input consults the same owner, never its own copy. */
    Serial.printf("[ORIENTATION] SET %s->%s rotation=%u physical=%dx%d framebuffer=untouched gamma=preserved\n",
                  before ? "INVERSE" : "NORMAL", orientation ? "INVERSE" : "NORMAL",
                  (unsigned)rotation, platformDisplay->width(), platformDisplay->height());
    return 1;
}

extern "C" int Esp32Backlight_isReady(void) {
    return backlightReady ? 1 : 0;
}

extern "C" uint8_t Esp32Backlight_percent(void) {
    return backlightPercent;
}

extern "C" int Esp32Backlight_setPercent(uint8_t percent) {
    if (!backlightReady ||
        (percent != 25U && percent != 50U &&
         percent != 75U && percent != 100U)) {
        return 0;
    }

    const uint32_t duty = ((uint32_t)percent * 255U + 50U) / 100U;
    ledcWrite(kBacklightChannel, duty);
    backlightPercent = percent;
    Serial.printf("[BACKLIGHT] SET percent=%u duty=%u/255 gpio=21 frame=untouched gameplayRng=untouched\n",
                  (unsigned int)percent, (unsigned int)duty);
    return 1;
}

uint16_t* PlatformVideo_framebuffer() {
    return framebuffer;
}

size_t PlatformVideo_framebufferSizeBytes() {
    return kFramebufferBytes;
}

void PlatformVideo_clear(uint16_t color) {
    if (framebuffer == nullptr) {
        return;
    }
    std::fill(framebuffer, framebuffer + kPixelCount, color);
}

/* The ILI9341 owns the unchanged wallpaper in GRAM. Never present the
 * entire shared framebuffer during LOADING: map/priming code may overwrite
 * it while the 4-MB PAK is copied to internal flash. Region pixels must be
 * fully opaque, freshly painted by the caller (currently the 128x18 bar).
 * No SD/PAK access, heap allocation, framebuffer copy or hidden full redraw. */
bool PlatformVideo_presentRegion(int logicalLeft, int logicalTop,
                                 int logicalWidth, int logicalHeight) {
    if (platformDisplay == nullptr || framebuffer == nullptr ||
        logicalLeft < 0 || logicalTop < 0 ||
        logicalWidth <= 0 || logicalHeight <= 0 ||
        logicalWidth > DOOMRPG_LOGICAL_WIDTH ||
        logicalHeight > DOOMRPG_LOGICAL_HEIGHT ||
        logicalLeft > DOOMRPG_LOGICAL_WIDTH - logicalWidth ||
        logicalTop > DOOMRPG_LOGICAL_HEIGHT - logicalHeight) return false;

    /* A regional owner can replace the patch independently of this lamp.
     * Forget the old baseline on overlap; never restore stale HUD pixels. */
    if (!EspNativeResidentGameplay_turnLampWorld() ||
        (logicalLeft < kLampLeft+kLampSize && logicalLeft+logicalWidth > kLampLeft &&
         logicalTop < kLampTop+kLampSize && logicalTop+logicalHeight > kLampTop)) {
        lampWorldFrame = false;
        EspTurnLamp_reset(&lampState);
    }
    uint16_t outputRow[DOOMRPG_PHYSICAL_WIDTH]; /* <=640B stack; no heap */
    const int scaledWidth = logicalWidth * DOOMRPG_INTEGER_SCALE;
    platformDisplay->startWrite();
    platformDisplay->setAddrWindow(logicalLeft * DOOMRPG_INTEGER_SCALE,
                                   logicalTop * DOOMRPG_INTEGER_SCALE,
                                   scaledWidth,
                                   logicalHeight * DOOMRPG_INTEGER_SCALE);
    for (int y = logicalTop; y < logicalTop + logicalHeight; ++y) {
        const uint16_t* source = framebuffer +
            y * DOOMRPG_LOGICAL_WIDTH + logicalLeft;
        for (int x = 0; x < logicalWidth; ++x) {
            const uint16_t color = source[x];
            const int scaledX = x * DOOMRPG_INTEGER_SCALE;
            outputRow[scaledX] = color;
            outputRow[scaledX + 1] = color;
        }
        for (int repeat = 0; repeat < DOOMRPG_INTEGER_SCALE; ++repeat)
            platformDisplay->pushPixels(outputRow, scaledWidth);
    }
    platformDisplay->endWrite();
    return true;
}

void PlatformVideo_serviceTurnLamp() {
    if (!lampWorldFrame || !EspNativeResidentGameplay_turnLampWorld()) {
        /* A modal/transition owns the next paint; never restore world HUD
         * over it. Its opaque presentation replaces the physical patch. */
        lampWorldFrame = false;
        EspTurnLamp_reset(&lampState);
        return;
    }
    const int change = EspTurnLamp_update(&lampState, millis(),
                         EspNativeResidentGameplay_turnLampBusy());
    if (change != 0) turnLampWrite(change > 0);
}

bool PlatformVideo_present() {
    if (platformDisplay == nullptr || framebuffer == nullptr) {
        return false;
    }
    lampWorldFrame = false;
    EspTurnLamp_replaced(&lampState);

#if !DOOMRPG_RES60_P1_DIAGNOSTIC
    /* Activated by the native MENU_MAIN touch owner without allocation.
     * The first present after root composition/heap/FNV validation consumes
     * this marker and chooses UAC-120 or the fail-closed classic fallback. */
    (void)EspUacMenuPreview_enterDefault();
    (void)EspUacMenuPreview_enterOptions();

    /* Experimental UAC owns the *physical* scanout only while MENU_MAIN is
     * active. Do not briefly display the old dashboard before every UAC
     * selection (flicker/two SPI frame writes). The proven normal presenter
     * below remains entirely unchanged for classic and gameplay. */
    if (EspUacMenuPreview_mode() != 0) {
        if (EspUacMenuPreview_draw(platformDisplay, framebuffer,
                                   DOOMRPG_LOGICAL_WIDTH)) return true;
        EspUacMenuPreview_reset(); /* on any SD error, full classic repaint */
    }

#endif /* UAC physical scanout is production 160x120 only */

    uint16_t outputRow[DOOMRPG_PHYSICAL_WIDTH];
#if DOOMRPG_LOG_LEVEL >= DOOMRPG_LOG_TRACE || DOOMRPG_RES60_P1_DIAGNOSTIC
    const uint32_t started = micros();
#endif

    platformDisplay->startWrite();
    platformDisplay->setAddrWindow(0, 0, DOOMRPG_PHYSICAL_WIDTH,
                                   DOOMRPG_PHYSICAL_HEIGHT);

#if DOOMRPG_RES60_P1_DIAGNOSTIC
    /* One 640-byte stack scanline, no full-size LCD staging framebuffer.
     * Both physical coordinates map to a native 192x144 source, not a
     * previously rendered 160x120 game world. */
    for (uint16_t py = 0; py < DOOMRPG_PHYSICAL_HEIGHT; ++py) {
        const uint16_t sy = EspVideoNearest_source(
            py, DOOMRPG_VIDEO_BUFFER_HEIGHT, DOOMRPG_PHYSICAL_HEIGHT);
        const uint16_t* source = framebuffer +
            (size_t)sy * DOOMRPG_VIDEO_BUFFER_WIDTH;
        for (uint16_t px = 0; px < DOOMRPG_PHYSICAL_WIDTH; ++px) {
            const uint16_t sx = EspVideoNearest_source(
                px, DOOMRPG_VIDEO_BUFFER_WIDTH, DOOMRPG_PHYSICAL_WIDTH);
            outputRow[px] = source[sx];
        }
        platformDisplay->pushPixels(outputRow, DOOMRPG_PHYSICAL_WIDTH);
    }
#else
    for (int sourceY = 0; sourceY < DOOMRPG_LOGICAL_HEIGHT; ++sourceY) {
        const uint16_t* source = framebuffer + sourceY * DOOMRPG_LOGICAL_WIDTH;
        for (int sourceX = 0; sourceX < DOOMRPG_LOGICAL_WIDTH; ++sourceX) {
            const uint16_t color = source[sourceX];
            const int outputX = sourceX * DOOMRPG_INTEGER_SCALE;
            outputRow[outputX] = color;
            outputRow[outputX + 1] = color;
        }

        for (int repeatY = 0; repeatY < DOOMRPG_INTEGER_SCALE; ++repeatY) {
            platformDisplay->pushPixels(outputRow, DOOMRPG_PHYSICAL_WIDTH);
        }
    }

#endif

    platformDisplay->endWrite();
    /* Record only pixels actually sent in this full frame. Repaint the
     * indicator after every world present without restarting its clock. */
    lampWorldFrame = EspNativeResidentGameplay_turnLampWorld() != 0;
    if (lampWorldFrame) {
        for (int y=0; y<kLampSize; ++y)
            for (int x=0; x<kLampSize; ++x)
                lampUnderlay[y*kLampSize+x] =
                    framebuffer[(kLampTop+y)*DOOMRPG_LOGICAL_WIDTH+kLampLeft+x];
    }
    PlatformVideo_serviceTurnLamp();
#if DOOMRPG_ESP32_TOUCH_HITBOX_OVERLAY
    drawDebugOverlay();
#endif

#if DOOMRPG_RES60_P1_DIAGNOSTIC
    Serial.printf("[RES60P1] PRESENT src=192x144 dst=320x240 method=floor-nearest lineBytes=%u us=%lu\n",
                  (unsigned int)sizeof(outputRow),
                  (unsigned long)(micros() - started));
#else
    DRPG_LOGT("[VIDEO] Present 160x120 -> 320x240 exact 2x raw RGB565: %lu us\n",
              micros() - started);
#endif
    return true;
}

#if DOOMRPG_ESP32_TOUCH_HITBOX_OVERLAY
void PlatformVideo_debugOverlayClear() {
    for (int i = 0; i < kDebugOverlayMaxZones; ++i) {
        debugOverlayZones[i] = DebugOverlayZone{};
    }
    debugOverlayZoneCount = 0;
    debugTouchValid = false;
}

void PlatformVideo_debugOverlaySetZone(int index,
                                       int16_t logicalLeft,
                                       int16_t logicalTop,
                                       int16_t logicalRight,
                                       int16_t logicalBottom) {
    if (index < 0 || index >= kDebugOverlayMaxZones ||
        logicalLeft < 0 || logicalTop < 0 ||
        logicalRight < logicalLeft || logicalBottom < logicalTop ||
        logicalRight >= DOOMRPG_LOGICAL_WIDTH ||
        logicalBottom >= DOOMRPG_LOGICAL_HEIGHT) {
        Serial.printf("[HITBOX] REFUSED zone=%d logical=x%d..%d y%d..%d\n",
                      index, logicalLeft, logicalRight,
                      logicalTop, logicalBottom);
        return;
    }

    debugOverlayZones[index].left = logicalLeft;
    debugOverlayZones[index].top = logicalTop;
    debugOverlayZones[index].right = logicalRight;
    debugOverlayZones[index].bottom = logicalBottom;
    debugOverlayZones[index].valid = true;
    debugOverlayZoneCount = std::max(debugOverlayZoneCount, index + 1);
}

void PlatformVideo_debugOverlayRefresh() {
    drawDebugOverlay();
}

void PlatformVideo_debugOverlayMarkTouch(int16_t physicalX,
                                         int16_t physicalY,
                                         uint16_t pressure,
                                         uint16_t rawX,
                                         uint16_t rawY) {
    debugTouchX = physicalX;
    debugTouchY = physicalY;
    debugTouchValid = true;

    Serial.printf("[HITBOX] TOUCH raw=%u,%u pressure=%u physical=%d,%d logical=%d,%d\n",
                  rawX, rawY, pressure,
                  physicalX, physicalY,
                  physicalX / DOOMRPG_INTEGER_SCALE,
                  physicalY / DOOMRPG_INTEGER_SCALE);
    drawDebugOverlay();
}
#endif

void PlatformVideo_showTestPattern() {
#if DOOMRPG_RES60_P1_DIAGNOSTIC
    if (framebuffer == nullptr || kFramebufferBytes != 55296U) {
        Serial.println("[RES60P1] FAIL framebuffer missing or wrong footprint");
        return;
    }
    /* Genuine 192x144 pixels: quadrants, 1px checker, 1px border, diagonals.
     * No SD/PAK, gameplay raster or secondary full-screen scratch. */
    const uint32_t started = micros();
    const uint16_t red = rgb565(255, 0, 0);
    const uint16_t green = rgb565(0, 255, 0);
    const uint16_t blue = rgb565(0, 0, 255);
    const uint16_t yellow = rgb565(255, 255, 0);
    const uint16_t white = rgb565(255, 255, 255);
    const uint16_t black = rgb565(0, 0, 0);
    for (int y = 0; y < DOOMRPG_VIDEO_BUFFER_HEIGHT; ++y) {
        for (int x = 0; x < DOOMRPG_VIDEO_BUFFER_WIDTH; ++x) {
            const bool upper = y < DOOMRPG_VIDEO_BUFFER_HEIGHT / 2;
            const bool left = x < DOOMRPG_VIDEO_BUFFER_WIDTH / 2;
            uint16_t color = upper ? (left ? red : green)
                                   : (left ? blue : yellow);
            if (x >= 48 && x < 144 && y >= 48 && y < 96) {
                color = ((x + y) & 1) ? white : black;
            }
            framebuffer[(size_t)y * DOOMRPG_VIDEO_BUFFER_WIDTH + x] = color;
        }
    }
    for (int x = 0; x < DOOMRPG_VIDEO_BUFFER_WIDTH; ++x) {
        setPixel(x, 0, white);
        setPixel(x, DOOMRPG_VIDEO_BUFFER_HEIGHT - 1, white);
    }
    for (int y = 0; y < DOOMRPG_VIDEO_BUFFER_HEIGHT; ++y) {
        setPixel(0, y, white);
        setPixel(DOOMRPG_VIDEO_BUFFER_WIDTH - 1, y, white);
    }
    drawLine(0, 0, DOOMRPG_VIDEO_BUFFER_WIDTH - 1,
             DOOMRPG_VIDEO_BUFFER_HEIGHT - 1, black);
    drawLine(DOOMRPG_VIDEO_BUFFER_WIDTH - 1, 0, 0,
             DOOMRPG_VIDEO_BUFFER_HEIGHT - 1, white);
    Serial.printf("[RES60P1] RASTER src=192x144 us=%lu\n",
                  (unsigned long)(micros() - started));
    if (!PlatformVideo_present()) {
        Serial.println("[RES60P1] FAIL present");
    } else {
        Serial.printf("[RES60P1] FRAME totalUs=%lu\n",
                      (unsigned long)(micros() - started));
    }
#else
    if (framebuffer == nullptr) {
        return;
    }

    const uint16_t black = rgb565(0, 0, 0);
    const uint16_t white = rgb565(255, 255, 255);
    const uint16_t red = rgb565(255, 0, 0);
    const uint16_t green = rgb565(0, 255, 0);
    const uint16_t blue = rgb565(0, 0, 255);
    const uint16_t yellow = rgb565(255, 255, 0);
    const uint16_t cyan = rgb565(0, 255, 255);
    const uint16_t magenta = rgb565(255, 0, 255);

    PlatformVideo_clear(black);

    // Four equal 80x60 logical quadrants must become exact 160x120 physical
    // rectangles. This makes any non-integer scaling immediately visible.
    fillRect(1, 1, 79, 59, red);
    fillRect(80, 1, 79, 59, green);
    fillRect(1, 60, 79, 59, blue);
    fillRect(80, 60, 79, 59, yellow);

    // One logical pixel becomes exactly 2x2 physical pixels.
    for (int x = 0; x < DOOMRPG_LOGICAL_WIDTH; ++x) {
        setPixel(x, 0, white);
        setPixel(x, DOOMRPG_LOGICAL_HEIGHT - 1, white);
    }
    for (int y = 0; y < DOOMRPG_LOGICAL_HEIGHT; ++y) {
        setPixel(0, y, white);
        setPixel(DOOMRPG_LOGICAL_WIDTH - 1, y, white);
    }

    drawLine(0, 0, DOOMRPG_LOGICAL_WIDTH - 1,
             DOOMRPG_LOGICAL_HEIGHT - 1, cyan);
    drawLine(DOOMRPG_LOGICAL_WIDTH - 1, 0, 0,
             DOOMRPG_LOGICAL_HEIGHT - 1, magenta);

    // 10x10 logical checker blocks become 20x20 physical squares.
    for (int blockY = 0; blockY < 4; ++blockY) {
        for (int blockX = 0; blockX < 6; ++blockX) {
            const uint16_t color = ((blockX + blockY) & 1) ? white : black;
            fillRect(50 + blockX * 10, 40 + blockY * 10, 10, 10, color);
        }
    }

    PlatformVideo_present();
#endif
}

#if DOOMRPG_RES60_P2_LAYOUT
void PlatformVideo_showRes60LayoutPattern() {
    if (framebuffer == nullptr ||
        kFramebufferBytes != ESP_RES60_FRAMEBUFFER_BYTES) {
        Serial.println("[RES60P2] FAIL CLOSED framebuffer missing or wrong footprint");
        return;
    }
    /* Pure native 192x144 output: top 24px HUD, 96px 3D viewport REGION,
     * bottom 24px HUD. Diagnostic pixels only, NO BSP/PAK/world renderer. */
    const uint32_t started = micros();
    if (!EspRes60Layout_paintDiagnostic(framebuffer, kFramebufferBytes)) {
        Serial.println("[RES60P2] FAIL CLOSED layout raster rejected");
        return;
    }
    const uint32_t rasterUs = micros() - started;
    const uint32_t frameFNV = EspRes60Layout_fnv(framebuffer, kFramebufferBytes);
    Serial.printf("[RES60P2] RASTER src=192x144 top=24 world=192x96@y24 bottom=24 framebufferBytes=%u fnv=%08lx rasterUs=%lu\n",
                  (unsigned int)kFramebufferBytes, (unsigned long)frameFNV,
                  (unsigned long)rasterUs);
    if (!PlatformVideo_present()) {
        Serial.println("[RES60P2] FAIL CLOSED present");
        return;
    }
    Serial.printf("[RES60P2] FRAME complete totalUs=%lu world3d=NOT_STARTED hudArt=NOT_STARTED map=NOT_LOADED audio=NOT_STARTED\n",
                  (unsigned long)(micros() - started));
}
#endif

#if DOOMRPG_RES60_P3_WORLD
void PlatformVideo_showRes60WorldPreview() {
    if (framebuffer == nullptr ||
        kFramebufferBytes != ESP_RES60_FRAMEBUFFER_BYTES) {
        Serial.println("[RES60P3] FAIL CLOSED stage=framebuffer");
        return;
    }
    EspRes60WorldProbeReport report{};
    const uint32_t started = micros();
    if (!EspRes60WorldProbe_renderIntro(framebuffer, kFramebufferBytes,
                                        &report)) {
        Serial.printf("[RES60P3] FAIL CLOSED stage=compact-map-projection heap8=%u largest8=%u\n",
                      (unsigned int)heap_caps_get_free_size(MALLOC_CAP_8BIT),
                      (unsigned int)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
        return;
    }
    const uint32_t rasterUs = micros() - started;
    Serial.printf("[RES60P3] RASTER source=MAP_INTRO width=192 world=96 hudTop=24 hudBottom=24 lines=%lu rayTests=%lu intersections=%lu hitColumns=%lu minDepth=%lu maxDepth=%lu centerDepth=%lu centerLine=%lu fullHeightColumns=%lu wallPixels=%lu fnv=%08lx us=%lu render=FLAT_WALLS textureSamples=0 sprites=0 engine=NOT_STARTED\n",
                  (unsigned long)report.lineCount,
                  (unsigned long)report.raster.rayTests,
                  (unsigned long)report.raster.intersections,
                  (unsigned long)report.raster.visibleColumns,
                  (unsigned long)report.raster.minDepth,
                  (unsigned long)report.raster.maxDepth,
                  (unsigned long)report.raster.centerDepth,
                  (unsigned long)report.raster.centerLineIndex,
                  (unsigned long)report.raster.fullHeightColumns,
                  (unsigned long)report.raster.wallPixels,
                  (unsigned long)report.raster.frameFNV,
                  (unsigned long)rasterUs);
    if (!PlatformVideo_present()) {
        Serial.println("[RES60P3] FAIL CLOSED stage=LCD_PRESENT");
        return;
    }
    Serial.printf("[RES60P3] FRAME totalUs=%lu heap8=%u largest8=%u minHeap8=%u source=PAK mapRuntime=RELEASED audio=NOT_STARTED legacyWorld=NOT_STARTED\n",
                  (unsigned long)(micros() - started),
                  (unsigned int)heap_caps_get_free_size(MALLOC_CAP_8BIT),
                  (unsigned int)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT),
                  (unsigned int)heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT));
}
#endif

#if DOOMRPG_RES60_P7_NAV
bool PlatformVideo_showRes60WorldPreviewQuarterTurns(uint8_t turns) {
    if (framebuffer == nullptr ||
        kFramebufferBytes != ESP_RES60_FRAMEBUFFER_BYTES || turns > 3U) {
        Serial.println("[RES60P7] FAIL CLOSED stage=framebuffer-or-angle");
        return false;
    }
    EspRes60WorldProbeReport report{};
    const uint32_t started=micros();
    if (!EspRes60WorldProbe_renderIntroQuarterTurns(framebuffer,
            kFramebufferBytes, turns, &report)) {
#if DOOMRPG_RES60_P12_INVENTORY_CACHE
        EspRes60InventoryCache_reset();
#endif
#if DOOMRPG_RES60_P13_MAP_LEASE
        EspRes60MapLease_reset();
#endif
        Serial.println("[RES60P7] FAIL CLOSED stage=compact-map-projection");
        return false;
    }
    /* P3 raw FNV is captured BEFORE P8's texture inset and P7 navigation.
     * It remains exact ed9b68c4 at spawn on the unchanged PAK. */
    if (!EspRes60Nav_paint(framebuffer, kFramebufferBytes,
                           (uint8_t)(report.spawnAngle - turns * 64U),
                           turns) || EspAssetPack_isOpen() ||
#if DOOMRPG_RES60_P13_MAP_LEASE
        !EspRes60MapLease_isReady()) {
#else
        EspMapRuntime_isLoaded()) {
#endif
#if DOOMRPG_RES60_P12_INVENTORY_CACHE
        EspRes60InventoryCache_reset();
#endif
#if DOOMRPG_RES60_P13_MAP_LEASE
        EspRes60MapLease_reset();
#endif
        Serial.println("[RES60P7] FAIL CLOSED stage=navigation-or-leases");
        return false;
    }
    const uint32_t fnv=EspRes60Layout_fnv(framebuffer,kFramebufferBytes);
#if DOOMRPG_RES60_P11_PROFILE
    /* Capture before any Serial logger; P11 is observational, and itself
     * adds micros() overhead. Do NOT compare absolute P10/P11 blindly. */
    const uint32_t preparedUs=micros()-started;
#endif
#if DOOMRPG_RES60_P10_WALL_MATERIAL
    const unsigned long wallTextureSamples=(unsigned long)report.texture.wallPixels;
#else
    const unsigned long wallTextureSamples=0UL;
#endif
#if DOOMRPG_RES60_P20_COMPACT_SERIAL
    /* P20: keep all cold evidence and all failure logs unchanged. A fully
     * cached rotation gets ONE short witness instead of seven P8-P18
     * paragraphs + RASTER. Serial at 115200 can block the input loop.
     * P11 preparedUs was captured before logging in both paths. */
    if (EspRes60Serial_compactWarm(
            (uint8_t)report.mapLeaseHit,
            (uint8_t)report.texture.cacheHit)) {
        Serial.printf("[RES60P20] WARM angle=%u turns=%u mapUs=%lu raysUs=%lu preparedUs=%lu tests=%lu rawP3FNV=%08lx fnv=%08lx p10FNV=%08lx swatchFNV=%08lx heap8=%u largest8=%u minHeap8=%u"
#if DOOMRPG_RES60_P37_WALL_COVERAGE
            " wallIds=%u wallCols=%u wallTotal=%u insetWall=%u unsupportedCols=%u textured=%lu"
#endif
#if DOOMRPG_RES60_P38_WALL_ADMISSION
            " admitted=%u"
#endif
#if DOOMRPG_RES60_P39_WALL_STREAM
            " stream=%u/%lu meta=%u streamFNV=%08lx"
#endif
#if DOOMRPG_RES60_P40_FULL_WALLS
            " fullWalls=1 swatch=OFF"
#endif
            "\n",
            (unsigned)report.spawnAngle, (unsigned)turns,
            (unsigned long)report.loadUs,
            (unsigned long)report.raster.raysUs,
            (unsigned long)preparedUs,
            (unsigned long)report.raster.rayTests,
            (unsigned long)report.raster.frameFNV,
            (unsigned long)fnv,
            (unsigned long)report.texture.wallFNV,
            (unsigned long)report.texture.sampleFNV,
            (unsigned)heap_caps_get_free_size(MALLOC_CAP_8BIT),
            (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT),
            (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT)
#if DOOMRPG_RES60_P37_WALL_COVERAGE
            , (unsigned)report.wallDemand.materials,
            (unsigned)report.wallDemand.columns,
            (unsigned)report.wallDemand.pixels,
            (unsigned)report.wallDemand.insetPixels,
            (unsigned)report.wallDemand.unsupportedColumns,
            wallTextureSamples
#endif
#if DOOMRPG_RES60_P38_WALL_ADMISSION
            , (unsigned)report.texture.admittedWallIds
#endif
#if DOOMRPG_RES60_P39_WALL_STREAM
            , (unsigned)report.texture.streamReads,
            (unsigned long)report.texture.streamBytes,
            (unsigned)report.texture.streamMetadataBytes,
            (unsigned long)report.texture.streamFNV
#endif
            );
    } else
#endif
    {
#if DOOMRPG_RES60_P40_FULL_WALLS
    Serial.printf("[RES60P40] FULL-WALLS source=PAK cache=%s catalog=%u catalogFNV=%08lx reads=%u bytes=%lu meta=%u columns=%lu pixels=%lu streamFNV=%08lx swatch=OFF tileBss=0\n",
        report.texture.cacheHit ? "HIT" : "MISS",
        (unsigned)report.texture.catalogCount,
        (unsigned long)report.texture.catalogFNV,
        (unsigned)report.texture.streamReads,
        (unsigned long)report.texture.readBytes,
        (unsigned)report.texture.streamMetadataBytes,
        (unsigned long)report.texture.wallColumns,
        wallTextureSamples,
        (unsigned long)report.texture.streamFNV);
#elif DOOMRPG_RES60_P8_TEXEL
    Serial.printf("[RES60P8] SWATCH source=PAK map=/intro.bsp file=wtexels.bin cache=%s logical=%u catalog=%u sourceNibble=%lu catalogFNV=%08lx texelFNV=%08lx sampleFNV=%08lx bytes=%lu inset=48x48 pixels=%lu framebuffer=192x144 worldTextureSamples=%lu\n",
        report.texture.cacheHit ? "HIT" : "MISS",
        (unsigned)report.texture.logicalId,
        (unsigned)report.texture.catalogCount,
        (unsigned long)report.texture.sourceOffsetNibbles,
        (unsigned long)report.texture.catalogFNV,
        (unsigned long)report.texture.texelFNV,
        (unsigned long)report.texture.sampleFNV,
        (unsigned long)report.texture.readBytes,
        (unsigned long)report.texture.pixels,
        wallTextureSamples);
#endif
#if DOOMRPG_RES60_P15_COMPACT_SNAPSHOT
#if DOOMRPG_RES60_P17_HEAP_SNAPSHOT
    Serial.printf("[RES60P17] COMPACT lines=%lu candidates=%lu snapshotBss=0 snapshotHeap=6144 indexBss=0 P14CombinedBss=9216 savedBss=9216 readerCalls=%lu raysUs=%lu heap8=%u largest8=%u minHeap8=%u owner=P13-immutable\n",
#else
    Serial.printf("[RES60P15] COMPACT lines=%lu candidates=%lu snapshotBss=6144 indexBss=0 P14CombinedBss=9216 savedBss=3072 readerCalls=%lu raysUs=%lu heap8=%u largest8=%u minHeap8=%u owner=P13-immutable\n",
#endif
        (unsigned long)report.lineCount,
        (unsigned long)report.raster.wedgeCandidates,
        (unsigned long)report.raster.readerCalls,
        (unsigned long)report.raster.raysUs,
        (unsigned)heap_caps_get_free_size(MALLOC_CAP_8BIT),
        (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT),
        (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT));
#elif DOOMRPG_RES60_P14_LINE_SNAPSHOT
    Serial.printf("[RES60P14] SNAPSHOT lines=%lu candidates=%lu bytesBss=%lu readerCalls=%lu avoidedDecodes=%lu raysUs=%lu owner=P13-immutable\n",
        (unsigned long)report.lineCount,
        (unsigned long)report.raster.wedgeCandidates,
        (unsigned long)(512U*sizeof(EspRes60PreviewLine)),
        (unsigned long)report.raster.readerCalls,
        (unsigned long)report.raster.rayTests,
        (unsigned long)report.raster.raysUs);
#endif
#if DOOMRPG_RES60_P9_WEDGE
    Serial.printf("[RES60P9] WEDGE lines=%lu candidates=%lu culled=%lu rayTests=%lu legacyRayTests=%lu rawP3FNV=%08lx\n",
        (unsigned long)report.lineCount,
        (unsigned long)report.raster.wedgeCandidates,
        (unsigned long)report.raster.wedgeRejected,
        (unsigned long)report.raster.rayTests,
        (unsigned long)(report.lineCount*ESP_RES60_WIDTH),
        (unsigned long)report.raster.frameFNV);
#endif
#if DOOMRPG_RES60_P10_WALL_MATERIAL && !DOOMRPG_RES60_P40_FULL_WALLS
    Serial.printf("[RES60P10] MATERIAL source=PAK logical=%u selected=%s visibleColumns=%u columns=%lu pixels=%lu sampleFNV=%08lx rawP3FNV=%08lx"
#if DOOMRPG_RES60_P39_WALL_STREAM
        " extraPakReads=%u"
#else
        " extraPakReads=0"
#endif
#if DOOMRPG_RES60_P37_WALL_COVERAGE
        " wallIds=%u wallCols=%u wallTotal=%u insetWall=%u unsupportedCols=%u"
#endif
#if DOOMRPG_RES60_P38_WALL_ADMISSION
        " admitted=%u"
#endif
#if DOOMRPG_RES60_P39_WALL_STREAM
        " streamBytes=%lu meta=%u streamFNV=%08lx"
#endif
        "\n",
        (unsigned)report.texture.logicalId,
        report.texture.selectedFromVisible ? "VISIBLE-RAY" : "FALLBACK-LINE",
        (unsigned)report.texture.visibleWallColumns,
        (unsigned long)report.texture.wallColumns,
        (unsigned long)report.texture.wallPixels,
        (unsigned long)report.texture.wallFNV,
        (unsigned long)report.raster.frameFNV
#if DOOMRPG_RES60_P39_WALL_STREAM
        , (unsigned)report.texture.streamReads
#endif
#if DOOMRPG_RES60_P37_WALL_COVERAGE
        , (unsigned)report.wallDemand.materials,
        (unsigned)report.wallDemand.columns,
        (unsigned)report.wallDemand.pixels,
        (unsigned)report.wallDemand.insetPixels,
        (unsigned)report.wallDemand.unsupportedColumns
#endif
#if DOOMRPG_RES60_P38_WALL_ADMISSION
        , (unsigned)report.texture.admittedWallIds
#endif
#if DOOMRPG_RES60_P39_WALL_STREAM
        , (unsigned long)report.texture.streamBytes,
        (unsigned)report.texture.streamMetadataBytes,
        (unsigned long)report.texture.streamFNV
#endif
        );
#endif
#if DOOMRPG_RES60_P12_INVENTORY_CACHE
    Serial.printf("[RES60P12] INVENTORY cache=%s crc=%08lx arenaFNV=%08lx invUs=%lu validated=pak-entry+arena\n",
        report.inventoryCacheHit ? "HIT" : "MISS",
        (unsigned long)report.mapCrc32,
        (unsigned long)report.arenaFNV,
        (unsigned long)report.inventoryUs);
#endif
#if DOOMRPG_RES60_P13_MAP_LEASE
    Serial.printf("[RES60P13] MAP lease=%s crc=%08lx arenaFNV=%08lx mapUs=%lu bytesResident=%lu validated=pak-metadata+resident-arena\n",
        report.mapLeaseHit ? "HIT" : "MISS",
        (unsigned long)report.mapCrc32,
        (unsigned long)report.arenaFNV,
        (unsigned long)report.loadUs,
        (unsigned long)report.arenaBytes);
#endif
#if DOOMRPG_RES60_P11_PROFILE
    Serial.printf("[RES60P11] PROFILE angle=%u cache=%s invUs=%lu loadUs=%lu preflightUs=%lu bandUs=%lu raysUs=%lu geometryUs=%lu fillUs=%lu hashUs=%lu p3Us=%lu catalogUs=%lu texPakUs=%lu swatchUs=%lu overlayUs=%lu texUs=%lu releaseUs=%lu preparedUs=%lu rayTests=%lu overlayPixels=%lu heap8=%u instrumentation=ON\n",
        (unsigned)report.spawnAngle,
        report.texture.cacheHit ? "HIT" : "MISS",
        (unsigned long)report.inventoryUs,
        (unsigned long)report.loadUs,
        (unsigned long)report.raster.preflightUs,
        (unsigned long)report.raster.bandUs,
        (unsigned long)report.raster.raysUs,
        (unsigned long)report.raster.geometryUs,
        (unsigned long)report.raster.fillUs,
        (unsigned long)report.raster.hashUs,
        (unsigned long)report.p3Us,
        (unsigned long)report.texture.profileCatalogUs,
        (unsigned long)report.texture.profilePackUs,
        (unsigned long)report.texture.profileSwatchUs,
        (unsigned long)report.texture.profileOverlayUs,
        (unsigned long)report.textureUs,
        (unsigned long)report.releaseUs,
        (unsigned long)preparedUs,
        (unsigned long)report.raster.rayTests,
        (unsigned long)report.texture.wallPixels,
        (unsigned)heap_caps_get_free_size(MALLOC_CAP_8BIT));
#endif
    Serial.printf("[RES60P7] RASTER source=MAP_INTRO angle=%u turns=%u width=192 world=96 rayTests=%lu hitColumns=%lu wallPixels=%lu rawP3FNV=%08lx fnv=%08lx us=%lu textureSamples=%lu sprites=0 gameplay=NOT_STARTED\n",
        (unsigned)report.spawnAngle,(unsigned)turns,
        (unsigned long)report.raster.rayTests,
        (unsigned long)report.raster.visibleColumns,
        (unsigned long)report.raster.wallPixels,
        (unsigned long)report.raster.frameFNV,(unsigned long)fnv,
        (unsigned long)(micros()-started),
        wallTextureSamples);
    }
    if (!PlatformVideo_present()) {
#if DOOMRPG_RES60_P12_INVENTORY_CACHE
        EspRes60InventoryCache_reset();
#endif
#if DOOMRPG_RES60_P13_MAP_LEASE
        EspRes60MapLease_reset();
#endif
        Serial.println("[RES60P7] FAIL CLOSED stage=LCD_PRESENT");
        return false;
    }
#if DOOMRPG_RES60_P13_MAP_LEASE
    Serial.printf("[RES60P7] FRAME totalUs=%lu heap8=%u largest8=%u minHeap8=%u mapRuntime=RETAINED pak=closed audio=NOT_STARTED\n",
#else
    Serial.printf("[RES60P7] FRAME totalUs=%lu heap8=%u largest8=%u minHeap8=%u mapRuntime=RELEASED pak=closed audio=NOT_STARTED\n",
#endif
         (unsigned long)(micros()-started),
         (unsigned)heap_caps_get_free_size(MALLOC_CAP_8BIT),
         (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT),
         (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT));
#if DOOMRPG_RES60_P16_HEAP_TOPOLOGY
    /* Only once, after successful LCD presentation + full profile logging.
     * Any Serial perturbation cannot contaminate raysUs/preparedUs values. */
    if (!res60P16WarmDumped && report.mapLeaseHit) {
        res60P16WarmDumped=1U;
        res60P16_printHeapTopology("first-map-hit");
    }
#endif
    return true;
}
#endif

#if DOOMRPG_RES60_P6_MENU
bool PlatformVideo_showRes60MainMenu(int8_t selected) {
    if (framebuffer == nullptr || kFramebufferBytes != ESP_RES60_FRAMEBUFFER_BYTES) {
        Serial.println("[RES60P6] FAIL CLOSED stage=framebuffer");
        return false;
    }
    EspRes60MenuReport report{};
    const uint32_t started = micros();
    if (!EspRes60Menu_render(framebuffer, kFramebufferBytes, selected, &report)) {
        Serial.println("[RES60P6] FAIL CLOSED stage=menu-PAK-or-skin");
        return false;
    }
    Serial.printf("[RES60P6] MENU source=PAK bg=c.bmp/%ux%u logo=j.bmp/%ux%u cards=4 selected=%d native=192x144 titleFNV=%08lx fnv=%08lx reads=%lu rasterUs=%lu gameplay=NOT_STARTED\n",
       (unsigned)report.title.backgroundWidth, (unsigned)report.title.backgroundHeight,
       (unsigned)report.title.logoWidth, (unsigned)report.title.logoHeight,
       (int)selected, (unsigned long)report.title.frameFNV,
       (unsigned long)report.finalFNV, (unsigned long)report.title.packReads,
       (unsigned long)(micros()-started));
    if (!PlatformVideo_present()) {
        Serial.println("[RES60P6] FAIL CLOSED stage=LCD_PRESENT");
        return false;
    }
    Serial.printf("[RES60P6] FRAME totalUs=%lu heap8=%u largest8=%u minHeap8=%u pak=closed audio=NOT_STARTED\n",
       (unsigned long)(micros()-started),
       (unsigned)heap_caps_get_free_size(MALLOC_CAP_8BIT),
       (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT),
       (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT));
    return true;
}
bool PlatformVideo_updateRes60MenuArmed(int8_t previous, int8_t selected) {
    if (framebuffer == nullptr || kFramebufferBytes != ESP_RES60_FRAMEBUFFER_BYTES ||
        !EspRes60Menu_repaintSelection(framebuffer, kFramebufferBytes,
                                      previous, selected))
        return false;
    const uint32_t fnv = EspRes60Layout_fnv(framebuffer, kFramebufferBytes);
    if (!PlatformVideo_present()) {
        (void)EspRes60Menu_repaintSelection(framebuffer, kFramebufferBytes,
                                            selected, previous);
        return false;
    }
    Serial.printf("[RES60P6] SELECT before=%d after=%d fnv=%08lx pak=closed\n",
       (int)previous, (int)selected, (unsigned long)fnv);
    return true;
}
#endif

#if DOOMRPG_RES60_P5_TITLE
bool PlatformVideo_showRes60TitlePreview() {
    if (framebuffer == nullptr || kFramebufferBytes != ESP_RES60_FRAMEBUFFER_BYTES) {
        Serial.println("[RES60P5] FAIL CLOSED stage=framebuffer");
        return false;
    }
    EspRes60TitlePreviewReport report{};
    const uint32_t started = micros();
    if (!EspRes60TitlePreview_render(framebuffer, kFramebufferBytes, &report)) {
        Serial.println("[RES60P5] FAIL CLOSED stage=original-PAK-title");
        return false;
    }
    Serial.printf("[RES60P5] TITLE source=PAK bg=c.bmp/%ux%u logo=j.bmp/%ux%u native=192x144 fnv=%08lx reads=%lu bytes=%lu pixels=%lu rasterUs=%lu gameplay=NOT_STARTED\n",
        (unsigned)report.backgroundWidth, (unsigned)report.backgroundHeight,
        (unsigned)report.logoWidth, (unsigned)report.logoHeight,
        (unsigned long)report.frameFNV, (unsigned long)report.packReads,
        (unsigned long)report.bytesRead, (unsigned long)report.pixelsWritten,
        (unsigned long)(micros()-started));
    if (!PlatformVideo_present()) {
        Serial.println("[RES60P5] FAIL CLOSED stage=present");
        return false;
    }
    Serial.printf("[RES60P5] FRAME totalUs=%lu heap8=%u largest8=%u minHeap8=%u pak=closed audio=NOT_STARTED\n",
        (unsigned long)(micros()-started),
        (unsigned)heap_caps_get_free_size(MALLOC_CAP_8BIT),
        (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT),
        (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT));
    return true;
}
#endif
