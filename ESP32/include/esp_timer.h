#ifndef DOOMRPG_ESP32_ESP_TIMER_COMPAT_H
#define DOOMRPG_ESP32_ESP_TIMER_COMPAT_H

/*
 * ESP-IDF's esp_timer.h eventually exposes the C stdbool true/false macros.
 * The recovered Doom RPG headers predate stdbool and declare their own
 * `typedef enum { false, true } boolean;`.  Any ESP32-native source that needs
 * esp_timer after SDL must therefore establish the legacy boolean type first.
 *
 * Keep this compatibility boundary ESP32-local: do not alter the desktop/J2ME
 * executable specification just to satisfy an ESP-IDF include-order detail.
 */
#include <SDL.h>
#include "DoomRPG.h"
#include_next <esp_timer.h>

#endif
