#include <stdio.h>

#include "DoomRPG.h"
#include "DoomCanvas.h"
#include "Hud.h"
#include "Menu.h"
#include "MenuSystem.h"

#include "native_main_menu_model.h"

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

int DoomRPG_esp32MainMenuModelEnter(struct DoomRPG_s* doomRpgBase,
                                    int menuId) {
    DoomRPG_t* doomRpg = (DoomRPG_t*)doomRpgBase;
    MenuSystem_t* menuSystem;
    DoomCanvas_t* canvas;

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
    Menu_initMenu(doomRpg->menu, menuId);
    menuSystem->maxItems = canvas->screenRect.h / 12;

    if (menuSystem->numItems <= 0 ||
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

    printf("[MAINMODEL] ENTER target=%d type=%d old=%d items=%d selected=%d scroll=%d maxItems=%d state=%d dispatcher=native builder=Menu_initMenu-transitional\n",
           menuSystem->menu,
           menuSystem->type,
           menuSystem->oldMenu,
           menuSystem->numItems,
           menuSystem->selectedIndex,
           menuSystem->scrollIndex,
           menuSystem->maxItems,
           canvas->state);
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
