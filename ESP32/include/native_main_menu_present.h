#ifndef DOOMRPG_ESP32_NATIVE_MAIN_MENU_PRESENT_H
#define DOOMRPG_ESP32_NATIVE_MAIN_MENU_PRESENT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct DoomRPG_s;
struct Render_s;

/* Shared presentation invariants for the pre-game native menu domain. */
uint32_t DoomRPG_esp32MainMenuFramebufferHash(const struct Render_s* render);
int DoomRPG_esp32MainMenuGraphicsBoundaryIsSafe(
    const struct DoomRPG_s* doomRpg);

/* Recover only while ST_MENU still owns the UI. Rebuilds MENU_MAIN, repaints
 * the opaque dashboard and re-arms its touch owner. It deliberately refuses
 * to pull ST_INTRO/ST_PLAYING back into the menu after a real transition.
 */
int DoomRPG_esp32MainMenuRecover(struct DoomRPG_s* doomRpg,
                                 const char* reason);

#ifdef __cplusplus
}
#endif

#endif
