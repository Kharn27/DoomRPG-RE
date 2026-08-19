#ifndef DOOMRPG_MENU_BSP_STRUCTURE_PROBE_H
#define DOOMRPG_MENU_BSP_STRUCTURE_PROBE_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Parse the complete serialized structure section of /menu.bsp without
 * allocating runtime map structures. The probe mirrors the read order used by
 * Render_beginLoadMapData(), reports the real ESP32 sizeof() costs, verifies
 * that the parser reaches the exact end of the BSP and estimates the peak
 * structural allocation payload while the decompressed BSP is resident.
 *
 * Returns non-zero only when the previous menu BSP header probe succeeded and
 * the complete BSP layout can be parsed safely.
 */
int DoomRPG_probeMenuBspStructure(int menuBspReady);

#ifdef __cplusplus
}
#endif

#endif
