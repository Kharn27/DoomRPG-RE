#include <SDL.h>
#include <stdint.h>
#include <stdio.h>

#include "DoomRPG.h"
#include "DoomCanvas.h"
#include "Game.h"
#include "Menu.h"
#include "esp_native_menu_state.h"
#include "esp_native_gameplay_player_state.h"
#include "Render.h"
#include "esp_native_audio_intent.h"
#include "esp_native_intro_state.h"

#include "esp_legacy_asset_source.h"
#include "native_intro_first_frame.h"
#include "native_story_fit.h"
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

static int playerHasFreshResetContract(
    const EspNativeGameplayPlayerState* player) {
    return player != NULL && player->active == 1U &&
           player->level == 1U &&
           player->currentXP == 0U &&
           player->nextLevelXP == 80U &&
           player->keys == 0U &&
           player->credits == 0U &&
           player->ammo[1] == 8U &&
           player->weapon == 2U &&
           player->weapons == 4U &&
           player->disabledWeapons == 0U &&
           EspNativeGameplayPlayerState_health() == 30U &&
           EspNativeGameplayPlayerState_maxHealth() == 30U &&
           EspNativeGameplayPlayerState_armor() == 0U &&
           EspNativeGameplayPlayerState_maxArmor() == 20U &&
           EspNativeGameplayPlayerState_defense() == 16U &&
           EspNativeGameplayPlayerState_strength() == 12U &&
           EspNativeGameplayPlayerState_agility() == 14U &&
           EspNativeGameplayPlayerState_accuracy() == 16U;
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
    EspNativeMenuState_t* menuSystem;
    EspNativeGameplayPlayerState playerState;
    EspNativeIntroState_t* introState;
    Render_t* render;
    uint32_t inputHash;
    uint32_t outputHash;
    uint32_t heapBefore;
    uint32_t heapAfter;
    uint32_t largestBefore;
    uint32_t largestAfter;
    uint32_t expectedInputHash;

    printf("\n=== Doom RPG ESP32 real MENU_MAIN -> Start Game entry ===\n");

    if (!DoomRPG_esp32MainMenuGraphicsBoundaryIsSafe(doomRpg) ||
        doomRpg->player != NULL) {
        printf("[MAINSTART] FAILED core/graphics boundary unavailable legacyPlayer=%p expected=NULL\n",
               doomRpg != NULL ? (void*)doomRpg->player : NULL);
        return 0;
    }

    doomCanvas = doomRpg->doomCanvas;
    menuSystem = doomRpg->menuSystem;
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

    /*
     * The ESP32 START owner is deliberately new-game + intro only. The
     * historical skipIntro branch entered DoomCanvas_loadMap(), bypassing the
     * native resident bootstrap; keep that unsupported route fail-closed before
     * releasing menu memory or mutating player state.
     */
    if (doomCanvas->skipIntro) {
        printf("[MAINSTART] REFUSE skipIntro=%d\n", doomCanvas->skipIntro);
        return 0;
    }

    printf("[MAINSTART] Native player before stateFNV=%08x legacyPlayer=%p owner=native-gameplay-player-state\n",
           (unsigned int)EspNativeGameplayPlayerState_fingerprint(),
           (void*)doomRpg->player);

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
     * The dedicated LOAD card owns resume. Reproduce only the new-game branch
     * of legacy Menu_startGame() here so the linker can discard its unrelated
     * load-state / legacy-load-map branches.
     */
    (void)EspNativeAudioIntent_publish(5046U, 0U, 3U);
    menuSystem->imgBG = NULL;
    EspNativeGameplayPlayerState_resetFresh();
    if (!EspNativeGameplayPlayerState_snapshot(&playerState) ||
        !playerHasFreshResetContract(&playerState)) {
        printf("[MAINSTART] FAILED native fresh-player reset contract stateFNV=%08x\n",
               (unsigned int)EspNativeGameplayPlayerState_fingerprint());
        return 0;
    }
    DoomCanvas_setState(doomCanvas, ST_INTRO);
    introState = EspNativeIntroState_get(doomRpg);
    if (doomCanvas->state != ST_INTRO || introState == NULL) {
        printf("[MAINSTART] FAILED native transient intro owner was not established\n");
        return 0;
    }
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

    if (!EspNativeGameplayPlayerState_snapshot(&playerState)) {
        printf("[MAINSTART] FAILED native player snapshot after ST_INTRO\n");
        return 0;
    }
    printf("[MAINSTART] Player after level=%u xp=%u nextXP=%u credits=%u keys=%08x ammo1=%u weapon=%u weapons=%04x disabled=%04x hp=%u/%u armor=%u/%u stateFNV=%08x legacyPlayer=%p\n",
           (unsigned int)playerState.level,
           (unsigned int)playerState.currentXP,
           (unsigned int)playerState.nextLevelXP,
           (unsigned int)playerState.credits,
           (unsigned int)playerState.keys,
           (unsigned int)playerState.ammo[1],
           (unsigned int)playerState.weapon,
           (unsigned int)playerState.weapons,
           (unsigned int)playerState.disabledWeapons,
           (unsigned int)EspNativeGameplayPlayerState_health(),
           (unsigned int)EspNativeGameplayPlayerState_maxHealth(),
           (unsigned int)EspNativeGameplayPlayerState_armor(),
           (unsigned int)EspNativeGameplayPlayerState_maxArmor(),
           (unsigned int)EspNativeGameplayPlayerState_fingerprint(),
           (void*)doomRpg->player);
    printf("[MAINSTART] Intro story pointers page0=%p page1=%p story2=%p storyPage=%d storyTextPage=%d owner=native-transient\n",
           (void*)introState->storyText1[0],
           (void*)introState->storyText1[1],
           (void*)introState->storyText2,
           introState->storyPage,
           introState->storyTextPage);

    if (menuSystem->menu != MENU_NONE ||
        doomCanvas->state != ST_INTRO ||
        doomRpg->player != NULL ||
        !playerHasFreshResetContract(&playerState)) {
        printf("[MAINSTART] FAILED fresh-game transition expected menu=%d state=%d nativeReset=yes legacyPlayer=NULL, got menu=%d state=%d resetContract=%s legacyPlayer=%p\n",
               MENU_NONE,
               ST_INTRO,
               menuSystem->menu,
               doomCanvas->state,
               playerHasFreshResetContract(&playerState) ? "yes" : "NO",
               (void*)doomRpg->player);
        return 0;
    }

    printf("[MAINSTART] READY native new-game -> PlayerState_resetFresh -> ST_INTRO legacyPlayer=NULL\n");
    printf("[MAINSTART] READY prologue loader executed; dead legal/menu runtime released before intro allocation\n");

    if (!Esp32StoryFit_prepare(doomCanvas)) {
        printf("[MAINSTART] FAILED native story hand prepare before bounded first frame\n");
        return 0;
    }

    if (!DoomRPG_esp32RenderFirstIntroFrame(doomRpg)) {
        printf("[MAINSTART] FAILED bounded first ST_INTRO frame / clock+input handoff\n");
        return 0;
    }

    printf("[MAINSTART] READY first fitted ST_INTRO frame presented; 50 ms ESP32 intro clock + semantic touch input armed\n");
    printf("[MAINSTART] NEXT boundary = bounded intro disposal/loading handoff; final Continue currently parks before dispose/map load\n");
    return 1;
}
