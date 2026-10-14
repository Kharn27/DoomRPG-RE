#!/usr/bin/env python3
"""Pin the PAK/indexed-BMP DoomRPG image ABI and exercise its real native owner.

Reference source is test-only. The effective source bodies must match exactly,
including failure/cleanup and imageMemory quirks, after the documented PAK/BMP
rewrites. Mocked SDL/PAK here do NOT substitute for a CYD/SD hardware test.
"""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile
import zlib

ROOT = Path(__file__).resolve().parents[2]
CC = shlex.split(os.environ.get("CC", "cc"))
METHODS = (
    ("void DoomRPG_createImage(DoomRPG_t* doomrpg, const char* resourceName, boolean isTransparentMask, Image_t* img)", 0x824cd7c6, 0xb853aa58),
    ("void DoomRPG_freeImage(DoomRPG_t* doomrpg, Image_t* image)", 0x34799bd6, 0x34799bd6),
    ("byte *DoomRPG_fileOpenRead(DoomRPG_t* doomrpg, const char* resourceName)", 0x7346044d, 0xbf98b090),
)


def extract(source, signature):
    anchor = signature + "\n{"
    assert source.count(anchor) == 1, "missing/duplicate: " + signature
    start = source.index(anchor)
    depth = 0
    for index in range(source.index("{", start), len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[start:index + 1]
    raise AssertionError("unterminated " + signature)


def effective(source):
    return (source.replace("SDL_LoadBMP_RW(", "Esp32Bmp_LoadRW(")
                  .replace("readZipFileEntry(fileName, &zipFile, &fSize)",
                           "EspLegacyAssetSource_readAlloc(fileName, &fSize)"))


HARNESS = r'''
#include <assert.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL.h>
#include "DoomRPG.h"
#include "SDL_Video.h"
#include "esp32_bmp.h"
#include "esp_legacy_asset_source.h"

struct SDL_RWops { void *bytes; int length; };
struct SDL_Texture { int w, h, mask; SDL_Color colors[3]; };

SDLVideo_t sdlVideo;
static int activeTextures, readCalls, rwCloses, loads, freedSurfaces;
static int destroyCalls, maskCalls, createCalls;
static char lastRequested[64];

void DoomRPG_Error(const char* fmt, ...) {
    fprintf(stderr, "Unexpected fatal image path: %s\n", fmt);
    abort();
}
int DoomRPG_freeMemory(void) { return 10000 - 64 * activeTextures; }

uint8_t *EspLegacyAssetSource_readAlloc(const char* name, int* size) {
    ++readCalls;
    snprintf(lastRequested, sizeof(lastRequested), "%s", name);
    uint8_t *bytes = malloc(8);
    assert(bytes != NULL);
    memset(bytes, 0xA7, 8);
    if (size) *size = 8;
    return bytes;
}
SDL_RWops* SDL_RWFromMem(void* bytes, int size) {
    assert(bytes != NULL && size == 8);
    SDL_RWops* stream = malloc(sizeof(*stream));
    assert(stream != NULL);
    stream->bytes = bytes;
    stream->length = size;
    return stream;
}
int SDL_RWclose(SDL_RWops* stream) {
    assert(stream != NULL && stream->length == 8);
    ++rwCloses;
    free(stream);
    return 0;
}
SDL_Surface* Esp32Bmp_LoadRW(SDL_RWops* stream, int freeSource) {
    assert(stream != NULL && freeSource == SDL_TRUE);
    ++loads;
    SDL_Surface* s = calloc(1, sizeof(*s));
    assert(s != NULL);
    s->w = 108;
    s->h = 74;
    s->format = calloc(1, sizeof(*s->format));
    assert(s->format != NULL);
    s->format->palette = calloc(1, sizeof(*s->format->palette));
    assert(s->format->palette != NULL);
    s->format->palette->ncolors = 3;
    s->format->palette->colors = calloc(3, sizeof(SDL_Color));
    assert(s->format->palette->colors != NULL);
    s->format->palette->colors[0] = (SDL_Color){255, 10, 20, 255};
    s->format->palette->colors[1] = (SDL_Color){3, 254, 89, 255};
    s->format->palette->colors[2] = (SDL_Color){81, 64, 247, 255};
    assert(SDL_RWclose(stream) == 0);
    return s;
}
Uint32 SDL_MapRGB(const SDL_PixelFormat* fmt, Uint8 r, Uint8 g, Uint8 b) {
    assert(fmt != NULL && r == 255 && g == 0 && b == 255);
    ++maskCalls;
    return 0xF81Fu;
}
int SDL_SetColorKey(SDL_Surface* s, int flag, Uint32 key) {
    assert(flag == SDL_TRUE && key == 0xF81Fu);
    s->hasColorKey = 1;
    return 0;
}
static SDL_Color quant(SDL_Color a) {
    const int rgb565 = ((a.r >> 3) << 11) | ((a.g >> 2) << 5) | (a.b >> 3);
    const int r = (rgb565 >> 11) & 31;
    const int g = (rgb565 >> 5) & 63;
    const int b = rgb565 & 31;
    return (SDL_Color){(r << 3) | (r >> 2),
                       (g << 2) | (g >> 4),
                       (b << 3) | (b >> 2), a.a};
}
SDL_Texture* SDL_CreateTextureFromSurface(SDL_Renderer* renderer, SDL_Surface* s) {
    assert(renderer == sdlVideo.renderer && s != NULL);
    assert(s->format->palette->ncolors == 3);
    const SDL_Color original[] = {
        {255, 10, 20, 255}, {3, 254, 89, 255}, {81, 64, 247, 255}
    };
    ++createCalls;
    for (int i=0; i<3; ++i) {
        SDL_Color expected = quant(original[i]);
        assert(memcmp(&s->format->palette->colors[i], &expected, sizeof(expected)) == 0);
    }
    SDL_Texture* texture = calloc(1, sizeof(*texture));
    assert(texture != NULL);
    texture->w = s->w;
    texture->h = s->h;
    texture->mask = s->hasColorKey;
    memcpy(texture->colors, s->format->palette->colors, sizeof(texture->colors));
    ++activeTextures;
    return texture;
}
void SDL_FreeSurface(SDL_Surface* s) {
    assert(s != NULL);
    ++freedSurfaces;
    free(s->format->palette->colors);
    free(s->format->palette);
    free(s->format);
    free(s);
}
void SDL_DestroyTexture(SDL_Texture* texture) {
    assert(texture != NULL && activeTextures > 0);
    ++destroyCalls;
    --activeTextures;
    free(texture);
}
int main(void) {
    DoomRPG_t core = {0};
    Image_t logo = {0}, font = {0};
    sdlVideo.renderer = (SDL_Renderer*)(uintptr_t)0x1234;
    core.imageMemory = 5;
    DoomRPG_createImage(&core, "j.bmp", true, &logo);
    assert(readCalls == 1 && strcmp(lastRequested, "j.bmp") == 0);
    assert(loads == 1 && rwCloses == 1 && freedSurfaces == 1);
    assert(createCalls == 1 && maskCalls == 1 && logo.imgBitmap != NULL);
    assert(logo.width == 108 && logo.height == 74 && logo.isTransparentMask);
    assert(((struct SDL_Texture*)logo.imgBitmap)->mask == 1);
    assert(core.imageMemory == -59); /* old 5 + (9936 - 10000) */
    DoomRPG_createImage(&core, "font.bmp", false, &font);
    assert(readCalls == 2 && strcmp(lastRequested, "font.bmp") == 0);
    assert(maskCalls == 1 && createCalls == 2 && rwCloses == 2);
    assert(((struct SDL_Texture*)font.imgBitmap)->mask == 0);
    assert(core.imageMemory == -123);
    DoomRPG_freeImage(&core, &font);
    assert(font.imgBitmap == NULL && activeTextures == 1);
    assert(core.imageMemory == -59);
    DoomRPG_freeImage(&core, &logo);
    assert(logo.imgBitmap == NULL && activeTextures == 0);
    assert(core.imageMemory == 5 && destroyCalls == 2);
    DoomRPG_freeImage(&core, &logo);
    assert(core.imageMemory == 5 && destroyCalls == 2);
    uint8_t* raw = DoomRPG_fileOpenRead(&core, "/data.bin");
    assert(raw != NULL && raw[0] == 0xA7);
    assert(strcmp(lastRequested, "data.bin") == 0 && readCalls == 3);
    free(raw);
    assert(activeTextures == 0 && loads == 2 && freedSurfaces == 2);
    puts("IMAGE-RESOURCES PASS PAK allocate, packed BMP, palette RGB565,"
         " mask, imageMemory, disposal and slash path");
    return 0;
}
'''


def main():
    desktop = (ROOT / "src/DoomRPG.c").read_text(encoding="latin-1")
    native = (ROOT / "ESP32/src/esp_doomrpg_image_resources.c").read_text(encoding="latin-1")
    generator = (ROOT / "ESP32/scripts/build_engine.py").read_text()
    assert 'generatedBodies=removed' in generator
    assert native.count("readZipFileEntry(") == 0, "legacy ZIP runtime dependency"
    for signature, crc_before, crc_after in METHODS:
        raw = extract(desktop, signature)
        patched = effective(raw)
        assert zlib.crc32(raw.encode("latin-1")) == crc_before, "desktop drift: " + signature
        assert zlib.crc32(patched.encode("latin-1")) == crc_after, "patch drift: " + signature
        assert extract(native, signature) == patched, "ESP32 owner drift: " + signature
    with tempfile.TemporaryDirectory(prefix="doomrpg-image-resource-") as tmp:
        test_file = Path(tmp) / "image_resources_host.c"
        binary = str(Path(tmp) / "image_resources_host")
        test_file.write_text(HARNESS)
        flags = ["-std=c11", "-Wall", "-Wextra", "-Werror",
                 "-Wno-unused-parameter", "-IESP32/include", "-Isrc"]
        flags += shlex.split(os.environ.get("IMAGE_SANITIZE_FLAGS", ""))
        subprocess.run(CC + flags + ["ESP32/src/esp_doomrpg_image_resources.c",
                                    str(test_file), "-o", binary], cwd=ROOT, check=True)
        subprocess.run([binary], cwd=ROOT, check=True)


if __name__ == "__main__":
    main()
