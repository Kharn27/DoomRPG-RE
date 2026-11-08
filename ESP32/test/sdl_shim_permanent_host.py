#!/usr/bin/env python3
"""Execute the *permanent* ESP32 SDL shim, including its real compact blitter.

Host Arduino/TFT/PlatformVideo stubs provide only the external hardware API.
No SDL_CreateTextureFromSurface/SDL_RenderCopy reimplementation or mock is used.
The 160x120 framebuffer is a host fixture, not a firmware allocation.
"""
from pathlib import Path
import os
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
CXX = shlex.split(os.environ.get("CXX", "g++"))

ARDUINO = r"""
#pragma once
#include <cmath>
#include <cstdint>
#include <cstdio>
struct HostSerial {
    void println(const char*) const {}
    template<typename... Args>
    void printf(const char*, Args...) const {}
};
inline HostSerial Serial;
inline uint32_t millis() { return 123456U; }
"""

HARNESS = r"""
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>

static bool failNextCalloc = false;
static void* trackedPixels = nullptr;
static void* trackedPalette = nullptr;
static int pixelsFreed = 0;
static int paletteFreed = 0;

extern "C" void* __real_calloc(size_t, size_t);
extern "C" void __real_free(void*);
extern "C" void* __wrap_calloc(size_t n, size_t size) {
    if (failNextCalloc) {
        failNextCalloc = false;
        return nullptr;
    }
    return __real_calloc(n, size);
}
extern "C" void __wrap_free(void* ptr) {
    if (ptr && ptr == trackedPixels) ++pixelsFreed;
    if (ptr && ptr == trackedPalette) ++paletteFreed;
    __real_free(ptr);
}

#include "ESP32/src/esp32_sdl.cpp"

static Uint16 framebuffer[DOOMRPG_LOGICAL_WIDTH * DOOMRPG_LOGICAL_HEIGHT];
static int presents = 0;
uint16_t* PlatformVideo_framebuffer() { return framebuffer; }
size_t PlatformVideo_framebufferSizeBytes() { return sizeof framebuffer; }
bool PlatformVideo_present() { ++presents; return true; }

static SDL_Surface* surfaceFor(int bpp) {
    SDL_Surface* s = static_cast<SDL_Surface*>(__real_calloc(1, sizeof(SDL_Surface)));
    s->format = static_cast<SDL_PixelFormat*>(__real_calloc(1, sizeof(SDL_PixelFormat)));
    s->format->palette = static_cast<SDL_Palette*>(__real_calloc(1, sizeof(SDL_Palette)));
    s->format->palette->ncolors = 4;
    s->format->palette->colors = static_cast<SDL_Color*>(__real_calloc(4, sizeof(SDL_Color)));
    s->w = 4;
    s->h = 2;
    s->pitch = (4 * bpp + 7) / 8;
    s->format->BitsPerPixel = static_cast<Uint8>(bpp);
    s->format->BytesPerPixel = static_cast<Uint8>((bpp + 7) / 8);
    s->format->palette->colors[0] = {255, 0, 255, 255};
    s->format->palette->colors[1] = {255, 9, 7, 255};
    s->format->palette->colors[2] = {8, 255, 9, 255};
    s->format->palette->colors[3] = {7, 8, 255, 255};
    s->pixels = __real_calloc(s->pitch, s->h);
    assert(s->pixels);
    return s;
}

static int indexAt(int bpp, int x, int y) {
    return bpp == 1 ? ((x + y) & 1) : ((x + 3 * y) & 3);
}

static void packIndices(SDL_Surface* s, int bpp) {
    for (int y = 0; y < s->h; ++y) {
        Uint8* row = static_cast<Uint8*>(s->pixels) + y * s->pitch;
        for (int x = 0; x < s->w; ++x) {
            const Uint8 idx = static_cast<Uint8>(indexAt(bpp, x, y));
            if (bpp == 8) row[x] = idx;
            else if (bpp == 4) row[x >> 1] |= static_cast<Uint8>(idx << ((x & 1) ? 0 : 4));
            else row[x >> 3] |= static_cast<Uint8>(idx << (7 - (x & 7)));
        }
    }
}

static Uint16 referenceColor(int idx, bool modulated) {
    constexpr Uint8 r[4] = {255, 255, 8, 7};
    constexpr Uint8 g[4] = {0, 9, 255, 8};
    constexpr Uint8 b[4] = {255, 7, 9, 255};
    Uint16 v = static_cast<Uint16>(((r[idx] & 0xf8) << 8) |
                                   ((g[idx] & 0xfc) << 3) | (b[idx] >> 3));
    if (modulated) {
        const int red = ((v >> 11) & 31) * 128 / 255;
        const int green = ((v >> 5) & 63) * 255 / 255;
        const int blue = (v & 31) * 64 / 255;
        v = static_cast<Uint16>((red << 11) | (green << 5) | blue);
    }
    return v;
}

static void track(SDL_Surface* s) {
    trackedPixels = s->pixels;
    trackedPalette = s->format->palette->colors;
    pixelsFreed = 0;
    paletteFreed = 0;
}

static void exerciseIndexed(int bpp) {
    SDL_Surface* s = surfaceFor(bpp);
    packIndices(s, bpp);
    assert(SDL_SetColorKey(s, SDL_TRUE, SDL_MapRGB(s->format, 255, 0, 255)) == 0);
    track(s);
    SDL_Texture* t = SDL_CreateTextureFromSurface(nullptr, s);
    assert(t != nullptr && t->indexed && t->indexedBitsPerPixel == bpp);
    assert(t->pixels == nullptr && t->indexedPixels == trackedPixels);
    assert(t->palette == trackedPalette && t->paletteSize == 4);
    assert(t->pitch == s->pitch && t->width == 4 && t->height == 2);
    assert(s->pixels == nullptr && s->format->palette->colors == nullptr);
    SDL_FreeSurface(s);
    assert(pixelsFreed == 0 && paletteFreed == 0);

    const Uint16 pixel = 0xabcd;
    SDL_Rect src{0, 0, 4, 2};
    SDL_Rect dst{20, 20, 8, 4};
    SDL_RenderSetClipRect(nullptr, nullptr);
    std::fill(std::begin(framebuffer), std::end(framebuffer), pixel);
    assert(SDL_RenderCopy(nullptr, t, &src, &dst) == 0);
    for (int y = 19; y < 25; ++y) {
        for (int x = 19; x < 29; ++x) {
            const bool inside = x >= 20 && x < 28 && y >= 20 && y < 24;
            const int idx = inside ? indexAt(bpp, (x - 20) / 2, (y - 20) / 2) : 0;
            const Uint16 expected = inside && idx != 0 ? referenceColor(idx, false) : pixel;
            assert(framebuffer[y * 160 + x] == expected);
        }
    }

    assert(SDL_SetTextureColorMod(t, 128, 255, 64) == 0);
    SDL_Rect clip{22, 21, 3, 2};
    SDL_RenderSetClipRect(nullptr, &clip);
    std::fill(std::begin(framebuffer), std::end(framebuffer), pixel);
    assert(SDL_RenderCopy(nullptr, t, &src, &dst) == 0);
    for (int y = 19; y < 25; ++y) {
        for (int x = 19; x < 29; ++x) {
            const bool inside = x >= 22 && x < 25 && y >= 21 && y < 23;
            const int idx = inside ? indexAt(bpp, (x - 20) / 2, (y - 20) / 2) : 0;
            const Uint16 expected = inside && idx != 0 ? referenceColor(idx, true) : pixel;
            assert(framebuffer[y * 160 + x] == expected);
        }
    }
    SDL_RenderSetClipRect(nullptr, nullptr);

    const Uint16 rgb[2] = {0xf800, 0x07e0};
    assert(SDL_UpdateTexture(t, nullptr, rgb, sizeof rgb) == -1);
    SDL_Rect bad{0, 0, 0, 2};
    assert(SDL_RenderCopy(nullptr, t, &bad, &dst) == -1);
    SDL_DestroyTexture(t);
    assert(pixelsFreed == 1 && paletteFreed == 1);
    trackedPixels = trackedPalette = nullptr;
}

static void exerciseRgb565() {
    SDL_Texture* t = SDL_CreateTexture(nullptr, SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING, 2, 1);
    assert(t != nullptr && !t->indexed && t->indexedPixels == nullptr);
    const Uint16 colors[2] = {0xf800, 0x07e0};
    assert(SDL_UpdateTexture(t, nullptr, colors, sizeof colors) == 0);
    SDL_Rect dest{40, 10, 4, 2};
    std::fill(std::begin(framebuffer), std::end(framebuffer), static_cast<Uint16>(0));
    assert(SDL_RenderCopy(nullptr, t, nullptr, &dest) == 0);
    for (int y = 10; y < 12; ++y) for (int x = 40; x < 44; ++x)
        assert(framebuffer[y * 160 + x] == colors[(x - 40) / 2]);
    SDL_DestroyTexture(t);
}

static void exerciseFailures() {
    assert(SDL_CreateTextureFromSurface(nullptr, nullptr) == nullptr);
    assert(std::strstr(SDL_GetError(), "unsupported indexed surface") != nullptr);
    for (int badBpp : {2, 16}) {
        SDL_Surface* s = surfaceFor(badBpp);
        track(s);
        assert(SDL_CreateTextureFromSurface(nullptr, s) == nullptr);
        assert(s->pixels != nullptr && s->format->palette->colors != nullptr);
        SDL_FreeSurface(s);
        assert(pixelsFreed == 1 && paletteFreed == 1);
    }
    SDL_Surface* s = surfaceFor(4);
    packIndices(s, 4);
    track(s);
    failNextCalloc = true;
    assert(SDL_CreateTextureFromSurface(nullptr, s) == nullptr);
    assert(std::strstr(SDL_GetError(), "out of memory creating indexed texture") != nullptr);
    assert(s->pixels == trackedPixels && s->format->palette->colors == trackedPalette);
    SDL_FreeSurface(s);
    assert(pixelsFreed == 1 && paletteFreed == 1);
    trackedPixels = trackedPalette = nullptr;
    assert(SDL_CreateTexture(nullptr, 0, 0, 4, 4) == nullptr);
    assert(SDL_UpdateTexture(nullptr, nullptr, nullptr, 0) == -1);
}

int main() {
    static_assert(sizeof framebuffer == 38400, "single logical RGB565 host fixture");
    assert(SDL_GetTicks() == 123456U);
    assert(PlatformVideo_framebufferSizeBytes() == 38400);
    Esp32Sdl_attachDisplay(reinterpret_cast<TFT_eSPI*>(1));
    for (int bpp : {1, 4, 8}) exerciseIndexed(bpp);
    exerciseRgb565();
    exerciseFailures();
    SDL_RenderPresent(nullptr);
    assert(presents == 1);
    std::puts("SDL-A PASS real permanent shim: indexed 1/4/8 bpp, zero-copy, palette, "
              "magenta key, RGB565/mod, scale, clip, UpdateTexture refusal, OOM and frees");
}
"""

def main():
    sdl = ROOT / "ESP32/src/esp32_sdl.cpp"
    assert "Uint8* indexedPixels;" in sdl.read_text()
    with tempfile.TemporaryDirectory(prefix="doomrpg-sdl-permanent-") as temp:
        tmp = Path(temp)
        (tmp / "Arduino.h").write_text(ARDUINO)
        (tmp / "TFT_eSPI.h").write_text("#pragma once\nclass TFT_eSPI {};\n")
        test = tmp / "permanent.cpp"
        test.write_text(HARNESS)
        output = tmp / "permanent"
        cmd = CXX + ["-std=c++17", "-O2", "-Wall", "-Wextra", "-Werror",
                     "-Wl,--wrap=calloc", "-Wl,--wrap=free",
                     "-I", str(tmp), "-I", str(ROOT),
                     "-I", str(ROOT / "ESP32/include"),
                     "-I", str(ROOT / "ESP32/src"),
                     str(test), "-o", str(output)]
        subprocess.run(cmd, check=True, cwd=ROOT)
        subprocess.run([str(output)], check=True, cwd=ROOT)

if __name__ == "__main__":
    main()
