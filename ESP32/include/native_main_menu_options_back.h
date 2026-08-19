#ifndef DOOMRPG_ESP32_NATIVE_MAIN_MENU_OPTIONS_BACK_H
#define DOOMRPG_ESP32_NATIVE_MAIN_MENU_OPTIONS_BACK_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct DoomRPG_s;

/* Arm the deliberately narrow MENU_MAIN_OPTIONS touch frontend. Only Back is
 * interactive in this increment; Video/Input/Sound remain deferred.
 */
int DoomRPG_esp32OptionsBackActivate(struct DoomRPG_s* doomRpg,
                                     uint32_t optionsFramebufferFNV);

#ifdef __cplusplus
}
#endif

#endif
