#include <SDL.h>
#include <stdint.h>
#include <stdio.h>

#include "DoomRPG.h"
#include "DoomCanvas.h"
#include "Menu.h"
#include "MenuSystem.h"
#include "Player.h"
#include "Render.h"

#include "native_main_menu_start_action.h"
#include "native_sprite_lru_cache.h"
#include "native_wall_lru_cache.h"
#include "platform_video_config.h"

/* Keep ESP-IDF's stdbool macros after DoomRPG's legacy boolean enum. */
#include <esp_heap_caps.h>

#define EXPECTED_MAIN_START_SELECTED_FNV 0x58a11171U

static uint32_t heap8Free(void) {
    return (uint32_t)heap_caps_get_free_size(MALLOC_CAP_8BIT);
}

static uint32_t largest8Block(void) {
    return (uint32_t)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
}

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

static int graphicsBoundaryIsSafe(const DoomRPG_t* doomRpg) {
    const Render_t* render;

    if (doomRpg == NULL || doomRpg->render == NULL ||
        doomRpg->doomCanvas == NULL || doomRpg->menuSystem == NULL ||
        doomRpg->menu == NULL || doomRpg->player == NULL) {
        return 0;
    }

    render = doomRpg->render;
    return render->framebuffer != NULL &&
           render->shapeData == NULL &&
           render->mediaTexels == NULL &&
           !EspNativeWallCache_isActive() &&
           !EspNativeSpriteCache_isActive();
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

    printf("\n=== Doom RPG ESP32 real MENU_MAIN -> Start Game entry ===\n");

    if (!graphicsBoundaryIsSafe(doomRpg)) {
        printf("[MAINSTART] FAILED core/graphics boundary unavailable\n");
        return 0;
    }

    doomCanvas = doomRpg->doomCanvas;
    menuSystem = doomRpg->menuSystem;
    player = doomRpg->player;
    render = doomRpg->render;
    inputHash = framebufferHash(render);

    printf("[MAINSTART] Begin menu=%d selected=%d state=%d framebufferFNV=%08x expected=%08x skipIntro=%d startupMap=%d heap8=%u largest8=%u shapeData=%p mediaTexels=%p\n",
           menuSystem->menu,
           menuSystem->selectedIndex,
           doomCanvas->state,
           (unsigned int)inputHash,
           (unsigned int)EXPECTED_MAIN_START_SELECTED_FNV,
           doomCanvas->skipIntro,
           doomCanvas->startupMap,
           (unsigned int)heap8Free(),
           (unsigned int)largest8Block(),
           (void*)render->shapeData,
           (void*)render->mediaTexels);

    if (menuSystem->menu != MENU_MAIN ||
        menuSystem->selectedIndex != 0 ||
        doomCanvas->state != ST_MENU ||
        inputHash != EXPECTED_MAIN_START_SELECTED_FNV) {
        printf("[MAINSTART] FAILED precondition menu=%d selected=%d state=%d framebuffer=%08x expected=%08x\n",
               menuSystem->menu,
               menuSystem->selectedIndex,
               doomCanvas->state,
               (unsigned int)inputHash,
               (unsigned int)EXPECTED_MAIN_START_SELECTED_FNV);
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

    heapBefore = heap8Free();
    largestBefore = largest8Block();

    /* This is the original menu action. On a fresh profile Menu_select() calls
     * Menu_startGame(menu, 1), which performs Player_reset() and enters the
     * intro. If a save is present, the original engine instead transitions to
     * MENU_MAIN_CONTINUE; that path is detected and deliberately left for a
     * separate bounded menu milestone.
     */
    MenuSystem_select(menuSystem);

    outputHash = framebufferHash(render);
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

    if (!graphicsBoundaryIsSafe(doomRpg)) {
        printf("[MAINSTART] FAILED graphics boundary changed shapeData=%p mediaTexels=%p wallCache=%d spriteCache=%d\n",
               (void*)render->shapeData,
               (void*)render->mediaTexels,
               EspNativeWallCache_isActive(),
               EspNativeSpriteCache_isActive());
        return 0;
    }

    if (menuSystem->menu == MENU_MAIN_CONTINUE) {
        printf("[MAINSTART] READY existing-save path reached MENU_MAIN_CONTINUE; Continue/New Game painter intentionally deferred\n");
        return 1;
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

    printf("[MAINSTART] READY real MenuSystem_select -> Menu_startGame(new) -> Player_reset -> ST_INTRO\n");
    printf("[MAINSTART] READY prologue loader executed; map/gameplay loader not advanced because ESP32 loop remains parked\n");
    printf("[MAINSTART] NEXT boundary = drive one real ST_INTRO frame through DoomCanvas_drawStory without entering map load\n");
    return 1;
}
