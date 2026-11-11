#!/usr/bin/env python3
"""SDL-C1: execute the real DoomRPG image ABI with a mocked native importer.

The separate native_image_owner_host.c compiles and executes the real packed
PAK decoder. This harness pins the preserved desktop freeImage/fileOpenRead
source behavior while checking creation/adoption, imageMemory, fatal rollback,
and zero-copy transition from the C resource owner into the texture bridge.
"""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
import zlib

ROOT = Path(__file__).resolve().parents[2]
CC = shlex.split(os.environ.get("CC", "cc"))
METHODS = (
    ("void DoomRPG_freeImage(DoomRPG_t* doomrpg, Image_t* image)",
     0x34799bd6, 0x34799bd6),
    ("byte *DoomRPG_fileOpenRead(DoomRPG_t* doomrpg, const char* resourceName)",
     0x7346044d, 0xbf98b090),
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


HARNESS = r"""
#include <assert.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL.h>
#include "DoomRPG.h"
#include "esp_native_root_state.h"
#include "esp_native_indexed_image.h"
#include "esp_native_texture_bridge.h"
#include "esp_legacy_asset_source.h"

static int activeTextures, loads, adoptions, failures, freedBuffers;
static int failLoad, failAdopt, readCalls;
static char lastRequested[64];
static char fatalMessage[256];

void DoomRPG_Error(const char* fmt, ...) {
    assert(fmt != NULL);
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(fatalMessage, sizeof(fatalMessage), fmt, ap);
    va_end(ap);
    ++failures;
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

EspNativeImageStatus EspNativeImage_loadPack(const char* name, int mask,
                                             EspNativeIndexedImage* out) {
    ++loads;
    snprintf(lastRequested, sizeof(lastRequested), "%s", name);
    memset(out, 0, sizeof(*out));
    if (failLoad) return ESP_NATIVE_IMAGE_PACK_UNAVAILABLE;
    out->width = 108;
    out->height = 74;
    out->pitch = 54;
    out->bitsPerPixel = 4;
    out->hasColorKey = mask ? 1 : 0;
    out->paletteSize = 3;
    out->pixels = malloc(54 * 74);
    out->palette = calloc(3, sizeof(*out->palette));
    assert(out->pixels && out->palette);
    out->palette[0] = (EspNativeImageColor){255, 0, 255, 255};
    out->palette[1] = (EspNativeImageColor){255, 24, 8, 255};
    out->palette[2] = (EspNativeImageColor){8, 252, 24, 255};
    return ESP_NATIVE_IMAGE_OK;
}
void EspNativeImage_dispose(EspNativeIndexedImage* image) {
    if (image->pixels) { ++freedBuffers; free(image->pixels); }
    if (image->palette) { ++freedBuffers; free(image->palette); }
    memset(image, 0, sizeof(*image));
}
SDL_Texture* EspNativeTexture_adoptIndexed(EspNativeIndexedImage* image) {
    ++adoptions;
    if (failAdopt) return NULL;
    SDL_Texture* t = calloc(1, sizeof(*t));
    assert(t);
    t->width = image->width;
    t->height = image->height;
    t->hasColorKey = image->hasColorKey;
    t->indexedPixels = image->pixels;
    t->palette = image->palette;
    assert(t->palette[0].r == 255 && t->palette[0].g == 0 &&
           t->palette[0].b == 255);
    image->pixels = NULL;
    image->palette = NULL;
    ++activeTextures;
    return t;
}
void EspNativeTexture_destroy(EspNativeTexture* t) {
    assert(t && activeTextures > 0);
    free(t->indexedPixels); free(t->palette);
    freedBuffers += 2;
    --activeTextures;
    free(t);
}

int main(void) {
    DoomRPG_t core = {0};
    Image_t logo = {0}, font = {0}, failed = {0};
    EspNativeImageResources_resetAccounting();
    assert(EspNativeImageResources_accountedBytes() == 0);
    DoomRPG_createImage(&core, "j.bmp", true, &logo);
    assert(loads == 1 && adoptions == 1 && failures == 0);
    assert(strcmp(lastRequested, "j.bmp") == 0);
    assert(logo.imgBitmap != NULL && logo.width == 108 &&
           logo.height == 74 && logo.isTransparentMask);
    assert(((SDL_Texture*)logo.imgBitmap)->hasColorKey == 1);
    assert(EspNativeImageResources_accountedBytes() == -64);
    DoomRPG_createImage(&core, "font.bmp", false, &font);
    assert(loads == 2 && adoptions == 2);
    assert(((SDL_Texture*)font.imgBitmap)->hasColorKey == 0);
    assert(EspNativeImageResources_accountedBytes() == -128);

    failLoad = 1;
    DoomRPG_createImage(&core, "broken.bmp", false, &failed);
    assert(failures == 1 && loads == 3 && adoptions == 2);
    assert(failed.imgBitmap == NULL && EspNativeImageResources_accountedBytes() == -128);
    failLoad = 0;
    failAdopt = 1;
    DoomRPG_createImage(&core, "oom.bmp", false, &failed);
    assert(failures == 2 && loads == 4 && adoptions == 3);
    assert(freedBuffers == 2 && failed.imgBitmap == NULL);
    assert(strstr(fatalMessage, "native texture adoption failed") != NULL);
    assert(strstr(fatalMessage, "SDL") == NULL);
    assert(EspNativeImageResources_accountedBytes() == -128);
    failAdopt = 0;

    DoomRPG_freeImage(&core, &font);
    assert(font.imgBitmap == NULL && activeTextures == 1);
    assert(EspNativeImageResources_accountedBytes() == -64);
    DoomRPG_freeImage(&core, &logo);
    assert(logo.imgBitmap == NULL && activeTextures == 0);
    assert(EspNativeImageResources_accountedBytes() == 0 && freedBuffers == 6);
    DoomRPG_freeImage(&core, &logo);
    assert(EspNativeImageResources_accountedBytes() == 0);
    uint8_t* raw = DoomRPG_fileOpenRead(&core, "/data.bin");
    assert(raw && raw[0] == 0xA7);
    assert(strcmp(lastRequested, "data.bin") == 0 && readCalls == 1);
    free(raw);
    puts("SDL-C1 IMAGE ABI PASS native load/adoption, mask, exact metric,"
         " fatal rollback, zero-copy handoff and legacy slash data reader");
    return 0;
}
"""


def main():
    desktop = (ROOT / "src/DoomRPG.c").read_text(encoding="latin-1")
    native = (ROOT / "ESP32/src/esp_doomrpg_image_resources.c").read_text(encoding="latin-1")
    create = extract(native, "void DoomRPG_createImage(DoomRPG_t* doomrpg, const char* resourceName, boolean isTransparentMask, Image_t* img)")
    for required in ("EspNativeImage_loadPack(", "EspNativeTexture_adoptIndexed(",
                     "EspNativeImage_dispose(", "DoomRPG_freeMemory() - mem"):
        assert required in create, required
    for forbidden in ("SDL_RWFromMem(", "SDL_CreateTextureFromSurface(",
                      "SDL_FreeSurface(", "readZipFileEntry(",
                      "EspLegacyAssetSource_readAlloc("):
        assert forbidden not in create, forbidden
    for signature, crc_before, crc_after in METHODS:
        raw = extract(desktop, signature)
        historical = raw.replace("readZipFileEntry(fileName, &zipFile, &fSize)",
                                 "EspLegacyAssetSource_readAlloc(fileName, &fSize)")
        assert zlib.crc32(raw.encode("latin-1")) == crc_before
        assert zlib.crc32(historical.encode("latin-1")) == crc_after
        # SDL-D1 changes only which owner frees the identical texture payload.
        patched = historical.replace("SDL_DestroyTexture(image->imgBitmap)",
                                     "EspNativeTexture_destroy(image->imgBitmap)")
        assert extract(native, signature) == patched.replace("doomrpg->imageMemory", "nativeImageMemory")
    with tempfile.TemporaryDirectory(prefix="doomrpg-image-resource-") as tmp:
        test_file = Path(tmp) / "image_resources_host.c"
        binary = str(Path(tmp) / "image_resources_host")
        test_file.write_text(HARNESS)
        # This exercises the actual ESP32-native Image_t ABI. The legacy
        # host used the desktop ABI accidentally through the obsolete SDL.h
        # include in the production adapter; SDL-F10 removes that include.
        # Desktop/oracle layout remains checked by SDL-F5/F6 independently.
        flags = ["-std=c11", "-Wall", "-Wextra", "-Werror",
                 "-Wno-unused-parameter", "-DDOOMRPG_ESP32=1",
                 "-IESP32/include", "-Isrc"]
        flags += shlex.split(os.environ.get("IMAGE_SANITIZE_FLAGS", ""))
        subprocess.run(CC + flags + ["ESP32/src/esp_doomrpg_image_resources.c",
                                    str(test_file), "-o", binary], cwd=ROOT, check=True)
        subprocess.run([binary], cwd=ROOT, check=True)


if __name__ == "__main__":
    main()
