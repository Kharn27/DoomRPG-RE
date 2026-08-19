#include <SDL.h>
#include <stdint.h>
#include <stdio.h>

#include "DoomRPG.h"

#include "native_main_menu_160x120_layout.h"
#include "native_main_menu_options_action.h"
#include "native_main_menu_options_back.h"
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
#define EXPECTED_OPTIONS_FRAMEBUFFER_FNV 0x6058d47dU

static PlatformTapCallback downstreamTapCallback = NULL;
static int gateSelectedItem = 0;
static int lastTappedItem = -1;
static uint32_t gateTapCount = 0;

extern DoomRPG_t* doomRpg;

void __real_PlatformInput_setTapCallback(PlatformTapCallback callback);

static int gateHitItem(int16_t screenX, int16_t screenY) {
    const int logicalX = screenX / DOOMRPG_INTEGER_SCALE;
    const int logicalY = screenY / DOOMRPG_INTEGER_SCALE;
    const int relativeY = logicalY - DOOMRPG_ESP32_MAIN_MENU_ITEM_START_Y;

    if (logicalX < MENU_TAP_GATE_HIT_LEFT ||
        logicalX > MENU_TAP_GATE_HIT_RIGHT ||
        relativeY < 0 ||
        relativeY >= (DOOMRPG_ESP32_MAIN_MENU_ITEM_COUNT *
                      DOOMRPG_ESP32_MAIN_MENU_ITEM_LINE_HEIGHT)) {
        return -1;
    }

    return relativeY / DOOMRPG_ESP32_MAIN_MENU_ITEM_LINE_HEIGHT;
}

#if DOOMRPG_ESP32_TOUCH_HITBOX_OVERLAY
static void registerMainMenuHitboxOverlay(void) {
    int item;

    Esp32PlatformVideo_debugOverlayClear();

    for (item = 0; item < DOOMRPG_ESP32_MAIN_MENU_ITEM_COUNT; ++item) {
        const int top = DOOMRPG_ESP32_MAIN_MENU_ITEM_START_Y +
                        (item * DOOMRPG_ESP32_MAIN_MENU_ITEM_LINE_HEIGHT);
        const int bottom = top + DOOMRPG_ESP32_MAIN_MENU_ITEM_LINE_HEIGHT - 1;

        Esp32PlatformVideo_debugOverlaySetZone(item,
                                               MENU_TAP_GATE_HIT_LEFT,
                                               (int16_t)top,
                                               MENU_TAP_GATE_HIT_RIGHT,
                                               (int16_t)bottom);
    }

    printf("[HITBOX] MAIN overlay registered from final tap gate zones=%d framebuffer=untouched\n",
           DOOMRPG_ESP32_MAIN_MENU_ITEM_COUNT);
}
#endif

static void executeConfirmedOptions(void) {
    /* Remove MENU_MAIN touch before mutating the real menu model. The Options
     * action owns the display transition; after it succeeds, arm only the
     * deliberately narrow Back callback for the new menu.
     */
    downstreamTapCallback = NULL;
    __real_PlatformInput_setTapCallback(NULL);
#if DOOMRPG_ESP32_TOUCH_HITBOX_OVERLAY
    Esp32PlatformVideo_debugOverlayClear();
#endif

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

#if DOOMRPG_ESP32_TOUCH_HITBOX_OVERLAY
    if (callback == NULL) {
        Esp32PlatformVideo_debugOverlayClear();
    }
#endif

    if (callback == DoomRPG_esp32MainMenuTouchOnTap) {
        downstreamTapCallback = callback;
#if DOOMRPG_ESP32_TOUCH_HITBOX_OVERLAY
        registerMainMenuHitboxOverlay();
#endif
        __real_PlatformInput_setTapCallback(gatedTap);
        printf("[MENUTOUCH] GATE READY initialSelected=0 firstSameTap=arm secondReleasedSameTap=confirm optionsAction=enabled\n");
    }
    else {
        downstreamTapCallback = callback;
        __real_PlatformInput_setTapCallback(callback);
    }
}
