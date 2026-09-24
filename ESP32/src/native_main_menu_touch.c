#include <SDL.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "DoomRPG.h"
#include "DoomCanvas.h"
#include "Menu.h"
#include "MenuSystem.h"
#include "Render.h"

#include "native_main_menu_160x120_layout.h"
#include "native_main_menu_touch.h"
#include "native_main_menu_touch_layout.h"
#include "native_sprite_lru_cache.h"
#include "native_wall_lru_cache.h"
#include "platform_touch_events.h"
#include "platform_video_config.h"

/* Keep ESP-IDF's stdbool macros after DoomRPG's legacy boolean enum. */
#include <esp_heap_caps.h>

static DoomRPG_t* touchDoomRpg = NULL;
static uint32_t selectionHashes[DOOMRPG_ESP32_MAIN_MENU_ITEM_COUNT];
static uint32_t tapCount = 0U;
static uint32_t selectionCount = 0U;
static uint32_t confirmCount = 0U;
static uint32_t missCount = 0U;
static int touchPrepared = 0;
static int touchActive = 0;

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

static void cardRect(int item,
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

static int findHitItem(int logicalX, int logicalY) {
    int i;
    for (i = 0; i < DOOMRPG_ESP32_MAIN_MENU_ITEM_COUNT; ++i) {
        int left;
        int top;
        int right;
        int bottom;
        cardRect(i, &left, &top, &right, &bottom);
        if (logicalX >= left && logicalX <= right &&
            logicalY >= top && logicalY <= bottom) {
            return i;
        }
    }
    return -1;
}

static int graphicsBoundaryIsSafe(const DoomRPG_t* doomRpg) {
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

int DoomRPG_esp32MainMenuTouchPrepare(struct DoomRPG_s* doomRpgBase) {
    DoomRPG_t* doomRpg = (DoomRPG_t*)doomRpgBase;
    int i;

    touchActive = 0;
    touchPrepared = 0;
    touchDoomRpg = NULL;
    PlatformInput_setTapCallback(NULL);
    memset(selectionHashes, 0, sizeof(selectionHashes));

    if (!graphicsBoundaryIsSafe(doomRpg) ||
        doomRpg->menuSystem->menu != MENU_MAIN ||
        doomRpg->menuSystem->numItems != DOOMRPG_ESP32_MAIN_MENU_ITEM_COUNT ||
        doomRpg->doomCanvas->displayRect.w != DOOMRPG_LOGICAL_WIDTH ||
        doomRpg->doomCanvas->displayRect.h != DOOMRPG_LOGICAL_HEIGHT) {
        printf("[MENUTOUCH] FAILED prepare dashboard/model boundary\n");
        return 0;
    }

    for (i = 0; i < DOOMRPG_ESP32_MAIN_MENU_ITEM_COUNT; ++i) {
        int left;
        int top;
        int right;
        int bottom;
        cardRect(i, &left, &top, &right, &bottom);
        printf("[MENUTOUCH] ZONE item=%d logical=x%d..%d y%d..%d physical=x%d..%d y%d..%d text=\"%s\"\n",
               i,
               left,
               right,
               top,
               bottom,
               left * DOOMRPG_INTEGER_SCALE,
               ((right + 1) * DOOMRPG_INTEGER_SCALE) - 1,
               top * DOOMRPG_INTEGER_SCALE,
               ((bottom + 1) * DOOMRPG_INTEGER_SCALE) - 1,
               doomRpg->menuSystem->items[i].textField);
    }

    touchDoomRpg = doomRpg;
    touchPrepared = 1;
    tapCount = 0U;
    selectionCount = 0U;
    confirmCount = 0U;
    missCount = 0U;

    printf("[MENUTOUCH] PREPARED dashboard=2x2 cursorPatchBytes=0 targetLogical=74x28 targetPhysical=148x56\n");
    return 1;
}

int DoomRPG_esp32MainMenuTouchActivate(struct DoomRPG_s* doomRpgBase,
                                       uint32_t initialFramebufferFNV) {
    DoomRPG_t* doomRpg = (DoomRPG_t*)doomRpgBase;
    uint32_t currentHash;

    if (!touchPrepared || doomRpg == NULL || doomRpg != touchDoomRpg ||
        !graphicsBoundaryIsSafe(doomRpg)) {
        printf("[MENUTOUCH] FAILED activate prepared=%d sameDoom=%d safe=%d\n",
               touchPrepared,
               doomRpg == touchDoomRpg,
               graphicsBoundaryIsSafe(doomRpg));
        return 0;
    }

    if (doomRpg->menuSystem->menu != MENU_MAIN ||
        doomRpg->menuSystem->selectedIndex != 0) {
        printf("[MENUTOUCH] FAILED activate menu=%d selected=%d\n",
               doomRpg->menuSystem->menu,
               doomRpg->menuSystem->selectedIndex);
        return 0;
    }

    currentHash = framebufferHash(doomRpg->render);
    if (currentHash == 0U || currentHash != initialFramebufferFNV) {
        printf("[MENUTOUCH] FAILED activate framebuffer=%08x supplied=%08x\n",
               (unsigned int)currentHash,
               (unsigned int)initialFramebufferFNV);
        return 0;
    }

    memset(selectionHashes, 0, sizeof(selectionHashes));
    selectionHashes[0] = currentHash;
    touchActive = 1;
    PlatformInput_setTapCallback(DoomRPG_esp32MainMenuTouchOnTap);

    printf("[MENUTOUCH] READY physical=%dx%d logical=%dx%d scale=%d selected=0 initialFNV=%08x cursorPatchBytes=0 releaseDebounce=50ms\n",
           DOOMRPG_PHYSICAL_WIDTH,
           DOOMRPG_PHYSICAL_HEIGHT,
           DOOMRPG_LOGICAL_WIDTH,
           DOOMRPG_LOGICAL_HEIGHT,
           DOOMRPG_INTEGER_SCALE,
           (unsigned int)currentHash);
    printf("[MENUTOUCH] READY first tap arms bright card; second released tap on same card confirms\n");
    return 1;
}

int DoomRPG_esp32MainMenuTouchIsActive(void) {
    return touchActive;
}

int DoomRPG_esp32MainMenuTouchArmSelected(int itemIndex) {
    uint32_t heapBefore;
    uint32_t heapAfter;
    uint32_t largestBefore;
    uint32_t largestAfter;
    uint32_t frameHash = 0U;

    if (!touchActive || touchDoomRpg == NULL ||
        !graphicsBoundaryIsSafe(touchDoomRpg) ||
        touchDoomRpg->menuSystem->menu != MENU_MAIN ||
        touchDoomRpg->menuSystem->selectedIndex != itemIndex ||
        itemIndex < 0 ||
        itemIndex >= DOOMRPG_ESP32_MAIN_MENU_ITEM_COUNT) {
        printf("[MENUTOUCH] FAILED arm item=%d active=%d selected=%d\n",
               itemIndex,
               touchActive,
               touchDoomRpg != NULL && touchDoomRpg->menuSystem != NULL
                   ? touchDoomRpg->menuSystem->selectedIndex : -999);
        return 0;
    }

    heapBefore = heap8Free();
    largestBefore = largest8Block();

    if (!DoomRPG_esp32MainMenuPaintDashboardSelection(
            touchDoomRpg, itemIndex, 1, &frameHash)) {
        printf("[MENUTOUCH] FAILED arm repaint item=%d\n", itemIndex);
        return 0;
    }

    heapAfter = heap8Free();
    largestAfter = largest8Block();
    if (heapAfter != heapBefore || largestAfter != largestBefore ||
        !graphicsBoundaryIsSafe(touchDoomRpg)) {
        printf("[MENUTOUCH] FAILED arm invariant item=%d heap8=%u->%u largest8=%u->%u\n",
               itemIndex,
               (unsigned int)heapBefore,
               (unsigned int)heapAfter,
               (unsigned int)largestBefore,
               (unsigned int)largestAfter);
        return 0;
    }

    memset(selectionHashes, 0, sizeof(selectionHashes));
    selectionHashes[itemIndex] = frameHash;
    SDL_RenderPresent(NULL);
    printf("[MENUTOUCH] ARM-VISUAL item=%d framebufferFNV=%08x style=ivory+amber-double-border allocation=no\n",
           itemIndex,
           (unsigned int)frameHash);
    return 1;
}

uint32_t DoomRPG_esp32MainMenuSelectionFramebufferFNV(int itemIndex) {
    if (itemIndex < 0 ||
        itemIndex >= DOOMRPG_ESP32_MAIN_MENU_ITEM_COUNT) {
        return 0U;
    }
    return selectionHashes[itemIndex];
}

void DoomRPG_esp32MainMenuTouchRebaseFrame(int selectedIndex,
                                           uint32_t framebufferFNV) {
    memset(selectionHashes, 0, sizeof(selectionHashes));
    if (selectedIndex >= 0 &&
        selectedIndex < DOOMRPG_ESP32_MAIN_MENU_ITEM_COUNT) {
        selectionHashes[selectedIndex] = framebufferFNV;
    }
}

void DoomRPG_esp32MainMenuTouchOnTap(int16_t screenX,
                                     int16_t screenY,
                                     uint16_t pressure,
                                     uint16_t rawX,
                                     uint16_t rawY) {
    MenuSystem_t* menuSystem;
    Render_t* render;
    int logicalX;
    int logicalY;
    int hit;
    int selectedBefore;
    uint32_t hashBefore;
    uint32_t hashAfter = 0U;
    uint32_t heapBefore;
    uint32_t heapAfter;
    uint32_t largestBefore;
    uint32_t largestAfter;

    if (!touchActive || touchDoomRpg == NULL) return;

    menuSystem = touchDoomRpg->menuSystem;
    render = touchDoomRpg->render;

    if (!graphicsBoundaryIsSafe(touchDoomRpg) ||
        menuSystem->menu != MENU_MAIN ||
        menuSystem->selectedIndex < 0 ||
        menuSystem->selectedIndex >= DOOMRPG_ESP32_MAIN_MENU_ITEM_COUNT) {
        printf("[MENUTOUCH] FAILED runtime boundary menu=%d selected=%d shapeData=%p mediaTexels=%p\n",
               menuSystem != NULL ? menuSystem->menu : -999,
               menuSystem != NULL ? menuSystem->selectedIndex : -999,
               render != NULL ? (void*)render->shapeData : NULL,
               render != NULL ? (void*)render->mediaTexels : NULL);
        touchActive = 0;
        PlatformInput_setTapCallback(NULL);
        return;
    }

    logicalX = screenX / DOOMRPG_INTEGER_SCALE;
    logicalY = screenY / DOOMRPG_INTEGER_SCALE;
    hit = findHitItem(logicalX, logicalY);
    selectedBefore = menuSystem->selectedIndex;
    hashBefore = framebufferHash(render);
    tapCount++;

    printf("[MENUTOUCH] TAP n=%u raw=%u,%u pressure=%u physical=%d,%d logical=%d,%d hit=%d selectedBefore=%d\n",
           (unsigned int)tapCount,
           rawX,
           rawY,
           pressure,
           screenX,
           screenY,
           logicalX,
           logicalY,
           hit,
           selectedBefore);

    if (hit < 0) {
        missCount++;
        printf("[MENUTOUCH] MISS taps=%u misses=%u framebufferFNV=%08x\n",
               (unsigned int)tapCount,
               (unsigned int)missCount,
               (unsigned int)hashBefore);
        return;
    }

    if (hit == selectedBefore) {
        confirmCount++;
        printf("[MENUTOUCH] CONFIRM item=%d text=\"%s\" count=%u framebufferFNV=%08x action=deferred\n",
               hit,
               menuSystem->items[hit].textField,
               (unsigned int)confirmCount,
               (unsigned int)hashBefore);
        printf("[MENUTOUCH] CONFIRM routed to final menu-action gate\n");
        return;
    }

    heapBefore = heap8Free();
    largestBefore = largest8Block();
    menuSystem->selectedIndex = hit;

    if (!DoomRPG_esp32MainMenuPaintDashboardSelection(
            touchDoomRpg, hit, 1, &hashAfter)) {
        menuSystem->selectedIndex = selectedBefore;
        printf("[MENUTOUCH] FAILED selection repaint %d->%d\n",
               selectedBefore,
               hit);
        return;
    }

    heapAfter = heap8Free();
    largestAfter = largest8Block();

    memset(selectionHashes, 0, sizeof(selectionHashes));
    selectionHashes[hit] = hashAfter;

    printf("[MENUTOUCH] SELECT %d->%d text=\"%s\" framebufferFNV=%08x heap8=%u->%u largest8=%u->%u style=armed-card\n",
           selectedBefore,
           hit,
           menuSystem->items[hit].textField,
           (unsigned int)hashAfter,
           (unsigned int)heapBefore,
           (unsigned int)heapAfter,
           (unsigned int)largestBefore,
           (unsigned int)largestAfter);

    if (hashAfter == 0U || hashAfter == hashBefore ||
        heapAfter != heapBefore ||
        largestAfter != largestBefore ||
        !graphicsBoundaryIsSafe(touchDoomRpg)) {
        printf("[MENUTOUCH] FAILED selection invariant before=%08x after=%08x heapDelta=%d largestDelta=%d\n",
               (unsigned int)hashBefore,
               (unsigned int)hashAfter,
               (int)heapBefore - (int)heapAfter,
               (int)largestBefore - (int)largestAfter);
        return;
    }

    selectionCount++;
    SDL_RenderPresent(NULL);
    printf("[MENUTOUCH] READY selection=%d selections=%u confirms=%u misses=%u noSceneRerender=yes noSDRead=yes\n",
           hit,
           (unsigned int)selectionCount,
           (unsigned int)confirmCount,
           (unsigned int)missCount);
}
