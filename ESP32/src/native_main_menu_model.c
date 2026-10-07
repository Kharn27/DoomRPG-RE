#include <SDL.h>
#include <stdio.h>

#include "DoomRPG.h"
#include "DoomCanvas.h"
#include "Menu.h"
#include "esp_native_menu_state.h"

#include "esp_legacy_asset_source.h"
#include "native_main_menu_model.h"
#include "platform_video_config.h"

#define MAIN_HELP_ASSET_NAME "help.txt"
#define MAIN_HELP_HEADER_BYTES 3U
#define MAIN_HELP_ASSET_MAX_BYTES 4096U
#define MAIN_HELP_LINE_MAX_CHARS 31
#define MAIN_HELP_MAX_LINES 96

static uint8_t* helpData;
static uint16_t helpLineOffsets[MAIN_HELP_MAX_LINES];
static int helpLineCount;
static uint32_t helpAssetSize;

static void releaseHelpData(void) {
    if (helpData != NULL) {
        SDL_free(helpData);
        helpData = NULL;
    }
    helpLineCount = 0;
    helpAssetSize = 0U;
}

const char* DoomRPG_esp32MainMenuHelpLine(int index) {
    if (helpData == NULL || index < 0 || index >= helpLineCount) return NULL;
    if ((uint32_t)helpLineOffsets[index] >= helpAssetSize) return NULL;
    return (const char*)&helpData[helpLineOffsets[index]];
}

int DoomRPG_esp32MainMenuHelpLineCount(void) {
    return helpLineCount;
}

static int supportedModel(int menuId) {
    return menuId == MENU_MAIN ||
           menuId == MENU_MAIN_HELP_ABOUT ||
           menuId == MENU_MAIN_CONTINUE ||
           menuId == MENU_MAIN_OPTIONS;
}

static void setFixedItem(MenuItem_t* item, const char* text, int flags) {
    if (item == NULL) return;
    SDL_memset(item, 0, sizeof(*item));
    if (text != NULL) {
        SDL_snprintf(item->textField, sizeof(item->textField), "%s", text);
    }
    item->flags = (byte)flags;
    item->action = 0;
}


static void resetFixedModel(DoomRPG_t* doomRpg) {
    EspNativeMenuState_t* menuSystem = doomRpg->menuSystem;

    menuSystem->scrollIndex = 0;
    menuSystem->selectedIndex = 0;
    menuSystem->numItems = 0;
}

int DoomRPG_esp32MainMenuModelBuildMain(struct DoomRPG_s* doomRpgBase) {
    DoomRPG_t* doomRpg = (DoomRPG_t*)doomRpgBase;
    EspNativeMenuState_t* menuSystem;

    if (doomRpg == NULL || doomRpg->menuSystem == NULL) {
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
    EspNativeMenuState_t* menuSystem = doomRpg->menuSystem;

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
    EspNativeMenuState_t* menuSystem = doomRpg->menuSystem;

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

static int buildBoundedHelpModel(DoomRPG_t* doomRpg) {
    EspNativeMenuState_t* menuSystem;
    uint8_t* data = NULL;
    uint32_t assetSize = 0U;
    int readSize = 0;
    int declaredItems;
    int item;
    uint32_t readPos = MAIN_HELP_HEADER_BYTES;
    uint32_t writePos = MAIN_HELP_HEADER_BYTES;

    releaseHelpData();

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
               readSize, (unsigned int)assetSize);
        if (data != NULL) SDL_free(data);
        return 0;
    }

    declaredItems = (int)data[1] + ((int)data[0] * 10) - 528;
    if (declaredItems <= 0 || declaredItems > MAIN_HELP_MAX_LINES) {
        printf("[MAINMODEL] HELP-PARSE FAILED declaredItems=%d header=%02x/%02x/%02x lineBound=%d\n",
               declaredItems,
               (unsigned int)data[0],
               (unsigned int)data[1],
               (unsigned int)data[2],
               MAIN_HELP_MAX_LINES);
        SDL_free(data);
        return 0;
    }

    menuSystem = doomRpg->menuSystem;
    menuSystem->scrollIndex = 0;
    menuSystem->selectedIndex = 0;
    menuSystem->numItems = declaredItems;
    menuSystem->imgBG = NULL;
    menuSystem->oldMenu = MENU_MAIN;
    menuSystem->type = 5;

    for (item = 0; item < declaredItems; ++item) {
        int lineLength = 0;
        int terminated = 0;

        helpLineOffsets[item] = (uint16_t)writePos;
        while (readPos < assetSize) {
            uint8_t ch = data[readPos++];

            if (ch == 0U) {
                printf("[MAINMODEL] HELP-PARSE FAILED premature-NUL item=%d offset=%u/%u\n",
                       item,
                       (unsigned int)(readPos - 1U),
                       (unsigned int)assetSize);
                SDL_free(data);
                menuSystem->numItems = 0;
                return 0;
            }

            if (ch == (uint8_t)'\n') {
                data[writePos++] = 0U;
                terminated = 1;
                break;
            }

            if (ch == (uint8_t)'\r') continue;

            if (lineLength >= MAIN_HELP_LINE_MAX_CHARS || writePos >= assetSize) {
                printf("[MAINMODEL] HELP-PARSE FAILED line-too-long item=%d offset=%u len>%d\n",
                       item,
                       (unsigned int)(readPos - 1U),
                       MAIN_HELP_LINE_MAX_CHARS);
                SDL_free(data);
                menuSystem->numItems = 0;
                return 0;
            }

            data[writePos++] = ch == (uint8_t)'~' ? 0x80U : ch;
            ++lineLength;
        }

        if (!terminated) {
            printf("[MAINMODEL] HELP-PARSE FAILED truncated item=%d offset=%u/%u\n",
                   item,
                   (unsigned int)readPos,
                   (unsigned int)assetSize);
            SDL_free(data);
            menuSystem->numItems = 0;
            return 0;
        }
    }

    helpData = data;
    helpLineCount = declaredItems;
    helpAssetSize = assetSize;

    printf("[MAINMODEL] HELP-PARSE bytes=%u header=%02x/%02x/%02x declared=%d parsed=%d consumed=%u compactBytes=%u menuItemSlots=%d lineChars<=%d trailing=%u headerTrusted=no source=pak result=valid\n",
           (unsigned int)assetSize,
           (unsigned int)data[0],
           (unsigned int)data[1],
           (unsigned int)data[2],
           declaredItems,
           helpLineCount,
           (unsigned int)readPos,
           (unsigned int)writePos,
           ESP_NATIVE_MENU_MAX_ITEMS,
           MAIN_HELP_LINE_MAX_CHARS,
           (unsigned int)(assetSize - readPos));
    return 1;
}

int DoomRPG_esp32MainMenuModelEnter(struct DoomRPG_s* doomRpgBase,
                                    int menuId) {
    DoomRPG_t* doomRpg = (DoomRPG_t*)doomRpgBase;
    EspNativeMenuState_t* menuSystem;
    DoomCanvas_t* canvas;
    const char* builder = "Menu_initMenu-transitional";

    if (doomRpg == NULL || doomRpg->menuSystem == NULL ||
        doomRpg->doomCanvas == NULL || !supportedModel(menuId)) {
        printf("[MAINMODEL] FAILED enter target=%d objectGraph=%s\n",
               menuId,
               doomRpg != NULL ? "partial" : "null");
        return 0;
    }

    menuSystem = doomRpg->menuSystem;
    canvas = doomRpg->doomCanvas;

    if (menuId != MENU_MAIN_HELP_ABOUT) releaseHelpData();

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

    menuSystem->maxItems = DOOMRPG_VIEWPORT_HEIGHT / 12;

    if (menuSystem->numItems <= 0 ||
        (menuId == MENU_MAIN_HELP_ABOUT
             ? menuSystem->numItems > MAIN_HELP_MAX_LINES
             : menuSystem->numItems > ESP_NATIVE_MENU_MAX_ITEMS) ||
        menuSystem->selectedIndex < 0 ||
        menuSystem->selectedIndex >= menuSystem->numItems) {
        printf("[MAINMODEL] FAILED target=%d items=%d selected=%d\n",
               menuId,
               menuSystem->numItems,
               menuSystem->selectedIndex);
        return 0;
    }

    DoomCanvas_setState(canvas, ST_MENU);

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
    EspNativeMenuState_t* menuSystem;
    DoomCanvas_t* canvas;

    if (doomRpg == NULL || doomRpg->menuSystem == NULL ||
        doomRpg->doomCanvas == NULL) {
        printf("[MAINMODEL] FAILED leave objectGraph\n");
        return 0;
    }

    menuSystem = doomRpg->menuSystem;
    canvas = doomRpg->doomCanvas;

    releaseHelpData();
    menuSystem->menu = MENU_NONE;
    menuSystem->numItems = 0;
    printf("[MAINMODEL] LEAVE target=%d state=%d items=%d viewInvalidation=retired dispatcher=native\n",
           menuSystem->menu,
           canvas->state,
           menuSystem->numItems);
    return 1;
}
