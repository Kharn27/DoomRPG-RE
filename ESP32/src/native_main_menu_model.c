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

/* MenuItem_Set() inherits a mutable char* signature from the desktop code, but
 * only copies the supplied label. Keep the const cast at this single boundary.
 */
static void setFixedItem(MenuItem_t* item, const char* text, int flags) {
    MenuItem_Set(item, (char*)text, flags, 0);
}


static void resetFixedModel(DoomRPG_t* doomRpg) {
    MenuSystem_t* menuSystem = doomRpg->menuSystem;

    doomRpg->hud->logMessage[0] = '\0';
    menuSystem->scrollIndex = 0;
    menuSystem->selectedIndex = 0;
    menuSystem->numItems = 0;
    menuSystem->setBind = false;
}

int DoomRPG_esp32MainMenuModelBuildMain(struct DoomRPG_s* doomRpgBase) {
    DoomRPG_t* doomRpg = (DoomRPG_t*)doomRpgBase;
    MenuSystem_t* menuSystem;

    if (doomRpg == NULL || doomRpg->menuSystem == NULL ||
        doomRpg->hud == NULL) {
        printf("[MAINMODEL] FAILED build-main objectGraph\n");
        return 0;
    }

    menuSystem = doomRpg->menuSystem;
    resetFixedModel(doomRpg);
    menuSystem->menu = MENU_MAIN;
    menuSystem->type = 4;
    menuSystem->imgBG = &menuSystem->imgLogo;
    menuSystem->oldMenu = -1;

    /*
     * Preserve the exact legacy MENU_MAIN model here. The finger-first painter
     * still adapts slots 1..3 to LOAD/OPTIONS/HELP before presentation.
     */
    setFixedItem(&menuSystem->items[menuSystem->numItems++], "Start Game", 2);
    setFixedItem(&menuSystem->items[menuSystem->numItems++], "Options   ", 2);
    setFixedItem(&menuSystem->items[menuSystem->numItems++], "Help/About", 2);
    setFixedItem(&menuSystem->items[menuSystem->numItems++], "Exit      ", 2);
    return menuSystem->numItems == 4;
}

static int buildFixedContinueModel(DoomRPG_t* doomRpg) {
    MenuSystem_t* menuSystem = doomRpg->menuSystem;

    resetFixedModel(doomRpg);
    menuSystem->menu = MENU_MAIN_CONTINUE;
    menuSystem->type = 4;
    menuSystem->imgBG = &menuSystem->imgLogo;
    menuSystem->oldMenu = MENU_MAIN;

    setFixedItem(&menuSystem->items[menuSystem->numItems++], "Continue", 2);
    setFixedItem(&menuSystem->items[menuSystem->numItems++], "New Game", 2);
    setFixedItem(&menuSystem->items[menuSystem->numItems++], "Back    ", 2);
    return menuSystem->numItems == 3;
}

static int buildFixedOptionsModel(DoomRPG_t* doomRpg) {
    MenuSystem_t* menuSystem = doomRpg->menuSystem;

    resetFixedModel(doomRpg);
    menuSystem->menu = MENU_MAIN_OPTIONS;
    menuSystem->type = 7;
    menuSystem->imgBG = &menuSystem->imgLogo;
    menuSystem->oldMenu = MENU_MAIN;

    setFixedItem(&menuSystem->items[menuSystem->numItems++], "Back", 0);
    setFixedItem(&menuSystem->items[menuSystem->numItems++], "Video", 0);
    setFixedItem(&menuSystem->items[menuSystem->numItems++], "Input", 0);
    setFixedItem(&menuSystem->items[menuSystem->numItems++], "Sound", 0);
    return menuSystem->numItems == 4;
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
    int declaredItems;
    int item;
    uint32_t pos = MAIN_HELP_HEADER_BYTES;

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

    /*
     * Preserve the original file format's logical line-count field, but never
     * trust it for memory access. The desktop parser used this arithmetic as an
     * unchecked loop bound:
     *
     *     data[1] + data[0] * 10 - 528
     *
     * Here it is only a declaration. It must fit MenuSystem_t, and every byte
     * consumed for every declared line is independently bounded by assetSize.
     */
    declaredItems = (int)data[1] + ((int)data[0] * 10) - 528;
    if (declaredItems <= 0 || declaredItems > MAX_MENUITEMS) {
        printf("[MAINMODEL] HELP-PARSE FAILED declaredItems=%d header=%02x/%02x/%02x itemBound=%d\n",
               declaredItems,
               (unsigned int)data[0],
               (unsigned int)data[1],
               (unsigned int)data[2],
               MAX_MENUITEMS);
        SDL_free(data);
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

    for (item = 0; item < declaredItems; ++item) {
        char line[MAIN_HELP_LINE_MAX_CHARS + 1];
        int lineLength = 0;
        int terminated = 0;

        while (pos < assetSize) {
            uint8_t ch = data[pos++];

            if (ch == 0U) {
                printf("[MAINMODEL] HELP-PARSE FAILED premature-NUL item=%d offset=%u/%u\n",
                       item,
                       (unsigned int)(pos - 1U),
                       (unsigned int)assetSize);
                SDL_free(data);
                menuSystem->numItems = 0;
                return 0;
            }

            if (ch == (uint8_t)'\n') {
                terminated = 1;
                break;
            }

            if (ch == (uint8_t)'\r') {
                continue;
            }

            if (lineLength >= MAIN_HELP_LINE_MAX_CHARS) {
                printf("[MAINMODEL] HELP-PARSE FAILED line-too-long item=%d offset=%u len>%d\n",
                       item,
                       (unsigned int)(pos - 1U),
                       MAIN_HELP_LINE_MAX_CHARS);
                SDL_free(data);
                menuSystem->numItems = 0;
                return 0;
            }

            line[lineLength++] =
                (char)(ch == (uint8_t)'~' ? 0x80U : ch);
        }

        if (!terminated) {
            printf("[MAINMODEL] HELP-PARSE FAILED truncated item=%d offset=%u/%u\n",
                   item,
                   (unsigned int)pos,
                   (unsigned int)assetSize);
            SDL_free(data);
            menuSystem->numItems = 0;
            return 0;
        }

        if (!appendHelpLine(menuSystem, line, lineLength)) {
            printf("[MAINMODEL] HELP-PARSE FAILED append item=%d len=%d\n",
                   item,
                   lineLength);
            SDL_free(data);
            menuSystem->numItems = 0;
            return 0;
        }
    }

    printf("[MAINMODEL] HELP-PARSE bytes=%u header=%02x/%02x/%02x declared=%d parsed=%d consumed=%u itemBound=%d lineChars<=%d trailing=%u headerTrusted=no source=pak result=valid\n",
           (unsigned int)assetSize,
           (unsigned int)data[0],
           (unsigned int)data[1],
           (unsigned int)data[2],
           declaredItems,
           menuSystem->numItems,
           (unsigned int)pos,
           MAX_MENUITEMS,
           MAIN_HELP_LINE_MAX_CHARS,
           (unsigned int)(assetSize - pos));

    SDL_free(data);
    return menuSystem->numItems == declaredItems;
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

    if (menuId == MENU_MAIN) {
        builder = "native-fixed-main";
        if (!DoomRPG_esp32MainMenuModelBuildMain(doomRpg)) {
            printf("[MAINMODEL] FAILED target=%d builder=%s\n",
                   menuId,
                   builder);
            return 0;
        }
    }
    else if (menuId == MENU_MAIN_HELP_ABOUT) {
        builder = "native-bounded-help";
        menuSystem->menu = menuId;
        if (!buildBoundedHelpModel(doomRpg)) {
            printf("[MAINMODEL] FAILED target=%d builder=%s\n",
                   menuId,
                   builder);
            return 0;
        }
    }
    else if (menuId == MENU_MAIN_CONTINUE) {
        builder = "native-fixed-continue";
        if (!buildFixedContinueModel(doomRpg)) {
            printf("[MAINMODEL] FAILED target=%d builder=%s\n",
                   menuId,
                   builder);
            return 0;
        }
    }
    else {
        builder = "native-fixed-options";
        if (!buildFixedOptionsModel(doomRpg)) {
            printf("[MAINMODEL] FAILED target=%d builder=%s\n",
                   menuId,
                   builder);
            return 0;
        }
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
