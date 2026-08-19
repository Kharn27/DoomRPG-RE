#include <SDL.h>
#include <stdint.h>
#include <stdio.h>

#include "DoomRPG.h"
#include "Menu.h"
#include "MenuSystem.h"

#include "native_main_menu_160x120_layout.h"
#include "native_main_menu_options_action.h"
#include "native_main_menu_touch.h"
#include "platform_touch_events.h"
#include "platform_video_config.h"

void __real_DoomRPG_esp32MainMenuTouchOnTap(int16_t screenX,
                                             int16_t screenY,
                                             uint16_t pressure,
                                             uint16_t rawX,
                                             uint16_t rawY);

static int hitOptionsRow(int16_t screenX, int16_t screenY) {
    const int logicalX = screenX / DOOMRPG_INTEGER_SCALE;
    const int logicalY = screenY / DOOMRPG_INTEGER_SCALE;
    const int optionsTop = DOOMRPG_ESP32_MAIN_MENU_ITEM_START_Y +
                           DOOMRPG_ESP32_MAIN_MENU_ITEM_LINE_HEIGHT;
    const int optionsBottom = optionsTop +
                              DOOMRPG_ESP32_MAIN_MENU_ITEM_LINE_HEIGHT - 1;

    /* Keep the x boundary identical to the hardware-validated MENU_MAIN zone. */
    return logicalX >= 28 && logicalX <= 119 &&
           logicalY >= optionsTop && logicalY <= optionsBottom;
}

void __wrap_DoomRPG_esp32MainMenuTouchOnTap(int16_t screenX,
                                             int16_t screenY,
                                             uint16_t pressure,
                                             uint16_t rawX,
                                             uint16_t rawY) {
    MenuSystem_t* menuSystem;

    if (doomRpg == NULL || doomRpg->menuSystem == NULL) {
        __real_DoomRPG_esp32MainMenuTouchOnTap(screenX,
                                                screenY,
                                                pressure,
                                                rawX,
                                                rawY);
        return;
    }

    menuSystem = doomRpg->menuSystem;

    /* The tap gate from the previous validated increment suppresses the first
     * tap on an already-selected row. Therefore reaching this wrapper with
     * MENU_MAIN selectedIndex=1 and a hit on row 1 means the released second
     * tap has been confirmed.
     */
    if (menuSystem->menu == MENU_MAIN &&
        menuSystem->selectedIndex == 1 &&
        hitOptionsRow(screenX, screenY)) {
        printf("[MENUTOUCH] CONFIRM item=1 text=\"Options   \" action=execute-options raw=%u,%u physical=%d,%d\n",
               rawX,
               rawY,
               screenX,
               screenY);

        /* Stop MENU_MAIN touch before mutating the real menu model. The Options
         * screen is intentionally display-only in this increment.
         */
        PlatformInput_setTapCallback(NULL);

        if (!DoomRPG_esp32ActivateMainMenuOptions(doomRpg)) {
            printf("[MAINOPTIONS] FAILED confirmed Options action\n");
        }
        return;
    }

    __real_DoomRPG_esp32MainMenuTouchOnTap(screenX,
                                            screenY,
                                            pressure,
                                            rawX,
                                            rawY);
}
