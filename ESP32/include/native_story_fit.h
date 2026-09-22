#ifndef DOOMRPG_ESP32_NATIVE_STORY_FIT_H
#define DOOMRPG_ESP32_NATIVE_STORY_FIT_H

#include "platform_video_config.h"

#define ESP32_STORY_VIRTUAL_SIZE 128
#define ESP32_STORY_VIEWPORT_WIDTH DOOMRPG_LOGICAL_WIDTH
#define ESP32_STORY_VIEWPORT_HEIGHT DOOMRPG_LOGICAL_HEIGHT
#define ESP32_STORY_VIEWPORT_X 0
#define ESP32_STORY_VIEWPORT_Y 0

/* Retain the historical square-size name only as a source-compatibility alias
 * for code that uses the vertical extent. New geometry must use WIDTH/HEIGHT. */
#define ESP32_STORY_VIEWPORT_SIZE ESP32_STORY_VIEWPORT_HEIGHT

#if ESP32_STORY_VIEWPORT_WIDTH != DOOMRPG_LOGICAL_WIDTH || \
    ESP32_STORY_VIEWPORT_HEIGHT != DOOMRPG_LOGICAL_HEIGHT
#error "ESP32 story viewport must cover the logical framebuffer"
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
 * original 128x128 coordinates as a virtual space and maps them directly onto
 * the full 160x120 logical framebuffer. X and Y are scaled independently at
 * draw time; no intermediate framebuffer or extra frame-sized RAM is used.
 */
void Esp32StoryFit_draw(struct DoomCanvas_s* doomCanvas);

#ifdef __cplusplus
}
#endif

#endif
