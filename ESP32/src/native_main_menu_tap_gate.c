#include <SDL.h>
#include <stdint.h>
#include <stdio.h>

#include "DoomRPG.h"
#include "DoomCanvas.h"

#include "native_main_menu_160x120_layout.h"
#include "native_main_menu_actions.h"
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
        printf("[MAINACTION] FAILED dispatch item=%d\n", item);
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
