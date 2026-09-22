#include <SDL.h>
#include <stdint.h>
#include <stdio.h>

#include "DoomRPG.h"
#include "DoomCanvas.h"
#include "Menu.h"
#include "MenuSystem.h"
#include "Render.h"

#include "esp_native_gameplay_save_ui.h"
#include "native_main_menu_160x120_layout.h"
#include "native_main_menu_load_action.h"
#include "native_main_menu_start_action.h"
#include "native_main_menu_touch.h"
#include "native_main_menu_touch_layout.h"
#include "native_sprite_lru_cache.h"
#include "native_wall_lru_cache.h"
#include "platform_video_config.h"

#define MAIN_LOAD_ITEM_INDEX 1
#define MAIN_LOAD_GLYPH_ADVANCE 7

static char noSaveLabel[] = "No Save   ";

static uint32_t fnv1a32(const uint8_t* data, uint32_t length) {
    uint32_t hash = 2166136261U;
    uint32_t i;

    for (i = 0; i < length; ++i) {
        hash ^= data[i];
        hash *= 16777619U;
    }
    return hash;
}

static uint32_t framebufferHash(const Render_t* render) {
    if (render == NULL || render->framebuffer == NULL || render->pitch <= 0) {
        return 0U;
    }
    return fnv1a32((const uint8_t*)render->framebuffer,
                   (uint32_t)render->pitch * DOOMRPG_LOGICAL_HEIGHT);
}

static int menuBoundaryIsSafe(const DoomRPG_t* doomRpg) {
    const Render_t* render;

    if (doomRpg == NULL || doomRpg->doomCanvas == NULL ||
        doomRpg->menuSystem == NULL || doomRpg->render == NULL) {
        return 0;
    }
    render = doomRpg->render;
    return render->framebuffer != NULL && render->shapeData == NULL &&
           render->mediaTexels == NULL &&
           !EspNativeWallCache_isActive() &&
           !EspNativeSpriteCache_isActive();
}

static void showNoSaveFeedback(DoomRPG_t* doomRpg) {
    DoomCanvas_t* canvas = doomRpg->doomCanvas;
    const int width = (int)(sizeof(noSaveLabel) - 1U) *
                      MAIN_LOAD_GLYPH_ADVANCE;
    const int x = canvas->SCR_CX - (width >> 1);
    const int y = DOOMRPG_ESP32_MAIN_MENU_ITEM_START_Y +
                  (MAIN_LOAD_ITEM_INDEX *
                   DOOMRPG_ESP32_MAIN_MENU_ITEM_LINE_HEIGHT);
    uint32_t frameFNV;

    DoomRPG_setColor(doomRpg, 0x000000);
    DoomRPG_fillRect(doomRpg, x, y, width,
                     DOOMRPG_ESP32_MAIN_MENU_ITEM_LINE_HEIGHT);
    DoomRPG_setFontColor(doomRpg, 0xffff0000);
    DoomCanvas_drawFont(canvas, noSaveLabel, x, y, 0, 0, -1, false);
    DoomRPG_setFontColor(doomRpg, 0xffffffff);
    SDL_RenderPresent(NULL);

    frameFNV = framebufferHash(doomRpg->render);
    DoomRPG_esp32MainMenuTouchRebaseFrame(MAIN_LOAD_ITEM_INDEX, frameFNV);
    printf("[MAINLOAD] FEEDBACK text=\"No Save\" color=red framebufferFNV=%08x\n",
           (unsigned int)frameFNV);
}

static void restoreMainMenuAfterFailedLoad(DoomRPG_t* doomRpg) {
    uint32_t frameFNV = 0U;

    doomRpg->menuSystem->menu = MENU_MAIN;
    doomRpg->menuSystem->selectedIndex = 0;
    doomRpg->menuSystem->scrollIndex = 0;
    doomRpg->menuSystem->paintMenu = true;
    DoomCanvas_setState(doomRpg->doomCanvas, ST_MENU);

    if (!DoomRPG_esp32RepaintOpaqueMainMenu(doomRpg, &frameFNV)) {
        printf("[MAINLOAD] FAILED restoring MENU_MAIN after checkpoint failure\n");
        return;
    }
    printf("[MAINLOAD] RECOVERED MENU_MAIN framebufferFNV=%08x selected=0\n",
           (unsigned int)frameFNV);
}

int DoomRPG_esp32ActivateMainMenuLoad(struct DoomRPG_s* doomRpgBase) {
    DoomRPG_t* doomRpg = (DoomRPG_t*)doomRpgBase;
    uint32_t inputHash;
    uint32_t expectedHash;

    printf("\n=== Doom RPG ESP32 MENU_MAIN -> Load Game ===\n");

    if (!menuBoundaryIsSafe(doomRpg)) {
        printf("[MAINLOAD] FAILED core/graphics boundary unavailable\n");
        return 0;
    }

    inputHash = framebufferHash(doomRpg->render);
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
        return 0;
    }

    if (!EspNativeGameplaySave_hasReadableCheckpoint()) {
        printf("[MAINLOAD] NO-SAVE missing-or-invalid; MENU_MAIN remains active\n");
        showNoSaveFeedback(doomRpg);
        return 0;
    }

    if (!DoomRPG_esp32ReleaseMainMenuMemory(doomRpg)) {
        printf("[MAINLOAD] FAILED menu runtime cleanup\n");
        return 0;
    }

    if (!EspNativeGameplaySave_loadCheckpoint()) {
        printf("[MAINLOAD] FAILED checkpoint restore; returning to MENU_MAIN\n");
        restoreMainMenuAfterFailedLoad(doomRpg);
        return 0;
    }

    doomRpg->menuSystem->menu = MENU_NONE;
    doomRpg->menuSystem->numItems = 0;
    doomRpg->menuSystem->paintMenu = false;
    DoomCanvas_setState(doomRpg->doomCanvas, ST_PLAYING);

    printf("[MAINLOAD] READY checkpoint restored; intro=skipped session=resume-pending state=%d\n",
           doomRpg->doomCanvas->state);
    return 1;
}
