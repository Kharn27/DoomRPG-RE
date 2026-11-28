#ifndef DOOMRPG_ESP32_PLATFORM_VIDEO_H
#define DOOMRPG_ESP32_PLATFORM_VIDEO_H

#include <stddef.h>
#include <stdint.h>

#ifndef DOOMRPG_ESP32_TOUCH_HITBOX_OVERLAY
#define DOOMRPG_ESP32_TOUCH_HITBOX_OVERLAY 0
#endif

class TFT_eSPI;

bool PlatformVideo_begin(TFT_eSPI* display);
uint16_t* PlatformVideo_framebuffer();
size_t PlatformVideo_framebufferSizeBytes();
void PlatformVideo_clear(uint16_t color);
bool PlatformVideo_present();
/* Main-task service: only a 12x12 physical LCD region on blink/clear. */
void PlatformVideo_serviceTurnLamp();
/* Only the specified opaque region is pushed to ILI9341 GRAM (nearest x2).
 * The remaining LCD pixels are retained by the controller, not re-rendered. */
bool PlatformVideo_presentRegion(int logicalLeft, int logicalTop,
                                 int logicalWidth, int logicalHeight);
void PlatformVideo_showTestPattern();

/* P2: diagnostic HUD/world region raster. Never calls the game renderer. */
#if DOOMRPG_RES60_P2_LAYOUT
void PlatformVideo_showRes60LayoutPattern();
#endif

#if DOOMRPG_RES60_P3_WORLD
/* Reads compact original BSP; the 3D viewport is flat shaded only. */
void PlatformVideo_showRes60WorldPreview();
#endif

#if DOOMRPG_RES60_P7_NAV
/* P7: original Entrance BSP at one of four real cardinal orientations;
 * native turn/menu touch chassis overlays only the 24px HUD bands. */
bool PlatformVideo_showRes60WorldPreviewQuarterTurns(uint8_t turns);
#endif

#if DOOMRPG_RES60_P5_TITLE
/* PAK-backed original title, only in 192x144 pilot. */
bool PlatformVideo_showRes60TitlePreview();
#endif

#if DOOMRPG_RES60_P6_MENU
/* Original logo + native industrial 160x120 skin translated inside 192x144. */
bool PlatformVideo_showRes60MainMenu(int8_t selected);
bool PlatformVideo_updateRes60MenuArmed(int8_t previous, int8_t selected);
#endif

#if DOOMRPG_ESP32_TOUCH_HITBOX_OVERLAY
/* Bring-up-only diagnostics drawn directly on the physical TFT after the game
 * framebuffer has been presented. These never modify the 160x120 framebuffer,
 * so deterministic framebuffer hashes remain valid.
 */
void PlatformVideo_debugOverlayClear();
void PlatformVideo_debugOverlaySetZone(int index,
                                       int16_t logicalLeft,
                                       int16_t logicalTop,
                                       int16_t logicalRight,
                                       int16_t logicalBottom);
void PlatformVideo_debugOverlayRefresh();
void PlatformVideo_debugOverlayMarkTouch(int16_t physicalX,
                                         int16_t physicalY,
                                         uint16_t pressure,
                                         uint16_t rawX,
                                         uint16_t rawY);
#endif

#endif
