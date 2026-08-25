#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "esp_native_indexed_bmp.h"

#define BMP_HEADER_BYTES 54U
#define BMP_INFO_MIN_BYTES 40U
#define BMP_PALETTE_ENTRY_BYTES 4U
#define BMP_TRANSPARENT_MAGENTA_RGB565 0xf81fU

static uint16_t readLe16(const uint8_t* p) {
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static uint32_t readLe32(const uint8_t* p) {
    return (uint32_t)p[0] |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static int32_t readLeS32(const uint8_t* p) {
    return (int32_t)readLe32(p);
}

static uint16_t rgb565(uint8_t red, uint8_t green, uint8_t blue) {
    return (uint16_t)((((uint16_t)red & 0xf8U) << 8) |
                      (((uint16_t)green & 0xfcU) << 3) |
                      ((uint16_t)blue >> 3));
}

static void addRead(EspNativeIndexedBmpStats* stats, uint32_t bytes) {
    if (stats == NULL) return;
    ++stats->packReads;
    stats->bytesRead += bytes;
}

static int readRange(const EspAssetPackEntry* entry,
                     uint32_t offset,
                     void* destination,
                     uint32_t bytes,
                     EspNativeIndexedBmpStats* stats) {
    if (!EspAssetPack_readRange(entry, offset, destination, bytes)) return 0;
    addRead(stats, bytes);
    return 1;
}

static uint16_t sampleIndex(const EspNativeIndexedBmp* bmp,
                            const uint8_t* row,
                            uint16_t x) {
    if (bmp->bitsPerPixel == 8U) {
        return row[x];
    }
    if (bmp->bitsPerPixel == 4U) {
        uint8_t value = row[x >> 1];
        return (uint16_t)((x & 1U) != 0U ? (value & 0x0fU)
                                             : ((value >> 4) & 0x0fU));
    }
    return (uint16_t)((row[x >> 3] >> (7U - (x & 7U))) & 1U);
}

static int readSourceRow(const EspNativeIndexedBmp* bmp,
                         uint16_t sourceY,
                         uint8_t row[ESP_NATIVE_INDEXED_BMP_MAX_ROW_BYTES],
                         EspNativeIndexedBmpStats* stats) {
    uint32_t fileY;
    uint32_t offset;

    if (bmp == NULL || sourceY >= bmp->height ||
        bmp->filePitch == 0U ||
        bmp->filePitch > ESP_NATIVE_INDEXED_BMP_MAX_ROW_BYTES) {
        return 0;
    }

    fileY = bmp->topDown ? sourceY : ((uint32_t)bmp->height - 1U - sourceY);
    offset = bmp->pixelOffset + fileY * (uint32_t)bmp->filePitch;
    if (!readRange(&bmp->entry, offset, row, bmp->filePitch, stats)) return 0;
    if (stats != NULL) ++stats->rowsRead;
    return 1;
}

EspNativeIndexedBmpStatus EspNativeIndexedBmp_open(
    const char* name,
    EspNativeIndexedBmp* outBmp,
    EspNativeIndexedBmpStats* stats) {
    uint8_t header[BMP_HEADER_BYTES];
    uint8_t paletteRaw[ESP_NATIVE_INDEXED_BMP_MAX_PALETTE *
                       BMP_PALETTE_ENTRY_BYTES];
    EspAssetPackEntry entry;
    uint32_t dibSize;
    int32_t width;
    int32_t signedHeight;
    uint32_t height;
    uint16_t bitsPerPixel;
    uint32_t compression;
    uint32_t paletteCount;
    uint32_t paletteOffset;
    uint32_t paletteBytes;
    uint32_t pixelOffset;
    uint64_t rowBits;
    uint64_t packedPitch;
    uint64_t filePitch;
    uint64_t pixelBytes;
    uint32_t i;

    if (outBmp != NULL) memset(outBmp, 0, sizeof(*outBmp));
    if (name == NULL || outBmp == NULL) return ESP_NATIVE_INDEXED_BMP_INVALID;
    if (!EspAssetPack_isOpen()) return ESP_NATIVE_INDEXED_BMP_PACK_NOT_OPEN;
    if (!EspAssetPack_findEntry(name, &entry)) return ESP_NATIVE_INDEXED_BMP_NOT_FOUND;
    if (entry.size < BMP_HEADER_BYTES ||
        !readRange(&entry, 0U, header, sizeof(header), stats)) {
        return ESP_NATIVE_INDEXED_BMP_READ_FAILED;
    }
    if (header[0] != 'B' || header[1] != 'M') {
        return ESP_NATIVE_INDEXED_BMP_UNSUPPORTED;
    }

    pixelOffset = readLe32(header + 10U);
    dibSize = readLe32(header + 14U);
    width = readLeS32(header + 18U);
    signedHeight = readLeS32(header + 22U);
    bitsPerPixel = readLe16(header + 28U);
    compression = readLe32(header + 30U);
    paletteCount = readLe32(header + 46U);

    if (dibSize < BMP_INFO_MIN_BYTES || width <= 0 || width > UINT16_MAX ||
        signedHeight == 0 || signedHeight == INT32_MIN || compression != 0U ||
        (bitsPerPixel != 1U && bitsPerPixel != 4U && bitsPerPixel != 8U)) {
        return ESP_NATIVE_INDEXED_BMP_UNSUPPORTED;
    }

    height = signedHeight < 0 ? (uint32_t)(-signedHeight)
                              : (uint32_t)signedHeight;
    if (height == 0U || height > UINT16_MAX) {
        return ESP_NATIVE_INDEXED_BMP_UNSUPPORTED;
    }
    if (paletteCount == 0U) paletteCount = 1U << bitsPerPixel;
    if (paletteCount == 0U || paletteCount > ESP_NATIVE_INDEXED_BMP_MAX_PALETTE) {
        return ESP_NATIVE_INDEXED_BMP_UNSUPPORTED;
    }

    paletteOffset = 14U + dibSize;
    paletteBytes = paletteCount * BMP_PALETTE_ENTRY_BYTES;
    if (paletteOffset > entry.size || paletteBytes > entry.size - paletteOffset ||
        pixelOffset < paletteOffset + paletteBytes || pixelOffset > entry.size) {
        return ESP_NATIVE_INDEXED_BMP_UNSUPPORTED;
    }

    rowBits = (uint64_t)(uint32_t)width * bitsPerPixel;
    packedPitch = (rowBits + 7U) / 8U;
    filePitch = ((rowBits + 31U) / 32U) * 4U;
    if (packedPitch == 0U || packedPitch > UINT16_MAX ||
        filePitch == 0U || filePitch > ESP_NATIVE_INDEXED_BMP_MAX_ROW_BYTES) {
        return ESP_NATIVE_INDEXED_BMP_UNSUPPORTED;
    }
    pixelBytes = filePitch * height;
    if (pixelBytes > entry.size - pixelOffset) {
        return ESP_NATIVE_INDEXED_BMP_UNSUPPORTED;
    }

    if (!readRange(&entry, paletteOffset, paletteRaw, paletteBytes, stats)) {
        return ESP_NATIVE_INDEXED_BMP_READ_FAILED;
    }

    memset(outBmp, 0, sizeof(*outBmp));
    outBmp->entry = entry;
    outBmp->pixelOffset = pixelOffset;
    outBmp->width = (uint16_t)width;
    outBmp->height = (uint16_t)height;
    outBmp->filePitch = (uint16_t)filePitch;
    outBmp->packedPitch = (uint16_t)packedPitch;
    outBmp->paletteCount = (uint16_t)paletteCount;
    outBmp->bitsPerPixel = (uint8_t)bitsPerPixel;
    outBmp->topDown = signedHeight < 0 ? 1U : 0U;

    for (i = 0U; i < paletteCount; ++i) {
        const uint8_t* bgra = &paletteRaw[i * BMP_PALETTE_ENTRY_BYTES];
        outBmp->paletteRgb565[i] = rgb565(bgra[2], bgra[1], bgra[0]);
    }
    return ESP_NATIVE_INDEXED_BMP_OK;
}

EspNativeIndexedBmpStatus EspNativeIndexedBmp_blit(
    const EspNativeIndexedBmp* bmp,
    uint16_t* framebuffer,
    uint16_t framebufferWidth,
    uint16_t framebufferHeight,
    uint16_t sourceX,
    uint16_t sourceY,
    uint16_t width,
    uint16_t height,
    int16_t destinationX,
    int16_t destinationY,
    uint8_t transparentMagenta,
    EspNativeIndexedBmpStats* stats) {
    uint8_t row[ESP_NATIVE_INDEXED_BMP_MAX_ROW_BYTES];
    uint32_t sourceRight;
    uint32_t sourceBottom;
    uint16_t y;

    if (bmp == NULL || framebuffer == NULL || framebufferWidth == 0U ||
        framebufferHeight == 0U || width == 0U || height == 0U) {
        return ESP_NATIVE_INDEXED_BMP_INVALID;
    }
    sourceRight = (uint32_t)sourceX + width;
    sourceBottom = (uint32_t)sourceY + height;
    if (sourceRight > bmp->width || sourceBottom > bmp->height) {
        return ESP_NATIVE_INDEXED_BMP_UNSUPPORTED;
    }

    for (y = 0U; y < height; ++y) {
        int32_t dy = (int32_t)destinationY + y;
        uint16_t x;
        if (dy < 0 || dy >= framebufferHeight) continue;
        if (!readSourceRow(bmp, (uint16_t)(sourceY + y), row, stats)) {
            return ESP_NATIVE_INDEXED_BMP_READ_FAILED;
        }
        for (x = 0U; x < width; ++x) {
            int32_t dx = (int32_t)destinationX + x;
            uint16_t index;
            uint16_t color;
            if (dx < 0 || dx >= framebufferWidth) continue;
            index = sampleIndex(bmp, row, (uint16_t)(sourceX + x));
            if (index >= bmp->paletteCount) {
                return ESP_NATIVE_INDEXED_BMP_UNSUPPORTED;
            }
            color = bmp->paletteRgb565[index];
            if (transparentMagenta != 0U &&
                color == BMP_TRANSPARENT_MAGENTA_RGB565) {
                continue;
            }
            framebuffer[(uint32_t)dy * framebufferWidth + (uint32_t)dx] = color;
            if (stats != NULL) ++stats->pixelsWritten;
        }
    }
    return ESP_NATIVE_INDEXED_BMP_OK;
}

EspNativeIndexedBmpStatus EspNativeIndexedBmp_tile(
    const EspNativeIndexedBmp* bmp,
    uint16_t* framebuffer,
    uint16_t framebufferWidth,
    uint16_t framebufferHeight,
    int16_t destinationX,
    int16_t destinationY,
    uint16_t width,
    uint16_t height,
    uint8_t transparentMagenta,
    EspNativeIndexedBmpStats* stats) {
    uint8_t row[ESP_NATIVE_INDEXED_BMP_MAX_ROW_BYTES];
    uint16_t y;

    if (bmp == NULL || framebuffer == NULL || framebufferWidth == 0U ||
        framebufferHeight == 0U || bmp->width == 0U || bmp->height == 0U ||
        width == 0U || height == 0U) {
        return ESP_NATIVE_INDEXED_BMP_INVALID;
    }

    for (y = 0U; y < height; ++y) {
        int32_t dy = (int32_t)destinationY + y;
        uint16_t sourceY;
        uint16_t x;
        if (dy < 0 || dy >= framebufferHeight) continue;
        sourceY = (uint16_t)(y % bmp->height);
        if (!readSourceRow(bmp, sourceY, row, stats)) {
            return ESP_NATIVE_INDEXED_BMP_READ_FAILED;
        }
        for (x = 0U; x < width; ++x) {
            int32_t dx = (int32_t)destinationX + x;
            uint16_t sourceX;
            uint16_t index;
            uint16_t color;
            if (dx < 0 || dx >= framebufferWidth) continue;
            sourceX = (uint16_t)(x % bmp->width);
            index = sampleIndex(bmp, row, sourceX);
            if (index >= bmp->paletteCount) {
                return ESP_NATIVE_INDEXED_BMP_UNSUPPORTED;
            }
            color = bmp->paletteRgb565[index];
            if (transparentMagenta != 0U &&
                color == BMP_TRANSPARENT_MAGENTA_RGB565) {
                continue;
            }
            framebuffer[(uint32_t)dy * framebufferWidth + (uint32_t)dx] = color;
            if (stats != NULL) ++stats->pixelsWritten;
        }
    }
    return ESP_NATIVE_INDEXED_BMP_OK;
}
