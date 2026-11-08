#include <SDL.h>
#include <stdio.h>

#include "DoomRPG.h"
#include "DoomCanvas.h"
#include "Menu.h"
#include "esp_native_menu_state.h"
#include "esp_native_audio_intent.h"
#include "esp_native_canvas_text.h"
#include "esp_native_intro_state.h"

#include "native_main_menu_model.h"
#include "native_main_menu_160x120_layout.h"
#include "platform_video_config.h"

#define ESP32_SND_FLG_LOOP       1U
#define ESP32_SND_FLG_STOPSOUNDS 2U
#define ESP32_SND_FLG_ISMUSIC    8U

/* Native MENU/INTRO/LOAD canvas-state handoff; unchanged legacy side effects. */
static int doomCanvasBeginIntro(DoomCanvas_t* doomCanvas)
{
    if (doomCanvas == NULL || doomCanvas->doomRpg == NULL) {
        return 0;
    }

    DoomRPG_setColor(doomCanvas->doomRpg, 0x000000);
    DoomRPG_fillRect(doomCanvas->doomRpg,
                     0,
                     0,
                     DOOMRPG_CANVAS_WIDTH,
                     DOOMRPG_CANVAS_HEIGHT);
    EspNativeCanvas_drawText(
        doomCanvas, "Loading...", DOOMRPG_CANVAS_CENTER_X, DOOMRPG_CANVAS_CENTER_Y, 17);
    DoomRPG_flushGraphics(doomCanvas->doomRpg);
    /* Intro music is a native intent, independent of the retired Sound_t.
     * Playback remains deferred until a bounded hardware backend exists. */
    (void)EspNativeAudioIntent_publish(
        5039U, ESP32_SND_FLG_LOOP | ESP32_SND_FLG_STOPSOUNDS |
                   ESP32_SND_FLG_ISMUSIC, 5U);

    if (!EspNativeIntroState_begin(doomCanvas->doomRpg)) {
        EspNativeAudioIntent_stopAll();
        printf("[DOOMCANVASBRIDGE] INTRO-REJECT owner=native-transient allocation/load failed\n");
        return 0;
    }

    DoomRPG_setColor(doomCanvas->doomRpg, 0x000000);
    DoomRPG_fillRect(doomCanvas->doomRpg,
                     0,
                     0,
                     DOOMRPG_CANVAS_WIDTH,
                     DOOMRPG_CANVAS_HEIGHT);
    DoomRPG_flushGraphics(doomCanvas->doomRpg);
    return 1;
}

void EspNativeMainMenu_transitionCanvasState(DoomCanvas_t* doomCanvas, int stateNum)
{
    int oldState;

    if (doomCanvas == NULL) {
        return;
    }

    /*
     * ESP32 native owners handle gameplay dialogs, combat, automap, loading,
     * death, cast, credits and epilogue presentation. Only the three canvas
     * states still used as compatibility handoff markers remain accepted here.
     * Any future inherited state dependency must receive its own native owner
     * instead of silently widening this bridge.
     */
    if (stateNum != ST_MENU && stateNum != ST_PLAYING && stateNum != ST_INTRO) {
        printf("[DOOMCANVASBRIDGE] STATE-REJECT requested=%d current=%d owner=native\n",
               stateNum,
               doomCanvas->state);
        return;
    }

    oldState = doomCanvas->state;
    if (oldState == ST_MENU && stateNum != ST_MENU) {
        EspNativeAudioIntent_stopAll();
    }

    if (stateNum == ST_INTRO && stateNum != oldState) {
        if (!doomCanvasBeginIntro(doomCanvas)) {
            return;
        }
    }

    doomCanvas->state = stateNum;

    if (stateNum == ST_MENU && oldState == ST_PLAYING) {
        (void)EspNativeAudioIntent_publish(5042U, 0U, 3U);
        (void)EspNativeAudioIntent_publish(5067U, 0U, 3U);
    }
}
/* Native ROM-owned CYD guide: no obsolete 2005 keypad help.txt parser,
 * no SD allocation/read and no pointer-heavy MenuItem table. ASCII text
 * is at most 20 glyphs wide (7px advance, x=12 on the 160px canvas).
 */
#define MAIN_HELP_MAX_LINES 96
static const char* const helpLines[] = {
    "DOOM RPG ON CYD",
    "",
    "YOU ARE THE MARINE",
    "Explore the UAC",
    "Defeat the demons.",
    "Find the exit.",
    "DOWN: controls",
    "TURN-BASED GAME",
    "",
    "Moves take a turn.",
    "Enemies take turns.",
    "Plan each attack.",
    "Actions use turns.",
    "Think before moving.",
    "CYD TOUCH (1/2)",
    "",
    "TOP ROW (L/C/R):",
    "Strafe L / Forward /",
    "Strafe R",
    "",
    "Middle row is next.",
    "CYD TOUCH (2/2)",
    "MIDDLE (L/C/R):",
    "Turn L / ACT /",
    "Turn R",
    "BOTTOM (L/C/R):",
    "Prev gun / Back /",
    "Next gun",
    "ACTION / CENTER",
    "",
    "ACT attacks enemies.",
    "Open doors, crates.",
    "Use nearby objects.",
    "Walk onto pickups.",
    "Stay alert.",
    "TOP HUD BAR",
    "",
    "LEFT: game menu",
    "CENTER: pass a turn",
    "RIGHT: show automap",
    "",
    "Tap gun: weapons",
    "WEAPONS & STATUS",
    "",
    "Choose your weapons.",
    "Watch ammo and HP.",
    "Armor helps protect.",
    "HUD has direction.",
    "Change guns often.",
    "SYSTEM / SAVE LOAD",
    "",
    "INVENTORY: use gear",
    "STATUS: hero info",
    "SYSTEM: save, load,",
    "options and exit.",
    "SD card saves.",
    "MENU CONTROLS",
    "",
    "Tap once to select.",
    "Tap again: confirm.",
    "BACK to return.",
    "VIDEO: brightness,",
    "rotation and theme.",
    "TACTICAL TIPS",
    "",
    "Search every room.",
    "Keys unlock doors.",
    "Health items heal.",
    "Watch for secrets.",
    "Use the automap.",
    "ABOUT CYD PORT",
    "",
    "Original DOOM RPG",
    "Ported to ESP32 CYD.",
    "PAK data on SD card.",
    "No phone keys here!",
    "Have fun, Marine!",
};
#define MAIN_HELP_LINE_COUNT ((int)(sizeof(helpLines) / sizeof(helpLines[0])))
_Static_assert(MAIN_HELP_LINE_COUNT <= MAIN_HELP_MAX_LINES,
               "CYD native help exceeds fixed bounded model");

const char* DoomRPG_esp32MainMenuHelpLine(int index) {
    return index >= 0 && index < MAIN_HELP_LINE_COUNT ? helpLines[index] : NULL;
}

int DoomRPG_esp32MainMenuHelpLineCount(void) {
    return MAIN_HELP_LINE_COUNT;
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
    setFixedItem(&menuSystem->items[menuSystem->numItems++], "Sound", 0);
    return menuSystem->numItems == DOOMRPG_ESP32_OPTIONS_CARD_COUNT;
}

static int buildBoundedHelpModel(DoomRPG_t* doomRpg) {
    EspNativeMenuState_t* menuSystem;
    if (doomRpg == NULL || doomRpg->menuSystem == NULL ||
        MAIN_HELP_LINE_COUNT <= 0 || MAIN_HELP_LINE_COUNT > MAIN_HELP_MAX_LINES)
        return 0;
    menuSystem = doomRpg->menuSystem;
    menuSystem->scrollIndex = 0;
    menuSystem->selectedIndex = 0;
    menuSystem->numItems = MAIN_HELP_LINE_COUNT;
    menuSystem->imgBG = NULL;
    menuSystem->oldMenu = MENU_MAIN;
    menuSystem->type = 5;
    printf("[MAINMODEL] HELP-NATIVE lines=%d source=ROM keypadLegacy=no heap=none sdRead=no\n",
           MAIN_HELP_LINE_COUNT);
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

    EspNativeMainMenu_transitionCanvasState(canvas, ST_MENU);

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

    menuSystem->menu = MENU_NONE;
    menuSystem->numItems = 0;
    printf("[MAINMODEL] LEAVE target=%d state=%d items=%d viewInvalidation=retired dispatcher=native\n",
           menuSystem->menu,
           canvas->state,
           menuSystem->numItems);
    return 1;
}
