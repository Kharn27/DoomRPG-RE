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
    DoomCanvas_drawFont(canvas, noSaveLabel, x, y, 0, 0, -1, false);
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

    if (!DoomRPG_esp32MainMenuGraphicsBoundaryIsSafe(doomRpg)) {
        printf("[MAINLOAD] FAILED core/graphics boundary unavailable\n");
        return DOOMRPG_ESP32_MAIN_MENU_LOAD_FATAL;
    }

    inputHash = DoomRPG_esp32MainMenuFramebufferHash(doomRpg->render);
    expectedHash = DoomRPG_esp32MainMenuSelectionFramebufferFNV(
        MAIN_LOAD_ITEM_INDEX);
    if (doomRpg->menuSystem->menu != MENU_MAIN ||
        doomRpg->menuSystem->selectedIndex != MAIN_LOAD_ITEM_INDEX ||
        doomRpg->doomCanvas->state != ST_MENU ||
        expectedHash == 0U || inputHash != expectedHash) {
        printf("[MAINLOAD] FAILED precondition menu=%d selected=%d state=%d framebuffer=%08x expected=%08x\n",
               doomRpg->menuSystem->menu,
               doomRpg->menuSystem->selectedIndex,
               doomRpg->doomCanvas->state,
               (unsigned int)inputHash,
               (unsigned int)expectedHash);
        return DOOMRPG_ESP32_MAIN_MENU_LOAD_FATAL;
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
            return DOOMRPG_ESP32_MAIN_MENU_LOAD_RECOVERED;
        }
        return DOOMRPG_ESP32_MAIN_MENU_LOAD_FATAL;
    }

    doomRpg->menuSystem->menu = MENU_NONE;
    doomRpg->menuSystem->numItems = 0;
    doomRpg->menuSystem->paintMenu = false;
    DoomCanvas_setState(doomRpg->doomCanvas, ST_PLAYING);

    printf("[MAINLOAD] READY checkpoint restored; intro=skipped session=resume-pending state=%d\n",
           doomRpg->doomCanvas->state);
    return DOOMRPG_ESP32_MAIN_MENU_LOAD_TRANSITIONED;
}
