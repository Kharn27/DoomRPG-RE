#ifndef DOOMRPG_ESP32_NATIVE_GAMEPLAY_GIB_SHEET_H
#define DOOMRPG_ESP32_NATIVE_GAMEPLAY_GIB_SHEET_H

#include <stdint.h>

/* ParticleSystem_startup chooses gibs_24.bmp when displayRect.w >= 160.
 * ParticleSystem_render selects one of four 24px frames per row and one of
 * six negative particleSize rows. The 160x120 CYD therefore uses a
 * 96x144 indexed sheet: never a monolithic sprite-cache or desktop graph. */
#define ESP_NATIVE_GIB_SHEET_FRAME_WH 24U
#define ESP_NATIVE_GIB_SHEET_FRAMES 4U
#define ESP_NATIVE_GIB_SHEET_ROWS 6U
#define ESP_NATIVE_GIB_SHEET_WIDTH 96U
#define ESP_NATIVE_GIB_SHEET_HEIGHT 144U
#define ESP_NATIVE_GIB_FRAME_MS 175U

typedef struct EspNativeGibSheetRect_s {
    uint16_t sourceX;
    uint16_t sourceY;
    uint16_t width;
    uint16_t height;
    int16_t destinationX;
    int16_t destinationY;
    uint8_t frame;
    uint8_t variant;
} EspNativeGibSheetRect;

/* Exact source frame/row selection and viewport clipping, independent of
 * asset availability. VelX sign mirrors the 0..3 frame direction.
 * Returns 0 for malformed metadata, invalid row or completely offscreen. */
/* DoomRPG_createImageBerserkColor(): red+8 saturate; green/blue half.
 * The magenta transparency index must remain keyed by the caller. */
uint16_t EspNativeGibSheet_berserk565(uint16_t original);

int EspNativeGibSheet_plan(uint32_t presentMs,
                           uint8_t row,
                           int16_t centerX,
                           int16_t centerY,
                           int16_t velX,
                           uint16_t sheetWidth,
                           uint16_t sheetHeight,
                           EspNativeGibSheetRect* outRect);

#endif
