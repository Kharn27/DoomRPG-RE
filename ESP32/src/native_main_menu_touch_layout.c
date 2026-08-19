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
#include "native_main_menu_touch.h"
#include "native_main_menu_touch_layout.h"
#include "native_sprite_lru_cache.h"
#include "native_wall_lru_cache.h"
#include "platform_video_config.h"

/* Keep ESP-IDF's stdbool macros after DoomRPG's legacy boolean enum. */
#include <esp_heap_caps.h>

#if DOOMRPG_LOGICAL_WIDTH != 160 || DOOMRPG_LOGICAL_HEIGHT != 120
#error "The ESP32 touch-ready main menu is defined only for 160x120"
#endif

#define EXPECTED_NATIVE_SCENE_FNV 0xffe0995eU
#define FAITHFUL_ORIGINAL_MENU_FNV 0x86c38260U
#define PRIOR_FITTED_MENU_FNV 0x1afa0223U
#define EXPECTED_MAIN_MENU_MODEL_FNV 0xbbc2149bU
#define EXPECTED_LAYOUT_FNV 0x47b3656eU
#define EXPECTED_BLACK_LOGO_FNV 0x0ac1f9c6U
#define EXPECTED_FONT_WIDTH 144
#define EXPECTED_FONT_HEIGHT 72
#define EXPECTED_HAND_WIDTH 13
#define EXPECTED_HAND_HEIGHT 10
#define MENU_GLYPH_ADVANCE 7

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

static int graphicsBoundaryIsSafe(const DoomRPG_t* doomRpg) {
    const Render_t* render;

    if (doomRpg == NULL || doomRpg->doomCanvas == NULL ||
        doomRpg->menuSystem == NULL || doomRpg->menu == NULL ||
        doomRpg->render == NULL) {
        return 0;
    }

    render = doomRpg->render;
    return render->framebuffer != NULL &&
           render->shapeData == NULL &&
           render->mediaTexels == NULL &&
           !EspNativeWallCache_isActive() &&
           !EspNativeSpriteCache_isActive();
}

static int validateMainMenuModel(const MenuSystem_t* menuSystem) {
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

static int validatePresentationContract(DoomRPG_t* doomRpg,
                                        uint32_t* modelHashOut,
                                        uint32_t* layoutHashOut) {
    DoomCanvas_t* doomCanvas;
    MenuSystem_t* menuSystem;
    int logoX;
    int logoBottom;
    int contentBottom;
    uint32_t modelHash;
    uint32_t layoutHash;

    if (!graphicsBoundaryIsSafe(doomRpg)) {
        return 0;
    }

    doomCanvas = doomRpg->doomCanvas;
    menuSystem = doomRpg->menuSystem;

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
        return 0;
    }

    menuSystem->paintMenu = true;
    menuSystem->maxItems = doomCanvas->displayRect.h /
                           DOOMRPG_ESP32_MAIN_MENU_ITEM_LINE_HEIGHT;

    if (!validateMainMenuModel(menuSystem)) {
        return 0;
    }

    modelHash = menuModelHash(menuSystem);
    logoX = doomCanvas->displayRect.x +
            ((doomCanvas->displayRect.w -
              DOOMRPG_ESP32_MAIN_MENU_LOGO_WIDTH) >> 1);
    logoBottom = doomCanvas->displayRect.y +
                 DOOMRPG_ESP32_MAIN_MENU_LOGO_Y +
                 DOOMRPG_ESP32_MAIN_MENU_LOGO_HEIGHT;
    contentBottom = doomCanvas->displayRect.y +
                    DOOMRPG_ESP32_MAIN_MENU_CONTENT_BOTTOM;
    layoutHash = menuLayoutHash(doomCanvas, menuSystem, logoX);

    if (modelHash != EXPECTED_MAIN_MENU_MODEL_FNV ||
        layoutHash != EXPECTED_LAYOUT_FNV ||
        logoBottom > doomCanvas->displayRect.y +
                     DOOMRPG_ESP32_MAIN_MENU_ITEM_START_Y ||
        contentBottom > doomCanvas->displayRect.y + doomCanvas->displayRect.h) {
        return 0;
    }

    if (modelHashOut != NULL) {
        *modelHashOut = modelHash;
    }
    if (layoutHashOut != NULL) {
        *layoutHashOut = layoutHash;
    }
    return 1;
}

static int drawTouchReadyMainMenuOpaque(
    DoomRPG_t* doomRpg,
    uint32_t stageHashes[DOOMRPG_ESP32_MAIN_MENU_ITEM_COUNT + 1]) {
    DoomCanvas_t* doomCanvas = doomRpg->doomCanvas;
    MenuSystem_t* menuSystem = doomRpg->menuSystem;
    SDL_Rect logoDst;
    int itemIndex;

    DoomRPG_setColor(doomRpg, 0x000000);
    DoomRPG_fillRect(doomRpg,
                     doomCanvas->displayRect.x,
                     doomCanvas->displayRect.y,
                     doomCanvas->displayRect.w,
                     doomCanvas->displayRect.h);

    logoDst.x = doomCanvas->displayRect.x +
                ((doomCanvas->displayRect.w -
                  DOOMRPG_ESP32_MAIN_MENU_LOGO_WIDTH) >> 1);
    logoDst.y = doomCanvas->displayRect.y +
                DOOMRPG_ESP32_MAIN_MENU_LOGO_Y;
    logoDst.w = DOOMRPG_ESP32_MAIN_MENU_LOGO_WIDTH;
    logoDst.h = DOOMRPG_ESP32_MAIN_MENU_LOGO_HEIGHT;

    if (SDL_RenderCopy(NULL, menuSystem->imgLogo.imgBitmap, NULL, &logoDst) != 0) {
        return 0;
    }
    stageHashes[0] = framebufferHash(doomRpg->render);

    if (stageHashes[0] != EXPECTED_BLACK_LOGO_FNV) {
        printf("[MAINOPAQUE] FAILED black+logo fnv=%08x expected=%08x\n",
               (unsigned int)stageHashes[0],
               (unsigned int)EXPECTED_BLACK_LOGO_FNV);
        return 0;
    }

    /* Capture black+logo pixels under all future hand positions before rows. */
    if (!DoomRPG_esp32MainMenuTouchPrepare(doomRpg)) {
        return 0;
    }

    DoomRPG_setFontColor(doomRpg, 0xffffffff);

    for (itemIndex = 0;
         itemIndex < DOOMRPG_ESP32_MAIN_MENU_ITEM_COUNT;
         ++itemIndex) {
        MenuItem_t* item = &menuSystem->items[itemIndex];
        int y = DOOMRPG_ESP32_MAIN_MENU_ITEM_START_Y +
                (itemIndex * DOOMRPG_ESP32_MAIN_MENU_ITEM_LINE_HEIGHT);
        int length = ((((int)strlen(item->textField) << 16) >> 9) *
                      MENU_GLYPH_ADVANCE) >> 8;
        int x = doomCanvas->SCR_CX - length;

        if (itemIndex == menuSystem->selectedIndex) {
            DoomCanvas_drawImage(doomCanvas,
                                 &menuSystem->imgHand,
                                 x,
                                 y +
                                     (DOOMRPG_ESP32_MAIN_MENU_ITEM_LINE_HEIGHT >> 1),
                                 40);
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

int DoomRPG_esp32RepaintOpaqueMainMenu(struct DoomRPG_s* doomRpgBase,
                                       uint32_t* finalFramebufferFNV) {
    DoomRPG_t* doomRpg = (DoomRPG_t*)doomRpgBase;
    Render_t* render;
    uint32_t stageHashes[DOOMRPG_ESP32_MAIN_MENU_ITEM_COUNT + 1] = {0};
    uint32_t modelHash = 0;
    uint32_t layoutHash = 0;
    uint32_t finalHash;
    uint32_t heapBefore;
    uint32_t heapAfter;
    uint32_t largestBefore;
    uint32_t largestAfter;
    uint32_t composeStart;
    uint32_t composeMs;
    int i;

    if (!validatePresentationContract(doomRpg, &modelHash, &layoutHash)) {
        printf("[MAINOPAQUE] FAILED presentation contract menu=%d selected=%d\n",
               doomRpg != NULL && doomRpg->menuSystem != NULL
                   ? doomRpg->menuSystem->menu : -999,
               doomRpg != NULL && doomRpg->menuSystem != NULL
                   ? doomRpg->menuSystem->selectedIndex : -999);
        return 0;
    }

    render = doomRpg->render;
    heapBefore = heap8Free();
    largestBefore = largest8Block();
    composeStart = (uint32_t)DoomRPG_GetTimeMS();

    if (!drawTouchReadyMainMenuOpaque(doomRpg, stageHashes)) {
        printf("[MAINOPAQUE] FAILED composition\n");
        return 0;
    }

    composeMs = (uint32_t)DoomRPG_GetTimeMS() - composeStart;
    finalHash = framebufferHash(render);

    for (i = 1; i < DOOMRPG_ESP32_MAIN_MENU_ITEM_COUNT + 1; ++i) {
        if (stageHashes[i] == stageHashes[i - 1]) {
            printf("[MAINOPAQUE] FAILED item %d did not change framebuffer\n",
                   i - 1);
            return 0;
        }
    }

    if (finalHash == 0 || render->shapeData != NULL || render->mediaTexels != NULL ||
        EspNativeWallCache_isActive() || EspNativeSpriteCache_isActive()) {
        printf("[MAINOPAQUE] FAILED graphics boundary final=%08x shapeData=%p mediaTexels=%p\n",
               (unsigned int)finalHash,
               (void*)render->shapeData,
               (void*)render->mediaTexels);
        return 0;
    }

    if (!DoomRPG_esp32MainMenuTouchActivate(doomRpg, finalHash)) {
        printf("[MAINOPAQUE] FAILED touch activation\n");
        return 0;
    }

    heapAfter = heap8Free();
    largestAfter = largest8Block();

    if (heapAfter != heapBefore || largestAfter != largestBefore) {
        printf("[MAINOPAQUE] FAILED heap changed heap8=%u->%u largest8=%u->%u\n",
               (unsigned int)heapBefore,
               (unsigned int)heapAfter,
               (unsigned int)largestBefore,
               (unsigned int)largestAfter);
        return 0;
    }

    printf("[MAINOPAQUE] modelFNV=%08x layoutFNV=%08x blackLogoFNV=%08x finalFNV=%08x composeMs=%u heap8=%u largest8=%u\n",
           (unsigned int)modelHash,
           (unsigned int)layoutHash,
           (unsigned int)stageHashes[0],
           (unsigned int)finalHash,
           (unsigned int)composeMs,
           (unsigned int)heapAfter,
           (unsigned int)largestAfter);
    printf("[MAINOPAQUE] HASH item0=%08x item1=%08x item2=%08x item3=%08x\n",
           (unsigned int)stageHashes[1],
           (unsigned int)stageHashes[2],
           (unsigned int)stageHashes[3],
           (unsigned int)stageHashes[4]);

    SDL_RenderPresent(NULL);

    if (finalFramebufferFNV != NULL) {
        *finalFramebufferFNV = finalHash;
    }

    printf("[MAINOPAQUE] READY opaque MENU_MAIN painted without BSP/wall/sprite replay\n");
    return 1;
}

/* Boot-time bridge: still require the fully validated native menu scene before
 * UI composition, then deliberately replace it with an opaque J2ME-style main
 * menu. The expensive 3D scene remains a bring-up regression proof, not a menu
 * navigation dependency.
 */
int __wrap_DoomRPG_probeNativeMainMenuOverlay(struct DoomRPG_s* doomRpgBase) {
    DoomRPG_t* doomRpg = (DoomRPG_t*)doomRpgBase;
    DoomCanvas_t* doomCanvas;
    MenuSystem_t* menuSystem;
    Render_t* render;
    uint32_t sceneHash;
    uint32_t finalHash = 0;
    uint32_t modelHash = 0;
    uint32_t layoutHash = 0;
    int i;

    printf("\n=== Doom RPG ESP32 MENU_MAIN opaque touch layout ===\n");

    if (!graphicsBoundaryIsSafe(doomRpg)) {
        printf("[MAINTOUCHLAYOUT] FAILED core/graphics boundary unavailable\n");
        return 0;
    }

    doomCanvas = doomRpg->doomCanvas;
    menuSystem = doomRpg->menuSystem;
    render = doomRpg->render;
    sceneHash = framebufferHash(render);

    printf("[MAINTOUCHLAYOUT] Begin sceneFNV=%08x expected=%08x priorFittedFNV=%08x faithfulOriginalFNV=%08x heap8=%u largest8=%u background=opaque-black\n",
           (unsigned int)sceneHash,
           (unsigned int)EXPECTED_NATIVE_SCENE_FNV,
           (unsigned int)PRIOR_FITTED_MENU_FNV,
           (unsigned int)FAITHFUL_ORIGINAL_MENU_FNV,
           (unsigned int)heap8Free(),
           (unsigned int)largest8Block());

    if (sceneHash != EXPECTED_NATIVE_SCENE_FNV) {
        printf("[MAINTOUCHLAYOUT] FAILED walls+sprites regression changed before UI\n");
        return 0;
    }

    menuSystem->menu = MENU_MAIN;
    Menu_initMenu(doomRpg->menu, MENU_MAIN);
    menuSystem->menu = MENU_MAIN;
    menuSystem->paintMenu = true;
    menuSystem->maxItems = doomCanvas->displayRect.h /
                           DOOMRPG_ESP32_MAIN_MENU_ITEM_LINE_HEIGHT;

    if (!validatePresentationContract(doomRpg, &modelHash, &layoutHash)) {
        printf("[MAINTOUCHLAYOUT] FAILED original MENU_MAIN model/layout contract\n");
        return 0;
    }

    printf("[MAINTOUCHLAYOUT] Model FNV=%08x items=%d selected=%d layoutFNV=%08x expectedLayout=%08x\n",
           (unsigned int)modelHash,
           menuSystem->numItems,
           menuSystem->selectedIndex,
           (unsigned int)layoutHash,
           (unsigned int)EXPECTED_LAYOUT_FNV);

    for (i = 0; i < DOOMRPG_ESP32_MAIN_MENU_ITEM_COUNT; ++i) {
        printf("[MAINTOUCHLAYOUT] ITEM index=%d y=%d selected=%s text=\"%s\"\n",
               i,
               doomCanvas->displayRect.y + DOOMRPG_ESP32_MAIN_MENU_ITEM_START_Y +
                   (i * DOOMRPG_ESP32_MAIN_MENU_ITEM_LINE_HEIGHT),
               i == menuSystem->selectedIndex ? "yes" : "no",
               menuSystem->items[i].textField);
    }

    if (!DoomRPG_esp32RepaintOpaqueMainMenu(doomRpg, &finalHash)) {
        printf("[MAINTOUCHLAYOUT] FAILED opaque main-menu paint\n");
        return 0;
    }

    printf("[MAINTOUCHLAYOUT] framebufferFNV=%08x sceneFNV=%08x changed=yes shapeData=%p mediaTexels=%p\n",
           (unsigned int)finalHash,
           (unsigned int)sceneHash,
           (void*)render->shapeData,
           (void*)render->mediaTexels);
    printf("[MAINTOUCHLAYOUT] READY native scene %08x validated, then hidden behind opaque MENU_MAIN\n",
           (unsigned int)sceneHash);
    printf("[MAINTOUCHLAYOUT] READY same bounded painter is reusable by Options Back\n");
    return 1;
}
