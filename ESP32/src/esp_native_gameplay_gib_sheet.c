#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "esp_native_gameplay_gib_sheet.h"

int EspNativeGibSheet_plan(uint32_t presentMs,
                           uint8_t row,
                           int16_t centerX,
                           int16_t centerY,
                           int16_t velX,
                           uint16_t sheetWidth,
                           uint16_t sheetHeight,
                           EspNativeGibSheetRect* outRect) {
    int32_t sx, sy, x, y, w, h;
    uint8_t frame;
    if (outRect == NULL) return 0;
    memset(outRect,0,sizeof *outRect);
    if (row >= ESP_NATIVE_GIB_SHEET_ROWS ||
        sheetWidth < ESP_NATIVE_GIB_SHEET_WIDTH ||
        sheetHeight < ESP_NATIVE_GIB_SHEET_HEIGHT) return 0;
    frame = (uint8_t)((presentMs / ESP_NATIVE_GIB_FRAME_MS) & 3U);
    if (velX > 0) frame = 3U - frame;
    sx = (int32_t)frame * ESP_NATIVE_GIB_SHEET_FRAME_WH;
    sy = (int32_t)row * ESP_NATIVE_GIB_SHEET_FRAME_WH;
    x = (int32_t)centerX - 12;
    y = (int32_t)centerY - 12;
    w = 24; h = 24;
    /* No writes to HUD bars. Source rectangle follows each crop exactly. */
    if (x < 0) { sx -= x; w += x; x = 0; }
    if (y < 20) { sy += 20 - y; h -= 20 - y; y = 20; }
    if (x + w > 160) w = 160 - x;
    if (y + h > 100) h = 100 - y;
    if (w <= 0 || h <= 0 || sx < 0 || sy < 0 ||
        sx + w > sheetWidth || sy + h > sheetHeight) return 0;
    outRect->sourceX = (uint16_t)sx;
    outRect->sourceY = (uint16_t)sy;
    outRect->width = (uint16_t)w;
    outRect->height = (uint16_t)h;
    outRect->destinationX = (int16_t)x;
    outRect->destinationY = (int16_t)y;
    outRect->frame = frame;
    outRect->variant = row;
    return 1;
}
