#include <SDL.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "DoomRPG.h"
#include "DoomCanvas.h"
#include "Menu.h"
#include "esp_native_menu_state.h"
#include "Render.h"

#include "native_main_menu_160x120_layout.h"
#include "native_main_menu_touch.h"
#include "native_main_menu_present.h"
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

int DoomRPG_esp32MainMenuTouchPrepare(struct DoomRPG_s* doomRpgBase) {
    DoomRPG_t* doomRpg = (DoomRPG_t*)doomRpgBase;
    int i;

    touchActive = 0;
    touchPrepared = 0;
    touchDoomRpg = NULL;
    PlatformInput_setTapCallback(NULL);
    memset(selectionHashes, 0, sizeof(selectionHashes));

    if (!DoomRPG_esp32MainMenuGraphicsBoundaryIsSafe(doomRpg) ||
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

    printf("[MENUTOUCH] PREPARED dashboard=2x2 cursorPatchBytes=0 targetLogical=68x23 targetPhysical=136x46\n");
    return 1;
}

int DoomRPG_esp32MainMenuTouchActivate(struct DoomRPG_s* doomRpgBase,
                                       uint32_t initialFramebufferFNV) {
    DoomRPG_t* doomRpg = (DoomRPG_t*)doomRpgBase;
    uint32_t currentHash;

    if (!touchPrepared || doomRpg == NULL || doomRpg != touchDoomRpg ||
        !DoomRPG_esp32MainMenuGraphicsBoundaryIsSafe(doomRpg)) {
        printf("[MENUTOUCH] FAILED activate prepared=%d sameDoom=%d safe=%d\n",
               touchPrepared,
               doomRpg == touchDoomRpg,
               DoomRPG_esp32MainMenuGraphicsBoundaryIsSafe(doomRpg));
        return 0;
    }

    if (doomRpg->menuSystem->menu != MENU_MAIN ||
        doomRpg->menuSystem->selectedIndex != 0) {
        printf("[MENUTOUCH] FAILED activate menu=%d selected=%d\n",
               doomRpg->menuSystem->menu,
               doomRpg->menuSystem->selectedIndex);
        return 0;
    }

    currentHash = DoomRPG_esp32MainMenuFramebufferHash(doomRpg->render);
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
        !DoomRPG_esp32MainMenuGraphicsBoundaryIsSafe(touchDoomRpg) ||
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
        !DoomRPG_esp32MainMenuGraphicsBoundaryIsSafe(touchDoomRpg)) {
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
    EspNativeMenuState_t* menuSystem;
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

    if (!DoomRPG_esp32MainMenuGraphicsBoundaryIsSafe(touchDoomRpg) ||
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
    hashBefore = DoomRPG_esp32MainMenuFramebufferHash(render);
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
        !DoomRPG_esp32MainMenuGraphicsBoundaryIsSafe(touchDoomRpg)) {
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


/* Consolidated main-menu tap gate / PlatformInput wrapper. */
#include <SDL.h>
#include <stdint.h>
#include <stdio.h>

#include "DoomRPG.h"
#include "DoomCanvas.h"
#include "MenuSystem.h"

#include "native_main_menu_160x120_layout.h"
#include "native_main_menu_actions.h"
#include "native_main_menu_present.h"
#include "native_main_menu_touch.h"
#include "platform_touch_events.h"
#include "platform_video_config.h"

#ifndef DOOMRPG_ESP32_TOUCH_HITBOX_OVERLAY
#define DOOMRPG_ESP32_TOUCH_HITBOX_OVERLAY 0
#endif

#if DOOMRPG_ESP32_TOUCH_HITBOX_OVERLAY
#include "platform_video_c_bridge.h"
#endif

#define MENU_TAP_GATE_LABEL_CHARS 10
#define MENU_TAP_GATE_GLYPH_ADVANCE 7
#define MENU_TAP_GATE_HAND_WIDTH 13
#define MENU_TAP_GATE_PAD_X 4
#define MENU_TAP_GATE_START_TOP_TOLERANCE 3
#define MENU_TAP_GATE_HALF_TEXT_WIDTH \
    ((MENU_TAP_GATE_LABEL_CHARS * MENU_TAP_GATE_GLYPH_ADVANCE) >> 1)
#define MENU_TAP_GATE_TEXT_X \
    ((DOOMRPG_LOGICAL_WIDTH >> 1) - MENU_TAP_GATE_HALF_TEXT_WIDTH)
#define MENU_TAP_GATE_HIT_LEFT \
    (MENU_TAP_GATE_TEXT_X - MENU_TAP_GATE_HAND_WIDTH - MENU_TAP_GATE_PAD_X)
#define MENU_TAP_GATE_HIT_RIGHT \
    (MENU_TAP_GATE_TEXT_X + \
     (MENU_TAP_GATE_LABEL_CHARS * MENU_TAP_GATE_GLYPH_ADVANCE) + \
     MENU_TAP_GATE_PAD_X)
static PlatformTapCallback downstreamTapCallback = NULL;
static int gateSelectedItem = 0;
static int lastTappedItem = -1;
static uint32_t gateTapCount = 0;

extern DoomRPG_t* doomRpg;

void __real_PlatformInput_setTapCallback(PlatformTapCallback callback);

static int gateHitItem(int16_t screenX, int16_t screenY) {
    const int logicalX = screenX / DOOMRPG_INTEGER_SCALE;
    const int logicalY = screenY / DOOMRPG_INTEGER_SCALE;
    int col;
    int row;

    if (logicalX >= DOOMRPG_ESP32_MAIN_MENU_CARD_COL0_LEFT &&
        logicalX <= DOOMRPG_ESP32_MAIN_MENU_CARD_COL0_RIGHT) {
        col = 0;
    }
    else if (logicalX >= DOOMRPG_ESP32_MAIN_MENU_CARD_COL1_LEFT &&
             logicalX <= DOOMRPG_ESP32_MAIN_MENU_CARD_COL1_RIGHT) {
        col = 1;
    }
    else {
        return -1;
    }

    if (logicalY >= DOOMRPG_ESP32_MAIN_MENU_CARD_ROW0_TOP &&
        logicalY <= DOOMRPG_ESP32_MAIN_MENU_CARD_ROW0_BOTTOM) {
        row = 0;
    }
    else if (logicalY >= DOOMRPG_ESP32_MAIN_MENU_CARD_ROW1_TOP &&
             logicalY <= DOOMRPG_ESP32_MAIN_MENU_CARD_ROW1_BOTTOM) {
        row = 1;
    }
    else {
        return -1;
    }

    return (row << 1) | col;
}

#if DOOMRPG_ESP32_TOUCH_HITBOX_OVERLAY
static void registerMainMenuHitboxOverlay(void) {
    static const int16_t left[DOOMRPG_ESP32_MAIN_MENU_ITEM_COUNT] = {
        DOOMRPG_ESP32_MAIN_MENU_CARD_COL0_LEFT,
        DOOMRPG_ESP32_MAIN_MENU_CARD_COL1_LEFT,
        DOOMRPG_ESP32_MAIN_MENU_CARD_COL0_LEFT,
        DOOMRPG_ESP32_MAIN_MENU_CARD_COL1_LEFT
    };
    static const int16_t right[DOOMRPG_ESP32_MAIN_MENU_ITEM_COUNT] = {
        DOOMRPG_ESP32_MAIN_MENU_CARD_COL0_RIGHT,
        DOOMRPG_ESP32_MAIN_MENU_CARD_COL1_RIGHT,
        DOOMRPG_ESP32_MAIN_MENU_CARD_COL0_RIGHT,
        DOOMRPG_ESP32_MAIN_MENU_CARD_COL1_RIGHT
    };
    static const int16_t top[DOOMRPG_ESP32_MAIN_MENU_ITEM_COUNT] = {
        DOOMRPG_ESP32_MAIN_MENU_CARD_ROW0_TOP,
        DOOMRPG_ESP32_MAIN_MENU_CARD_ROW0_TOP,
        DOOMRPG_ESP32_MAIN_MENU_CARD_ROW1_TOP,
        DOOMRPG_ESP32_MAIN_MENU_CARD_ROW1_TOP
    };
    static const int16_t bottom[DOOMRPG_ESP32_MAIN_MENU_ITEM_COUNT] = {
        DOOMRPG_ESP32_MAIN_MENU_CARD_ROW0_BOTTOM,
        DOOMRPG_ESP32_MAIN_MENU_CARD_ROW0_BOTTOM,
        DOOMRPG_ESP32_MAIN_MENU_CARD_ROW1_BOTTOM,
        DOOMRPG_ESP32_MAIN_MENU_CARD_ROW1_BOTTOM
    };
    int item;

    Esp32PlatformVideo_debugOverlayClear();
    for (item = 0; item < DOOMRPG_ESP32_MAIN_MENU_ITEM_COUNT; ++item) {
        Esp32PlatformVideo_debugOverlaySetZone(item,
                                               left[item],
                                               top[item],
                                               right[item],
                                               bottom[item]);
    }
    printf("[HITBOX] MAIN dashboard overlay zones=4 target=68x23 logical framebuffer=untouched\n");
}
#endif

static void disableMainMenuTouchForTransition(void) {
    downstreamTapCallback = NULL;
    __real_PlatformInput_setTapCallback(NULL);
#if DOOMRPG_ESP32_TOUCH_HITBOX_OVERLAY
    Esp32PlatformVideo_debugOverlayClear();
#endif
}

static void executeConfirmedAction(int item) {
    DoomRpgEsp32MainMenuDispatchResult result;

    if (doomRpg == NULL) {
        printf("[MAINACTION] FAILED global DoomRPG unavailable item=%d\n", item);
        return;
    }

    /* LOAD can fail closed while intentionally leaving MENU_MAIN active (for
     * example the hardware-tested NO SAVE feedback). Other actions leave the
     * dashboard on success, so retire its callback before changing ownership.
     */
    if (item != DOOMRPG_ESP32_MAIN_MENU_ACTION_LOAD) {
        disableMainMenuTouchForTransition();
    }

    result = DoomRPG_esp32MainMenuDispatchConfirmed(doomRpg, item);
    if (result == DOOMRPG_ESP32_MAIN_MENU_DISPATCH_FAILED) {
        printf("[MAINACTION] FAILED dispatch item=%d; recovery=attempt\n", item);
        if (!DoomRPG_esp32MainMenuRecover(doomRpg, "dispatch-failed")) {
            printf("[MAINACTION] FAILED recovery item=%d state=%d menu=%d\n",
                   item,
                   doomRpg->doomCanvas != NULL
                       ? doomRpg->doomCanvas->state : -999,
                   doomRpg->menuSystem != NULL
                       ? doomRpg->menuSystem->menu : -999);
        }
    }
    else if (result == DOOMRPG_ESP32_MAIN_MENU_DISPATCH_STAY_MAIN) {
        printf("[MAINACTION] STAY item=%d owner=MENU_MAIN\n", item);
    }
    else {
        printf("[MAINACTION] COMPLETE item=%d owner=transitioned\n", item);
    }
}

static void gatedTap(int16_t screenX,
                     int16_t screenY,
                     uint16_t pressure,
                     uint16_t rawX,
                     uint16_t rawY) {
    int hit;

    if (downstreamTapCallback == NULL) {
        return;
    }

    gateTapCount++;
    hit = gateHitItem(screenX, screenY);

    if (hit < 0) {
        lastTappedItem = -1;
        downstreamTapCallback(screenX, screenY, pressure, rawX, rawY);
        return;
    }

    if (hit != gateSelectedItem) {
        printf("[MENUTOUCH] GATE tap=%u SELECT-ARM current=%d hit=%d\n",
               (unsigned int)gateTapCount,
               gateSelectedItem,
               hit);
        lastTappedItem = hit;
        downstreamTapCallback(screenX, screenY, pressure, rawX, rawY);
        gateSelectedItem = hit;
        return;
    }

    if (lastTappedItem == hit) {
        printf("[MENUTOUCH] GATE tap=%u CONFIRM-PASS item=%d action=native-dispatch\n",
               (unsigned int)gateTapCount,
               hit);
        lastTappedItem = -1;
        executeConfirmedAction(hit);
        return;
    }

    lastTappedItem = hit;
    if (!DoomRPG_esp32MainMenuTouchArmSelected(hit)) {
        printf("[MENUTOUCH] FAILED visible arm item=%d tap=%u\n",
               hit,
               (unsigned int)gateTapCount);
    }
    printf("[MENUTOUCH] ARM item=%d tap=%u selected=%d awaitingReleasedSecondTap=yes visual=bright-card\n",
           hit,
           (unsigned int)gateTapCount,
           gateSelectedItem);
}

/* Intercept only callback registration, not XPT2046 sampling. This keeps the
 * generic PlatformInput driver unaware of menu semantics. MENU_MAIN gets the
 * validated select/confirm gate; other callbacks (currently bounded Options/Help Back owners)
 * pass through unchanged.
 */
void __wrap_PlatformInput_setTapCallback(PlatformTapCallback callback) {
    gateSelectedItem = 0;
    lastTappedItem = -1;
    gateTapCount = 0;

#if DOOMRPG_ESP32_TOUCH_HITBOX_OVERLAY
    if (callback == NULL) {
        Esp32PlatformVideo_debugOverlayClear();
    }
#endif

    if (callback == DoomRPG_esp32MainMenuTouchOnTap) {
        /* The ESP32 boot path initializes MENU_MAIN directly instead of calling
         * the original MenuSystem_setMenu(), because that function also owns
         * legacy menu-map/media transitions. MenuSystem_setMenu() normally
         * establishes ST_MENU after Menu_initMenu(). Keep that state contract
         * here at the exact point where our native MENU_MAIN becomes interactive.
         * This also covers the full bring-up path; Options -> Back already arrives
         * here in ST_MENU and therefore remains a no-op. */
        if (doomRpg != NULL && doomRpg->doomCanvas != NULL &&
            doomRpg->doomCanvas->state != ST_MENU) {
            const int priorState = doomRpg->doomCanvas->state;
            DoomCanvas_setState(doomRpg->doomCanvas, ST_MENU);
            printf("[MENUTOUCH] STATE SYNC canvas=%d->%d source=native-MENU_MAIN activation\n",
                   priorState,
                   doomRpg->doomCanvas->state);
        }

        downstreamTapCallback = callback;
#if DOOMRPG_ESP32_TOUCH_HITBOX_OVERLAY
        registerMainMenuHitboxOverlay();
#endif
        __real_PlatformInput_setTapCallback(gatedTap);
        printf("[MENUTOUCH] GATE READY initialSelected=0 dashboard=2x2 firstTap=bright-arm secondReleasedSameTap=confirm startAction=enabled loadAction=enabled optionsAction=enabled helpAction=enabled\n");
    }
    else {
        downstreamTapCallback = callback;
        __real_PlatformInput_setTapCallback(callback);
    }
}
