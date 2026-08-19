#include <SDL.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "DoomRPG.h"
#include "DoomCanvas.h"
#include "Menu.h"
#include "MenuItem.h"
#include "MenuSystem.h"
#include "Render.h"

#include "native_main_menu_overlay_probe.h"
#include "native_sprite_lru_cache.h"
#include "native_wall_lru_cache.h"
#include "platform_video_config.h"

/* Keep ESP-IDF's stdbool macros after DoomRPG's legacy boolean enum. */
#include <esp_heap_caps.h>

#define EXPECTED_NATIVE_SCENE_FNV 0xffe0995eU
#define EXPECTED_MAIN_MENU_ITEMS 4
#define EXPECTED_MAIN_MENU_TYPE 4

static const char* expectedMainItems[EXPECTED_MAIN_MENU_ITEMS] = {
    "Start Game",
    "Options   ",
    "Help/About",
    "Exit      "
};

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

static uint32_t fnvMixU32(uint32_t hash, uint32_t value) {
    int shift;
    for (shift = 0; shift < 32; shift += 8) {
        hash ^= (value >> shift) & 0xffU;
        hash *= 16777619U;
    }
    return hash;
}

static uint32_t fnvMixString(uint32_t hash, const char* text) {
    if (text == NULL) {
        return fnvMixU32(hash, 0xffffffffU);
    }

    while (*text != '\0') {
        hash ^= (uint8_t)*text++;
        hash *= 16777619U;
    }
    hash ^= 0U;
    hash *= 16777619U;
    return hash;
}

static uint32_t menuModelHash(const MenuSystem_t* menuSystem) {
    uint32_t hash = 2166136261U;
    int i;

    hash = fnvMixU32(hash, (uint32_t)menuSystem->menu);
    hash = fnvMixU32(hash, (uint32_t)menuSystem->type);
    hash = fnvMixU32(hash, (uint32_t)menuSystem->numItems);
    hash = fnvMixU32(hash, (uint32_t)menuSystem->selectedIndex);
    hash = fnvMixU32(hash, (uint32_t)menuSystem->scrollIndex);
    hash = fnvMixU32(hash, (uint32_t)menuSystem->maxItems);

    for (i = 0; i < menuSystem->numItems; ++i) {
        hash = fnvMixString(hash, menuSystem->items[i].textField);
        hash = fnvMixString(hash, menuSystem->items[i].textField2);
        hash = fnvMixU32(hash, (uint32_t)menuSystem->items[i].flags);
        hash = fnvMixU32(hash, (uint32_t)menuSystem->items[i].action);
    }

    return hash;
}

static int validateMainMenuModel(MenuSystem_t* menuSystem) {
    int i;

    if (menuSystem->menu != MENU_MAIN ||
        menuSystem->type != EXPECTED_MAIN_MENU_TYPE ||
        menuSystem->numItems != EXPECTED_MAIN_MENU_ITEMS ||
        menuSystem->selectedIndex != 0 ||
        menuSystem->scrollIndex != 0 ||
        menuSystem->oldMenu != -1 ||
        menuSystem->imgBG != &menuSystem->imgLogo) {
        return 0;
    }

    for (i = 0; i < EXPECTED_MAIN_MENU_ITEMS; ++i) {
        if (strcmp(menuSystem->items[i].textField, expectedMainItems[i]) != 0 ||
            menuSystem->items[i].textField2[0] != '\0' ||
            menuSystem->items[i].flags != 2 ||
            menuSystem->items[i].action != 0) {
            return 0;
        }
    }

    return 1;
}

static int drawExactMainMenuOverlay(DoomRPG_t* doomRpg,
                                    uint32_t stageHashes[EXPECTED_MAIN_MENU_ITEMS + 1]) {
    DoomCanvas_t* doomCanvas = doomRpg->doomCanvas;
    MenuSystem_t* menuSystem = doomRpg->menuSystem;
    int i = 40;
    int y = 80;
    int baseX;
    int lineHeight = 12;
    int halfLine;
    int glyphWidth = 7;
    boolean isLargerFont = false;
    int itemIndex;

    DoomRPG_setFontColor(doomRpg, 0xffffffff);

    if (menuSystem->imgBG == NULL) {
        return 0;
    }

    DoomCanvas_drawImage(doomCanvas,
                         menuSystem->imgBG,
                         doomCanvas->SCR_CX,
                         0,
                         17);
    stageHashes[0] = framebufferHash(doomRpg->render);

    /* MenuSystem_paint() sets this after the background/3D composition. */
    menuSystem->maxItems = doomCanvas->displayRect.h / 12;

    if ((menuSystem->menu > MENU_NONE) &&
        (menuSystem->menu < MENU_MAIN_OPTIONS) &&
        (menuSystem->menu != MENU_MAIN_HELP_ABOUT) &&
        doomCanvas->largeStatus) {
        glyphWidth = 10;
        lineHeight = 17;
        isLargerFont = true;
    }

    halfLine = lineHeight >> 1;
    baseX = doomCanvas->SCR_CX + i - 64;

    for (itemIndex = menuSystem->scrollIndex;
         itemIndex < menuSystem->numItems;
         ++itemIndex) {
        MenuItem_t* item = &menuSystem->items[itemIndex];
        char textField[32];
        char textField2[16];
        int x = baseX;

        memcpy(textField, item->textField, sizeof(textField));
        memcpy(textField2, item->textField2, sizeof(textField2));
        textField[sizeof(textField) - 1] = '\0';
        textField2[sizeof(textField2) - 1] = '\0';

        DoomRPG_setFontColor(doomRpg, 0xffffffff);

        if (textField[0] != '\0' && (item->flags & 2) != 0) {
            int length = ((((int)strlen(textField) << 16) >> 9) * glyphWidth) >> 8;
            x = (menuSystem->maxItems == 0 ||
                 menuSystem->numItems <= menuSystem->maxItems)
                    ? doomCanvas->SCR_CX - length
                    : (doomCanvas->SCR_CX - 6) - length;
        }
        else if (textField2[0] != '\0') {
            int rightX = (doomCanvas->SCR_CX + 64) - 2;
            if (menuSystem->maxItems != 0) {
                rightX -= isLargerFont ? 13 : 9;
            }
            DoomCanvas_drawFont(doomCanvas,
                                textField2,
                                rightX,
                                y,
                                9,
                                0,
                                -1,
                                isLargerFont);
        }

        if (menuSystem->type != 5 && itemIndex == menuSystem->selectedIndex) {
            DoomCanvas_drawImage(doomCanvas,
                                 &menuSystem->imgHand,
                                 x,
                                 y + halfLine,
                                 40);
            x += 2;
        }

        if (textField[0] != '\0') {
            DoomCanvas_drawFont(doomCanvas,
                                textField,
                                x,
                                y,
                                0,
                                0,
                                -1,
                                isLargerFont);
        }

        if (itemIndex < EXPECTED_MAIN_MENU_ITEMS) {
            stageHashes[itemIndex + 1] = framebufferHash(doomRpg->render);
        }

        y += lineHeight;
        if (menuSystem->maxItems != 0 &&
            itemIndex == (menuSystem->scrollIndex + menuSystem->maxItems) - 1) {
            break;
        }
    }

    DoomRPG_setFontColor(doomRpg, 0xffffffff);
    return 1;
}

int DoomRPG_probeNativeMainMenuOverlay(struct DoomRPG_s* doomRpgBase) {
    DoomRPG_t* doomRpg = (DoomRPG_t*)doomRpgBase;
    DoomCanvas_t* doomCanvas;
    MenuSystem_t* menuSystem;
    Render_t* render;
    uint32_t stageHashes[EXPECTED_MAIN_MENU_ITEMS + 1] = {0};
    uint32_t sceneHash;
    uint32_t finalHash;
    uint32_t modelHash;
    uint32_t heapBefore;
    uint32_t largestBefore;
    uint32_t heapAfter;
    uint32_t largestAfter;
    uint32_t composeStart;
    uint32_t composeMs;
    int i;

    printf("\n=== Doom RPG ESP32 real MENU_MAIN overlay ===\n");

    if (doomRpg == NULL || doomRpg->doomCanvas == NULL ||
        doomRpg->menuSystem == NULL || doomRpg->menu == NULL ||
        doomRpg->render == NULL) {
        printf("[MAINMENU] FAILED core menu/render objects unavailable\n");
        return 0;
    }

    doomCanvas = doomRpg->doomCanvas;
    menuSystem = doomRpg->menuSystem;
    render = doomRpg->render;

    if (render->framebuffer == NULL || render->shapeData != NULL ||
        render->mediaTexels != NULL || EspNativeWallCache_isActive() ||
        EspNativeSpriteCache_isActive()) {
        printf("[MAINMENU] FAILED graphics boundary framebuffer=%p shapeData=%p mediaTexels=%p wallCache=%d spriteCache=%d\n",
               (void*)render->framebuffer,
               (void*)render->shapeData,
               (void*)render->mediaTexels,
               EspNativeWallCache_isActive(),
               EspNativeSpriteCache_isActive());
        return 0;
    }

    sceneHash = framebufferHash(render);
    printf("[MAINMENU] Begin sceneFNV=%08x expected=%08x heap8=%u largest8=%u shapeData=%p mediaTexels=%p\n",
           (unsigned int)sceneHash,
           (unsigned int)EXPECTED_NATIVE_SCENE_FNV,
           (unsigned int)heap8Free(),
           (unsigned int)largest8Block(),
           (void*)render->shapeData,
           (void*)render->mediaTexels);

    if (sceneHash != EXPECTED_NATIVE_SCENE_FNV) {
        printf("[MAINMENU] FAILED walls+sprites regression changed before overlay\n");
        return 0;
    }

    if (menuSystem->imgLogo.imgBitmap == NULL ||
        menuSystem->imgHand.imgBitmap == NULL ||
        doomCanvas->imgFont.imgBitmap == NULL ||
        menuSystem->imgLogo.width <= 0 || menuSystem->imgLogo.height <= 0 ||
        menuSystem->imgHand.width <= 0 || menuSystem->imgHand.height <= 0 ||
        doomCanvas->imgFont.width <= 0 || doomCanvas->imgFont.height <= 0) {
        printf("[MAINMENU] FAILED resident menu/font image assets unavailable\n");
        return 0;
    }

    printf("[MAINMENU] Assets logo=%dx%d transparent=%d hand=%dx%d transparent=%d font=%dx%d largeStatus=%d\n",
           menuSystem->imgLogo.width,
           menuSystem->imgLogo.height,
           (int)menuSystem->imgLogo.isTransparentMask,
           menuSystem->imgHand.width,
           menuSystem->imgHand.height,
           (int)menuSystem->imgHand.isTransparentMask,
           doomCanvas->imgFont.width,
           doomCanvas->imgFont.height,
           (int)doomCanvas->largeStatus);

    if (doomCanvas->largeStatus && doomCanvas->imgLargerFont.imgBitmap == NULL) {
        printf("[MAINMENU] FAILED largeStatus requires resident larger font\n");
        return 0;
    }

    heapBefore = heap8Free();
    largestBefore = largest8Block();

    /* Reuse the original deterministic menu model builder without invoking
     * MenuSystem_setMenu(), which would also perform state/map transitions.
     */
    menuSystem->menu = MENU_MAIN;
    Menu_initMenu(doomRpg->menu, MENU_MAIN);
    menuSystem->menu = MENU_MAIN;
    menuSystem->paintMenu = true;
    menuSystem->maxItems = doomCanvas->displayRect.h / 12;

    if (!validateMainMenuModel(menuSystem)) {
        printf("[MAINMENU] FAILED MENU_MAIN model differs from expected original contract menu=%d type=%d items=%d selected=%d scroll=%d old=%d imgBG=%p logo=%p\n",
               menuSystem->menu,
               menuSystem->type,
               menuSystem->numItems,
               menuSystem->selectedIndex,
               menuSystem->scrollIndex,
               menuSystem->oldMenu,
               (void*)menuSystem->imgBG,
               (void*)&menuSystem->imgLogo);
        return 0;
    }

    modelHash = menuModelHash(menuSystem);
    printf("[MAINMENU] Model menu=%d type=%d items=%d selected=%d scroll=%d maxItems=%d modelFNV=%08x imgBG=logo\n",
           menuSystem->menu,
           menuSystem->type,
           menuSystem->numItems,
           menuSystem->selectedIndex,
           menuSystem->scrollIndex,
           menuSystem->maxItems,
           (unsigned int)modelHash);

    for (i = 0; i < EXPECTED_MAIN_MENU_ITEMS; ++i) {
        printf("[MAINMENU] ITEM index=%d selected=%s flags=%u action=%d text=\"%s\"\n",
               i,
               i == menuSystem->selectedIndex ? "yes" : "no",
               (unsigned int)menuSystem->items[i].flags,
               menuSystem->items[i].action,
               menuSystem->items[i].textField);
    }

    composeStart = (uint32_t)DoomRPG_GetTimeMS();
    if (!drawExactMainMenuOverlay(doomRpg, stageHashes)) {
        printf("[MAINMENU] FAILED exact post-3D menu composition\n");
        return 0;
    }
    composeMs = (uint32_t)DoomRPG_GetTimeMS() - composeStart;

    printf("[MAINMENU] HASH stage=logo fnv=%08x\n",
           (unsigned int)stageHashes[0]);
    for (i = 0; i < EXPECTED_MAIN_MENU_ITEMS; ++i) {
        printf("[MAINMENU] HASH stage=item%d text=\"%s\" fnv=%08x\n",
               i,
               expectedMainItems[i],
               (unsigned int)stageHashes[i + 1]);
    }

    finalHash = framebufferHash(render);
    heapAfter = heap8Free();
    largestAfter = largest8Block();

    printf("[MAINMENU] framebufferFNV=%08x sceneFNV=%08x changed=%s composeMs=%u shapeData=%p mediaTexels=%p\n",
           (unsigned int)finalHash,
           (unsigned int)sceneHash,
           finalHash != sceneHash ? "yes" : "NO",
           (unsigned int)composeMs,
           (void*)render->shapeData,
           (void*)render->mediaTexels);
    printf("[MAINMENU] End heap8=%u largest8=%u deltaFromStart=%d largestDelta=%d\n",
           (unsigned int)heapAfter,
           (unsigned int)largestAfter,
           (int)heapBefore - (int)heapAfter,
           (int)largestBefore - (int)largestAfter);

    if (stageHashes[0] == sceneHash ||
        finalHash == sceneHash ||
        render->shapeData != NULL || render->mediaTexels != NULL ||
        EspNativeWallCache_isActive() || EspNativeSpriteCache_isActive() ||
        heapAfter != heapBefore || largestAfter != largestBefore) {
        printf("[MAINMENU] FAILED real MENU_MAIN overlay contract changed\n");
        return 0;
    }

    SDL_RenderPresent(NULL);
    printf("[MAINMENU] Presented real Doom RPG MENU_MAIN overlay over native menu scene\n");
    printf("[MAINMENU] READY original Menu model + logo + hand + font composed without legacy Render_render\n");
    printf("[MAINMENU] READY framebuffer now contains native scene plus real main-menu UI; interaction remains intentionally out of scope\n");
    return 1;
}
