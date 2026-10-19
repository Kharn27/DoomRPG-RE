#ifndef ESP_NATIVE_CANVAS_TEXT_H
#define ESP_NATIVE_CANVAS_TEXT_H

#include "DoomCanvas.h"

/* CYD-owned immutable text glyph entry. No buffering or allocation.
 * Draws to the shared native canvas; the caller owns presentation. */
#ifdef __cplusplus
extern "C" {
#endif
void EspNativeCanvas_drawText(DoomCanvas_t* canvas,
                              const char* text, int x, int y, int flags);
#ifdef __cplusplus
}
#endif
#endif
