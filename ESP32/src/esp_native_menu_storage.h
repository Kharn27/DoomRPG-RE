#ifndef ESP_NATIVE_MENU_STORAGE_H
#define ESP_NATIVE_MENU_STORAGE_H

#include "DoomRPG.h"
#include "MenuSystem.h"

#ifdef __cplusplus
extern "C" {
#endif

MenuSystem_t* EspNativeMenuStorage_init(MenuSystem_t* storage,
                                        DoomRPG_t* doomRpg);
int EspNativeMenuStorage_startup(MenuSystem_t* storage);
void EspNativeMenuStorage_free(MenuSystem_t* storage, boolean freePtr);

#ifdef __cplusplus
}
#endif

#endif
