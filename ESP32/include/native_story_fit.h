#ifndef DOOMRPG_ESP32_NATIVE_STORY_FIT_H
#define DOOMRPG_ESP32_NATIVE_STORY_FIT_H

#include "platform_video_config.h"

#define ESP32_STORY_VIRTUAL_SIZE 128
#define ESP32_STORY_VIEWPORT_SIZE DOOMRPG_LOGICAL_HEIGHT
#define ESP32_STORY_VIEWPORT_X \
    ((DOOMRPG_LOGICAL_WIDTH - ESP32_STORY_VIEWPORT_SIZE) / 2)
#define ESP32_STORY_VIEWPORT_Y 0
#define ESP32_STORY_BACKGROUND_WIDTH DOOMRPG_LOGICAL_WIDTH
#define ESP32_STORY_BACKGROUND_X 0

#if ESP32_STORY_VIEWPORT_SIZE > DOOMRPG_LOGICAL_WIDTH
#error "ESP32 story content viewport must fit inside the logical framebuffer"
#endif

#ifdef __cplusplus
extern "C" {
#endif

struct DoomCanvas_s;

/*
 * ESP32-native ST_INTRO renderer.
 *
 * Doom RPG's original story renderer assumes a 128x128 viewport. The classic
 * CYD framebuffer is deliberately 160x120. Keep text, sprites and line art in
 * the existing centered 120x120 aspect-preserving content viewport so glyphs
 * are not horizontally distorted, but stretch only the scrolling starfield
 * background to the full 160x120 surface. No intermediate framebuffer or extra
 * frame-sized RAM is used.
 */
void Esp32StoryFit_draw(struct DoomCanvas_s* doomCanvas);

#ifdef __cplusplus
}
#endif

#endif
