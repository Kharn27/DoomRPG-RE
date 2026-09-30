#include <SDL.h>
#include <stdio.h>

#include "DoomRPG.h"
#include "DoomCanvas.h"
#include "Hud.h"
#include "Menu.h"
#include "MenuItem.h"
#include "MenuSystem.h"

#include "esp_legacy_asset_source.h"
#include "native_main_menu_model.h"

#define MAIN_HELP_ASSET_NAME "help.txt"
#define MAIN_HELP_HEADER_BYTES 3U
#define MAIN_HELP_ASSET_MAX_BYTES 4096U
#define MAIN_HELP_LINE_MAX_CHARS 31

static int supportedModel(int menuId) {
    return menuId == MENU_MAIN ||
           menuId == MENU_MAIN_HELP_ABOUT ||
           menuId == MENU_MAIN_CONTINUE ||
           menuId == MENU_MAIN_OPTIONS;
}

static void resetSelectionAccumulator(MenuSystem_t* menuSystem) {
    menuSystem->cheatCombo = 0;
    menuSystem->digitCount = 0;
}

static int appendHelpLine(MenuSystem_t* menuSystem,
                          const char* line,
                          int length) {
    char text[MAIN_HELP_LINE_MAX_CHARS + 1];

    if (menuSystem == NULL || line == NULL || length < 0 ||
        length > MAIN_HELP_LINE_MAX_CHARS ||
        menuSystem->numItems < 0 ||
        menuSystem->numItems >= MAX_MENUITEMS) {
        return 0;
    }

    SDL_memcpy(text, line, (size_t)length);
    text[length] = '\0';
    MenuItem_Set(&menuSystem->items[menuSystem->numItems], text, 0, 0);
    menuSystem->items[menuSystem->numItems].textField[
        sizeof(menuSystem->items[menuSystem->numItems].textField) - 1U] = '\0';
    menuSystem->numItems++;
    return 1;
}

static int buildBoundedHelpModel(DoomRPG_t* doomRpg) {
    MenuSystem_t* menuSystem;
    uint8_t* data = NULL;
    uint32_t assetSize = 0U;
    int readSize = 0;
    uint32_t pos;
    char line[MAIN_HELP_LINE_MAX_CHARS + 1];
    int lineLength = 0;
    int sawTerminator = 0;

    if (doomRpg == NULL || doomRpg->menuSystem == NULL ||
        !EspLegacyAssetSource_stat(MAIN_HELP_ASSET_NAME, &assetSize) ||
        assetSize < MAIN_HELP_HEADER_BYTES ||
        assetSize > MAIN_HELP_ASSET_MAX_BYTES) {
        printf("[MAINMODEL] HELP-PARSE FAILED stat bytes=%u bound=%u\n",
               (unsigned int)assetSize,
               (unsigned int)MAIN_HELP_ASSET_MAX_BYTES);
        return 0;
    }

    data = EspLegacyAssetSource_readAlloc(MAIN_HELP_ASSET_NAME, &readSize);
    if (data == NULL || readSize != (int)assetSize) {
        printf("[MAINMODEL] HELP-PARSE FAILED read bytes=%d expected=%u\n",
               readSize,
               (unsigned int)assetSize);
        if (data != NULL) SDL_free(data);
        return 0;
    }

    menuSystem = doomRpg->menuSystem;
    menuSystem->scrollIndex = 0;
    menuSystem->selectedIndex = 0;
    menuSystem->numItems = 0;
    menuSystem->setBind = false;
    menuSystem->imgBG = NULL;
    menuSystem->oldMenu = MENU_MAIN;
    menuSystem->type = 5;

    for (pos = MAIN_HELP_HEADER_BYTES; pos < assetSize; ++pos) {
        uint8_t c = data[pos];

        if (sawTerminator) {
            if (c != 0U) {
                printf("[MAINMODEL] HELP-PARSE FAILED data-after-NUL offset=%u\n",
                       (unsigned int)pos);
                SDL_free(data);
                menuSystem->numItems = 0;
                return 0;
            }
            continue;
        }

        if (c == 0U) {
            sawTerminator = 1;
            continue;
        }
        if (c == '\r') {
            continue;
        }
        if (c == '\n') {
            if (!appendHelpLine(menuSystem, line, lineLength)) {
                printf("[MAINMODEL] HELP-PARSE FAILED line/item bound item=%d len=%d\n",
                       menuSystem->numItems,
                       lineLength);
                SDL_free(data);
                menuSystem->numItems = 0;
                return 0;
            }
            lineLength = 0;
            continue;
        }

        if (lineLength >= MAIN_HELP_LINE_MAX_CHARS) {
            printf("[MAINMODEL] HELP-PARSE FAILED line-too-long item=%d len>%d\n",
                   menuSystem->numItems,
                   MAIN_HELP_LINE_MAX_CHARS);
            SDL_free(data);
            menuSystem->numItems = 0;
            return 0;
        }

        line[lineLength++] = (char)(c == (uint8_t)'~' ? 0x80U : c);
    }

    if (lineLength > 0) {
        if (!appendHelpLine(menuSystem, line, lineLength)) {
            printf("[MAINMODEL] HELP-PARSE FAILED final-line/item bound item=%d len=%d\n",
                   menuSystem->numItems,
                   lineLength);
            SDL_free(data);
            menuSystem->numItems = 0;
            return 0;
        }
    }

    printf("[MAINMODEL] HELP-PARSE bytes=%u header=%02x/%02x/%02x headerTrusted=no items=%d itemBound=%d lineChars<=%d transientFreed=yes source=pak result=%s\n",
           (unsigned int)assetSize,
           (unsigned int)data[0],
           (unsigned int)data[1],
           (unsigned int)data[2],
           menuSystem->numItems,
           MAX_MENUITEMS,
           MAIN_HELP_LINE_MAX_CHARS,
           menuSystem->numItems > 0 ? "valid" : "EMPTY");

    SDL_free(data);
    return menuSystem->numItems > 0;
}

int DoomRPG_esp32MainMenuModelEnter(struct DoomRPG_s* doomRpgBase,
                                    int menuId) {
    DoomRPG_t* doomRpg = (DoomRPG_t*)doomRpgBase;
    MenuSystem_t* menuSystem;
    DoomCanvas_t* canvas;
    const char* builder = "Menu_initMenu-transitional";

    if (doomRpg == NULL || doomRpg->menu == NULL ||
        doomRpg->menuSystem == NULL || doomRpg->doomCanvas == NULL ||
        !supportedModel(menuId)) {
        printf("[MAINMODEL] FAILED enter target=%d objectGraph=%s\n",
               menuId,
               doomRpg != NULL ? "partial" : "null");
        return 0;
    }

    menuSystem = doomRpg->menuSystem;
    canvas = doomRpg->doomCanvas;

    resetSelectionAccumulator(menuSystem);
    menuSystem->menu = menuId;

    if (menuId == MENU_MAIN_HELP_ABOUT) {
        builder = "native-bounded-help";
        if (!buildBoundedHelpModel(doomRpg)) {
            printf("[MAINMODEL] FAILED target=%d builder=%s\n",
                   menuId,
                   builder);
            return 0;
        }
    }
    else {
        Menu_initMenu(doomRpg->menu, menuId);
    }

    menuSystem->maxItems = canvas->screenRect.h / 12;

    if (menuSystem->numItems <= 0 ||
        menuSystem->numItems > MAX_MENUITEMS ||
        menuSystem->selectedIndex < 0 ||
        menuSystem->selectedIndex >= menuSystem->numItems) {
        printf("[MAINMODEL] FAILED target=%d items=%d selected=%d\n",
               menuId,
               menuSystem->numItems,
               menuSystem->selectedIndex);
        return 0;
    }

    DoomCanvas_setState(canvas, ST_MENU);
    menuSystem->paintMenu = true;

    printf("[MAINMODEL] ENTER target=%d type=%d old=%d items=%d selected=%d scroll=%d maxItems=%d state=%d dispatcher=native builder=%s\n",
           menuSystem->menu,
           menuSystem->type,
           menuSystem->oldMenu,
           menuSystem->numItems,
           menuSystem->selectedIndex,
           menuSystem->scrollIndex,
           menuSystem->maxItems,
           canvas->state,
           builder);
    return 1;
}

int DoomRPG_esp32MainMenuModelLeave(struct DoomRPG_s* doomRpgBase) {
    DoomRPG_t* doomRpg = (DoomRPG_t*)doomRpgBase;
    MenuSystem_t* menuSystem;
    DoomCanvas_t* canvas;

    if (doomRpg == NULL || doomRpg->menuSystem == NULL ||
        doomRpg->doomCanvas == NULL || doomRpg->hud == NULL) {
        printf("[MAINMODEL] FAILED leave objectGraph\n");
        return 0;
    }

    menuSystem = doomRpg->menuSystem;
    canvas = doomRpg->doomCanvas;

    resetSelectionAccumulator(menuSystem);
    menuSystem->menu = MENU_NONE;
    menuSystem->numItems = 0;
    doomRpg->hud->logMessage[0] = '\0';
    DoomCanvas_invalidateRectAndUpdateView(canvas);

    printf("[MAINMODEL] LEAVE target=%d state=%d items=%d staleView=%d updateView=%d dispatcher=native\n",
           menuSystem->menu,
           canvas->state,
           menuSystem->numItems,
           canvas->staleView ? 1 : 0,
           canvas->isUpdateView ? 1 : 0);
    return 1;
}
