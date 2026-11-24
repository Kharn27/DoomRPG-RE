#ifndef DOOMRPG_ESP_NATIVE_RASTER_CONTRACT_H
#define DOOMRPG_ESP_NATIVE_RASTER_CONTRACT_H

/* Permanent, immutable native raster admission. This is not a new renderer.
 * A validated frame owns exactly ONE RGB565 framebuffer; no allocations,
 * external files, callbacks, PAK reads, or desktop Render_t dependencies.
 *
 * Mode is explicit: P23 must never infer the gameplay layout from a physical
 * TFT size or from the presence of the 192x144 preview framebuffer. */
#include <stddef.h>
#include <stdint.h>
#include "platform_video_config.h"

#define ESP_NATIVE_RASTER_CLASSIC_FRAME_WIDTH 160U
#define ESP_NATIVE_RASTER_CLASSIC_FRAME_HEIGHT 120U
#define ESP_NATIVE_RASTER_CLASSIC_WORLD_WIDTH 160U
#define ESP_NATIVE_RASTER_CLASSIC_WORLD_HEIGHT 80U
#define ESP_NATIVE_RASTER_CLASSIC_HUD_ROWS 20U
#define ESP_NATIVE_RASTER_RES60_FRAME_WIDTH 192U
#define ESP_NATIVE_RASTER_RES60_FRAME_HEIGHT 144U
#define ESP_NATIVE_RASTER_RES60_WORLD_WIDTH 192U
#define ESP_NATIVE_RASTER_RES60_WORLD_HEIGHT 96U
#define ESP_NATIVE_RASTER_RES60_HUD_ROWS 24U

#if DOOMRPG_RES60_P23_NATIVE_RASTER_GEOMETRY
#define ESP_NATIVE_RASTER_COLUMN_CAPACITY ESP_NATIVE_RASTER_RES60_WORLD_WIDTH
#else
#define ESP_NATIVE_RASTER_COLUMN_CAPACITY ESP_NATIVE_RASTER_CLASSIC_WORLD_WIDTH
#endif

typedef enum EspNativeRasterMode_e {
    ESP_NATIVE_RASTER_CLASSIC = 0,
    ESP_NATIVE_RASTER_RES60 = 1
} EspNativeRasterMode;

typedef struct EspNativeRasterContract_s {
    uint16_t frameWidth;
    uint16_t frameHeight;
    uint16_t pitchPixels;
    uint16_t worldX;
    uint16_t worldY;
    uint16_t worldWidth;
    uint16_t worldHeight;
    uint16_t hudRows;
    uint32_t framebufferBytes;
    uint8_t mode;
} EspNativeRasterContract;

/* No dynamic allocation or ownership transfer. The caller owns out. */
static inline int EspNativeRasterContract_select(EspNativeRasterMode mode,
                                                 EspNativeRasterContract* out)
{
    uint16_t width, height, worldHeight, hud;
    if (out == NULL) return 0;
    if (mode == ESP_NATIVE_RASTER_CLASSIC) {
        width = ESP_NATIVE_RASTER_CLASSIC_FRAME_WIDTH;
        height = ESP_NATIVE_RASTER_CLASSIC_FRAME_HEIGHT;
        worldHeight = ESP_NATIVE_RASTER_CLASSIC_WORLD_HEIGHT;
        hud = ESP_NATIVE_RASTER_CLASSIC_HUD_ROWS;
    } else if (mode == ESP_NATIVE_RASTER_RES60) {
        width = ESP_NATIVE_RASTER_RES60_FRAME_WIDTH;
        height = ESP_NATIVE_RASTER_RES60_FRAME_HEIGHT;
        worldHeight = ESP_NATIVE_RASTER_RES60_WORLD_HEIGHT;
        hud = ESP_NATIVE_RASTER_RES60_HUD_ROWS;
    } else {
        return 0;
    }
    out->frameWidth = width;
    out->frameHeight = height;
    out->pitchPixels = width;
    out->worldX = 0U;
    out->worldY = hud;
    out->worldWidth = width;
    out->worldHeight = worldHeight;
    out->hudRows = hud;
    out->framebufferBytes = (uint32_t)width * (uint32_t)height * 2U;
    out->mode = (uint8_t)mode;
    return 1;
}

/* Fail-closed admission: ONLY two known exact layouts and exact byte size.
 * A forged pitch, partial framebuffer, overlapping HUD, or unknown mode
 * cannot manufacture an in-bounds viewport from a different allocation. */
static inline int EspNativeRasterContract_validate(
    const EspNativeRasterContract* current, size_t actualFramebufferBytes)
{
    EspNativeRasterContract expected;
    if (current == NULL ||
        !EspNativeRasterContract_select((EspNativeRasterMode)current->mode,
                                         &expected) ||
        actualFramebufferBytes != (size_t)expected.framebufferBytes) return 0;
    return current->frameWidth == expected.frameWidth &&
           current->frameHeight == expected.frameHeight &&
           current->pitchPixels == expected.pitchPixels &&
           current->worldX == expected.worldX &&
           current->worldY == expected.worldY &&
           current->worldWidth == expected.worldWidth &&
           current->worldHeight == expected.worldHeight &&
           current->hudRows == expected.hudRows &&
           current->framebufferBytes == expected.framebufferBytes;
}

/* One caller-owned row span in RGB565 pixels. No hidden drawing or clipping;
 * offsets are indexed by actual framebuffer pitch, not 160x120 constants.
 * Reject any row outside the world, including both HUD bands. */
static inline int EspNativeRasterContract_worldRow(
    const EspNativeRasterContract* current, size_t framebufferBytes,
    uint16_t worldRow, size_t* pixelOffset, size_t* pixelCount)
{
    size_t index;
    if (pixelOffset == NULL || pixelCount == NULL ||
        !EspNativeRasterContract_validate(current, framebufferBytes) ||
        worldRow >= current->worldHeight) return 0;
    index = ((size_t)current->worldY + worldRow) * current->pitchPixels +
             current->worldX;
    if (index + current->worldWidth > framebufferBytes / 2U) return 0;
    *pixelOffset = index;
    *pixelCount = current->worldWidth;
    return 1;
}
#endif /* DOOMRPG_ESP_NATIVE_RASTER_CONTRACT_H */
