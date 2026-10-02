#ifndef ESP_NATIVE_MENU_STORAGE_H
#define ESP_NATIVE_MENU_STORAGE_H

#include "DoomRPG.h"
#include "MenuSystem.h"

#ifdef __cplusplus
extern "C" {
#endif

EspNativeMenuState_t* EspNativeMenuStorage_init(EspNativeMenuState_t* storage);
int EspNativeMenuStorage_startup(EspNativeMenuState_t* storage, DoomRPG_t* doomRpg);
void EspNativeMenuStorage_free(EspNativeMenuState_t* storage, DoomRPG_t* doomRpg,
                               boolean freePtr);

#ifdef __cplusplus
}
#endif

#endif
