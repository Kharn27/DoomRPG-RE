#include <SDL.h>
#include <stdio.h>

#include "DoomRPG.h"
#include "Menu.h"
#include "esp_native_menu_state.h"
#include "esp_native_menu_storage.h"

EspNativeMenuState_t* EspNativeMenuStorage_init(EspNativeMenuState_t* storage) {
    if (storage == NULL) {
        storage = (EspNativeMenuState_t*)SDL_calloc(1, sizeof(EspNativeMenuState_t));
        if (storage == NULL) return NULL;
    }
    else {
        SDL_memset(storage, 0, sizeof(EspNativeMenuState_t));
    }

    storage->oldMenu = -1;
    storage->menu = MENU_NONE;

    printf("[MENUSTORAGE] INIT bytes=%u items=%u owner=esp-native compatibilityLayout=EspNativeMenuState_t desktopTU=no\n",
           (unsigned int)sizeof(EspNativeMenuState_t),
           (unsigned int)ESP_NATIVE_MENU_MAX_ITEMS);
    return storage;
}

int EspNativeMenuStorage_startup(EspNativeMenuState_t* storage, DoomRPG_t* doomRpg) {
    if (storage == NULL || doomRpg == NULL) return 0;

    DoomRPG_createImage(doomRpg, "j.bmp", true, &storage->imgLogo);
    storage->imgBG = NULL;

    if (storage->imgLogo.imgBitmap == NULL) {
        printf("[MENUSTORAGE] STARTUP FAILED logo=%p legacyAccessoryAssets=retired\n",
               (void*)storage->imgLogo.imgBitmap);
        return 0;
    }

    printf("[MENUSTORAGE] STARTUP READY assets=j owner=esp-native legacyAccessoryAssets=p/q-retired desktopMenuSystemStartup=no\n");
    return 1;
}

void EspNativeMenuStorage_free(EspNativeMenuState_t* storage, DoomRPG_t* doomRpg,
                               boolean freePtr) {
    if (storage == NULL) return;
    if (doomRpg != NULL) {
        DoomRPG_freeImage(doomRpg, &storage->imgLogo);
    }
    storage->imgBG = NULL;
    if (freePtr) SDL_free(storage);
}
