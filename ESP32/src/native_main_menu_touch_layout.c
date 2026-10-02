#include <SDL.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "DoomRPG.h"
#include "DoomCanvas.h"
#include "Menu.h"
#include "esp_native_menu_state.h"
#include "Render.h"

#include "esp_native_gameplay_hub_theme.h"
#include "native_main_menu_160x120_layout.h"
#include "native_main_menu_model.h"
#include "native_main_menu_touch.h"
#include "native_main_menu_present.h"
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
#define EXPECTED_MAIN_MENU_MODEL_FNV 0x292c7f95U
#define EXPECTED_FONT_WIDTH 144
#define EXPECTED_FONT_HEIGHT 72
#define MENU_GLYPH_ADVANCE 7

static const char* expectedMainItems[DOOMRPG_ESP32_MAIN_MENU_ITEM_COUNT] = {
    "Start Game",
    "Load Game ",
    "Options   ",
    "Help/About"
};

static const char* dashboardLabels[DOOMRPG_ESP32_MAIN_MENU_ITEM_COUNT] = {
    "START",
    "LOAD",
    "OPTIONS",
    "HELP"
};

static void setNativeMainItem(MenuItem_t* item, const char* text) {
    if (item == NULL) return;
    SDL_memset(item, 0, sizeof(*item));
    if (text != NULL) {
        SDL_snprintf(item->textField, sizeof(item->textField), "%s", text);
    }
    item->flags = 2;
    item->action = 0;
}

static int adaptMainMenuForEsp32(EspNativeMenuState_t* menuSystem) {
    if (menuSystem == NULL || menuSystem->menu != MENU_MAIN ||
        menuSystem->numItems != DOOMRPG_ESP32_MAIN_MENU_ITEM_COUNT) {
        return 0;
    }

    setNativeMainItem(&menuSystem->items[0], "Start Game");
    setNativeMainItem(&menuSystem->items[1], "Load Game ");
    setNativeMainItem(&menuSystem->items[2], "Options   ");
    setNativeMainItem(&menuSystem->items[3], "Help/About");
    return 1;
}

static uint32_t heap8Free(void) {
    return (uint32_t)heap_caps_get_free_size(MALLOC_CAP_8BIT);
}

static uint32_t largest8Block(void) {
    return (uint32_t)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
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
    if (text == NULL) return fnvMixU32(hash, 0xffffffffU);
    while (*text != '\0') {
        hash ^= (uint8_t)*text++;
        hash *= 16777619U;
    }
    hash ^= 0U;
    hash *= 16777619U;
    return hash;
}

static uint32_t menuModelHash(const EspNativeMenuState_t* menuSystem) {
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

static uint32_t dashboardLayoutHash(void) {
    uint32_t hash = 2166136261U;
    hash = fnvMixU32(hash, DOOMRPG_ESP32_MAIN_MENU_DASH_LOGO_WIDTH);
    hash = fnvMixU32(hash, DOOMRPG_ESP32_MAIN_MENU_DASH_LOGO_HEIGHT);
    hash = fnvMixU32(hash, DOOMRPG_ESP32_MAIN_MENU_DASH_LOGO_Y);
    hash = fnvMixU32(hash, DOOMRPG_ESP32_MAIN_MENU_CARD_COL0_LEFT);
    hash = fnvMixU32(hash, DOOMRPG_ESP32_MAIN_MENU_CARD_COL0_RIGHT);
    hash = fnvMixU32(hash, DOOMRPG_ESP32_MAIN_MENU_CARD_COL1_LEFT);
    hash = fnvMixU32(hash, DOOMRPG_ESP32_MAIN_MENU_CARD_COL1_RIGHT);
    hash = fnvMixU32(hash, DOOMRPG_ESP32_MAIN_MENU_CARD_ROW0_TOP);
    hash = fnvMixU32(hash, DOOMRPG_ESP32_MAIN_MENU_CARD_ROW0_BOTTOM);
    hash = fnvMixU32(hash, DOOMRPG_ESP32_MAIN_MENU_CARD_ROW1_TOP);
    hash = fnvMixU32(hash, DOOMRPG_ESP32_MAIN_MENU_CARD_ROW1_BOTTOM);
    return hash;
}

static int validateMainMenuModel(const EspNativeMenuState_t* menuSystem) {
    int i;
    if (menuSystem->menu != MENU_MAIN ||
        menuSystem->type != 4 ||
        menuSystem->numItems != DOOMRPG_ESP32_MAIN_MENU_ITEM_COUNT ||
        menuSystem->selectedIndex < 0 ||
        menuSystem->selectedIndex >= DOOMRPG_ESP32_MAIN_MENU_ITEM_COUNT ||
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
    DoomCanvas_t* canvas;
    EspNativeMenuState_t* menuSystem;
    uint32_t modelHash;
    uint32_t layoutHash;

    if (!DoomRPG_esp32MainMenuGraphicsBoundaryIsSafe(doomRpg)) return 0;
    canvas = doomRpg->doomCanvas;
    menuSystem = doomRpg->menuSystem;

    if (canvas->displayRect.w != DOOMRPG_LOGICAL_WIDTH ||
        canvas->displayRect.h != DOOMRPG_LOGICAL_HEIGHT ||
        canvas->largeStatus ||
        menuSystem->imgLogo.imgBitmap == NULL ||
        menuSystem->imgLogo.width != DOOMRPG_ESP32_MAIN_MENU_LOGO_SRC_WIDTH ||
        menuSystem->imgLogo.height != DOOMRPG_ESP32_MAIN_MENU_LOGO_SRC_HEIGHT ||
        canvas->imgFont.imgBitmap == NULL ||
        canvas->imgFont.width != EXPECTED_FONT_WIDTH ||
        canvas->imgFont.height != EXPECTED_FONT_HEIGHT) {
        return 0;
    }

    menuSystem->maxItems = canvas->displayRect.h /
                           DOOMRPG_ESP32_MAIN_MENU_ITEM_LINE_HEIGHT;

    if (!validateMainMenuModel(menuSystem)) return 0;

    modelHash = menuModelHash(menuSystem);
    layoutHash = dashboardLayoutHash();
    if (modelHash != EXPECTED_MAIN_MENU_MODEL_FNV ||
        DOOMRPG_ESP32_MAIN_MENU_DASH_LOGO_Y +
                DOOMRPG_ESP32_MAIN_MENU_DASH_LOGO_HEIGHT >
            DOOMRPG_ESP32_MAIN_MENU_DASH_RAIL_TOP ||
        DOOMRPG_ESP32_MAIN_MENU_CARD_COL0_LEFT < 0 ||
        DOOMRPG_ESP32_MAIN_MENU_CARD_COL1_RIGHT >= DOOMRPG_LOGICAL_WIDTH ||
        DOOMRPG_ESP32_MAIN_MENU_CARD_ROW0_TOP <=
            DOOMRPG_ESP32_MAIN_MENU_DASH_RAIL_BOTTOM ||
        DOOMRPG_ESP32_MAIN_MENU_CARD_ROW1_BOTTOM >= DOOMRPG_LOGICAL_HEIGHT) {
        return 0;
    }

    if (modelHashOut != NULL) *modelHashOut = modelHash;
    if (layoutHashOut != NULL) *layoutHashOut = layoutHash;
    return 1;
}

static void dashboardCardRect(int item,
                              int* left,
                              int* top,
                              int* right,
                              int* bottom) {
    const int col = item & 1;
    const int row = item >> 1;
    *left = col ? DOOMRPG_ESP32_MAIN_MENU_CARD_COL1_LEFT
                : DOOMRPG_ESP32_MAIN_MENU_CARD_COL0_LEFT;
    *right = col ? DOOMRPG_ESP32_MAIN_MENU_CARD_COL1_RIGHT
                 : DOOMRPG_ESP32_MAIN_MENU_CARD_COL0_RIGHT;
    *top = row ? DOOMRPG_ESP32_MAIN_MENU_CARD_ROW1_TOP
               : DOOMRPG_ESP32_MAIN_MENU_CARD_ROW0_TOP;
    *bottom = row ? DOOMRPG_ESP32_MAIN_MENU_CARD_ROW1_BOTTOM
                  : DOOMRPG_ESP32_MAIN_MENU_CARD_ROW0_BOTTOM;
}

static void putPixel(Render_t* render, int x, int y, uint16_t color) {
    uint16_t* framebuffer;
    int stride;
    if (render == NULL || render->framebuffer == NULL ||
        x < 0 || x >= DOOMRPG_LOGICAL_WIDTH ||
        y < 0 || y >= DOOMRPG_LOGICAL_HEIGHT ||
        render->pitch < DOOMRPG_LOGICAL_WIDTH * (int)sizeof(uint16_t)) {
        return;
    }
    framebuffer = (uint16_t*)render->framebuffer;
    stride = render->pitch / (int)sizeof(uint16_t);
    framebuffer[y * stride + x] = color;
}

static void fillRect565(Render_t* render,
                        int left,
                        int top,
                        int right,
                        int bottom,
                        uint16_t color) {
    int x;
    int y;
    for (y = top; y <= bottom; ++y) {
        for (x = left; x <= right; ++x) putPixel(render, x, y, color);
    }
}

static void drawRect565(Render_t* render,
                        int left,
                        int top,
                        int right,
                        int bottom,
                        uint16_t color) {
    int x;
    int y;
    for (x = left; x <= right; ++x) {
        putPixel(render, x, top, color);
        putPixel(render, x, bottom, color);
    }
    for (y = top + 1; y < bottom; ++y) {
        putPixel(render, left, y, color);
        putPixel(render, right, y, color);
    }
}

static void chamferCard(Render_t* render,
                        int left,
                        int top,
                        int right,
                        int bottom) {
    putPixel(render, left, top, ESP_HUB_COLOR_BLACK);
    putPixel(render, right, top, ESP_HUB_COLOR_BLACK);
    putPixel(render, left, bottom, ESP_HUB_COLOR_BLACK);
    putPixel(render, right, bottom, ESP_HUB_COLOR_BLACK);
}

static void drawDashboardCard(DoomRPG_t* doomRpg,
                              int item,
                              const char* label,
                              int selected,
                              int armed,
                              int enabled) {
    Render_t* render = doomRpg->render;
    int left;
    int top;
    int right;
    int bottom;
    int textWidth;
    int textX;
    int textY;
    uint16_t panel = selected ? ESP_HUB_COLOR_PANEL_ALT : ESP_HUB_COLOR_PANEL;
    uint16_t border = selected ? ESP_HUB_COLOR_AMBER : ESP_HUB_COLOR_STEEL;
    uint16_t rail = selected ? ESP_HUB_COLOR_AMBER : ESP_HUB_COLOR_STEEL_DARK;

    dashboardCardRect(item, &left, &top, &right, &bottom);

    if (!enabled) {
        panel = ESP_HUB_COLOR_BG;
        border = ESP_HUB_COLOR_STEEL_DARK;
        rail = ESP_HUB_COLOR_STEEL_DARK;
    }
    else if (armed && selected) {
        panel = ESP_HUB_COLOR_STEEL_DARK;
        border = ESP_HUB_COLOR_IVORY;
    }

    fillRect565(render, left, top, right, bottom, panel);
    drawRect565(render, left, top, right, bottom, border);
    if (selected) {
        drawRect565(render, left + 2, top + 2, right - 2, bottom - 2,
                    armed ? ESP_HUB_COLOR_AMBER : ESP_HUB_COLOR_AMBER_DIM);
    }

    fillRect565(render, left + 2, top + 4, left + (selected ? 4 : 3),
                bottom - 4, rail);

    /* Small Doom-tech corner teeth keep the cards from reading as phone UI. */
    putPixel(render, right - 3, top + 2, border);
    putPixel(render, right - 2, top + 2, border);
    putPixel(render, right - 2, top + 3, border);
    putPixel(render, right - 3, bottom - 2, border);
    putPixel(render, right - 2, bottom - 2, border);
    putPixel(render, right - 2, bottom - 3, border);
    chamferCard(render, left, top, right, bottom);

    textWidth = (int)strlen(label) * MENU_GLYPH_ADVANCE;
    textX = ((left + right + 1) >> 1) - (textWidth >> 1);
    textY = top + (((bottom - top + 1) - DOOMRPG_ESP32_MAIN_MENU_FONT_HEIGHT) >> 1);

    DoomRPG_setFontColor(doomRpg,
                         !enabled ? 0xff6f7881U
                                  : (selected
                                         ? (armed ? 0xffffffffU : 0xffffa000U)
                                         : 0xffffffffU));
    /* Legacy drawFont takes char* but only reads the glyph string. Keep these
     * labels in flash/rodata instead of spending writable RAM on four copies. */
    DoomCanvas_drawFont(doomRpg->doomCanvas,
                        (char*)label,
                        textX,
                        textY,
                        0,
                        0,
                        -1,
                        false);
}

int DoomRPG_esp32PaintMenuDashboardCards(
    struct DoomRPG_s* doomRpgBase,
    const char* const labels[4],
    int selectedIndex,
    int armed,
    uint8_t enabledMask,
    uint32_t* framebufferFNV) {
    DoomRPG_t* doomRpg = (DoomRPG_t*)doomRpgBase;
    Render_t* render;
    int item;
    uint32_t hash;

    if (!DoomRPG_esp32MainMenuGraphicsBoundaryIsSafe(doomRpg) || labels == NULL ||
        selectedIndex < 0 ||
        selectedIndex >= DOOMRPG_ESP32_MAIN_MENU_ITEM_COUNT) {
        return 0;
    }

    for (item = 0; item < DOOMRPG_ESP32_MAIN_MENU_ITEM_COUNT; ++item) {
        if (labels[item] == NULL) return 0;
    }

    render = doomRpg->render;

    /* Keep the four touch cards visually independent on the true black menu
     * background. The previous full-width ESP_HUB_COLOR_BG slab made the lower
     * half read as one oversized grey panel and visually competed with the
     * Doom RPG title. Repaint the bounded dashboard band black on every
     * selection change so no previous focus pixels survive between cards.
     */
    fillRect565(render,
                0,
                DOOMRPG_ESP32_MAIN_MENU_DASH_RAIL_TOP,
                DOOMRPG_LOGICAL_WIDTH - 1,
                DOOMRPG_LOGICAL_HEIGHT - 1,
                ESP_HUB_COLOR_BLACK);

    for (item = 0; item < DOOMRPG_ESP32_MAIN_MENU_ITEM_COUNT; ++item) {
        drawDashboardCard(doomRpg,
                          item,
                          labels[item],
                          item == selectedIndex &&
                              (enabledMask & (uint8_t)(1U << item)) != 0U,
                          armed && item == selectedIndex,
                          (enabledMask & (uint8_t)(1U << item)) != 0U);
    }

    DoomRPG_setFontColor(doomRpg, 0xffffffffU);

    hash = DoomRPG_esp32MainMenuFramebufferHash(render);
    if (hash == 0U) return 0;
    if (framebufferFNV != NULL) *framebufferFNV = hash;
    return 1;
}

int DoomRPG_esp32MainMenuPaintDashboardSelection(
    struct DoomRPG_s* doomRpgBase,
    int selectedIndex,
    int armed,
    uint32_t* framebufferFNV) {
    return DoomRPG_esp32PaintMenuDashboardCards(
        doomRpgBase,
        dashboardLabels,
        selectedIndex,
        armed,
        (uint8_t)((1U << DOOMRPG_ESP32_MAIN_MENU_ITEM_COUNT) - 1U),
        framebufferFNV);
}

static int drawTouchReadyMainMenuOpaque(DoomRPG_t* doomRpg,
                                         uint32_t* logoHashOut,
                                         uint32_t* finalHashOut) {
    DoomCanvas_t* canvas = doomRpg->doomCanvas;
    EspNativeMenuState_t* menuSystem = doomRpg->menuSystem;
    SDL_Rect logoDst;
    uint32_t logoHash;
    uint32_t finalHash;

    DoomRPG_setColor(doomRpg, 0x000000);
    DoomRPG_fillRect(doomRpg,
                     canvas->displayRect.x,
                     canvas->displayRect.y,
                     canvas->displayRect.w,
                     canvas->displayRect.h);

    logoDst.x = canvas->displayRect.x +
                ((canvas->displayRect.w -
                  DOOMRPG_ESP32_MAIN_MENU_DASH_LOGO_WIDTH) >> 1);
    logoDst.y = canvas->displayRect.y +
                DOOMRPG_ESP32_MAIN_MENU_DASH_LOGO_Y;
    logoDst.w = DOOMRPG_ESP32_MAIN_MENU_DASH_LOGO_WIDTH;
    logoDst.h = DOOMRPG_ESP32_MAIN_MENU_DASH_LOGO_HEIGHT;

    if (SDL_RenderCopy(NULL, menuSystem->imgLogo.imgBitmap, NULL, &logoDst) != 0) {
        return 0;
    }

    logoHash = DoomRPG_esp32MainMenuFramebufferHash(doomRpg->render);
    if (logoHash == 0U) return 0;

    if (!DoomRPG_esp32MainMenuTouchPrepare(doomRpg)) return 0;

    if (!DoomRPG_esp32MainMenuPaintDashboardSelection(
            doomRpg,
            menuSystem->selectedIndex,
            0,
            &finalHash)) {
        return 0;
    }

    if (finalHash == logoHash) return 0;
    if (logoHashOut != NULL) *logoHashOut = logoHash;
    if (finalHashOut != NULL) *finalHashOut = finalHash;
    return 1;
}

int DoomRPG_esp32RepaintOpaqueMainMenu(struct DoomRPG_s* doomRpgBase,
                                       uint32_t* finalFramebufferFNV) {
    DoomRPG_t* doomRpg = (DoomRPG_t*)doomRpgBase;
    Render_t* render;
    uint32_t modelHash = 0U;
    uint32_t layoutHash = 0U;
    uint32_t logoHash = 0U;
    uint32_t finalHash = 0U;
    uint32_t heapBefore;
    uint32_t heapAfter;
    uint32_t largestBefore;
    uint32_t largestAfter;
    uint32_t composeStart;
    uint32_t composeMs;

    if (doomRpg == NULL ||
        !adaptMainMenuForEsp32(doomRpg->menuSystem) ||
        !validatePresentationContract(doomRpg, &modelHash, &layoutHash)) {
        printf("[MAINOPAQUE] FAILED dashboard presentation contract menu=%d selected=%d\n",
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

    if (!drawTouchReadyMainMenuOpaque(doomRpg, &logoHash, &finalHash)) {
        printf("[MAINOPAQUE] FAILED finger-first dashboard composition\n");
        return 0;
    }

    composeMs = (uint32_t)DoomRPG_GetTimeMS() - composeStart;

    if (render->shapeData != NULL || render->mediaTexels != NULL ||
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

    printf("[MAINOPAQUE] DASHBOARD modelFNV=%08x layoutFNV=%08x logoFNV=%08x finalFNV=%08x composeMs=%u heap8=%u largest8=%u\n",
           (unsigned int)modelHash,
           (unsigned int)layoutHash,
           (unsigned int)logoHash,
           (unsigned int)finalHash,
           (unsigned int)composeMs,
           (unsigned int)heapAfter,
           (unsigned int)largestAfter);
    printf("[MAINOPAQUE] TARGET cards=68x23 logical=136x46 physical labels=START|LOAD|OPTIONS|HELP style=doom-tech-floating-on-black cursorPatchBytes=0\n");

    SDL_RenderPresent(NULL);

    if (finalFramebufferFNV != NULL) *finalFramebufferFNV = finalHash;

    printf("[MAINOPAQUE] READY finger-first MENU_MAIN painted without BSP/wall/sprite replay\n");
    return 1;
}

/* Boot-time bridge: still require the fully validated native menu scene before
 * UI composition, then deliberately replace it with the bounded opaque menu.
 * The expensive 3D scene remains a bring-up regression proof, not navigation.
 */
int __wrap_DoomRPG_probeNativeMainMenuOverlay(struct DoomRPG_s* doomRpgBase) {
    DoomRPG_t* doomRpg = (DoomRPG_t*)doomRpgBase;
    DoomCanvas_t* canvas;
    EspNativeMenuState_t* menuSystem;
    Render_t* render;
    uint32_t sceneHash;
    uint32_t finalHash = 0U;
    uint32_t modelHash = 0U;
    uint32_t layoutHash = 0U;
    int i;

    printf("\n=== Doom RPG ESP32 MENU_MAIN finger-first dashboard ===\n");

    if (!DoomRPG_esp32MainMenuGraphicsBoundaryIsSafe(doomRpg)) {
        printf("[MAINTOUCHLAYOUT] FAILED core/graphics boundary unavailable\n");
        return 0;
    }

    canvas = doomRpg->doomCanvas;
    menuSystem = doomRpg->menuSystem;
    render = doomRpg->render;
    sceneHash = DoomRPG_esp32MainMenuFramebufferHash(render);

    printf("[MAINTOUCHLAYOUT] Begin sceneFNV=%08x expected=%08x heap8=%u largest8=%u background=black-with-floating-cards\n",
           (unsigned int)sceneHash,
           (unsigned int)EXPECTED_NATIVE_SCENE_FNV,
           (unsigned int)heap8Free(),
           (unsigned int)largest8Block());

    if (sceneHash != EXPECTED_NATIVE_SCENE_FNV) {
        printf("[MAINTOUCHLAYOUT] FAILED walls+sprites regression changed before UI\n");
        return 0;
    }

    if (!DoomRPG_esp32MainMenuModelBuildMain(doomRpg)) {
        printf("[MAINTOUCHLAYOUT] FAILED native MENU_MAIN model build\n");
        return 0;
    }
    menuSystem->maxItems = canvas->displayRect.h /
                           DOOMRPG_ESP32_MAIN_MENU_ITEM_LINE_HEIGHT;

    if (!adaptMainMenuForEsp32(menuSystem) ||
        !validatePresentationContract(doomRpg, &modelHash, &layoutHash)) {
        printf("[MAINTOUCHLAYOUT] FAILED MENU_MAIN model/dashboard contract\n");
        return 0;
    }

    printf("[MAINTOUCHLAYOUT] Model FNV=%08x items=%d selected=%d dashboardFNV=%08x\n",
           (unsigned int)modelHash,
           menuSystem->numItems,
           menuSystem->selectedIndex,
           (unsigned int)layoutHash);

    for (i = 0; i < DOOMRPG_ESP32_MAIN_MENU_ITEM_COUNT; ++i) {
        int left;
        int top;
        int right;
        int bottom;
        dashboardCardRect(i, &left, &top, &right, &bottom);
        printf("[MAINTOUCHLAYOUT] CARD index=%d logical=x%d..%d y%d..%d physical=%dx%d label=%s model=\"%s\"\n",
               i,
               left,
               right,
               top,
               bottom,
               (right - left + 1) * DOOMRPG_INTEGER_SCALE,
               (bottom - top + 1) * DOOMRPG_INTEGER_SCALE,
               dashboardLabels[i],
               menuSystem->items[i].textField);
    }

    if (!DoomRPG_esp32RepaintOpaqueMainMenu(doomRpg, &finalHash)) {
        printf("[MAINTOUCHLAYOUT] FAILED finger-first main-menu paint\n");
        return 0;
    }

    printf("[MAINTOUCHLAYOUT] framebufferFNV=%08x sceneFNV=%08x changed=yes shapeData=%p mediaTexels=%p\n",
           (unsigned int)finalHash,
           (unsigned int)sceneHash,
           (void*)render->shapeData,
           (void*)render->mediaTexels);
    printf("[MAINTOUCHLAYOUT] READY 2x2 dashboard; existing MENU_MAIN model/actions preserved\n");
    printf("[MAINTOUCHLAYOUT] READY same bounded painter reusable by Options Back and failed-load recovery\n");
    return 1;
}


/* Consolidated main-menu presentation invariants/recovery. */
#include <SDL.h>
#include <stdint.h>
#include <stdio.h>

#include "DoomRPG.h"
#include "DoomCanvas.h"
#include "Menu.h"
#include "MenuSystem.h"
#include "Render.h"

#include "native_main_menu_model.h"
#include "native_main_menu_present.h"
#include "native_main_menu_touch_layout.h"
#include "native_sprite_lru_cache.h"
#include "native_wall_lru_cache.h"
#include "platform_touch_events.h"
#include "platform_video_config.h"

static uint32_t fnv1a32(const uint8_t* data, uint32_t length) {
    uint32_t hash = 2166136261U;
    uint32_t i;

    for (i = 0; i < length; ++i) {
        hash ^= data[i];
        hash *= 16777619U;
    }
    return hash;
}

uint32_t DoomRPG_esp32MainMenuFramebufferHash(const struct Render_s* renderBase) {
    const Render_t* render = (const Render_t*)renderBase;

    if (render == NULL || render->framebuffer == NULL || render->pitch <= 0) {
        return 0U;
    }
    return fnv1a32((const uint8_t*)render->framebuffer,
                   (uint32_t)render->pitch * DOOMRPG_LOGICAL_HEIGHT);
}

int DoomRPG_esp32MainMenuGraphicsBoundaryIsSafe(
    const struct DoomRPG_s* doomRpgBase) {
    const DoomRPG_t* doomRpg = (const DoomRPG_t*)doomRpgBase;
    const Render_t* render;

    if (doomRpg == NULL || doomRpg->render == NULL ||
        doomRpg->doomCanvas == NULL || doomRpg->menuSystem == NULL) {
        return 0;
    }

    render = doomRpg->render;
    return render->framebuffer != NULL &&
           render->shapeData == NULL &&
           render->mediaTexels == NULL &&
           !EspNativeWallCache_isActive() &&
           !EspNativeSpriteCache_isActive();
}

int DoomRPG_esp32MainMenuRecover(struct DoomRPG_s* doomRpgBase,
                                 const char* reason) {
    DoomRPG_t* doomRpg = (DoomRPG_t*)doomRpgBase;
    uint32_t frameFNV = 0U;

    if (doomRpg == NULL || doomRpg->doomCanvas == NULL ||
        doomRpg->menuSystem == NULL) {
        printf("[MAINRECOVER] FAILED reason=%s objectGraph\n",
               reason != NULL ? reason : "unknown");
        return 0;
    }

    if (doomRpg->doomCanvas->state != ST_MENU) {
        printf("[MAINRECOVER] REFUSE reason=%s state=%d menu=%d transitionAlreadyLeftMenu=yes\n",
               reason != NULL ? reason : "unknown",
               doomRpg->doomCanvas->state,
               doomRpg->menuSystem->menu);
        return 0;
    }

    if (!DoomRPG_esp32MainMenuGraphicsBoundaryIsSafe(doomRpg)) {
        printf("[MAINRECOVER] FAILED reason=%s graphicsBoundary\n",
               reason != NULL ? reason : "unknown");
        return 0;
    }

    PlatformInput_setTapCallback(NULL);

    if (!DoomRPG_esp32MainMenuModelEnter(doomRpg, MENU_MAIN) ||
        !DoomRPG_esp32RepaintOpaqueMainMenu(doomRpg, &frameFNV)) {
        printf("[MAINRECOVER] FAILED reason=%s rebuild/repaint menu=%d state=%d\n",
               reason != NULL ? reason : "unknown",
               doomRpg->menuSystem->menu,
               doomRpg->doomCanvas->state);
        return 0;
    }

    printf("[MAINRECOVER] READY reason=%s menu=%d selected=%d frame=%08x touch=rearmed\n",
           reason != NULL ? reason : "unknown",
           doomRpg->menuSystem->menu,
           doomRpg->menuSystem->selectedIndex,
           (unsigned int)frameFNV);
    return 1;
}


/* Consolidated native scene -> opaque main-menu bridge. */
#include <SDL.h>
#include <stdio.h>

#include "DoomRPG.h"
#include "Render.h"

#include "native_main_menu_overlay_probe.h"
#include "native_menu_sprite_frame_probe.h"

extern DoomRPG_t* doomRpg;

/*
 * Keep the hardware-validated native scene probe untouched. The ESP32 linker
 * chains the real MENU_MAIN composition only after that probe has completely
 * validated ffe0995e and torn down both wall/sprite caches.
 */
int __real_DoomRPG_probeNativeMenuSpriteFrame(struct Render_s* render);

int __wrap_DoomRPG_probeNativeMenuSpriteFrame(struct Render_s* render) {
    if (!__real_DoomRPG_probeNativeMenuSpriteFrame(render)) {
        return 0;
    }

    if (doomRpg == NULL) {
        printf("[MAINMENU] FAILED global DoomRPG unavailable after native scene\n");
        return 0;
    }

    return DoomRPG_probeNativeMainMenuOverlay(doomRpg);
}
