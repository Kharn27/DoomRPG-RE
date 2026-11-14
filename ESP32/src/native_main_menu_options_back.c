#if !defined(DOOMRPG_ESP32) || defined(DOOMRPG_ESP32_GEOMETRY_ORACLE)
#include <SDL.h>
#endif
#include "esp_native_root_state.h"
#include <stdint.h>
#include <stdio.h>

#include "DoomRPG.h"
#include "DoomCanvas.h"
#include "Menu.h"
#include "esp_native_menu_state.h"
#include "Render.h"

#include "native_main_menu_160x120_layout.h"
#include "native_main_menu_actions.h"
#include "esp_native_backlight.h"
#include "esp_native_config.h"
#include "esp_native_orientation.h"
#include "esp_main_menu_skin.h"
#include "native_main_menu_options_action.h"
#include "native_main_menu_options_back.h"
#include "native_main_menu_touch.h"
#include "native_main_menu_present.h"
#include "native_main_menu_touch_layout.h"
#include "native_sprite_lru_cache.h"
#include "native_wall_lru_cache.h"
#include "platform_touch_events.h"
#include "platform_video_config.h"

#ifndef DOOMRPG_ESP32_TOUCH_HITBOX_OVERLAY
#define DOOMRPG_ESP32_TOUCH_HITBOX_OVERLAY 0
#endif

#if DOOMRPG_ESP32_TOUCH_HITBOX_OVERLAY
#include "platform_video_c_bridge.h"
#endif

#define OPTIONS_ITEM_COUNT DOOMRPG_ESP32_OPTIONS_CARD_COUNT
#define OPTIONS_BACK_ITEM 0
#define OPTIONS_VIDEO_ITEM 1
#define VIDEO_BACK_ITEM 0
#define VIDEO_BRIGHTNESS_ITEM 1
#define VIDEO_ORIENTATION_ITEM 2
#define VIDEO_THEME_ITEM 3

static DoomRPG_t* optionsDoomRpg = NULL;
static int optionsBackActive = 0;
static int armedItem = -1; /* Two released taps in root/VIDEO; presets direct. */
static uint8_t videoOpen = 0U; /* UI-only intermediate page, model unchanged. */
static uint8_t luminositeOpen = 0U; /* Child uses direct single taps. */
static uint8_t orientationOpen = 0U;
static uint8_t themeOpen = 0U;
static uint32_t optionsTapCount = 0;
static uint32_t optionsExpectedFrameFNV = 0U;

static int optionsItemAt(int16_t screenX, int16_t screenY) {
    const int logicalX = screenX / DOOMRPG_INTEGER_SCALE;
    const int logicalY = screenY / DOOMRPG_INTEGER_SCALE;
    int item;
    for (item = 0; item < OPTIONS_ITEM_COUNT; ++item) {
        int left, top, right, bottom;
        DoomRPG_esp32OptionsCardRect(item, &left, &top, &right, &bottom);
        if (logicalX >= left && logicalX <= right &&
            logicalY >= top && logicalY <= bottom) return item;
    }
    return -1;
}

static int videoItemAt(int16_t screenX, int16_t screenY) {
    const int x = screenX / DOOMRPG_INTEGER_SCALE;
    const int y = screenY / DOOMRPG_INTEGER_SCALE;
    int item;
    for (item = 0; item < DOOMRPG_ESP32_VIDEO_CARD_COUNT; ++item) {
        int l, t, r, b;
        DoomRPG_esp32VideoCardRect(item, &l, &t, &r, &b);
        if (x >= l && x <= r && y >= t && y <= b) return item;
    }
    return -1;
}

static int luminositeItemAt(int16_t screenX, int16_t screenY) {
    const int logicalX = screenX / DOOMRPG_INTEGER_SCALE;
    const int logicalY = screenY / DOOMRPG_INTEGER_SCALE;
    int item;
    for (item = 0; item < DOOMRPG_ESP32_BRIGHTNESS_CARD_COUNT; ++item) {
        int left, top, right, bottom;
        DoomRPG_esp32BrightnessCardRect(item, &left, &top, &right, &bottom);
        if (logicalX >= left && logicalX <= right &&
            logicalY >= top && logicalY <= bottom) return item;
    }
    return -1;
}

static int orientationItemAt(int16_t screenX, int16_t screenY) {
    const int logicalX = screenX / DOOMRPG_INTEGER_SCALE;
    const int logicalY = screenY / DOOMRPG_INTEGER_SCALE;
    int item;
    for (item = 0; item < DOOMRPG_ESP32_ORIENTATION_CARD_COUNT; ++item) {
        int left, top, right, bottom;
        DoomRPG_esp32OptionsCardRect(item, &left, &top, &right, &bottom);
        if (logicalX >= left && logicalX <= right &&
            logicalY >= top && logicalY <= bottom) return item;
    }
    return -1;
}

static int themeItemAt(int16_t screenX, int16_t screenY) {
    const int x = screenX / DOOMRPG_INTEGER_SCALE;
    const int y = screenY / DOOMRPG_INTEGER_SCALE;
    int item;
    for (item = 0; item < DOOMRPG_ESP32_THEME_CARD_COUNT; ++item) {
        int l, t, r, b;
        DoomRPG_esp32VideoCardRect(item, &l, &t, &r, &b);
        if (x >= l && x <= r && y >= t && y <= b) return item;
    }
    return -1;
}

#if DOOMRPG_ESP32_TOUCH_HITBOX_OVERLAY
static void registerOptionsHitboxOverlay(void) {
    int item;
    Esp32PlatformVideo_debugOverlayClear();
    const int count = luminositeOpen ? DOOMRPG_ESP32_BRIGHTNESS_CARD_COUNT :
                      (themeOpen ? DOOMRPG_ESP32_THEME_CARD_COUNT :
                      (orientationOpen ? DOOMRPG_ESP32_ORIENTATION_CARD_COUNT :
                      (videoOpen ? DOOMRPG_ESP32_VIDEO_CARD_COUNT : OPTIONS_ITEM_COUNT)));
    for (item = 0; item < count; ++item) {
        int left, top, right, bottom;
        if (luminositeOpen)
            DoomRPG_esp32BrightnessCardRect(item, &left, &top, &right, &bottom);
        else if (themeOpen)
            DoomRPG_esp32VideoCardRect(item, &left, &top, &right, &bottom);
        else if (orientationOpen)
            DoomRPG_esp32OptionsCardRect(item, &left, &top, &right, &bottom);
        else if (videoOpen)
            DoomRPG_esp32VideoCardRect(item, &left, &top, &right, &bottom);
        else
            DoomRPG_esp32OptionsCardRect(item, &left, &top, &right, &bottom);
        Esp32PlatformVideo_debugOverlaySetZone(item,
                                               (int16_t)left,
                                               (int16_t)top,
                                               (int16_t)right,
                                               (int16_t)bottom);
    }
    Esp32PlatformVideo_debugOverlayRefresh();
    printf("[HITBOX] OPTIONS split-rail overlay zones=%d layer=%s framebuffer=untouched\n",
           count, luminositeOpen ? "luminosite" : (orientationOpen ? "orientation" :
                  (themeOpen ? "theme" : (videoOpen ? "video" : "options"))));
}

#endif

static int paintOptionsState(int selectedItem, int armed) {
    uint32_t nextHash = 0U;

    if (!DoomRPG_esp32PaintMainMenuOptionsDashboardSelection(
            optionsDoomRpg, selectedItem, armed, &nextHash)) {
        printf("[OPTIONBACK] FAILED repaint item=%d armed=%d\n",
               selectedItem, armed);
        return 0;
    }

    optionsExpectedFrameFNV = nextHash;
    (void)Esp32PlatformVideo_present();
    printf("[OPTIONBACK] VISUAL item=%d armed=%d brightness=%u framebufferFNV=%08x\n",
           selectedItem, armed, (unsigned int)Esp32Backlight_percent(),
           (unsigned int)nextHash);
    return 1;
}

static int paintVideoState(int selectedItem, int armed) {
    uint32_t nextHash = 0U;
    if (!DoomRPG_esp32PaintMainMenuVideoDashboardSelection(
            optionsDoomRpg, selectedItem, armed, &nextHash)) {
        printf("[OPTIONSVIDEO] FAILED repaint item=%d armed=%d\n",
               selectedItem, armed);
        return 0;
    }
    optionsExpectedFrameFNV = nextHash;
    (void)Esp32PlatformVideo_present();
    printf("[OPTIONSVIDEO] VISUAL item=%d armed=%d framebufferFNV=%08x\n",
           selectedItem, armed, (unsigned int)nextHash);
    return 1;
}

static int paintLuminositeState(void) {
    uint32_t nextHash = 0U;

    if (!DoomRPG_esp32PaintMainMenuBrightnessDashboard(
            optionsDoomRpg, &nextHash)) {
        printf("[OPTIONSBRIGHTNESS] FAILED paint child\n");
        return 0;
    }
    optionsExpectedFrameFNV = nextHash;
    (void)Esp32PlatformVideo_present();
    printf("[OPTIONSBRIGHTNESS] VISUAL percent=%u framebufferFNV=%08x\n",
           (unsigned int)Esp32Backlight_percent(), (unsigned int)nextHash);
    return 1;
}

static int paintOrientationState(void) {
    uint32_t nextHash = 0U;
    if (!DoomRPG_esp32PaintMainMenuOrientationDashboard(
            optionsDoomRpg, &nextHash)) {
        printf("[OPTIONSORIENTATION] FAILED paint child\n");
        return 0;
    }
    optionsExpectedFrameFNV = nextHash;
    (void)Esp32PlatformVideo_present();
    printf("[OPTIONSORIENTATION] VISUAL mode=%s framebufferFNV=%08x\n",
           Esp32Orientation_get() ? "INVERSE" : "NORMAL",
           (unsigned int)nextHash);
    return 1;
}

static int paintThemeState(void) {
    uint32_t nextHash = 0U;
    if (!DoomRPG_esp32PaintMainMenuThemeDashboard(
            optionsDoomRpg, &nextHash)) {
        printf("[OPTIONSTHEME] FAILED paint child\n");
        return 0;
    }
    optionsExpectedFrameFNV = nextHash;
    (void)Esp32PlatformVideo_present();
    printf("[OPTIONSTHEME] VISUAL theme=%u framebufferFNV=%08x\n",
           (unsigned int)EspMainMenuSkin_getTheme(), (unsigned int)nextHash);
    return 1;
}

/* Two released taps: first arms only UI pixels; second verifies the SD
 * transaction and returns automatically to the VIDEO parent.
 * Each arm paint has exactly one present and no world/render ownership. */
static int paintArmedPreference(int layer, int selectedItem) {
    uint32_t hash = 0U;
    int ok = 0;
    if (selectedItem <= 0) return 0;
    if (layer == 1 && selectedItem < DOOMRPG_ESP32_BRIGHTNESS_CARD_COUNT)
        ok = DoomRPG_esp32PaintBrightnessPresetCardsSelection(
            optionsDoomRpg, Esp32Backlight_percent(), selectedItem, &hash);
    else if (layer == 2 &&
             selectedItem < DOOMRPG_ESP32_ORIENTATION_CARD_COUNT)
        ok = DoomRPG_esp32PaintOrientationPresetCardsSelection(
            optionsDoomRpg, Esp32Orientation_get(), selectedItem, &hash);
    else if (layer == 3 &&
             selectedItem < DOOMRPG_ESP32_THEME_CARD_COUNT)
        ok = DoomRPG_esp32PaintThemePresetCardsSelection(
            optionsDoomRpg, EspMainMenuSkin_getTheme(), selectedItem, &hash);
    if (!ok || !hash) return 0;
    optionsExpectedFrameFNV = hash;
    (void)Esp32PlatformVideo_present();
    printf("[OPTIONCHOICE] ARM layer=%d item=%d confirm=second-released-tap frame=%08x\n",
           layer, selectedItem, (unsigned)hash);
    return 1;
}

static int returnToMainMenu(DoomRPG_t* doomRpg) {
    EspNativeMenuState_t* menuSystem = EspNativeRootOwners_live()->menuSystem;
    Render_t* render = EspNativeRootOwners_live()->render;
    uint32_t finalHash = 0U;

    printf("\n=== Doom RPG ESP32 fast Options -> MENU_MAIN Back ===\n");
    printf("[OPTIONBACK] Begin menu=%d selected=%d old=%d framebufferFNV=%08x shapeData=%p mediaTexels=%p\n",
           menuSystem->menu,
           menuSystem->selectedIndex,
           menuSystem->oldMenu,
           (unsigned int)DoomRPG_esp32MainMenuFramebufferHash(render),
           (void*)render->shapeData,
           (void*)render->mediaTexels);

    if (!DoomRPG_esp32MainMenuGraphicsBoundaryIsSafe(doomRpg) ||
        menuSystem->menu != MENU_MAIN_OPTIONS ||
        menuSystem->selectedIndex != OPTIONS_BACK_ITEM ||
        videoOpen != 0U || luminositeOpen != 0U ||
        orientationOpen != 0U || themeOpen != 0U ||
        optionsExpectedFrameFNV == 0U ||
        DoomRPG_esp32MainMenuFramebufferHash(render) != optionsExpectedFrameFNV) {
        printf("[OPTIONBACK] FAILED precondition safe=%d menu=%d selected=%d framebuffer=%08x expected=%08x\n",
               DoomRPG_esp32MainMenuGraphicsBoundaryIsSafe(doomRpg),
               menuSystem->menu,
               menuSystem->selectedIndex,
               (unsigned int)DoomRPG_esp32MainMenuFramebufferHash(render),
               (unsigned int)optionsExpectedFrameFNV);
        return 0;
    }

    if (!DoomRPG_esp32MainMenuReturnToMain(
            doomRpg,
            MENU_MAIN_OPTIONS,
            "options",
            &finalHash)) {
        printf("[OPTIONBACK] FAILED native Back action\n");
        return 0;
    }

    printf("[OPTIONBACK] FAST End framebufferFNV=%08x expected=%08x runtimeFNV=%08x menu=%d selected=%d touchActive=%d shapeData=%p mediaTexels=%p\n",
           (unsigned int)finalHash,
           (unsigned int)DoomRPG_esp32MainMenuSelectionFramebufferFNV(0),
           (unsigned int)DoomRPG_esp32MainMenuFramebufferHash(render),
           menuSystem->menu,
           menuSystem->selectedIndex,
           DoomRPG_esp32MainMenuTouchIsActive(),
           (void*)render->shapeData,
           (void*)render->mediaTexels);
    printf("[OPTIONBACK] READY native semantic Back + opaque bounded repaint; no MenuSystem_back, no MENUWALL/MENUSPRITE replay\n");
    printf("[OPTIONBACK] READY MENU_MAIN touch re-armed for another complete cycle\n");
    return 1;
}

static void optionsBackTap(int16_t screenX,
                           int16_t screenY,
                           uint16_t pressure,
                           uint16_t rawX,
                           uint16_t rawY) {
    int item;

    if (!optionsBackActive || optionsDoomRpg == NULL) return;

    optionsTapCount++;
    item = luminositeOpen ? luminositeItemAt(screenX, screenY) :
           (orientationOpen ? orientationItemAt(screenX, screenY) :
           (themeOpen ? themeItemAt(screenX, screenY) :
           (videoOpen ? videoItemAt(screenX, screenY) :
                        optionsItemAt(screenX, screenY))));
    printf("[OPTIONBACK] TAP n=%u raw=%u,%u pressure=%u physical=%d,%d logical=%d,%d layer=%s item=%d armedItem=%d\n",
           (unsigned int)optionsTapCount, rawX, rawY, pressure,
           screenX, screenY, screenX / DOOMRPG_INTEGER_SCALE,
           screenY / DOOMRPG_INTEGER_SCALE,
           luminositeOpen ? "luminosite" : (orientationOpen ? "orientation" :
               (themeOpen ? "theme" : (videoOpen ? "video" : "options"))),
           item, armedItem);

    if (!DoomRPG_esp32MainMenuGraphicsBoundaryIsSafe(optionsDoomRpg) ||
        EspNativeRootOwners_live()->menuSystem->menu != MENU_MAIN_OPTIONS ||
        optionsExpectedFrameFNV == 0U ||
        DoomRPG_esp32MainMenuFramebufferHash(EspNativeRootOwners_live()->render) !=
            optionsExpectedFrameFNV) {
        printf("[OPTIONBACK] FAILED runtime boundary menu=%d framebuffer=%08x expected=%08x\n",
               EspNativeRootOwners_live()->menuSystem->menu,
               (unsigned int)DoomRPG_esp32MainMenuFramebufferHash(EspNativeRootOwners_live()->render),
               (unsigned int)optionsExpectedFrameFNV);
        optionsBackActive = 0;
        PlatformInput_setTapCallback(NULL);
        DoomRPG_esp32MainMenuRecover(optionsDoomRpg, "options-touch-precondition");
        return;
    }

    if (luminositeOpen) {
        if (item == 0) {
            /* BACK returns to Options, not directly to MENU_MAIN. */
            if (!paintVideoState(VIDEO_BACK_ITEM, 0)) {
                optionsBackActive = 0;
                PlatformInput_setTapCallback(NULL);
                DoomRPG_esp32MainMenuRecover(optionsDoomRpg,
                                             "luminosite-back-repaint-failed");
                return;
            }
            luminositeOpen = 0U;
            armedItem = -1;
#if DOOMRPG_ESP32_TOUCH_HITBOX_OVERLAY
            registerOptionsHitboxOverlay();
#endif
            printf("[OPTIONSBRIGHTNESS] BACK to=VIDEO percent=%u mutation=ui-only\n",
                   (unsigned int)Esp32Backlight_percent());
            return;
        }
        if (item >= 1 && item < DOOMRPG_ESP32_BRIGHTNESS_CARD_COUNT) {
            if (armedItem != item) {
                if (!paintArmedPreference(1, item)) {
                    optionsBackActive = 0;
                    PlatformInput_setTapCallback(NULL);
                    DoomRPG_esp32MainMenuRecover(optionsDoomRpg, "brightness-arm-failed");
                    return;
                }
                armedItem = item;
                return;
            }
            const uint8_t before = Esp32Backlight_percent();
            const uint8_t after = (uint8_t)(item * 25U);
            if (after != before) {
                if (!EspNativeConfig_saveBrightness(after)) {
                    printf("[OPTIONSBRIGHTNESS] ROLLBACK reason=sd-save percent=%u retained=%u\n",
                           (unsigned)after, (unsigned)before);
                    return;
                }
                if (!Esp32Backlight_setPercent(after)) {
                    const int restored = EspNativeConfig_saveBrightness(before);
                    printf("[OPTIONSBRIGHTNESS] ROLLBACK reason=pwm restored=%d\n",
                           restored);
                    return;
                }
            }
            if (!paintVideoState(VIDEO_BACK_ITEM, 0)) {
                if (after != before) {
                    const int hw = Esp32Backlight_setPercent(before);
                    const int sd = EspNativeConfig_saveBrightness(before);
                    printf("[OPTIONSBRIGHTNESS] ROLLBACK reason=video-repaint hw=%d sd=%d\n",
                           hw, sd);
                }
                optionsBackActive = 0;
                PlatformInput_setTapCallback(NULL);
                DoomRPG_esp32MainMenuRecover(optionsDoomRpg,
                                             "brightness-autoback-failed");
                return;
            }
            luminositeOpen = 0U;
            armedItem = -1;
            PlatformInput_requireFreshTapAfterRelease();
#if DOOMRPG_ESP32_TOUCH_HITBOX_OVERLAY
            registerOptionsHitboxOverlay();
#endif
            printf("[OPTIONSBRIGHTNESS] SELECT %u->%u changed=%s confirmation=two-tap autoBack=VIDEO persistence=sd-v3\n",
                   (unsigned)before, (unsigned)after,
                   before == after ? "no" : "yes");
            return;
        }
        if (armedItem != -1) {
            if (!paintLuminositeState()) {
                optionsBackActive = 0;
                PlatformInput_setTapCallback(NULL);
                DoomRPG_esp32MainMenuRecover(optionsDoomRpg, "brightness-disarm-failed");
                return;
            }
            armedItem = -1;
        }
        printf("[OPTIONSBRIGHTNESS] MISS outside-presets no-action\n");
        return;
    }

    if (orientationOpen) {
        if (item == 0) {
            if (!paintVideoState(VIDEO_BACK_ITEM, 0)) {
                optionsBackActive = 0;
                PlatformInput_setTapCallback(NULL);
                DoomRPG_esp32MainMenuRecover(optionsDoomRpg,
                                             "orientation-back-repaint-failed");
                return;
            }
            orientationOpen = 0U;
            armedItem = -1;
#if DOOMRPG_ESP32_TOUCH_HITBOX_OVERLAY
            registerOptionsHitboxOverlay();
#endif
            printf("[OPTIONSORIENTATION] BACK to=VIDEO mode=%s mutation=ui-only\n",
                   Esp32Orientation_get() ? "INVERSE" : "NORMAL");
            return;
        }
        if (item == 1 || item == 2) {
            if (armedItem != item) {
                if (!paintArmedPreference(2, item)) {
                    optionsBackActive = 0;
                    PlatformInput_setTapCallback(NULL);
                    DoomRPG_esp32MainMenuRecover(optionsDoomRpg, "orientation-arm-failed");
                    return;
                }
                armedItem = item;
                return;
            }
            const uint8_t before = Esp32Orientation_get();
            const uint8_t after = item == 2 ? ESP32_ORIENTATION_INVERSE :
                                             ESP32_ORIENTATION_NORMAL;
            if (after != before) {
                /* Commit SD before TFT/XPT rotation; failures retain old state. */
                if (!EspNativeConfig_saveOrientation(after)) {
                    printf("[OPTIONSORIENTATION] ROLLBACK reason=sd-save requested=%u retained=%u\n",
                           (unsigned)after, (unsigned)before);
                    return;
                }
                if (!Esp32Orientation_set(after)) {
                    const int restored = EspNativeConfig_saveOrientation(before);
                    printf("[OPTIONSORIENTATION] ROLLBACK reason=rotation savedBefore=%d\n",
                           restored);
                    return;
                }
                PlatformInput_requireFreshTapAfterRelease();
            }
            if (!paintVideoState(VIDEO_BACK_ITEM, 0)) {
                if (after != before) {
                    const int hw = Esp32Orientation_set(before);
                    const int sd = EspNativeConfig_saveOrientation(before);
                    printf("[OPTIONSORIENTATION] ROLLBACK reason=video-repaint hw=%d sd=%d\n",
                           hw, sd);
                }
                optionsBackActive = 0;
                PlatformInput_setTapCallback(NULL);
                DoomRPG_esp32MainMenuRecover(optionsDoomRpg,
                                             "orientation-autoback-failed");
                return;
            }
            orientationOpen = 0U;
            armedItem = -1;
            PlatformInput_requireFreshTapAfterRelease();
#if DOOMRPG_ESP32_TOUCH_HITBOX_OVERLAY
            registerOptionsHitboxOverlay();
#endif
            printf("[OPTIONSORIENTATION] SELECT %s->%s changed=%s confirmation=two-tap autoBack=VIDEO persistence=sd-v3\n",
                   before ? "INVERSE" : "NORMAL",
                   after ? "INVERSE" : "NORMAL",
                   before == after ? "no" : "yes");
            return;
        }
        if (armedItem != -1) {
            if (!paintOrientationState()) {
                optionsBackActive = 0;
                PlatformInput_setTapCallback(NULL);
                DoomRPG_esp32MainMenuRecover(optionsDoomRpg, "orientation-disarm-failed");
                return;
            }
            armedItem = -1;
        }
        printf("[OPTIONSORIENTATION] MISS outside-choices no-action\n");
        return;
    }

    if (themeOpen) {
        if (item == 0) {
            if (!paintVideoState(VIDEO_BACK_ITEM, 0)) {
                optionsBackActive = 0;
                PlatformInput_setTapCallback(NULL);
                DoomRPG_esp32MainMenuRecover(optionsDoomRpg,
                                             "theme-back-repaint-failed");
                return;
            }
            themeOpen = 0U;
            armedItem = -1;
#if DOOMRPG_ESP32_TOUCH_HITBOX_OVERLAY
            registerOptionsHitboxOverlay();
#endif
            printf("[OPTIONSTHEME] BACK to=VIDEO theme=%u state=unchanged\n",
                   (unsigned)EspMainMenuSkin_getTheme());
            return;
        }
        if (item >= 1 && item < DOOMRPG_ESP32_THEME_CARD_COUNT) {
            if (armedItem != item) {
                if (!paintArmedPreference(3, item)) {
                    optionsBackActive = 0;
                    PlatformInput_setTapCallback(NULL);
                    DoomRPG_esp32MainMenuRecover(optionsDoomRpg, "theme-arm-failed");
                    return;
                }
                armedItem = item;
                return;
            }
            const uint8_t before = EspMainMenuSkin_getTheme();
            const uint8_t after = (uint8_t)(item - 1);
            if (after != before) {
                if (!EspNativeConfig_saveTheme(after)) {
                    printf("[OPTIONSTHEME] ROLLBACK reason=sd-save requested=%u retained=%u\n",
                           (unsigned)after, (unsigned)before);
                    return;
                }
                if (!EspMainMenuSkin_setTheme(after)) {
                    const int restored = EspNativeConfig_saveTheme(before);
                    printf("[OPTIONSTHEME] ROLLBACK reason=palette restored=%d\n",
                           restored);
                    return;
                }
            }
            if (!paintVideoState(VIDEO_BACK_ITEM, 0)) {
                if (after != before) {
                    (void)EspMainMenuSkin_setTheme(before);
                    const int restored = EspNativeConfig_saveTheme(before);
                    printf("[OPTIONSTHEME] ROLLBACK reason=video-repaint restored=%d\n",
                           restored);
                }
                optionsBackActive = 0;
                PlatformInput_setTapCallback(NULL);
                DoomRPG_esp32MainMenuRecover(optionsDoomRpg,
                                             "theme-autoback-failed");
                return;
            }
            themeOpen = 0U;
            armedItem = -1;
            PlatformInput_requireFreshTapAfterRelease();
#if DOOMRPG_ESP32_TOUCH_HITBOX_OVERLAY
            registerOptionsHitboxOverlay();
#endif
            printf("[OPTIONSTHEME] SELECT %u->%u changed=%s confirmation=two-tap autoBack=VIDEO persistence=sd-v3\n",
                   (unsigned)before, (unsigned)after,
                   before == after ? "no" : "yes");
            return;
        }
        if (armedItem != -1) {
            if (!paintThemeState()) {
                optionsBackActive = 0;
                PlatformInput_setTapCallback(NULL);
                DoomRPG_esp32MainMenuRecover(optionsDoomRpg, "theme-disarm-failed");
                return;
            }
            armedItem = -1;
        }
        printf("[OPTIONSTHEME] MISS outside-choices no-action\n");
        return;
    }

    /* VIDEO has its own four spacious touch zones. This is still the fixed
     * MENU_MAIN_OPTIONS model: only the native presentation layer changes. */
    if (videoOpen) {
        if (item < VIDEO_BACK_ITEM || item > VIDEO_THEME_ITEM ||
            (item == VIDEO_BRIGHTNESS_ITEM && !Esp32Backlight_isReady()) ||
            (item == VIDEO_ORIENTATION_ITEM && !Esp32Orientation_isReady())) {
            if (armedItem != -1) {
                if (!paintVideoState(VIDEO_BACK_ITEM, 0)) {
                    optionsBackActive = 0;
                    PlatformInput_setTapCallback(NULL);
                    DoomRPG_esp32MainMenuRecover(optionsDoomRpg, "video-disarm-failed");
                    return;
                }
                armedItem = -1;
            }
            printf("[OPTIONSVIDEO] DEFER item=%d disabled-or-outside\n", item);
            return;
        }
        if (armedItem != item) {
            if (!paintVideoState(item, 1)) {
                optionsBackActive = 0;
                PlatformInput_setTapCallback(NULL);
                DoomRPG_esp32MainMenuRecover(optionsDoomRpg, "video-arm-failed");
                return;
            }
            armedItem = item;
            printf("[OPTIONSVIDEO] ARM item=%d awaitingReleasedSecondTap=yes\n", item);
            return;
        }
        armedItem = -1;
        if (item == VIDEO_BACK_ITEM) {
            if (!paintOptionsState(OPTIONS_BACK_ITEM, 0)) {
                optionsBackActive = 0;
                PlatformInput_setTapCallback(NULL);
                DoomRPG_esp32MainMenuRecover(optionsDoomRpg, "video-back-failed");
                return;
            }
            videoOpen = 0U;
            printf("[OPTIONSVIDEO] BACK to=Options state=ui-only\n");
        } else if (item == VIDEO_BRIGHTNESS_ITEM) {
            if (!paintLuminositeState()) {
                optionsBackActive = 0;
                PlatformInput_setTapCallback(NULL);
                DoomRPG_esp32MainMenuRecover(optionsDoomRpg, "luminosite-open-failed");
                return;
            }
            luminositeOpen = 1U;
            printf("[OPTIONSBRIGHTNESS] OPEN from=VIDEO percent=%u presets=25/50/75/100 confirmTap=2 autoBack=VIDEO\n",
                   (unsigned)Esp32Backlight_percent());
        } else if (item == VIDEO_ORIENTATION_ITEM) {
            if (!paintOrientationState()) {
                optionsBackActive = 0;
                PlatformInput_setTapCallback(NULL);
                DoomRPG_esp32MainMenuRecover(optionsDoomRpg, "orientation-open-failed");
                return;
            }
            orientationOpen = 1U;
            printf("[OPTIONSORIENTATION] OPEN from=VIDEO mode=%s choices=NORMAL/INVERSE confirmTap=2 autoBack=VIDEO\n",
                   Esp32Orientation_get() ? "INVERSE" : "NORMAL");
        } else if (item == VIDEO_THEME_ITEM) {
            if (!paintThemeState()) {
                optionsBackActive = 0;
                PlatformInput_setTapCallback(NULL);
                DoomRPG_esp32MainMenuRecover(optionsDoomRpg, "theme-open-failed");
                return;
            }
            themeOpen = 1U;
            printf("[OPTIONSTHEME] OPEN from=VIDEO theme=%u choices=CLASSIC/INFERNO/PHOSPHORE confirmTap=2 autoBack=VIDEO\n",
                   (unsigned)EspMainMenuSkin_getTheme());
        }
#if DOOMRPG_ESP32_TOUCH_HITBOX_OVERLAY
        registerOptionsHitboxOverlay();
#endif
        return;
    }

    /* OPTIONS root has only BACK/VIDEO. SOUND is intentionally deferred. */
    if (item != OPTIONS_BACK_ITEM && item != OPTIONS_VIDEO_ITEM) {
        if (armedItem != -1) {
            if (!paintOptionsState(OPTIONS_BACK_ITEM, 0)) {
                optionsBackActive = 0;
                PlatformInput_setTapCallback(NULL);
                DoomRPG_esp32MainMenuRecover(optionsDoomRpg, "options-disarm-failed");
                return;
            }
            armedItem = -1;
        }
        printf("[OPTIONBACK] DEFER item=%d action=disabled-or-outside\n", item);
        return;
    }
    if (armedItem != item) {
        if (!paintOptionsState(item, 1)) {
            optionsBackActive = 0;
            PlatformInput_setTapCallback(NULL);
            DoomRPG_esp32MainMenuRecover(optionsDoomRpg, "options-arm-failed");
            return;
        }
        armedItem = item;
        printf("[OPTIONBACK] ARM item=%d awaitingReleasedSecondTap=yes\n", item);
        return;
    }
    armedItem = -1;
    if (item == OPTIONS_VIDEO_ITEM) {
        if (!paintVideoState(VIDEO_BACK_ITEM, 0)) {
            optionsBackActive = 0;
            PlatformInput_setTapCallback(NULL);
            DoomRPG_esp32MainMenuRecover(optionsDoomRpg, "video-open-failed");
            return;
        }
        videoOpen = 1U;
#if DOOMRPG_ESP32_TOUCH_HITBOX_OVERLAY
        registerOptionsHitboxOverlay();
#endif
        printf("[OPTIONSVIDEO] OPEN from=Options cards=BACK/LUMINOSITE/ORIENTATION/THEME\n");
        return;
    }
    printf("[OPTIONBACK] CONFIRM Back action=native-semantic-return\n");
    optionsBackActive = 0;
    PlatformInput_setTapCallback(NULL);
    if (!returnToMainMenu(optionsDoomRpg)) {
        printf("[OPTIONBACK] FAILED executing fast Back roundtrip\n");
        DoomRPG_esp32MainMenuRecover(optionsDoomRpg, "options-back-failed");
    }
}

int DoomRPG_esp32OptionsBackActivate(struct DoomRPG_s* doomRpgBase,
                                     uint32_t optionsFramebufferFNV) {
    DoomRPG_t* doomRpg = (DoomRPG_t*)doomRpgBase;

    optionsDoomRpg = NULL;
    optionsBackActive = 0;
    armedItem = -1;
    videoOpen = 0U;
    luminositeOpen = 0U;
    orientationOpen = 0U;
    themeOpen = 0U;
    optionsTapCount = 0;
    optionsExpectedFrameFNV = 0U;

    if (!DoomRPG_esp32MainMenuGraphicsBoundaryIsSafe(doomRpg) ||
        EspNativeRootOwners_live()->menuSystem->menu != MENU_MAIN_OPTIONS ||
        EspNativeRootOwners_live()->menuSystem->selectedIndex != 0 ||
        optionsFramebufferFNV == 0U ||
        DoomRPG_esp32MainMenuFramebufferHash(EspNativeRootOwners_live()->render) != optionsFramebufferFNV) {
        printf("[OPTIONBACK] FAILED activate safe=%d menu=%d selected=%d supplied=%08x framebuffer=%08x\n",
               DoomRPG_esp32MainMenuGraphicsBoundaryIsSafe(doomRpg),
               doomRpg != NULL && EspNativeRootOwners_live()->menuSystem != NULL
                   ? EspNativeRootOwners_live()->menuSystem->menu : -999,
               doomRpg != NULL && EspNativeRootOwners_live()->menuSystem != NULL
                   ? EspNativeRootOwners_live()->menuSystem->selectedIndex : -999,
               (unsigned int)optionsFramebufferFNV,
               doomRpg != NULL && EspNativeRootOwners_live()->render != NULL
                   ? (unsigned int)DoomRPG_esp32MainMenuFramebufferHash(EspNativeRootOwners_live()->render) : 0U);
        return 0;
    }

    optionsDoomRpg = doomRpg;
    optionsBackActive = 1;
    optionsExpectedFrameFNV = optionsFramebufferFNV;
#if DOOMRPG_ESP32_TOUCH_HITBOX_OVERLAY
    registerOptionsHitboxOverlay();
#endif
    PlatformInput_setTapCallback(optionsBackTap);

    {
        int left, top, right, bottom;
        DoomRPG_esp32OptionsCardRect(OPTIONS_BACK_ITEM,
                                     &left, &top, &right, &bottom);
        printf("[OPTIONBACK] READY Back card logical=x%d..%d y%d..%d physical=x%d..%d y%d..%d firstTap=red-arm secondReleasedTap=confirm VIDEO=intermediate-4-cards SOUND=deferred fastOpaqueReturn=yes\n",
               left, right, top, bottom,
               left * DOOMRPG_INTEGER_SCALE,
               ((right + 1) * DOOMRPG_INTEGER_SCALE) - 1,
               top * DOOMRPG_INTEGER_SCALE,
               ((bottom + 1) * DOOMRPG_INTEGER_SCALE) - 1);
    }
    return 1;
}
