#ifndef DOOMRPG_ESP32_NATIVE_BOOT_LEGALS_H
#define DOOMRPG_ESP32_NATIVE_BOOT_LEGALS_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Four original 128x128 g.bmp panels, followed by a 2s author-credit
 * screen in the same 160x120 framebuffer. No image-wide allocation, no new
 * PAK asset, and no resident PAK lease. Touch advances each screen. */
int EspNativeBootLegals_begin(uint32_t nowMs);
int EspNativeBootLegals_draw(uint16_t* framebuffer, size_t pixelCount);
/* 0: still showing; 1: draw the next panel; 2: last panel completed. */
int EspNativeBootLegals_advance(uint32_t nowMs, int touchPressedEdge);
void EspNativeBootLegals_cancel(void);
int EspNativeBootLegals_isActive(void);
unsigned int EspNativeBootLegals_currentPanel(void);

#ifdef __cplusplus
}
#endif

#endif
