#include "platform_video.h"

#include <Arduino.h>
#include <TFT_eSPI.h>

#include <algorithm>

#include "platform_video_config.h"

namespace {

TFT_eSPI* platformDisplay = nullptr;
uint16_t* framebuffer = nullptr;

constexpr size_t kPixelCount =
    static_cast<size_t>(DOOMRPG_LOGICAL_WIDTH) * DOOMRPG_LOGICAL_HEIGHT;
constexpr size_t kFramebufferBytes = kPixelCount * sizeof(uint16_t);

/* Hardware-selected CYD display profile.
 *
 * The four-way hardware comparison showed that the neutral gamma with a modest
 * 1.15 saturation boost gives the best visual match on the real ILI9341 panel.
 * Keep the logical RGB565 framebuffer untouched so every engine/rendering FNV
 * remains a source-of-truth hash; this transform exists only at the final TFT
 * presentation boundary.
 */
constexpr int kDisplaySaturationPercent = 115;

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

uint8_t clamp8(int value) {
    if (value < 0) return 0;
    if (value > 255) return 255;
    return static_cast<uint8_t>(value);
}

uint16_t tuneDisplayColor565(uint16_t color) {
    /* Expand RGB565 to 8-bit using bit replication (no divisions), preserve
     * BT.601-style luma, and scale only chroma by 1.15. Neutral grays and
     * black/white therefore remain neutral while Doom's colored artwork gains
     * the contrast observed in hardware comparison variant C.
     */
    const int red5 = (color >> 11) & 0x1f;
    const int green6 = (color >> 5) & 0x3f;
    const int blue5 = color & 0x1f;
    int red = (red5 << 3) | (red5 >> 2);
    int green = (green6 << 2) | (green6 >> 4);
    int blue = (blue5 << 3) | (blue5 >> 2);

    const int luma = (77 * red + 150 * green + 29 * blue + 128) >> 8;
    red = clamp8(luma + ((red - luma) * kDisplaySaturationPercent) / 100);
    green = clamp8(luma + ((green - luma) * kDisplaySaturationPercent) / 100);
    blue = clamp8(luma + ((blue - luma) * kDisplaySaturationPercent) / 100);

    return rgb565(static_cast<uint8_t>(red),
                  static_cast<uint8_t>(green),
                  static_cast<uint8_t>(blue));
}

void setPixel(int x, int y, uint16_t color) {
    if (framebuffer == nullptr || x < 0 || y < 0 ||
        x >= DOOMRPG_LOGICAL_WIDTH || y >= DOOMRPG_LOGICAL_HEIGHT) {
        return;
    }
    framebuffer[y * DOOMRPG_LOGICAL_WIDTH + x] = color;
}

void fillRect(int x, int y, int width, int height, uint16_t color) {
    if (framebuffer == nullptr || width <= 0 || height <= 0) {
        return;
    }

    const int x0 = std::max(0, x);
    const int y0 = std::max(0, y);
    const int x1 = std::min(DOOMRPG_LOGICAL_WIDTH, x + width);
    const int y1 = std::min(DOOMRPG_LOGICAL_HEIGHT, y + height);

    for (int row = y0; row < y1; ++row) {
        std::fill(framebuffer + row * DOOMRPG_LOGICAL_WIDTH + x0,
                  framebuffer + row * DOOMRPG_LOGICAL_WIDTH + x1, color);
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

    if (framebuffer == nullptr) {
        framebuffer = static_cast<uint16_t*>(calloc(kPixelCount, sizeof(uint16_t)));
        if (framebuffer == nullptr) {
            Serial.printf("[VIDEO] Unable to allocate %u-byte framebuffer\n",
                          static_cast<unsigned int>(kFramebufferBytes));
            return false;
        }
    }

    Serial.printf("[VIDEO] Logical framebuffer %dx%d RGB565, %u bytes\n",
                  DOOMRPG_LOGICAL_WIDTH, DOOMRPG_LOGICAL_HEIGHT,
                  static_cast<unsigned int>(kFramebufferBytes));
    Serial.printf("[VIDEO] Physical output %dx%d, integer scale %dx\n",
                  DOOMRPG_PHYSICAL_WIDTH, DOOMRPG_PHYSICAL_HEIGHT,
                  DOOMRPG_INTEGER_SCALE);
    Serial.printf("[VIDEO] CYD display profile gamma=1.00 saturation=%d%% resampling=nearest framebuffer=untouched\n",
                  kDisplaySaturationPercent);
#if DOOMRPG_ESP32_TOUCH_HITBOX_OVERLAY
    Serial.println("[HITBOX] Physical overlay enabled; framebuffer hashes remain untouched");
#endif
    return true;
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

bool PlatformVideo_present() {
    if (platformDisplay == nullptr || framebuffer == nullptr) {
        return false;
    }

    uint16_t outputRow[DOOMRPG_PHYSICAL_WIDTH];
    const uint32_t started = micros();

    platformDisplay->startWrite();
    platformDisplay->setAddrWindow(0, 0, DOOMRPG_PHYSICAL_WIDTH,
                                   DOOMRPG_PHYSICAL_HEIGHT);

    for (int sourceY = 0; sourceY < DOOMRPG_LOGICAL_HEIGHT; ++sourceY) {
        const uint16_t* source = framebuffer + sourceY * DOOMRPG_LOGICAL_WIDTH;
        for (int sourceX = 0; sourceX < DOOMRPG_LOGICAL_WIDTH; ++sourceX) {
            /* Apply the panel profile once per logical pixel, then duplicate the
             * corrected RGB565 value for exact nearest-neighbour 2x output. */
            const uint16_t color = tuneDisplayColor565(source[sourceX]);
            const int outputX = sourceX * DOOMRPG_INTEGER_SCALE;
            outputRow[outputX] = color;
            outputRow[outputX + 1] = color;
        }

        for (int repeatY = 0; repeatY < DOOMRPG_INTEGER_SCALE; ++repeatY) {
            platformDisplay->pushPixels(outputRow, DOOMRPG_PHYSICAL_WIDTH);
        }
    }

    platformDisplay->endWrite();
#if DOOMRPG_ESP32_TOUCH_HITBOX_OVERLAY
    drawDebugOverlay();
#endif

    Serial.printf("[VIDEO] Present 160x120 -> 320x240 exact 2x + sat1.15: %lu us\n",
                  micros() - started);
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
}
