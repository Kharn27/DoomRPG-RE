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

#include "native_main_menu_160x120_layout.h"
#include "native_main_menu_overlay_probe.h"
#include "native_sprite_lru_cache.h"
#include "native_wall_lru_cache.h"
#include "platform_video_config.h"

/* Keep ESP-IDF's stdbool macros after DoomRPG's legacy boolean enum. */
#include <esp_heap_caps.h>

#if DOOMRPG_LOGICAL_WIDTH != 160 || DOOMRPG_LOGICAL_HEIGHT != 120
#error "The ESP32 main-menu layout contract is defined only for 160x120"
#endif

#define EXPECTED_NATIVE_SCENE_FNV 0xffe0995eU
#define FAITHFUL_ORIGINAL_MENU_FNV 0x86c38260U
#define EXPECTED_MAIN_MENU_MODEL_FNV 0xbbc2149bU
#define EXPECTED_FONT_WIDTH 144
#define EXPECTED_FONT_HEIGHT 72
#define EXPECTED_HAND_WIDTH 13
#define EXPECTED_HAND_HEIGHT 10

static const char* expectedMainItems[DOOMRPG_ESP32_MAIN_MENU_ITEM_COUNT] = {
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

static uint32_t menuLayoutHash(const DoomCanvas_t* doomCanvas,
                               const MenuSystem_t* menuSystem,
                               int logoX) {
    uint32_t hash = 2166136261U;

    hash = fnvMixU32(hash, (uint32_t)doomCanvas->displayRect.w);
    hash = fnvMixU32(hash, (uint32_t)doomCanvas->displayRect.h);
    hash = fnvMixU32(hash, (uint32_t)menuSystem->imgLogo.width);
    hash = fnvMixU32(hash, (uint32_t)menuSystem->imgLogo.height);
    hash = fnvMixU32(hash, (uint32_t)logoX);
    hash = fnvMixU32(hash, DOOMRPG_ESP32_MAIN_MENU_LOGO_Y);
    hash = fnvMixU32(hash, DOOMRPG_ESP32_MAIN_MENU_LOGO_WIDTH);
    hash = fnvMixU32(hash, DOOMRPG_ESP32_MAIN_MENU_LOGO_HEIGHT);
    hash = fnvMixU32(hash, DOOMRPG_ESP32_MAIN_MENU_ITEM_START_Y);
    hash = fnvMixU32(hash, DOOMRPG_ESP32_MAIN_MENU_ITEM_LINE_HEIGHT);
    hash = fnvMixU32(hash, DOOMRPG_ESP32_MAIN_MENU_FONT_HEIGHT);
    hash = fnvMixU32(hash, (uint32_t)menuSystem->imgHand.width);
    hash = fnvMixU32(hash, (uint32_t)menuSystem->imgHand.height);
    hash = fnvMixU32(hash, DOOMRPG_ESP32_MAIN_MENU_ITEM_COUNT);
    return hash;
}

static int validateMainMenuModel(MenuSystem_t* menuSystem) {
    int i;

    if (menuSystem->menu != MENU_MAIN ||
        menuSystem->type != 4 ||
        menuSystem->numItems != DOOMRPG_ESP32_MAIN_MENU_ITEM_COUNT ||
        menuSystem->selectedIndex != 0 ||
        menuSystem->scrollIndex != 0 ||
        menuSystem->oldMenu != -1 ||
        menuSystem->imgBG != &menuSystem->imgLogo) {
        return 0;
    }

    for (i = 0; i < DOOMRPG_ESP32_MAIN_MENU_ITEM_COUNT; ++i) {
        if (strcmp(menuSystem->items[i].textField, expectedMainItems[i]) != 0 ||
            menuSystem->items[i].textField2[0] != '\0' ||
            menuSystem->items[i].flags != 2 ||
            menuSystem->items[i].action != 0) {
            return 0;
        }
    }

    return 1;
}

static int drawEsp32MainMenuLayout(
    DoomRPG_t* doomRpg,
    uint32_t stageHashes[DOOMRPG_ESP32_MAIN_MENU_ITEM_COUNT + 1]) {
    DoomCanvas_t* doomCanvas = doomRpg->doomCanvas;
    MenuSystem_t* menuSystem = doomRpg->menuSystem;
    SDL_Rect logoDst;
    int itemIndex;

    logoDst.x = doomCanvas->displayRect.x +
                ((doomCanvas->displayRect.w - DOOMRPG_ESP32_MAIN_MENU_LOGO_WIDTH) >> 1);
    logoDst.y = doomCanvas->displayRect.y + DOOMRPG_ESP32_MAIN_MENU_LOGO_Y;
    logoDst.w = DOOMRPG_ESP32_MAIN_MENU_LOGO_WIDTH;
    logoDst.h = DOOMRPG_ESP32_MAIN_MENU_LOGO_HEIGHT;

    if (SDL_RenderCopy(NULL, menuSystem->imgLogo.imgBitmap, NULL, &logoDst) != 0) {
        return 0;
    }
    stageHashes[0] = framebufferHash(doomRpg->render);

    DoomRPG_setFontColor(doomRpg, 0xffffffff);

    for (itemIndex = 0;
         itemIndex < DOOMRPG_ESP32_MAIN_MENU_ITEM_COUNT;
         ++itemIndex) {
        MenuItem_t* item = &menuSystem->items[itemIndex];
        int y = DOOMRPG_ESP32_MAIN_MENU_ITEM_START_Y +
                (itemIndex * DOOMRPG_ESP32_MAIN_MENU_ITEM_LINE_HEIGHT);
        int length = ((((int)strlen(item->textField) << 16) >> 9) * 7) >> 8;
        int x = doomCanvas->SCR_CX - length;

        if (itemIndex == menuSystem->selectedIndex) {
            DoomCanvas_drawImage(doomCanvas,
                                 &menuSystem->imgHand,
                                 x,
                                 y + (DOOMRPG_ESP32_MAIN_MENU_ITEM_LINE_HEIGHT >> 1),
                                 40);
            x += 2;
        }

        DoomCanvas_drawFont(doomCanvas,
                            item->textField,
                            x,
                            y,
                            0,
                            0,
                            -1,
                            false);

        stageHashes[itemIndex + 1] = framebufferHash(doomRpg->render);
    }

    DoomRPG_setFontColor(doomRpg, 0xffffffff);
    return 1;
}

/*
 * Replace only the ESP32 presentation of the already validated real main-menu
 * overlay. The faithful original-layout probe remains compiled and documented
 * as the 86c38260 reference, while this wrapper keeps the same MENU_MAIN model
 * and assets and changes only geometry for the 160x120 target.
 */
int __wrap_DoomRPG_probeNativeMainMenuOverlay(struct DoomRPG_s* doomRpgBase) {
    DoomRPG_t* doomRpg = (DoomRPG_t*)doomRpgBase;
    DoomCanvas_t* doomCanvas;
    MenuSystem_t* menuSystem;
    Render_t* render;
    uint32_t stageHashes[DOOMRPG_ESP32_MAIN_MENU_ITEM_COUNT + 1] = {0};
    uint32_t sceneHash;
    uint32_t finalHash;
    uint32_t modelHash;
    uint32_t layoutHash;
    uint32_t heapBefore;
    uint32_t largestBefore;
    uint32_t heapAfter;
    uint32_t largestAfter;
    uint32_t composeStart;
    uint32_t composeMs;
    int logoX;
    int logoBottom;
    int contentBottom;
    int i;

    printf("\n=== Doom RPG ESP32 MENU_MAIN 160x120 layout ===\n");

    if (doomRpg == NULL || doomRpg->doomCanvas == NULL ||
        doomRpg->menuSystem == NULL || doomRpg->menu == NULL ||
        doomRpg->render == NULL) {
        printf("[MAINLAYOUT] FAILED core menu/render objects unavailable\n");
        return 0;
    }

    doomCanvas = doomRpg->doomCanvas;
    menuSystem = doomRpg->menuSystem;
    render = doomRpg->render;

    if (render->framebuffer == NULL || render->shapeData != NULL ||
        render->mediaTexels != NULL || EspNativeWallCache_isActive() ||
        EspNativeSpriteCache_isActive()) {
        printf("[MAINLAYOUT] FAILED graphics boundary framebuffer=%p shapeData=%p mediaTexels=%p wallCache=%d spriteCache=%d\n",
               (void*)render->framebuffer,
               (void*)render->shapeData,
               (void*)render->mediaTexels,
               EspNativeWallCache_isActive(),
               EspNativeSpriteCache_isActive());
        return 0;
    }

    sceneHash = framebufferHash(render);
    printf("[MAINLAYOUT] Begin sceneFNV=%08x expected=%08x faithfulOriginalFNV=%08x heap8=%u largest8=%u\n",
           (unsigned int)sceneHash,
           (unsigned int)EXPECTED_NATIVE_SCENE_FNV,
           (unsigned int)FAITHFUL_ORIGINAL_MENU_FNV,
           (unsigned int)heap8Free(),
           (unsigned int)largest8Block());

    if (sceneHash != EXPECTED_NATIVE_SCENE_FNV) {
        printf("[MAINLAYOUT] FAILED walls+sprites regression changed before UI\n");
        return 0;
    }

    if (doomCanvas->displayRect.w != DOOMRPG_LOGICAL_WIDTH ||
        doomCanvas->displayRect.h != DOOMRPG_LOGICAL_HEIGHT ||
        doomCanvas->largeStatus ||
        menuSystem->imgLogo.imgBitmap == NULL ||
        menuSystem->imgLogo.width != DOOMRPG_ESP32_MAIN_MENU_LOGO_SRC_WIDTH ||
        menuSystem->imgLogo.height != DOOMRPG_ESP32_MAIN_MENU_LOGO_SRC_HEIGHT ||
        menuSystem->imgHand.imgBitmap == NULL ||
        menuSystem->imgHand.width != EXPECTED_HAND_WIDTH ||
        menuSystem->imgHand.height != EXPECTED_HAND_HEIGHT ||
        doomCanvas->imgFont.imgBitmap == NULL ||
        doomCanvas->imgFont.width != EXPECTED_FONT_WIDTH ||
        doomCanvas->imgFont.height != EXPECTED_FONT_HEIGHT) {
        printf("[MAINLAYOUT] FAILED target layout/assets display=%dx%d large=%d logo=%dx%d hand=%dx%d font=%dx%d\n",
               doomCanvas->displayRect.w,
               doomCanvas->displayRect.h,
               (int)doomCanvas->largeStatus,
               menuSystem->imgLogo.width,
               menuSystem->imgLogo.height,
               menuSystem->imgHand.width,
               menuSystem->imgHand.height,
               doomCanvas->imgFont.width,
               doomCanvas->imgFont.height);
        return 0;
    }

    heapBefore = heap8Free();
    largestBefore = largest8Block();

    menuSystem->menu = MENU_MAIN;
    Menu_initMenu(doomRpg->menu, MENU_MAIN);
    menuSystem->menu = MENU_MAIN;
    menuSystem->paintMenu = true;
    menuSystem->maxItems = doomCanvas->displayRect.h /
                           DOOMRPG_ESP32_MAIN_MENU_ITEM_LINE_HEIGHT;

    if (!validateMainMenuModel(menuSystem)) {
        printf("[MAINLAYOUT] FAILED original MENU_MAIN model changed menu=%d type=%d items=%d selected=%d scroll=%d old=%d\n",
               menuSystem->menu,
               menuSystem->type,
               menuSystem->numItems,
               menuSystem->selectedIndex,
               menuSystem->scrollIndex,
               menuSystem->oldMenu);
        return 0;
    }

    modelHash = menuModelHash(menuSystem);
    if (modelHash != EXPECTED_MAIN_MENU_MODEL_FNV) {
        printf("[MAINLAYOUT] FAILED MENU_MAIN model FNV=%08x expected=%08x\n",
               (unsigned int)modelHash,
               (unsigned int)EXPECTED_MAIN_MENU_MODEL_FNV);
        return 0;
    }

    logoX = doomCanvas->displayRect.x +
            ((doomCanvas->displayRect.w - DOOMRPG_ESP32_MAIN_MENU_LOGO_WIDTH) >> 1);
    logoBottom = doomCanvas->displayRect.y + DOOMRPG_ESP32_MAIN_MENU_LOGO_Y +
                 DOOMRPG_ESP32_MAIN_MENU_LOGO_HEIGHT;
    contentBottom = doomCanvas->displayRect.y +
                    DOOMRPG_ESP32_MAIN_MENU_CONTENT_BOTTOM;
    layoutHash = menuLayoutHash(doomCanvas, menuSystem, logoX);

    printf("[MAINLAYOUT] Model FNV=%08x items=%d selected=%d\n",
           (unsigned int)modelHash,
           menuSystem->numItems,
           menuSystem->selectedIndex);
    printf("[MAINLAYOUT] Geometry screen=%dx%d logoSrc=%dx%d logoDst=%d,%d %dx%d logoBottom=%d itemStart=%d line=%d rows=%d contentBottom=%d layoutFNV=%08x\n",
           doomCanvas->displayRect.w,
           doomCanvas->displayRect.h,
           menuSystem->imgLogo.width,
           menuSystem->imgLogo.height,
           logoX,
           doomCanvas->displayRect.y + DOOMRPG_ESP32_MAIN_MENU_LOGO_Y,
           DOOMRPG_ESP32_MAIN_MENU_LOGO_WIDTH,
           DOOMRPG_ESP32_MAIN_MENU_LOGO_HEIGHT,
           logoBottom,
           doomCanvas->displayRect.y + DOOMRPG_ESP32_MAIN_MENU_ITEM_START_Y,
           DOOMRPG_ESP32_MAIN_MENU_ITEM_LINE_HEIGHT,
           DOOMRPG_ESP32_MAIN_MENU_ITEM_COUNT,
           contentBottom,
           (unsigned int)layoutHash);

    if (logoBottom > doomCanvas->displayRect.y + DOOMRPG_ESP32_MAIN_MENU_ITEM_START_Y ||
        contentBottom > doomCanvas->displayRect.y + doomCanvas->displayRect.h) {
        printf("[MAINLAYOUT] FAILED layout geometry overlaps/overflows logoBottom=%d firstItem=%d contentBottom=%d screenBottom=%d\n",
               logoBottom,
               doomCanvas->displayRect.y + DOOMRPG_ESP32_MAIN_MENU_ITEM_START_Y,
               contentBottom,
               doomCanvas->displayRect.y + doomCanvas->displayRect.h);
        return 0;
    }

    for (i = 0; i < DOOMRPG_ESP32_MAIN_MENU_ITEM_COUNT; ++i) {
        printf("[MAINLAYOUT] ITEM index=%d y=%d selected=%s text=\"%s\"\n",
               i,
               doomCanvas->displayRect.y + DOOMRPG_ESP32_MAIN_MENU_ITEM_START_Y +
                   (i * DOOMRPG_ESP32_MAIN_MENU_ITEM_LINE_HEIGHT),
               i == menuSystem->selectedIndex ? "yes" : "no",
               menuSystem->items[i].textField);
    }

    composeStart = (uint32_t)DoomRPG_GetTimeMS();
    if (!drawEsp32MainMenuLayout(doomRpg, stageHashes)) {
        printf("[MAINLAYOUT] FAILED ESP32 160x120 menu composition\n");
        return 0;
    }
    composeMs = (uint32_t)DoomRPG_GetTimeMS() - composeStart;

    printf("[MAINLAYOUT] HASH stage=logo fnv=%08x\n",
           (unsigned int)stageHashes[0]);
    for (i = 0; i < DOOMRPG_ESP32_MAIN_MENU_ITEM_COUNT; ++i) {
        printf("[MAINLAYOUT] HASH stage=item%d text=\"%s\" fnv=%08x\n",
               i,
               expectedMainItems[i],
               (unsigned int)stageHashes[i + 1]);
    }

    finalHash = framebufferHash(render);
    heapAfter = heap8Free();
    largestAfter = largest8Block();

    printf("[MAINLAYOUT] framebufferFNV=%08x sceneFNV=%08x changed=%s composeMs=%u shapeData=%p mediaTexels=%p\n",
           (unsigned int)finalHash,
           (unsigned int)sceneHash,
           finalHash != sceneHash ? "yes" : "NO",
           (unsigned int)composeMs,
           (void*)render->shapeData,
           (void*)render->mediaTexels);
    printf("[MAINLAYOUT] End heap8=%u largest8=%u deltaFromStart=%d largestDelta=%d\n",
           (unsigned int)heapAfter,
           (unsigned int)largestAfter,
           (int)heapBefore - (int)heapAfter,
           (int)largestBefore - (int)largestAfter);

    if (stageHashes[0] == sceneHash || finalHash == sceneHash ||
        render->shapeData != NULL || render->mediaTexels != NULL ||
        EspNativeWallCache_isActive() || EspNativeSpriteCache_isActive() ||
        heapAfter != heapBefore || largestAfter != largestBefore) {
        printf("[MAINLAYOUT] FAILED ESP32 MENU_MAIN layout contract changed\n");
        return 0;
    }

    for (i = 1; i < DOOMRPG_ESP32_MAIN_MENU_ITEM_COUNT + 1; ++i) {
        if (stageHashes[i] == stageHashes[i - 1]) {
            printf("[MAINLAYOUT] FAILED item %d did not change framebuffer\n", i - 1);
            return 0;
        }
    }

    SDL_RenderPresent(NULL);
    printf("[MAINLAYOUT] Presented fitted Doom RPG MENU_MAIN on clean CYD display\n");
    printf("[MAINLAYOUT] READY original menu model/font/hand preserved; only logo scale + target geometry changed\n");
    printf("[MAINLAYOUT] READY item rows are stable for the next touch hit-test increment\n");
    return 1;
}
