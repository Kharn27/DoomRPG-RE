#include "platform_video.h"

#include <Arduino.h>
#include <TFT_eSPI.h>

#include <algorithm>
#if DOOMRPG_ESP32_COLOR_COMPARE
#include <math.h>
#endif

#include "platform_video_config.h"

#ifndef DOOMRPG_ESP32_COLOR_COMPARE
#define DOOMRPG_ESP32_COLOR_COMPARE 0
#endif

namespace {

TFT_eSPI* platformDisplay = nullptr;
uint16_t* framebuffer = nullptr;

constexpr size_t kPixelCount =
    static_cast<size_t>(DOOMRPG_LOGICAL_WIDTH) * DOOMRPG_LOGICAL_HEIGHT;
constexpr size_t kFramebufferBytes = kPixelCount * sizeof(uint16_t);

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

#if DOOMRPG_ESP32_COLOR_COMPARE
uint8_t gamma130Lut[256]{};
bool gamma130LutReady = false;
#endif

uint16_t rgb565(uint8_t red, uint8_t green, uint8_t blue) {
    return static_cast<uint16_t>(((red & 0xf8) << 8) |
                                 ((green & 0xfc) << 3) | (blue >> 3));
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

#if DOOMRPG_ESP32_COLOR_COMPARE
uint8_t clamp8(int value) {
    if (value < 0) return 0;
    if (value > 255) return 255;
    return static_cast<uint8_t>(value);
}

void prepareGamma130Lut() {
    if (gamma130LutReady) {
        return;
    }

    constexpr float gamma = 1.30f;
    constexpr float exponent = 1.0f / gamma;
    for (int value = 0; value < 256; ++value) {
        const float normalized = static_cast<float>(value) / 255.0f;
        const int corrected = static_cast<int>(
            powf(normalized, exponent) * 255.0f + 0.5f);
        gamma130Lut[value] = clamp8(corrected);
    }
    gamma130LutReady = true;
}

uint16_t tuneColor565(uint16_t color, bool applyGamma, bool applySaturation) {
    int red = ((color >> 11) & 0x1f) * 255 / 31;
    int green = ((color >> 5) & 0x3f) * 255 / 63;
    int blue = (color & 0x1f) * 255 / 31;

    if (applyGamma) {
        red = gamma130Lut[red];
        green = gamma130Lut[green];
        blue = gamma130Lut[blue];
    }

    if (applySaturation) {
        /* BT.601-style integer luma. Saturation 1.15 scales only chroma around
         * that luma, preserving neutral grays and black/white endpoints. */
        const int luma = (77 * red + 150 * green + 29 * blue + 128) >> 8;
        red = clamp8(luma + ((red - luma) * 115) / 100);
        green = clamp8(luma + ((green - luma) * 115) / 100);
        blue = clamp8(luma + ((blue - luma) * 115) / 100);
    }

    return rgb565(static_cast<uint8_t>(red),
                  static_cast<uint8_t>(green),
                  static_cast<uint8_t>(blue));
}

bool presentColorComparison() {
    prepareGamma130Lut();

    uint16_t outputRow[DOOMRPG_PHYSICAL_WIDTH];
    const uint32_t started = micros();

    platformDisplay->startWrite();
    platformDisplay->setAddrWindow(0, 0, DOOMRPG_PHYSICAL_WIDTH,
                                   DOOMRPG_PHYSICAL_HEIGHT);

    /* The physical 320x240 panel fits four exact 160x120 copies of the logical
     * framebuffer. No resize, interpolation or second framebuffer is involved.
     *
     *   A top-left     : gamma 1.00 / saturation 1.00
     *   B top-right    : gamma 1.30 / saturation 1.00
     *   C bottom-left  : gamma 1.00 / saturation 1.15
     *   D bottom-right : gamma 1.30 / saturation 1.15
     */
    for (int outputY = 0; outputY < DOOMRPG_PHYSICAL_HEIGHT; ++outputY) {
        const bool lowerHalf = outputY >= DOOMRPG_LOGICAL_HEIGHT;
        const int sourceY = outputY % DOOMRPG_LOGICAL_HEIGHT;
        const uint16_t* source = framebuffer + sourceY * DOOMRPG_LOGICAL_WIDTH;

        for (int sourceX = 0; sourceX < DOOMRPG_LOGICAL_WIDTH; ++sourceX) {
            const uint16_t color = source[sourceX];
            outputRow[sourceX] = lowerHalf
                ? tuneColor565(color, false, true)
                : color;
            outputRow[sourceX + DOOMRPG_LOGICAL_WIDTH] = lowerHalf
                ? tuneColor565(color, true, true)
                : tuneColor565(color, true, false);
        }

        platformDisplay->pushPixels(outputRow, DOOMRPG_PHYSICAL_WIDTH);
    }

    platformDisplay->endWrite();

    Serial.printf("[COLORPROBE] Present four 160x120 1:1 variants in %lu us; A=g1.00/s1.00 B=g1.30/s1.00 C=g1.00/s1.15 D=g1.30/s1.15 gammaFormula=x^(1/1.30)\n",
                  micros() - started);
    return true;
}
#endif

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
#if DOOMRPG_ESP32_TOUCH_HITBOX_OVERLAY
    Serial.println("[HITBOX] Physical overlay enabled; framebuffer hashes remain untouched");
#endif
#if DOOMRPG_ESP32_COLOR_COMPARE
    prepareGamma130Lut();
    Serial.println("[COLORPROBE] Enabled: physical TFT shows four 160x120 1:1 color variants; framebuffer remains untouched");
    Serial.println("[COLORPROBE] A=neutral B=gamma1.30 C=saturation1.15 D=gamma1.30+saturation1.15");
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

#if DOOMRPG_ESP32_COLOR_COMPARE
    return presentColorComparison();
#else
    uint16_t outputRow[DOOMRPG_PHYSICAL_WIDTH];
    const uint32_t started = micros();

    platformDisplay->startWrite();
    platformDisplay->setAddrWindow(0, 0, DOOMRPG_PHYSICAL_WIDTH,
                                   DOOMRPG_PHYSICAL_HEIGHT);

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

    platformDisplay->endWrite();
#if DOOMRPG_ESP32_TOUCH_HITBOX_OVERLAY
    drawDebugOverlay();
#endif

    Serial.printf("[VIDEO] Present 160x120 -> 320x240 exact 2x: %lu us\n",
                  micros() - started);
    return true;
#endif
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
