#include <SDL.h>
#include <stdint.h>
#include <stdio.h>

#include "DoomRPG.h"
#include "DoomCanvas.h"
#include "Game.h"
#include "Menu.h"
#include "MenuSystem.h"
#include "Player.h"
#include "Render.h"
#include "Sound.h"

#include "esp_legacy_asset_source.h"
#include "native_intro_first_frame.h"
#include "native_main_menu_model.h"
#include "native_main_menu_start_action.h"
#include "native_main_menu_touch.h"
#include "native_main_menu_present.h"
#include "native_sprite_lru_cache.h"
#include "native_wall_lru_cache.h"
#include "platform_video_config.h"

/* Keep ESP-IDF's stdbool macros after DoomRPG's legacy boolean enum. */
#include <esp_heap_caps.h>

#define INTRO_ASSET_COUNT 4

static const char* const introAssetNames[INTRO_ASSET_COUNT] = {
    "c.bmp",
    "d.bmp",
    "e.bmp",
    "f.bmp"
};

static uint32_t heap8Free(void) {
    return (uint32_t)heap_caps_get_free_size(MALLOC_CAP_8BIT);
}

static uint32_t largest8Block(void) {
    return (uint32_t)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
}

static int playerHasFreshResetContract(const Player_t* player) {
    return player != NULL &&
           player->level == 1 &&
           player->currentXP == 0 &&
           player->nextLevelXP == 80 &&
           player->keys == 0 &&
           player->credits == 0 &&
           player->ammo[1] == 8 &&
           player->weapon == 2 &&
           player->weapons == 4 &&
           player->disabledWeapons == 0 &&
           player->totalDeaths == 0;
}

static void printIntroAssetPlan(void) {
    uint32_t totalBytes = 0U;
    int i;

    printf("[MAINSTART] Intro asset PAK plan (%d files)\n", INTRO_ASSET_COUNT);
    for (i = 0; i < INTRO_ASSET_COUNT; ++i) {
        uint32_t bytes = 0U;
        if (!EspLegacyAssetSource_stat(introAssetNames[i], &bytes)) {
            printf("[MAINSTART] INTRO-ASSET %-5s MISSING\n", introAssetNames[i]);
            continue;
        }

        totalBytes += bytes;
        printf("[MAINSTART] INTRO-ASSET %-5s bytes=%u backing=pak\n",
               introAssetNames[i], (unsigned int)bytes);
    }

    printf("[MAINSTART] Intro asset PAK total=%u; loader peak is per-file, not total\n",
           (unsigned int)totalBytes);
}

int DoomRPG_esp32ReleaseMainMenuMemory(struct DoomRPG_s* doomRpgBase) {
    DoomRPG_t* doomRpg = (DoomRPG_t*)doomRpgBase;
    DoomCanvas_t* doomCanvas;
    Render_t* render;
    uint32_t heapBefore;
    uint32_t largestBefore;
    int legalsReleased = 0;

    if (doomRpg == NULL || doomRpg->doomCanvas == NULL ||
        doomRpg->render == NULL || doomRpg->game == NULL) {
        return 0;
    }

    doomCanvas = doomRpg->doomCanvas;
    render = doomRpg->render;
    heapBefore = heap8Free();
    largestBefore = largest8Block();

    if (doomCanvas->imgLegals.imgBitmap != NULL) {
        DoomRPG_freeImage(doomRpg, &doomCanvas->imgLegals);
        legalsReleased = 1;
    }

    Render_freeRuntime(render);
    Game_unloadMapData(doomRpg->game);

    printf("[MAINMENU] Runtime cleanup legals=%s heap8=%u->%u gained=%d largest8=%u->%u nodes=%p lines=%p mapSprites=%p mappings=%p/%p shapeData=%p mediaTexels=%p\n",
           legalsReleased ? "released" : "already-free",
           (unsigned int)heapBefore,
           (unsigned int)heap8Free(),
           (int)heap8Free() - (int)heapBefore,
           (unsigned int)largestBefore,
           (unsigned int)largest8Block(),
           (void*)render->nodes,
           (void*)render->lines,
           (void*)render->mapSprites,
           (void*)render->mediaTexelOffsets,
           (void*)render->mediaBitShapeOffsets,
           (void*)render->shapeData,
           (void*)render->mediaTexels);

    return render->nodes == NULL &&
           render->lines == NULL &&
           render->mapSprites == NULL &&
           render->mediaTexelOffsets == NULL &&
           render->mediaBitShapeOffsets == NULL &&
           render->mapTextureTexels == NULL &&
           render->mapSpriteTexels == NULL &&
           render->shapeData == NULL &&
           render->mediaTexels == NULL &&
           !EspNativeWallCache_isActive() &&
           !EspNativeSpriteCache_isActive();
}

int DoomRPG_esp32ActivateMainMenuStart(struct DoomRPG_s* doomRpgBase) {
    DoomRPG_t* doomRpg = (DoomRPG_t*)doomRpgBase;
    DoomCanvas_t* doomCanvas;
    MenuSystem_t* menuSystem;
    Player_t* player;
    Render_t* render;
    uint32_t inputHash;
    uint32_t outputHash;
    uint32_t heapBefore;
    uint32_t heapAfter;
    uint32_t largestBefore;
    uint32_t largestAfter;
    uint32_t expectedInputHash;

    printf("\n=== Doom RPG ESP32 real MENU_MAIN -> Start Game entry ===\n");

    if (!DoomRPG_esp32MainMenuGraphicsBoundaryIsSafe(doomRpg) || doomRpg->player == NULL)  {
        printf("[MAINSTART] FAILED core/graphics boundary unavailable\n");
        return 0;
    }

    doomCanvas = doomRpg->doomCanvas;
    menuSystem = doomRpg->menuSystem;
    player = doomRpg->player;
    render = doomRpg->render;
    inputHash = DoomRPG_esp32MainMenuFramebufferHash(render);
    expectedInputHash =
        DoomRPG_esp32MainMenuSelectionFramebufferFNV(0);

    printf("[MAINSTART] Begin menu=%d selected=%d state=%d framebufferFNV=%08x expected=%08x skipIntro=%d startupMap=%d heap8=%u largest8=%u shapeData=%p mediaTexels=%p\n",
           menuSystem->menu,
           menuSystem->selectedIndex,
           doomCanvas->state,
           (unsigned int)inputHash,
           (unsigned int)expectedInputHash,
           doomCanvas->skipIntro,
           doomCanvas->startupMap,
           (unsigned int)heap8Free(),
           (unsigned int)largest8Block(),
           (void*)render->shapeData,
           (void*)render->mediaTexels);

    if (menuSystem->menu != MENU_MAIN ||
        menuSystem->selectedIndex != 0 ||
        doomCanvas->state != ST_MENU ||
        expectedInputHash == 0U || inputHash != expectedInputHash) {
        printf("[MAINSTART] FAILED precondition menu=%d selected=%d state=%d framebuffer=%08x expected=%08x\n",
               menuSystem->menu,
               menuSystem->selectedIndex,
               doomCanvas->state,
               (unsigned int)inputHash,
               (unsigned int)expectedInputHash);
        return 0;
    }

    printf("[MAINSTART] Player before level=%d xp=%d nextXP=%d credits=%d keys=%d ammo1=%u weapon=%d weapons=%08x deaths=%d\n",
           player->level,
           player->currentXP,
           player->nextLevelXP,
           player->credits,
           player->keys,
           (unsigned int)player->ammo[1],
           player->weapon,
           (unsigned int)player->weapons,
           player->totalDeaths);

    printIntroAssetPlan();
    /*
     * START is unconditional on ESP32: the dedicated LOAD card owns checkpoint
     * resume. A save file must never redirect START into legacy Continue.
     */
    printf("[MAINSTART] Route=new-game-only savePresence=ignored loadOwner=dedicated-LOAD-card\n");

    if (!DoomRPG_esp32ReleaseMainMenuMemory(doomRpg)) {
        printf("[MAINSTART] FAILED new-game menu memory cleanup contract\n");
        return 0;
    }

    heapBefore = heap8Free();
    largestBefore = largest8Block();
    /*
     * The dedicated LOAD card owns resume. START always performs the real
     * legacy-compatible new-game/player/intro transition; the compact native
     * menu-model owner only closes MENU_MAIN afterward.
     */
    Sound_playSound(doomRpg->sound, 5046, 0, 3);
    Menu_startGame(doomRpg->menu, 1);
    if (!DoomRPG_esp32MainMenuModelLeave(doomRpg)) {
        printf("[MAINSTART] FAILED leaving MENU_MAIN model for intro\n");
        return 0;
    }

    outputHash = DoomRPG_esp32MainMenuFramebufferHash(render);
    heapAfter = heap8Free();
    largestAfter = largest8Block();

    printf("[MAINSTART] After select menu=%d type=%d old=%d selected=%d items=%d state=%d framebufferFNV=%08x heap8=%u largest8=%u delta=%d largestDelta=%d\n",
           menuSystem->menu,
           menuSystem->type,
           menuSystem->oldMenu,
           menuSystem->selectedIndex,
           menuSystem->numItems,
           doomCanvas->state,
           (unsigned int)outputHash,
           (unsigned int)heapAfter,
           (unsigned int)largestAfter,
           (int)heapBefore - (int)heapAfter,
           (int)largestBefore - (int)largestAfter);

    if (!DoomRPG_esp32MainMenuGraphicsBoundaryIsSafe(doomRpg)) {
        printf("[MAINSTART] FAILED graphics boundary changed shapeData=%p mediaTexels=%p wallCache=%d spriteCache=%d\n",
               (void*)render->shapeData,
               (void*)render->mediaTexels,
               EspNativeWallCache_isActive(),
               EspNativeSpriteCache_isActive());
        return 0;
    }

    printf("[MAINSTART] Player after level=%d xp=%d nextXP=%d credits=%d keys=%d ammo1=%u weapon=%d weapons=%08x disabled=%08x deaths=%d\n",
           player->level,
           player->currentXP,
           player->nextLevelXP,
           player->credits,
           player->keys,
           (unsigned int)player->ammo[1],
           player->weapon,
           (unsigned int)player->weapons,
           (unsigned int)player->disabledWeapons,
           player->totalDeaths);
    printf("[MAINSTART] Intro story pointers page0=%p page1=%p story2=%p storyPage=%d storyTextPage=%d\n",
           (void*)doomCanvas->storyText1[0],
           (void*)doomCanvas->storyText1[1],
           (void*)doomCanvas->storyText2,
           doomCanvas->storyPage,
           doomCanvas->storyTextPage);

    if (menuSystem->menu != MENU_NONE ||
        doomCanvas->state != ST_INTRO ||
        !playerHasFreshResetContract(player)) {
        printf("[MAINSTART] FAILED fresh-game transition expected menu=%d state=%d resetContract=yes, got menu=%d state=%d resetContract=%s\n",
               MENU_NONE,
               ST_INTRO,
               menuSystem->menu,
               doomCanvas->state,
               playerHasFreshResetContract(player) ? "yes" : "NO");
        return 0;
    }

    printf("[MAINSTART] READY explicit MENU_MAIN start composition -> Menu_startGame(new) -> Player_reset -> ST_INTRO\n");
    printf("[MAINSTART] READY prologue loader executed; dead legal/menu runtime released before intro allocation\n");

    if (!DoomRPG_esp32RenderFirstIntroFrame(doomRpg)) {
        printf("[MAINSTART] FAILED bounded first ST_INTRO frame / clock+input handoff\n");
        return 0;
    }

    printf("[MAINSTART] READY first fitted ST_INTRO frame presented; 50 ms ESP32 intro clock + semantic touch input armed\n");
    printf("[MAINSTART] NEXT boundary = bounded intro disposal/loading handoff; final Continue currently parks before dispose/map load\n");
    return 1;
}
