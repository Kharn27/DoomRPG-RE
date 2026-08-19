#ifndef DOOMRPG_ESP32_RESOURCE_MEMORY_PLAN_PROBE_H
#define DOOMRPG_ESP32_RESOURCE_MEMORY_PLAN_PROBE_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Inspect the real menu-map graphics resource requirements after the validated
 * runtime map structures are resident.
 *
 * The probe may temporarily decompress bitshapes.bin when that transient fits
 * the current heap. It never calls Render_loadBitShapes() or
 * Render_loadTexels(), never retains new graphics payloads, and refuses any
 * diagnostic allocation that would exceed the measured ESP32 heap budget.
 */
int DoomRPG_probeMenuResourceMemoryPlan(int mapStructuresReady);

#ifdef __cplusplus
}
#endif

#endif
