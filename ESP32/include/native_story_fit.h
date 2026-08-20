#ifndef DOOMRPG_ESP32_NATIVE_STORY_FIT_H
#define DOOMRPG_ESP32_NATIVE_STORY_FIT_H

#include "platform_video_config.h"

#define ESP32_STORY_VIRTUAL_SIZE 128
#define ESP32_STORY_VIEWPORT_SIZE DOOMRPG_LOGICAL_HEIGHT
#define ESP32_STORY_VIEWPORT_X \
    ((DOOMRPG_LOGICAL_WIDTH - ESP32_STORY_VIEWPORT_SIZE) / 2)
#define ESP32_STORY_VIEWPORT_Y 0

#if ESP32_STORY_VIEWPORT_SIZE > DOOMRPG_LOGICAL_WIDTH
#error "ESP32 story viewport must fit inside the logical framebuffer width"
#endif

#ifdef __cplusplus
extern "C" {
#endif

struct DoomCanvas_s;

/*
 * ESP32-native ST_INTRO renderer.
 *
 * Doom RPG's original story renderer assumes a 128x128 viewport. The classic
 * CYD framebuffer is deliberately 160x120, so the native port keeps the
 * original 128x128 coordinates as a virtual space and maps them directly into
 * a centered 120x120 viewport. Images, font glyphs, the menu hand and laser
 * lines are transformed at draw time; no intermediate framebuffer is used.
 */
void Esp32StoryFit_draw(struct DoomCanvas_s* doomCanvas);

#ifdef __cplusplus
}
#endif

#endif
