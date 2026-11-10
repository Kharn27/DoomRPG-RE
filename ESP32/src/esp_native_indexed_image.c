#include "esp_native_indexed_image.h"
#include "esp_asset_pack.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint16_t le16(const uint8_t* p) {
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static uint32_t le32(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void* imageAlloc(const EspNativeImageInput* input, size_t bytes) {
    return input->allocate != NULL ? input->allocate(bytes) : malloc(bytes);
}

static void imageFree(const EspNativeImageInput* input, void* pointer) {
    if (pointer == NULL) return;
    if (input->release != NULL) input->release(pointer);
    else free(pointer);
}

void EspNativeImage_dispose(EspNativeIndexedImage* image) {
    if (image == NULL) return;
    free(image->pixels);
    free(image->palette);
    memset(image, 0, sizeof(*image));
}

EspNativeImageStatus EspNativeImage_decode(const EspNativeImageInput* input,
                                           int transparentMask,
                                           EspNativeIndexedImage* output) {
    uint8_t header[54];
    uint8_t bgra[4];
    uint32_t dibSize, pixelOffset, paletteCount, bits, compression;
    uint64_t paletteStart, paletteEnd, pitch, filePitch, rowBits, dataBytes;
    int32_t width, signedHeight;
    uint32_t height, fileY, i;
    EspNativeIndexedImage image = {0};
    EspNativeImageStatus status;

    if (output != NULL) memset(output, 0, sizeof(*output));
    if (input == NULL || output == NULL || input->readRange == NULL ||
        input->size < sizeof(header)) return ESP_NATIVE_IMAGE_INVALID;
    if (!input->readRange(input->context, 0, header, sizeof(header)))
        return ESP_NATIVE_IMAGE_TRUNCATED;
    if (header[0] != 'B' || header[1] != 'M')
        return ESP_NATIVE_IMAGE_UNSUPPORTED;

    pixelOffset = le32(header + 10);
    dibSize = le32(header + 14);
    width = (int32_t)le32(header + 18);
    signedHeight = (int32_t)le32(header + 22);
    bits = le16(header + 28);
    compression = le32(header + 30);
    paletteCount = le32(header + 46);

    if (dibSize < 40U || width <= 0 || signedHeight == 0 ||
        signedHeight == INT32_MIN || le16(header + 26) != 1U ||
        compression != 0U || (bits != 1U && bits != 4U && bits != 8U))
        return ESP_NATIVE_IMAGE_UNSUPPORTED;

    height = (uint32_t)(signedHeight < 0 ? -signedHeight : signedHeight);
    if (paletteCount == 0U) paletteCount = 1U << bits;
    if (paletteCount == 0U || paletteCount > 256U)
        return ESP_NATIVE_IMAGE_UNSUPPORTED;

    paletteStart = 14ULL + dibSize;
    paletteEnd = paletteStart + 4ULL * paletteCount;
    rowBits = (uint64_t)(uint32_t)width * bits;
    pitch = (rowBits + 7ULL) / 8ULL;
    filePitch = ((rowBits + 31ULL) / 32ULL) * 4ULL;
    dataBytes = filePitch * height;
    if (pitch == 0ULL || pitch > INT_MAX || height > INT_MAX ||
        pitch * height > SIZE_MAX || pitch * height > UINT32_MAX)
        return ESP_NATIVE_IMAGE_UNSUPPORTED;
    if (paletteEnd > input->size || pixelOffset < paletteEnd ||
        pixelOffset > input->size || dataBytes > input->size - pixelOffset)
        return ESP_NATIVE_IMAGE_TRUNCATED;

    image.width = width;
    image.height = (int)height;
    image.pitch = (int)pitch;
    image.bitsPerPixel = (uint8_t)bits;
    image.paletteSize = (uint16_t)paletteCount;
    image.hasColorKey = transparentMask ? 1U : 0U;
    image.palette = (EspNativeImageColor*)imageAlloc(
        input, (size_t)paletteCount * sizeof(EspNativeImageColor));
    if (image.palette == NULL) return ESP_NATIVE_IMAGE_OOM;
    image.pixels = (uint8_t*)imageAlloc(input, (size_t)pitch * height);
    if (image.pixels == NULL) {
        status = ESP_NATIVE_IMAGE_OOM;
        goto fail;
    }

    /* Exactly the pre-existing RGB565 -> RGB888 roundtrip. The magenta
     * color key is tested against RGB565 after palette lookup. */
    for (i = 0; i < paletteCount; ++i) {
        uint8_t r, g, b;
        if (!input->readRange(input->context,
                              (uint32_t)(paletteStart + 4ULL * i),
                              bgra, sizeof(bgra))) {
            status = ESP_NATIVE_IMAGE_TRUNCATED;
            goto fail;
        }
        r = (uint8_t)(bgra[2] >> 3);
        g = (uint8_t)(bgra[1] >> 2);
        b = (uint8_t)(bgra[0] >> 3);
        image.palette[i].r = (uint8_t)((r << 3) | (r >> 2));
        image.palette[i].g = (uint8_t)((g << 2) | (g >> 4));
        image.palette[i].b = (uint8_t)((b << 3) | (b >> 2));
        image.palette[i].a = 255;
    }

    /* Read straight into persistent packed storage. Skip BMP row padding
     * by offset; reverse file order for positive (bottom-up) BMP height.
     * No file-sized temp, SDL surface, RWops or extra row buffer. */
    for (fileY = 0; fileY < height; ++fileY) {
        uint32_t destY = signedHeight > 0 ? height - 1U - fileY : fileY;
        if (!input->readRange(input->context,
                              (uint32_t)((uint64_t)pixelOffset +
                                         (uint64_t)fileY * filePitch),
                              image.pixels + (size_t)destY * (size_t)pitch,
                              (uint32_t)pitch)) {
            status = ESP_NATIVE_IMAGE_TRUNCATED;
            goto fail;
        }
    }

    *output = image;
    return ESP_NATIVE_IMAGE_OK;

fail:
    imageFree(input, image.pixels);
    imageFree(input, image.palette);
    return status;
}

static int packRange(void* context, uint32_t offset, void* dest, uint32_t bytes) {
    return EspAssetPack_readRange((const EspAssetPackEntry*)context,
                                  offset, dest, (size_t)bytes);
}

EspNativeImageStatus EspNativeImage_loadPack(const char* name,
                                             int transparentMask,
                                             EspNativeIndexedImage* output) {
    EspAssetPackEntry entry;
    EspNativeImageInput input;
    EspNativeImageStatus result;
    if (output != NULL) memset(output, 0, sizeof(*output));
    if (output == NULL || name == NULL || name[0] == '\0')
        return ESP_NATIVE_IMAGE_INVALID;

    /* Match EspLegacyAssetSource's exclusive lease semantics. */
    if (EspAssetPack_isOpen() ||
        !EspAssetPack_open(ESP_ASSET_PACK_DEFAULT_PATH))
        return ESP_NATIVE_IMAGE_PACK_UNAVAILABLE;
    if (!EspAssetPack_findEntry(name, &entry) ||
        (entry.flags & ESP_ASSET_PACK_FLAG_DIRECTORY) != 0U ||
        entry.size == 0U) {
        EspAssetPack_close();
        return ESP_NATIVE_IMAGE_PACK_UNAVAILABLE;
    }

    memset(&input, 0, sizeof(input));
    input.context = &entry;
    input.size = entry.size;
    input.readRange = packRange;
    result = EspNativeImage_decode(&input, transparentMask, output);
    EspAssetPack_close();

    if (result == ESP_NATIVE_IMAGE_OK) {
        printf("[NATIVEIMAGE] DECODE %dx%d %u-bpp packed colors=%u bytes=%u key=%u\n",
               output->width, output->height, (unsigned)output->bitsPerPixel,
               (unsigned)output->paletteSize,
               (unsigned)((size_t)output->pitch * (size_t)output->height),
               (unsigned)output->hasColorKey);
    }
    return result;
}
