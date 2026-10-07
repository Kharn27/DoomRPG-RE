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

/* Permanent classic-CYD logical layout. DoomCanvas does not own mutable
 * geometry on ESP32; the hardware target is fixed at 160x120 logical. */
#define DOOMRPG_CANVAS_X 0
#define DOOMRPG_CANVAS_Y 0
#define DOOMRPG_CANVAS_WIDTH DOOMRPG_LOGICAL_WIDTH
#define DOOMRPG_CANVAS_HEIGHT DOOMRPG_LOGICAL_HEIGHT
#define DOOMRPG_CANVAS_CENTER_X (DOOMRPG_LOGICAL_WIDTH / 2)
#define DOOMRPG_CANVAS_CENTER_Y (DOOMRPG_LOGICAL_HEIGHT / 2)
#define DOOMRPG_HUD_TOP_HEIGHT 20
#define DOOMRPG_HUD_BOTTOM_HEIGHT 20
#define DOOMRPG_VIEWPORT_X 0
#define DOOMRPG_VIEWPORT_Y DOOMRPG_HUD_TOP_HEIGHT
#define DOOMRPG_VIEWPORT_WIDTH DOOMRPG_LOGICAL_WIDTH
#define DOOMRPG_VIEWPORT_HEIGHT \
    (DOOMRPG_LOGICAL_HEIGHT - DOOMRPG_HUD_TOP_HEIGHT - DOOMRPG_HUD_BOTTOM_HEIGHT)

#if DOOMRPG_VIEWPORT_HEIGHT != 80
#error "Classic CYD gameplay viewport must remain 160x80"
#endif

#if DOOMRPG_LOGICAL_WIDTH * DOOMRPG_INTEGER_SCALE != DOOMRPG_PHYSICAL_WIDTH
#error "Doom RPG CYD horizontal scale must remain integer"
#endif

#if DOOMRPG_LOGICAL_HEIGHT * DOOMRPG_INTEGER_SCALE != DOOMRPG_PHYSICAL_HEIGHT
#error "Doom RPG CYD vertical scale must remain integer"
#endif

#endif
