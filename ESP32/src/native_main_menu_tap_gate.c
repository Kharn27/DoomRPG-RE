#include <SDL.h>
#include <stdint.h>
#include <stdio.h>

#include "DoomRPG.h"

#include "native_main_menu_160x120_layout.h"
#include "native_main_menu_options_action.h"
#include "native_main_menu_options_back.h"
#include "native_main_menu_touch.h"
#include "platform_touch_events.h"
#include "platform_video_c_bridge.h"
#include "platform_video_config.h"

#ifndef DOOMRPG_ESP32_TOUCH_HITBOX_OVERLAY
#define DOOMRPG_ESP32_TOUCH_HITBOX_OVERLAY 0
#endif

#define MENU_TAP_GATE_LABEL_CHARS 10
#define MENU_TAP_GATE_GLYPH_ADVANCE 7
#define MENU_TAP_GATE_HAND_WIDTH 13
#define MENU_TAP_GATE_PAD_X 4
#define EXPECTED_OPTIONS_FRAMEBUFFER_FNV 0x6058d47dU

static PlatformTapCallback downstreamTapCallback = NULL;
static int gateSelectedItem = 0;
static int lastTappedItem = -1;
static uint32_t gateTapCount = 0;

extern DoomRPG_t* doomRpg;

void __real_PlatformInput_setTapCallback(PlatformTapCallback callback);

static void gateHitboxForItem(int item,
                              int* left,
                              int* top,
                              int* right,
                              int* bottom) {
    const int halfTextWidth =
        (MENU_TAP_GATE_LABEL_CHARS * MENU_TAP_GATE_GLYPH_ADVANCE) >> 1;
    const int textX = (DOOMRPG_LOGICAL_WIDTH >> 1) - halfTextWidth;

    if (left != NULL) {
        *left = textX - MENU_TAP_GATE_HAND_WIDTH - MENU_TAP_GATE_PAD_X;
    }
    if (right != NULL) {
        *right = textX +
                 (MENU_TAP_GATE_LABEL_CHARS * MENU_TAP_GATE_GLYPH_ADVANCE) +
                 MENU_TAP_GATE_PAD_X;
    }
    if (top != NULL) {
        *top = DOOMRPG_ESP32_MAIN_MENU_ITEM_START_Y +
               (item * DOOMRPG_ESP32_MAIN_MENU_ITEM_LINE_HEIGHT);
    }
    if (bottom != NULL) {
        *bottom = DOOMRPG_ESP32_MAIN_MENU_ITEM_START_Y +
                  (item * DOOMRPG_ESP32_MAIN_MENU_ITEM_LINE_HEIGHT) +
                  DOOMRPG_ESP32_MAIN_MENU_ITEM_LINE_HEIGHT - 1;
    }
}

static int gateHitItem(int16_t screenX, int16_t screenY) {
    const int logicalX = screenX / DOOMRPG_INTEGER_SCALE;
    const int logicalY = screenY / DOOMRPG_INTEGER_SCALE;
    const int relativeY = logicalY - DOOMRPG_ESP32_MAIN_MENU_ITEM_START_Y;
    int item;
    int left;
    int top;
    int right;
    int bottom;

    if (relativeY < 0 ||
        relativeY >= (DOOMRPG_ESP32_MAIN_MENU_ITEM_COUNT *
                      DOOMRPG_ESP32_MAIN_MENU_ITEM_LINE_HEIGHT)) {
        return -1;
    }

    item = relativeY / DOOMRPG_ESP32_MAIN_MENU_ITEM_LINE_HEIGHT;
    gateHitboxForItem(item, &left, &top, &right, &bottom);

    if (logicalX < left || logicalX > right ||
        logicalY < top || logicalY > bottom) {
        return -1;
    }

    return item;
}

static void registerMainMenuHitboxOverlay(void) {
    int item;

    Esp32PlatformVideo_debugOverlayClear();

    for (item = 0; item < DOOMRPG_ESP32_MAIN_MENU_ITEM_COUNT; ++item) {
        int left;
        int top;
        int right;
        int bottom;

        gateHitboxForItem(item, &left, &top, &right, &bottom);
        Esp32PlatformVideo_debugOverlaySetZone(item,
                                               (int16_t)left,
                                               (int16_t)top,
                                               (int16_t)right,
                                               (int16_t)bottom);
    }

#if DOOMRPG_ESP32_TOUCH_HITBOX_OVERLAY
    printf("[HITBOX] MAIN overlay registered from final tap gate zones=%d framebuffer=untouched\n",
           DOOMRPG_ESP32_MAIN_MENU_ITEM_COUNT);
#endif
}

static void executeConfirmedOptions(void) {
    /* Remove MENU_MAIN touch before mutating the real menu model. The Options
     * action owns the display transition; after it succeeds, arm only the
     * deliberately narrow Back callback for the new menu.
     */
    downstreamTapCallback = NULL;
    __real_PlatformInput_setTapCallback(NULL);
    Esp32PlatformVideo_debugOverlayClear();

    if (doomRpg == NULL) {
        printf("[MAINOPTIONS] FAILED global DoomRPG unavailable at confirmed Options tap\n");
        return;
    }

    if (!DoomRPG_esp32ActivateMainMenuOptions(doomRpg)) {
        printf("[MAINOPTIONS] FAILED confirmed Options action\n");
        return;
    }

    if (!DoomRPG_esp32OptionsBackActivate(doomRpg,
                                          EXPECTED_OPTIONS_FRAMEBUFFER_FNV)) {
        printf("[OPTIONBACK] FAILED arming Back after Options transition\n");
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
        if (hit == 1) {
            printf("[MENUTOUCH] GATE tap=%u CONFIRM-PASS item=1 action=execute-options\n",
                   (unsigned int)gateTapCount);
            lastTappedItem = -1;
            executeConfirmedOptions();
            return;
        }

        printf("[MENUTOUCH] GATE tap=%u CONFIRM-PASS item=%d action=deferred\n",
               (unsigned int)gateTapCount,
               hit);
        downstreamTapCallback(screenX, screenY, pressure, rawX, rawY);
        lastTappedItem = -1;
        return;
    }

    lastTappedItem = hit;
    printf("[MENUTOUCH] ARM item=%d tap=%u selected=%d awaitingReleasedSecondTap=yes\n",
           hit,
           (unsigned int)gateTapCount,
           gateSelectedItem);
}

/* Intercept only callback registration, not XPT2046 sampling. This keeps the
 * generic PlatformInput driver unaware of menu semantics. MENU_MAIN gets the
 * validated select/confirm gate; other callbacks (currently Options Back only)
 * pass through unchanged.
 */
void __wrap_PlatformInput_setTapCallback(PlatformTapCallback callback) {
    gateSelectedItem = 0;
    lastTappedItem = -1;
    gateTapCount = 0;

    if (callback == NULL) {
        Esp32PlatformVideo_debugOverlayClear();
    }

    if (callback == DoomRPG_esp32MainMenuTouchOnTap) {
        downstreamTapCallback = callback;
        registerMainMenuHitboxOverlay();
        __real_PlatformInput_setTapCallback(gatedTap);
        printf("[MENUTOUCH] GATE READY initialSelected=0 firstSameTap=arm secondReleasedSameTap=confirm optionsAction=enabled\n");
    }
    else {
        downstreamTapCallback = callback;
        __real_PlatformInput_setTapCallback(callback);
    }
}
