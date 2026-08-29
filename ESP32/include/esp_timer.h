#ifndef DOOMRPG_ESP32_ESP_TIMER_COMPAT_H
#define DOOMRPG_ESP32_ESP_TIMER_COMPAT_H

/*
 * Compatibility shim for the recovered Doom RPG C headers.
 *
 * IMPORTANT:
 * - In C, DoomRPG.h must establish its historical
 *   `typedef enum { false, true } boolean;` before ESP-IDF/stdBool exposes
 *   false/true macros.
 * - In C++, false/true are language keywords, so DoomRPG.h MUST NOT be
 *   injected here. Arduino/FreeRTOS includes esp_timer.h transitively from
 *   many .cpp translation units.
 *
 * This header therefore only establishes the legacy boolean boundary for C
 * translation units, then forwards to the real ESP-IDF header. Do not include
 * DoomRPG.h from framework-shadow headers in C++ paths.
 */
#ifndef __cplusplus
#include <SDL.h>
#include "DoomRPG.h"
#endif
#include_next <esp_timer.h>

#endif
