#include "esp_native_texture.h"

#include <Arduino.h>
#include <algorithm>
#include <cstdlib>
#include <limits>

#include "platform_video.h"
#include "platform_video_config.h"

namespace {
constexpr int kLogicalWidth = DOOMRPG_LOGICAL_WIDTH;
constexpr int kLogicalHeight = DOOMRPG_LOGICAL_HEIGHT;

bool clipEnabled = false;
EspNativeRect clipRect{0, 0, kLogicalWidth, kLogicalHeight};

uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
    return static_cast<uint16_t>(((r & 0xf8U) << 8U) |
                                 ((g & 0xfcU) << 3U) | (b >> 3U));
}

uint16_t modulate565(uint16_t color, const EspNativeTexture* texture) {
    const int red = ((color >> 11) & 0x1f) * texture->red / 255;
    const int green = ((color >> 5) & 0x3f) * texture->green / 255;
    const int blue = (color & 0x1f) * texture->blue / 255;
    return static_cast<uint16_t>((red << 11) | (green << 5) | blue);
}

bool inClip(int x, int y) {
    if (x < 0 || x >= kLogicalWidth || y < 0 || y >= kLogicalHeight) return false;
    if (!clipEnabled) return true;
    return x >= clipRect.x && y >= clipRect.y &&
           static_cast<int64_t>(x) < static_cast<int64_t>(clipRect.x) + clipRect.w &&
           static_cast<int64_t>(y) < static_cast<int64_t>(clipRect.y) + clipRect.h;
}
} // namespace

extern "C" EspNativeTexture* EspNativeTexture_adoptIndexed(
    EspNativeIndexedImage* image) {
    if (image == nullptr || image->pixels == nullptr || image->palette == nullptr ||
        image->width <= 0 || image->height <= 0 ||
        (image->bitsPerPixel != 1 && image->bitsPerPixel != 4 &&
         image->bitsPerPixel != 8) ||
        image->paletteSize == 0 || image->paletteSize > 256 ||
        image->pitch <= 0 ||
        static_cast<size_t>(image->pitch) !=
            (static_cast<size_t>(image->width) * image->bitsPerPixel + 7U) / 8U) {
        return nullptr;
    }
    EspNativeTexture* texture =
        static_cast<EspNativeTexture*>(calloc(1, sizeof(EspNativeTexture)));
    if (texture == nullptr) return nullptr;

    texture->width = image->width;
    texture->height = image->height;
    texture->pitch = image->pitch;
    texture->indexedPixels = image->pixels;
    texture->palette = image->palette;
    texture->paletteSize = image->paletteSize;
    texture->indexedBitsPerPixel = image->bitsPerPixel;
    texture->indexed = true;
    texture->red = texture->green = texture->blue = 255;
    texture->hasColorKey = image->hasColorKey != 0;
    texture->colorKey = 0xf81fU;
    image->pixels = nullptr;
    image->palette = nullptr;
    Serial.printf("[NATIVEIMAGE] ADOPT %dx%d bpp=%u bytes=%u palette=%d\n",
                  texture->width, texture->height, texture->indexedBitsPerPixel,
                  static_cast<unsigned>(texture->pitch * texture->height),
                  texture->paletteSize);
    return texture;
}

extern "C" void EspNativeTexture_destroy(EspNativeTexture* texture) {
    if (texture == nullptr) return;
    free(texture->pixels);
    free(texture->indexedPixels);
    free(texture->palette);
    free(texture);
}

extern "C" int EspNativeTexture_setColorMod(EspNativeTexture* texture,
                                             uint8_t red, uint8_t green,
                                             uint8_t blue) {
    if (texture == nullptr) return -1;
    texture->red = red;
    texture->green = green;
    texture->blue = blue;
    return 0;
}

extern "C" void EspNativeTexture_setClipRect(const EspNativeRect* clip) {
    clipEnabled = clip != nullptr;
    clipRect = clip == nullptr
        ? EspNativeRect{0, 0, kLogicalWidth, kLogicalHeight} : *clip;
}

// SDL-E1: the live SDL software renderer always used NONE blending.
namespace {
uint8_t drawRed = 255;
uint8_t drawGreen = 255;
uint8_t drawBlue = 255;
}

extern "C" void EspNativeTexture_setDrawColor(uint8_t red, uint8_t green,
                                               uint8_t blue, uint8_t alpha) {
    drawRed = red;
    drawGreen = green;
    drawBlue = blue;
    (void)alpha;
}

extern "C" int EspNativeTexture_fillRect(const EspNativeRect* rect) {
    uint16_t* framebuffer = PlatformVideo_framebuffer();
    if (framebuffer == nullptr) return -1;
    const EspNativeRect area = rect == nullptr
        ? EspNativeRect{0, 0, kLogicalWidth, kLogicalHeight} : *rect;
    if (area.w <= 0 || area.h <= 0) return 0;
    const int x0 = std::max(0, area.x);
    const int y0 = std::max(0, area.y);
    const int x1 = static_cast<int>(std::min<int64_t>(
        kLogicalWidth, static_cast<int64_t>(area.x) + area.w));
    const int y1 = static_cast<int>(std::min<int64_t>(
        kLogicalHeight, static_cast<int64_t>(area.y) + area.h));
    const uint16_t color = rgb565(drawRed, drawGreen, drawBlue);
    for (int y = y0; y < y1; ++y)
        for (int x = x0; x < x1; ++x)
            if (inClip(x, y)) framebuffer[y * kLogicalWidth + x] = color;
    return 0;
}

extern "C" int EspNativeTexture_drawLine(int x1, int y1, int x2, int y2) {
    uint16_t* framebuffer = PlatformVideo_framebuffer();
    if (framebuffer == nullptr) return -1;
    const uint16_t color = rgb565(drawRed, drawGreen, drawBlue);
    const int dx = std::abs(x2 - x1);
    const int sx = x1 < x2 ? 1 : -1;
    const int dy = -std::abs(y2 - y1);
    const int sy = y1 < y2 ? 1 : -1;
    int error = dx + dy;
    while (true) {
        if (inClip(x1, y1)) framebuffer[y1 * kLogicalWidth + x1] = color;
        if (x1 == x2 && y1 == y2) break;
        const int doubled = error * 2;
        if (doubled >= dy) { error += dy; x1 += sx; }
        if (doubled <= dx) { error += dx; y1 += sy; }
    }
    return 0;
}

extern "C" int EspNativeTexture_blit(const EspNativeTexture* texture,
                                     const EspNativeRect* source,
                                     const EspNativeRect* destination) {
    if (texture == nullptr) return -1;
    uint16_t* framebuffer = PlatformVideo_framebuffer();
    if (framebuffer == nullptr) return -1;

    const EspNativeRect src = source == nullptr
        ? EspNativeRect{0, 0, texture->width, texture->height} : *source;
    const EspNativeRect dst = destination == nullptr
        ? EspNativeRect{0, 0, src.w, src.h} : *destination;
    if (src.w <= 0 || src.h <= 0 || dst.w <= 0 || dst.h <= 0) return -1;

    const int startX = std::max(0, dst.x);
    const int startY = std::max(0, dst.y);
    const int endX = static_cast<int>(std::min<int64_t>(
        kLogicalWidth, static_cast<int64_t>(dst.x) + dst.w));
    const int endY = static_cast<int>(std::min<int64_t>(
        kLogicalHeight, static_cast<int64_t>(dst.y) + dst.h));

    for (int y = startY; y < endY; ++y) {
        if (clipEnabled && (y < clipRect.y ||
            static_cast<int64_t>(y) >= static_cast<int64_t>(clipRect.y) + clipRect.h))
            continue;
        const int sourceY = src.y + ((y - dst.y) * src.h) / dst.h;
        if (sourceY < 0 || sourceY >= texture->height) continue;
        for (int x = startX; x < endX; ++x) {
            if (!inClip(x, y)) continue;
            const int sourceX = src.x + ((x - dst.x) * src.w) / dst.w;
            if (sourceX < 0 || sourceX >= texture->width) continue;
            uint16_t color;
            if (texture->indexed) {
                if (texture->indexedPixels == nullptr || texture->palette == nullptr)
                    return -1;
                const uint8_t* indexedRow =
                    texture->indexedPixels + static_cast<size_t>(sourceY) * texture->pitch;
                uint8_t index;
                if (texture->indexedBitsPerPixel == 8) {
                    index = indexedRow[sourceX];
                } else if (texture->indexedBitsPerPixel == 4) {
                    const uint8_t packed = indexedRow[sourceX >> 1];
                    index = (sourceX & 1) ? (packed & 0x0f) : (packed >> 4);
                } else if (texture->indexedBitsPerPixel == 1) {
                    index = (indexedRow[sourceX >> 3] >>
                             (7 - (sourceX & 7))) & 0x01;
                } else {
                    return -1;
                }
                if (index >= texture->paletteSize) continue;
                const EspNativeImageColor& colorEntry = texture->palette[index];
                color = rgb565(colorEntry.r, colorEntry.g, colorEntry.b);
            } else {
                if (texture->pixels == nullptr) return -1;
                color = texture->pixels[static_cast<size_t>(sourceY) *
                                         texture->width + sourceX];
            }
            if (texture->hasColorKey && color == texture->colorKey) continue;
            framebuffer[y * kLogicalWidth + x] = modulate565(color, texture);
        }
    }
    return 0;
}
