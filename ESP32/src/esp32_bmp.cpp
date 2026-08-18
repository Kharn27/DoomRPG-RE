#include "esp32_bmp.h"

#include <Arduino.h>

#include <cstdlib>
#include <cstring>

namespace {

Uint16 readLe16(const Uint8* bytes) {
    return static_cast<Uint16>(bytes[0] | (bytes[1] << 8));
}

Uint32 readLe32(const Uint8* bytes) {
    return static_cast<Uint32>(bytes[0]) |
           (static_cast<Uint32>(bytes[1]) << 8) |
           (static_cast<Uint32>(bytes[2]) << 16) |
           (static_cast<Uint32>(bytes[3]) << 24);
}

void closeIfRequested(SDL_RWops* source, int freeSource) {
    if (freeSource && source != nullptr) {
        SDL_RWclose(source);
    }
}

SDL_Surface* fail(SDL_RWops* source, int freeSource, SDL_Surface* surface,
                  Uint8* row, const char* reason) {
    free(row);
    if (surface != nullptr) {
        SDL_FreeSurface(surface);
    }
    closeIfRequested(source, freeSource);
    Serial.printf("[BMP] ERROR %s\n", reason);
    return nullptr;
}

}  // namespace

extern "C" SDL_Surface* Esp32Bmp_LoadRW(SDL_RWops* source, int freeSource) {
    Uint8 header[54];
    if (source == nullptr || SDL_RWread(source, header, sizeof(header), 1) != 1 ||
        header[0] != 'B' || header[1] != 'M') {
        return fail(source, freeSource, nullptr, nullptr, "unsupported BMP header");
    }

    const Uint32 pixelOffset = readLe32(header + 10);
    const Uint32 dibSize = readLe32(header + 14);
    const Sint32 width = static_cast<Sint32>(readLe32(header + 18));
    const Sint32 signedHeight = static_cast<Sint32>(readLe32(header + 22));
    const Uint16 bitsPerPixel = readLe16(header + 28);
    const Uint32 compression = readLe32(header + 30);
    Uint32 colorCount = readLe32(header + 46);

    if (dibSize < 40 || width <= 0 || signedHeight == 0 || compression != 0 ||
        (bitsPerPixel != 1 && bitsPerPixel != 4 && bitsPerPixel != 8)) {
        return fail(source, freeSource, nullptr, nullptr,
                    "only uncompressed indexed 1/4/8-bpp BMP is supported");
    }

    if (colorCount == 0) {
        colorCount = 1U << bitsPerPixel;
    }
    if (colorCount == 0 || colorCount > 256U) {
        return fail(source, freeSource, nullptr, nullptr, "invalid BMP palette size");
    }

    const int height = abs(signedHeight);
    const size_t outputBytes = static_cast<size_t>(width) * static_cast<size_t>(height);
    const size_t rowBits = static_cast<size_t>(width) * bitsPerPixel;
    const size_t filePitch = ((rowBits + 31U) / 32U) * 4U;

    SDL_Surface* surface = static_cast<SDL_Surface*>(calloc(1, sizeof(SDL_Surface)));
    if (surface == nullptr) {
        return fail(source, freeSource, nullptr, nullptr, "out of memory creating BMP surface");
    }

    surface->format = static_cast<SDL_PixelFormat*>(calloc(1, sizeof(SDL_PixelFormat)));
    if (surface->format != nullptr) {
        surface->format->palette = static_cast<SDL_Palette*>(calloc(1, sizeof(SDL_Palette)));
    }
    if (surface->format != nullptr && surface->format->palette != nullptr) {
        surface->format->palette->colors =
            static_cast<SDL_Color*>(calloc(colorCount, sizeof(SDL_Color)));
    }
    surface->pixels = calloc(outputBytes, 1);

    if (surface->format == nullptr || surface->format->palette == nullptr ||
        surface->format->palette->colors == nullptr || surface->pixels == nullptr) {
        return fail(source, freeSource, surface, nullptr, "out of memory loading BMP");
    }

    surface->w = width;
    surface->h = height;
    surface->pitch = width;
    surface->format->BitsPerPixel = 8;
    surface->format->BytesPerPixel = 1;
    surface->format->palette->ncolors = static_cast<int>(colorCount);

    if (SDL_RWseek(source, 14 + dibSize, SEEK_SET) < 0) {
        return fail(source, freeSource, surface, nullptr, "invalid BMP palette offset");
    }

    for (Uint32 index = 0; index < colorCount; ++index) {
        Uint8 bgra[4];
        if (SDL_RWread(source, bgra, sizeof(bgra), 1) != 1) {
            return fail(source, freeSource, surface, nullptr, "truncated BMP palette");
        }
        SDL_Color& color = surface->format->palette->colors[index];
        color.r = bgra[2];
        color.g = bgra[1];
        color.b = bgra[0];
        color.a = 255;
    }

    Uint8* row = static_cast<Uint8*>(malloc(filePitch));
    if (row == nullptr || SDL_RWseek(source, pixelOffset, SEEK_SET) < 0) {
        return fail(source, freeSource, surface, row, "invalid BMP pixel data");
    }

    Serial.printf("[BMP] decode %ldx%d %u-bpp -> 8-bpp indexed colors=%u row=%u out=%u\n",
                  static_cast<long>(width), height, bitsPerPixel,
                  static_cast<unsigned int>(colorCount),
                  static_cast<unsigned int>(filePitch),
                  static_cast<unsigned int>(outputBytes));

    for (int fileY = 0; fileY < height; ++fileY) {
        if (SDL_RWread(source, row, filePitch, 1) != 1) {
            return fail(source, freeSource, surface, row, "truncated BMP pixel data");
        }

        const int destinationY = signedHeight > 0 ? height - 1 - fileY : fileY;
        Uint8* destination = static_cast<Uint8*>(surface->pixels) +
                             static_cast<size_t>(destinationY) * surface->pitch;

        if (bitsPerPixel == 8) {
            memcpy(destination, row, static_cast<size_t>(width));
        }
        else if (bitsPerPixel == 4) {
            for (int x = 0; x < width; ++x) {
                const Uint8 packed = row[x >> 1];
                destination[x] = (x & 1) ? (packed & 0x0f) : (packed >> 4);
            }
        }
        else {
            for (int x = 0; x < width; ++x) {
                destination[x] = (row[x >> 3] >> (7 - (x & 7))) & 0x01;
            }
        }
    }

    free(row);
    closeIfRequested(source, freeSource);
    return surface;
}
