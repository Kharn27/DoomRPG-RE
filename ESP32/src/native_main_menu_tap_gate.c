#include <SDL.h>
#include <stdint.h>
#include <stdio.h>

#include "DoomRPG.h"

#include "native_main_menu_160x120_layout.h"
#include "native_main_menu_options_action.h"
#include "native_main_menu_touch.h"
#include "platform_touch_events.h"
#include "platform_video_config.h"

#define MENU_TAP_GATE_LABEL_CHARS 10
#define MENU_TAP_GATE_GLYPH_ADVANCE 7
#define MENU_TAP_GATE_HAND_WIDTH 13
#define MENU_TAP_GATE_PAD_X 4

static PlatformTapCallback downstreamTapCallback = NULL;
static int gateSelectedItem = 0;
static int lastTappedItem = -1;
static uint32_t gateTapCount = 0;

extern DoomRPG_t* doomRpg;

void __real_PlatformInput_setTapCallback(PlatformTapCallback callback);

static int gateHitItem(int16_t screenX, int16_t screenY) {
    const int logicalX = screenX / DOOMRPG_INTEGER_SCALE;
    const int logicalY = screenY / DOOMRPG_INTEGER_SCALE;
    const int halfTextWidth =
        (MENU_TAP_GATE_LABEL_CHARS * MENU_TAP_GATE_GLYPH_ADVANCE) >> 1;
    const int textX = (DOOMRPG_LOGICAL_WIDTH >> 1) - halfTextWidth;
    const int hitLeft = textX - MENU_TAP_GATE_HAND_WIDTH - MENU_TAP_GATE_PAD_X;
    const int hitRight = textX +
                         (MENU_TAP_GATE_LABEL_CHARS *
                          MENU_TAP_GATE_GLYPH_ADVANCE) +
                         MENU_TAP_GATE_PAD_X;
    const int relativeY = logicalY - DOOMRPG_ESP32_MAIN_MENU_ITEM_START_Y;

    if (logicalX < hitLeft || logicalX > hitRight ||
        relativeY < 0 ||
        relativeY >= (DOOMRPG_ESP32_MAIN_MENU_ITEM_COUNT *
                      DOOMRPG_ESP32_MAIN_MENU_ITEM_LINE_HEIGHT)) {
        return -1;
    }

    return relativeY / DOOMRPG_ESP32_MAIN_MENU_ITEM_LINE_HEIGHT;
}

static void executeConfirmedOptions(void) {
    /* The Options screen is display-only in this increment. Remove the physical
     * tap callback before mutating the real MenuSystem model so no further
     * MENU_MAIN touch can race the transition.
     */
    downstreamTapCallback = NULL;
    __real_PlatformInput_setTapCallback(NULL);

    if (doomRpg == NULL) {
        printf("[MAINOPTIONS] FAILED global DoomRPG unavailable at confirmed Options tap\n");
        return;
    }

    if (!DoomRPG_esp32ActivateMainMenuOptions(doomRpg)) {
        printf("[MAINOPTIONS] FAILED confirmed Options action\n");
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
 * generic PlatformInput driver unaware of menu semantics. The validated gate
 * still enforces select-then-confirm; this increment promotes only confirmed
 * MENU_MAIN Options to a real action.
 */
void __wrap_PlatformInput_setTapCallback(PlatformTapCallback callback) {
    gateSelectedItem = 0;
    lastTappedItem = -1;
    gateTapCount = 0;

    if (callback == DoomRPG_esp32MainMenuTouchOnTap) {
        downstreamTapCallback = callback;
        __real_PlatformInput_setTapCallback(gatedTap);
        printf("[MENUTOUCH] GATE READY initialSelected=0 firstSameTap=arm secondReleasedSameTap=confirm optionsAction=enabled\n");
    }
    else {
        downstreamTapCallback = callback;
        __real_PlatformInput_setTapCallback(callback);
    }
}
