#include <SDL.h>
#include <stdint.h>
#include <stdio.h>

#include "DoomRPG.h"
#include "DoomCanvas.h"
#include "Menu.h"
#include "esp_native_menu_state.h"
#include "Render.h"
#include "esp_native_audio_intent.h"

#include "native_main_menu_actions.h"
#include "native_main_menu_load_action.h"
#include "native_main_menu_model.h"
#include "native_main_menu_options_action.h"
#include "native_main_menu_present.h"
#include "native_main_menu_options_back.h"
#include "native_main_menu_start_action.h"
#include "native_main_menu_touch.h"
#include "native_main_menu_touch_layout.h"
#include "native_sprite_lru_cache.h"
#include "native_wall_lru_cache.h"
#include "platform_touch_events.h"
#include "platform_video_config.h"

#define MAIN_HELP_LINE_HEIGHT 12
#define MAIN_HELP_CONTENT_BOTTOM 95
#define MAIN_HELP_VISIBLE_LINES 8
#define MAIN_HELP_FOOTER_TOP 96
#define MAIN_HELP_BACK_RIGHT 52
#define MAIN_HELP_UP_RIGHT 105

static DoomRPG_t* helpDoomRpg;
static uint32_t helpExpectedFrameFNV;

int DoomRPG_esp32MainMenuReturnToMain(struct DoomRPG_s* doomRpgBase,
                                      int expectedChildMenu,
                                      const char* source,
                                      uint32_t* finalFramebufferFNV) {
    DoomRPG_t* doomRpg = (DoomRPG_t*)doomRpgBase;
    EspNativeMenuState_t* menuSystem;
    uint32_t finalFNV = 0U;
    uint32_t startMs;
    uint32_t elapsedMs;

    if (finalFramebufferFNV != NULL) *finalFramebufferFNV = 0U;

    if (doomRpg == NULL || doomRpg->menuSystem == NULL ||
        doomRpg->doomCanvas == NULL ||
        (expectedChildMenu != MENU_MAIN_OPTIONS &&
         expectedChildMenu != MENU_MAIN_HELP_ABOUT)) {
        printf("[MAINBACK] FAILED source=%s child=%d object/model contract\n",
               source != NULL ? source : "unknown",
               expectedChildMenu);
        return 0;
    }

    menuSystem = doomRpg->menuSystem;
    printf("\n=== Doom RPG ESP32 native child -> MENU_MAIN Back ===\n");
    printf("[MAINBACK] BEGIN source=%s child=%d menu=%d old=%d selected=%d state=%d frame=%08x\n",
           source != NULL ? source : "unknown",
           expectedChildMenu,
           menuSystem->menu,
           menuSystem->oldMenu,
           menuSystem->selectedIndex,
           doomRpg->doomCanvas->state,
           (unsigned int)DoomRPG_esp32MainMenuFramebufferHash(doomRpg->render));

    if (!DoomRPG_esp32MainMenuGraphicsBoundaryIsSafe(doomRpg) ||
        doomRpg->doomCanvas->state != ST_MENU ||
        menuSystem->menu != expectedChildMenu ||
        menuSystem->oldMenu != MENU_MAIN) {
        printf("[MAINBACK] FAILED source=%s precondition safe=%d state=%d menu=%d expectedChild=%d old=%d\n",
               source != NULL ? source : "unknown",
               DoomRPG_esp32MainMenuGraphicsBoundaryIsSafe(doomRpg),
               doomRpg->doomCanvas->state,
               menuSystem->menu,
               expectedChildMenu,
               menuSystem->oldMenu);
        return 0;
    }

    startMs = (uint32_t)DoomRPG_GetTimeMS();

    /* Exact legacy Back cue, without importing MenuSystem_back() or its generic
     * MenuSystem_setMenu(oldMenu) hierarchy router.
     */
    (void)EspNativeAudioIntent_publish(5042U, 0U, 3U);

    if (!DoomRPG_esp32MainMenuModelEnter(doomRpg, MENU_MAIN) ||
        !DoomRPG_esp32RepaintOpaqueMainMenu(doomRpg, &finalFNV)) {
        printf("[MAINBACK] FAILED source=%s native model/repaint\n",
               source != NULL ? source : "unknown");
        return 0;
    }

    elapsedMs = (uint32_t)DoomRPG_GetTimeMS() - startMs;

    if (menuSystem->menu != MENU_MAIN ||
        menuSystem->selectedIndex != 0 ||
        menuSystem->numItems != 4 ||
        finalFNV == 0U ||
        finalFNV != DoomRPG_esp32MainMenuSelectionFramebufferFNV(0) ||
        finalFNV != DoomRPG_esp32MainMenuFramebufferHash(doomRpg->render) ||
        !DoomRPG_esp32MainMenuTouchIsActive() ||
        !DoomRPG_esp32MainMenuGraphicsBoundaryIsSafe(doomRpg)) {
        printf("[MAINBACK] FAILED source=%s final invariant menu=%d selected=%d items=%d frame=%08x expected=%08x touch=%d\n",
               source != NULL ? source : "unknown",
               menuSystem->menu,
               menuSystem->selectedIndex,
               menuSystem->numItems,
               (unsigned int)finalFNV,
               (unsigned int)DoomRPG_esp32MainMenuSelectionFramebufferFNV(0),
               DoomRPG_esp32MainMenuTouchIsActive());
        return 0;
    }

    if (finalFramebufferFNV != NULL) *finalFramebufferFNV = finalFNV;
    printf("[MAINBACK] READY source=%s child=%d->%d frame=%08x touch=rearmed elapsedMs=%u sound=5042 router=native noMenuSystemBack=yes noSetMenu=yes\n",
           source != NULL ? source : "unknown",
           expectedChildMenu,
           menuSystem->menu,
           (unsigned int)finalFNV,
           (unsigned int)elapsedMs);
    return 1;
}

static int paintHelp(DoomRPG_t* doomRpg, uint32_t* outFrameFNV) {
    DoomCanvas_t* canvas = doomRpg->doomCanvas;
    EspNativeMenuState_t* menuSystem = doomRpg->menuSystem;
    int visible;
    int maxScroll;
    int end;
    int i;
    int x;
    int y;
    uint32_t frameFNV;

    if (outFrameFNV != NULL) *outFrameFNV = 0U;
    if (!DoomRPG_esp32MainMenuGraphicsBoundaryIsSafe(doomRpg) ||
        menuSystem->menu != MENU_MAIN_HELP_ABOUT ||
        menuSystem->type != 5 ||
        menuSystem->oldMenu != MENU_MAIN ||
        menuSystem->numItems <= 0 ||
        menuSystem->numItems != DoomRPG_esp32MainMenuHelpLineCount()) {
        return 0;
    }

    menuSystem->maxItems = MAIN_HELP_VISIBLE_LINES;
    maxScroll = menuSystem->numItems > menuSystem->maxItems
                    ? menuSystem->numItems - menuSystem->maxItems
                    : 0;
    if (menuSystem->scrollIndex < 0) menuSystem->scrollIndex = 0;
    if (menuSystem->scrollIndex > maxScroll) {
        menuSystem->scrollIndex = maxScroll;
    }
    menuSystem->selectedIndex = menuSystem->scrollIndex;

    visible = menuSystem->numItems - menuSystem->scrollIndex;
    if (visible > menuSystem->maxItems) visible = menuSystem->maxItems;
    end = menuSystem->scrollIndex + visible;

    DoomRPG_setColor(doomRpg, 0x000000);
    DoomRPG_fillRect(doomRpg,
                     0,
                     0,
                     DOOMRPG_CANVAS_WIDTH,
                     DOOMRPG_CANVAS_HEIGHT);
    DoomRPG_setFontColor(doomRpg, 0xffffffff);

    x = DOOMRPG_CANVAS_CENTER_X - 64;
    y = 0;
    for (i = menuSystem->scrollIndex; i < end; ++i) {
        const char* line = DoomRPG_esp32MainMenuHelpLine(i);
        if (line == NULL) return 0;
        if (line[0] != '\0') {
            DoomCanvas_drawString1(canvas, (char*)line, x, y, 0);
        }
        y += MAIN_HELP_LINE_HEIGHT;
    }

    /* Permanent finger-first footer. Keeping it outside the scrolling text
     * makes all 83 help lines reachable without importing MenuSystem_paint().
     */
    DoomRPG_setColor(doomRpg, 0x404040);
    DoomRPG_drawLine(doomRpg,
                     0,
                     MAIN_HELP_FOOTER_TOP,
                     DOOMRPG_LOGICAL_WIDTH - 1,
                     MAIN_HELP_FOOTER_TOP);
    DoomRPG_drawLine(doomRpg,
                     MAIN_HELP_BACK_RIGHT + 1,
                     MAIN_HELP_FOOTER_TOP,
                     MAIN_HELP_BACK_RIGHT + 1,
                     DOOMRPG_LOGICAL_HEIGHT - 1);
    DoomRPG_drawLine(doomRpg,
                     MAIN_HELP_UP_RIGHT + 1,
                     MAIN_HELP_FOOTER_TOP,
                     MAIN_HELP_UP_RIGHT + 1,
                     DOOMRPG_LOGICAL_HEIGHT - 1);

    DoomRPG_setFontColor(doomRpg, 0xffffffff);
    DoomCanvas_drawString1(canvas, "BACK", 8, 102, 0);
    DoomCanvas_drawString1(canvas, "UP", 70, 102, 0);
    DoomCanvas_drawString1(canvas, "DOWN", 118, 102, 0);
    DoomRPG_setFontColor(doomRpg, 0xffffffff);

    frameFNV = DoomRPG_esp32MainMenuFramebufferHash(doomRpg->render);
    if (frameFNV == 0U) return 0;

    SDL_RenderPresent(NULL);
    if (outFrameFNV != NULL) *outFrameFNV = frameFNV;

    printf("[MAINHELP] PAINT lines=%d visible=%d range=%d..%d maxItems=%d maxScroll=%d framebufferFNV=%08x background=opaque-black renderer=native-list footer=BACK|UP|DOWN\n",
           menuSystem->numItems,
           visible,
           menuSystem->scrollIndex,
           end > menuSystem->scrollIndex ? end - 1 : menuSystem->scrollIndex,
           menuSystem->maxItems,
           maxScroll,
           (unsigned int)frameFNV);
    return 1;
}

static void helpTap(int16_t screenX,
                    int16_t screenY,
                    uint16_t pressure,
                    uint16_t rawX,
                    uint16_t rawY) {
    DoomRPG_t* doomRpg = helpDoomRpg;
    EspNativeMenuState_t* menuSystem;
    int logicalX;
    int logicalY;
    int maxScroll;
    int beforeScroll;
    uint32_t before;
    uint32_t finalFNV = 0U;

    (void)pressure;

    if (doomRpg == NULL || doomRpg->menuSystem == NULL) return;
    menuSystem = doomRpg->menuSystem;
    before = DoomRPG_esp32MainMenuFramebufferHash(doomRpg->render);
    logicalX = screenX / DOOMRPG_INTEGER_SCALE;
    logicalY = screenY / DOOMRPG_INTEGER_SCALE;

    if (!DoomRPG_esp32MainMenuGraphicsBoundaryIsSafe(doomRpg) ||
        menuSystem->menu != MENU_MAIN_HELP_ABOUT ||
        helpExpectedFrameFNV == 0U ||
        before != helpExpectedFrameFNV) {
        printf("[MAINHELP] FAILED touch precondition frame=%08x expected=%08x menu=%d\n",
               (unsigned int)before,
               (unsigned int)helpExpectedFrameFNV,
               menuSystem->menu);
        PlatformInput_setTapCallback(NULL);
        helpDoomRpg = NULL;
        helpExpectedFrameFNV = 0U;
        DoomRPG_esp32MainMenuRecover(doomRpg, "help-touch-precondition");
        return;
    }

    printf("[MAINHELP] TAP raw=%u,%u physical=%d,%d logical=%d,%d scroll=%d frame=%08x\n",
           rawX,
           rawY,
           screenX,
           screenY,
           logicalX,
           logicalY,
           menuSystem->scrollIndex,
           (unsigned int)before);

    if (logicalY < MAIN_HELP_FOOTER_TOP) {
        printf("[MAINHELP] CONTENT tap=no-action footerTop=%d\n",
               MAIN_HELP_FOOTER_TOP);
        return;
    }

    if (logicalX <= MAIN_HELP_BACK_RIGHT) {
        PlatformInput_setTapCallback(NULL);

        if (!DoomRPG_esp32MainMenuReturnToMain(
                doomRpg,
                MENU_MAIN_HELP_ABOUT,
                "help",
                &finalFNV)) {
            printf("[MAINHELP] FAILED native Back to MENU_MAIN menu=%d\n",
                   menuSystem->menu);
            helpDoomRpg = NULL;
            helpExpectedFrameFNV = 0U;
            DoomRPG_esp32MainMenuRecover(doomRpg, "help-back-failed");
            return;
        }

        printf("[MAINHELP] READY back=native mainFNV=%08x touch=armed\n",
               (unsigned int)finalFNV);
        helpDoomRpg = NULL;
        helpExpectedFrameFNV = 0U;
        return;
    }

    beforeScroll = menuSystem->scrollIndex;
    maxScroll = menuSystem->numItems > MAIN_HELP_VISIBLE_LINES
                    ? menuSystem->numItems - MAIN_HELP_VISIBLE_LINES
                    : 0;

    if (logicalX <= MAIN_HELP_UP_RIGHT) {
        menuSystem->scrollIndex -= MAIN_HELP_VISIBLE_LINES;
        if (menuSystem->scrollIndex < 0) menuSystem->scrollIndex = 0;
        printf("[MAINHELP] PAGE-UP scroll=%d->%d\n",
               beforeScroll,
               menuSystem->scrollIndex);
    }
    else {
        menuSystem->scrollIndex += MAIN_HELP_VISIBLE_LINES;
        if (menuSystem->scrollIndex > maxScroll) {
            menuSystem->scrollIndex = maxScroll;
        }
        printf("[MAINHELP] PAGE-DOWN scroll=%d->%d max=%d\n",
               beforeScroll,
               menuSystem->scrollIndex,
               maxScroll);
    }

    menuSystem->selectedIndex = menuSystem->scrollIndex;
    if (!paintHelp(doomRpg, &finalFNV)) {
        printf("[MAINHELP] FAILED repaint scroll=%d\n",
               menuSystem->scrollIndex);
        PlatformInput_setTapCallback(NULL);
        helpDoomRpg = NULL;
        helpExpectedFrameFNV = 0U;
        DoomRPG_esp32MainMenuRecover(doomRpg, "help-repaint-failed");
        return;
    }

    helpExpectedFrameFNV = finalFNV;
    printf("[MAINHELP] READY scroll=%d frame=%08x input=footer-back+page-up+page-down\n",
           menuSystem->scrollIndex,
           (unsigned int)finalFNV);
}

static int activateHelp(DoomRPG_t* doomRpg) {
    EspNativeMenuState_t* menuSystem;
    uint32_t inputFNV;
    uint32_t expectedFNV;
    uint32_t helpFNV = 0U;

    if (!DoomRPG_esp32MainMenuGraphicsBoundaryIsSafe(doomRpg)) {
        printf("[MAINHELP] FAILED graphics boundary\n");
        return 0;
    }

    menuSystem = doomRpg->menuSystem;
    inputFNV = DoomRPG_esp32MainMenuFramebufferHash(doomRpg->render);
    expectedFNV = DoomRPG_esp32MainMenuSelectionFramebufferFNV(
        DOOMRPG_ESP32_MAIN_MENU_ACTION_HELP);

    if (menuSystem->menu != MENU_MAIN ||
        menuSystem->selectedIndex != DOOMRPG_ESP32_MAIN_MENU_ACTION_HELP ||
        expectedFNV == 0U || inputFNV != expectedFNV) {
        printf("[MAINHELP] FAILED precondition menu=%d selected=%d frame=%08x expected=%08x\n",
               menuSystem->menu,
               menuSystem->selectedIndex,
               (unsigned int)inputFNV,
               (unsigned int)expectedFNV);
        return 0;
    }

    (void)EspNativeAudioIntent_publish(5046U, 0U, 3U);
    if (!DoomRPG_esp32MainMenuModelEnter(
            doomRpg, MENU_MAIN_HELP_ABOUT) ||
        !paintHelp(doomRpg, &helpFNV)) {
        printf("[MAINHELP] FAILED model/presentation\n");
        return 0;
    }

    helpDoomRpg = doomRpg;
    helpExpectedFrameFNV = helpFNV;
    PlatformInput_setTapCallback(helpTap);

    printf("[MAINHELP] READY menu=%d type=%d old=%d frame=%08x input=footer-back+page-up+page-down backOwner=native\n",
           menuSystem->menu,
           menuSystem->type,
           menuSystem->oldMenu,
           (unsigned int)helpFNV);
    return 1;
}

DoomRpgEsp32MainMenuDispatchResult
DoomRPG_esp32MainMenuDispatchConfirmed(struct DoomRPG_s* doomRpgBase,
                                       int action) {
    DoomRPG_t* doomRpg = (DoomRPG_t*)doomRpgBase;
    uint32_t optionsFNV = 0U;

    if (doomRpg == NULL) {
        return DOOMRPG_ESP32_MAIN_MENU_DISPATCH_FAILED;
    }

    printf("[MAINACTION] DISPATCH action=%d menu=%d selected=%d\n",
           action,
           doomRpg->menuSystem != NULL ? doomRpg->menuSystem->menu : -999,
           doomRpg->menuSystem != NULL
               ? doomRpg->menuSystem->selectedIndex : -999);

    switch (action) {
        case DOOMRPG_ESP32_MAIN_MENU_ACTION_START:
            return DoomRPG_esp32ActivateMainMenuStart(doomRpg)
                       ? DOOMRPG_ESP32_MAIN_MENU_DISPATCH_TRANSITIONED
                       : DOOMRPG_ESP32_MAIN_MENU_DISPATCH_FAILED;

        case DOOMRPG_ESP32_MAIN_MENU_ACTION_LOAD: {
            DoomRpgEsp32MainMenuLoadResult loadResult =
                DoomRPG_esp32ActivateMainMenuLoad(doomRpg);

            printf("[MAINACTION] LOAD result=%d\n", (int)loadResult);
            if (loadResult == DOOMRPG_ESP32_MAIN_MENU_LOAD_TRANSITIONED) {
                return DOOMRPG_ESP32_MAIN_MENU_DISPATCH_TRANSITIONED;
            }
            if (loadResult == DOOMRPG_ESP32_MAIN_MENU_LOAD_NO_SAVE ||
                loadResult == DOOMRPG_ESP32_MAIN_MENU_LOAD_RECOVERED) {
                return DOOMRPG_ESP32_MAIN_MENU_DISPATCH_STAY_MAIN;
            }
            return DOOMRPG_ESP32_MAIN_MENU_DISPATCH_FAILED;
        }

        case DOOMRPG_ESP32_MAIN_MENU_ACTION_OPTIONS:
            if (!DoomRPG_esp32ActivateMainMenuOptions(
                    doomRpg, &optionsFNV) ||
                !DoomRPG_esp32OptionsBackActivate(
                    doomRpg, optionsFNV)) {
                return DOOMRPG_ESP32_MAIN_MENU_DISPATCH_FAILED;
            }
            return DOOMRPG_ESP32_MAIN_MENU_DISPATCH_TRANSITIONED;

        case DOOMRPG_ESP32_MAIN_MENU_ACTION_HELP:
            return activateHelp(doomRpg)
                       ? DOOMRPG_ESP32_MAIN_MENU_DISPATCH_TRANSITIONED
                       : DOOMRPG_ESP32_MAIN_MENU_DISPATCH_FAILED;

        default:
            printf("[MAINACTION] FAILED unsupported action=%d mutation=no\n",
                   action);
            return DOOMRPG_ESP32_MAIN_MENU_DISPATCH_FAILED;
    }
}


/* Consolidated main-menu LOAD action. Keep action ownership in one TU. */
#include <SDL.h>
#include <stdint.h>
#include <stdio.h>

#include "DoomRPG.h"
#include "DoomCanvas.h"
#include "Menu.h"
#include "MenuSystem.h"
#include "Render.h"

#include "esp_asset_pack.h"
#include "esp_native_gameplay_save_ui.h"
#include "esp_native_transition_presentation.h"
#include "native_main_menu_160x120_layout.h"
#include "native_main_menu_load_action.h"
#include "native_main_menu_present.h"
#include "native_main_menu_start_action.h"
#include "native_main_menu_touch.h"
#include "native_main_menu_touch_layout.h"
#include "native_sprite_lru_cache.h"
#include "native_wall_lru_cache.h"
#include "platform_video_config.h"

#define MAIN_LOAD_ITEM_INDEX 1
#define MAIN_LOAD_GLYPH_ADVANCE 7

static char noSaveLabel[] = "No Save   ";

static void showNoSaveFeedback(DoomRPG_t* doomRpg) {
    DoomCanvas_t* canvas = doomRpg->doomCanvas;
    const int left = DOOMRPG_ESP32_MAIN_MENU_CARD_COL1_LEFT;
    const int right = DOOMRPG_ESP32_MAIN_MENU_CARD_COL1_RIGHT;
    const int top = DOOMRPG_ESP32_MAIN_MENU_CARD_ROW0_TOP;
    const int bottom = DOOMRPG_ESP32_MAIN_MENU_CARD_ROW0_BOTTOM;
    const int width = (int)(sizeof(noSaveLabel) - 1U) *
                      MAIN_LOAD_GLYPH_ADVANCE;
    const int x = ((left + right + 1) >> 1) - (width >> 1);
    const int y = top + (((bottom - top + 1) -
                          DOOMRPG_ESP32_MAIN_MENU_FONT_HEIGHT) >> 1);
    uint32_t frameFNV;

    DoomRPG_setColor(doomRpg, 0x000000);
    DoomRPG_fillRect(doomRpg,
                     left + 5,
                     top + 4,
                     right - left - 9,
                     bottom - top - 7);
    DoomRPG_setFontColor(doomRpg, 0xffff0000);
    DoomCanvas_drawString1(canvas, noSaveLabel, x, y, 0);
    DoomRPG_setFontColor(doomRpg, 0xffffffff);
    SDL_RenderPresent(NULL);

    frameFNV = DoomRPG_esp32MainMenuFramebufferHash(doomRpg->render);
    DoomRPG_esp32MainMenuTouchRebaseFrame(MAIN_LOAD_ITEM_INDEX, frameFNV);
    printf("[MAINLOAD] FEEDBACK text=\"NO SAVE\" card=LOAD color=red framebufferFNV=%08x\n",
           (unsigned int)frameFNV);
}

DoomRpgEsp32MainMenuLoadResult
DoomRPG_esp32ActivateMainMenuLoad(struct DoomRPG_s* doomRpgBase) {
    DoomRPG_t* doomRpg = (DoomRPG_t*)doomRpgBase;
    uint32_t inputHash;
    uint32_t expectedHash;

    printf("\n=== Doom RPG ESP32 MENU_MAIN -> Load Game ===\n");

    /* MENU_MAIN load must never inherit an in-level presentation heartbeat.
     * Keep its hardware-validated map-flash path synchronous/sequence-only. */
    EspAssetPack_mapFlashSetProgressCallback(NULL);
    EspNativeTransitionPresentation_reset();
    printf("[MAINLOAD] TRANSITION-UI reset=yes mapFlashProgress=off\n");

    if (!EspNativeGameplaySave_mainSelectorReady() &&
        !DoomRPG_esp32MainMenuGraphicsBoundaryIsSafe(doomRpg)) {
        printf("[MAINLOAD] FAILED core/graphics boundary unavailable\n");
        return DOOMRPG_ESP32_MAIN_MENU_LOAD_FATAL;
    }

    inputHash = DoomRPG_esp32MainMenuFramebufferHash(doomRpg->render);
    expectedHash = DoomRPG_esp32MainMenuSelectionFramebufferFNV(
        MAIN_LOAD_ITEM_INDEX);
    if (doomRpg->menuSystem->menu != MENU_MAIN ||
        doomRpg->menuSystem->selectedIndex != MAIN_LOAD_ITEM_INDEX ||
        doomRpg->doomCanvas->state != ST_MENU ||
        (!EspNativeGameplaySave_mainSelectorReady() && (expectedHash == 0U || inputHash != expectedHash))) {
        printf("[MAINLOAD] FAILED precondition menu=%d selected=%d state=%d framebuffer=%08x expected=%08x\n",
               doomRpg->menuSystem->menu,
               doomRpg->menuSystem->selectedIndex,
               doomRpg->doomCanvas->state,
               (unsigned int)inputHash,
               (unsigned int)expectedHash);
        return DOOMRPG_ESP32_MAIN_MENU_LOAD_FATAL;
    }

    if (!EspNativeGameplaySave_mainSelectorActive()) {
        (void)EspNativeGameplaySave_mainSelectorBegin();
        return DOOMRPG_ESP32_MAIN_MENU_LOAD_NO_SAVE;
    }
    if (!EspNativeGameplaySave_mainSelectorReady()) {
        return DOOMRPG_ESP32_MAIN_MENU_LOAD_NO_SAVE;
    }
    if (!EspNativeGameplaySave_hasReadableCheckpoint()) {
        printf("[MAINLOAD] NO-SAVE missing-or-invalid; MENU_MAIN remains active\n");
        showNoSaveFeedback(doomRpg);
        return DOOMRPG_ESP32_MAIN_MENU_LOAD_NO_SAVE;
    }

    if (!DoomRPG_esp32ReleaseMainMenuMemory(doomRpg)) {
        printf("[MAINLOAD] FAILED menu runtime cleanup\n");
        return DOOMRPG_ESP32_MAIN_MENU_LOAD_FATAL;
    }

    if (!EspNativeGameplaySave_loadCheckpoint()) {
        printf("[MAINLOAD] FAILED checkpoint restore; attempting MENU_MAIN recovery\n");
        EspNativeTransitionPresentation_reset();
        if (DoomRPG_esp32MainMenuRecover(doomRpg, "load-restore-failed")) {
            EspNativeGameplaySave_mainSelectorFinish();
            return DOOMRPG_ESP32_MAIN_MENU_LOAD_RECOVERED;
        }
        return DOOMRPG_ESP32_MAIN_MENU_LOAD_FATAL;
    }

    EspNativeGameplaySave_mainSelectorFinish();
    doomRpg->menuSystem->menu = MENU_NONE;
    doomRpg->menuSystem->numItems = 0;
    DoomCanvas_setState(doomRpg->doomCanvas, ST_PLAYING);

    printf("[MAINLOAD] READY checkpoint restored; intro=skipped session=resume-pending state=%d\n",
           doomRpg->doomCanvas->state);
    return DOOMRPG_ESP32_MAIN_MENU_LOAD_TRANSITIONED;
}
