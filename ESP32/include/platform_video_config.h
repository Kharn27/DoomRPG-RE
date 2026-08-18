#ifndef DOOMRPG_ESP32_PLATFORM_VIDEO_CONFIG_H
#define DOOMRPG_ESP32_PLATFORM_VIDEO_CONFIG_H

/*
 * Doom RPG renders into a small RGB565 software framebuffer on the CYD.
 * 160x120 maps exactly to the physical 320x240 panel with a 2x nearest-
 * neighbour upscale in both axes.
 *
 * Keep these constants usable from both C engine stubs and C++ platform code.
 */
#define DOOMRPG_LOGICAL_WIDTH 160
#define DOOMRPG_LOGICAL_HEIGHT 120
#define DOOMRPG_PHYSICAL_WIDTH 320
#define DOOMRPG_PHYSICAL_HEIGHT 240
#define DOOMRPG_INTEGER_SCALE 2

#if DOOMRPG_LOGICAL_WIDTH * DOOMRPG_INTEGER_SCALE != DOOMRPG_PHYSICAL_WIDTH
#error "Doom RPG CYD horizontal scale must remain integer"
#endif

#if DOOMRPG_LOGICAL_HEIGHT * DOOMRPG_INTEGER_SCALE != DOOMRPG_PHYSICAL_HEIGHT
#error "Doom RPG CYD vertical scale must remain integer"
#endif

#endif
