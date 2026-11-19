#ifndef DOOMRPG_ESP32_NATIVE_BOOT_LEGALS_H
#define DOOMRPG_ESP32_NATIVE_BOOT_LEGALS_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Optional /doomrpg/img/credits.bmp RGB24 128x640 (DIB40 or DIB124):
 * five 128x128 panels, 3s each or touch-to-skip. Missing/invalid/unreadable
 * SD asset: exactly four original g.bmp panels from PAK, and no ROM fifth
 * (the obsolete ESP32 CYD PORT screen was removed on main). No image-wide
 * allocation, new PAK asset or resident PAK lease; SD row scratch <= 1536B,
 * destination is the borrowed shared 160x120 RGB565 framebuffer. */
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
