#include <SDL.h>
#include <stdio.h>

#include "DoomRPG.h"
#include "Menu.h"
#include "MenuSystem.h"
#include "esp_native_menu_storage.h"

MenuSystem_t* EspNativeMenuStorage_init(MenuSystem_t* storage) {
    if (storage == NULL) {
        storage = (MenuSystem_t*)SDL_calloc(1, sizeof(MenuSystem_t));
        if (storage == NULL) return NULL;
    }
    else {
        SDL_memset(storage, 0, sizeof(MenuSystem_t));
    }

    storage->oldMenu = -1;
    storage->menu = MENU_NONE;

    printf("[MENUSTORAGE] INIT bytes=%u items=%u owner=esp-native compatibilityLayout=MenuSystem_t desktopTU=no\n",
           (unsigned int)sizeof(MenuSystem_t),
           (unsigned int)MAX_MENUITEMS);
    return storage;
}

int EspNativeMenuStorage_startup(MenuSystem_t* storage, DoomRPG_t* doomRpg) {
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

void EspNativeMenuStorage_free(MenuSystem_t* storage, DoomRPG_t* doomRpg,
                               boolean freePtr) {
    if (storage == NULL) return;
    if (doomRpg != NULL) {
        DoomRPG_freeImage(doomRpg, &storage->imgLogo);
    }
    storage->imgBG = NULL;
    if (freePtr) SDL_free(storage);
}
