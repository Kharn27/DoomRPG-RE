#ifndef DOOMRPG_ESP32_NATIVE_MAIN_MENU_OVERLAY_PROBE_H
#define DOOMRPG_ESP32_NATIVE_MAIN_MENU_OVERLAY_PROBE_H

#ifdef __cplusplus
extern "C" {
#endif

struct DoomRPG_s;

/*
 * Dormant historical scene-to-dashboard bridge; absent from both linked ELFs.
 * It is not current boot ownership. Its body remains in the live painter TU
 * until a separate change validates GCC's altered inlining after its removal.
 */
int DoomRPG_probeNativeMainMenuOverlay(struct DoomRPG_s* doomRpg);

#ifdef __cplusplus
}
#endif

#endif
