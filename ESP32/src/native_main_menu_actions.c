#include <SDL.h>
#include <stdint.h>
#include <stdio.h>

#include "DoomRPG.h"
#include "DoomCanvas.h"
#include "Menu.h"
#include "esp_native_menu_state.h"
#include "Render.h"
#include "esp_native_audio_intent.h"
#include "esp_native_canvas_text.h"
#include "esp_main_menu_skin.h"
#include "platform_video_c_bridge.h"

#include "native_main_menu_actions.h"
#include "native_main_menu_load_action.h"
#include "native_main_menu_model.h"
#include "native_main_menu_options_action.h"
#include "native_main_menu_present.h"
#include "native_main_menu_options_back.h"
#include "native_main_menu_start_action.h"
#include "native_main_menu_touch.h"
#include "native_main_menu_touch_layout.h"
#include "native_sprite_lru_cache.h"
#include "native_wall_lru_cache.h"
#include "platform_touch_events.h"
#include "platform_video_config.h"

#define MAIN_HELP_LINE_HEIGHT 12
#define MAIN_HELP_VISIBLE_LINES 7
#define MAIN_HELP_FOOTER_TOP 100
#define MAIN_HELP_BACK_LEFT 1
#define MAIN_HELP_BACK_RIGHT 51
#define MAIN_HELP_UP_LEFT 54
#define MAIN_HELP_UP_RIGHT 104
#define MAIN_HELP_DOWN_LEFT 107
#define MAIN_HELP_DOWN_RIGHT 158
#define MAIN_HELP_BUTTON_BOTTOM 119
/* c.bmp is the original scrolling starfield (192 virtual pixels wide).
 * Draw once per resulting logical pixel of motion, about 5Hz, not 60 FPS. */
#define MAIN_HELP_STAR_PERIOD_MS 157U
#define MAIN_HELP_STAR_VIRTUAL_WIDTH 192U

static DoomRPG_t* helpDoomRpg;
static uint32_t helpExpectedFrameFNV;
static Image_t helpSpaceBG;
static DoomRPG_t* helpSpaceOwner;
static uint32_t helpSpaceEpoch;
static int helpSpaceShift;
static int helpSpaceActive;
static int helpLoggedScroll = -1;

static void helpStop(void) {
    if (helpSpaceOwner != NULL) {
        DoomRPG_freeImage(helpSpaceOwner, &helpSpaceBG);
    }
    SDL_memset(&helpSpaceBG, 0, sizeof(helpSpaceBG));
    helpSpaceOwner = NULL;
    helpSpaceEpoch = 0U;
    helpSpaceShift = 0;
    helpSpaceActive = 0;
    helpLoggedScroll = -1;
}

static int helpStart(DoomRPG_t* doomRpg) {
    if (doomRpg == NULL || helpSpaceOwner != NULL || helpSpaceActive)
        return 0;
    /* Uses the same native PAK-backed c.bmp as ST_INTRO, but does not
     * enter ST_INTRO or own any intro story/animation state. */
    helpSpaceOwner = doomRpg;
    DoomRPG_createImage(doomRpg, "c.bmp", false, &helpSpaceBG);
    if (helpSpaceBG.imgBitmap == NULL) {
        printf("[MAINHELP] FAILED c.bmp backing image allocation/load\n");
        helpStop();
        return 0;
    }
    helpSpaceEpoch = DoomRPG_GetUpTimeMS();
    helpSpaceShift = -1;
    helpLoggedScroll = -1;
    helpSpaceActive = 1;
    printf("[MAINHELP] BACKGROUND asset=c.bmp source=pak scroll=intro-157ms/192virtual owner=help\n");
    return 1;
}

static int helpDrawStarfield(DoomRPG_t* doomRpg) {
    uint16_t* framebuffer = (uint16_t*)Esp32PlatformVideo_framebuffer();
    const size_t bytes = Esp32PlatformVideo_framebufferSizeBytes();
    const uint32_t elapsed = DoomRPG_GetUpTimeMS() - helpSpaceEpoch;
    const int phase = (int)((elapsed / MAIN_HELP_STAR_PERIOD_MS) %
                            MAIN_HELP_STAR_VIRTUAL_WIDTH);
    const int shift = (phase * DOOMRPG_LOGICAL_WIDTH) /
                      (int)MAIN_HELP_STAR_VIRTUAL_WIDTH;
    SDL_Rect primary = {-shift, 0, DOOMRPG_LOGICAL_WIDTH,
                        DOOMRPG_LOGICAL_HEIGHT};
    SDL_Rect wrap = {DOOMRPG_LOGICAL_WIDTH - shift, 0,
                     DOOMRPG_LOGICAL_WIDTH, DOOMRPG_LOGICAL_HEIGHT};
    uint16_t accent = EspMainMenuSkin_uiAccent();
    uint16_t dimAccent = EspMainMenuSkin_uiAccentDim();
    int x, y;

    if (!helpSpaceActive || helpSpaceOwner != doomRpg ||
        helpSpaceBG.imgBitmap == NULL || framebuffer == NULL ||
        bytes != (size_t)DOOMRPG_LOGICAL_WIDTH *
                     DOOMRPG_LOGICAL_HEIGHT * sizeof(uint16_t))
        return 0;

    if (doomRpg->graphSetCliping) DoomRPG_setClipFalse(doomRpg);
    if (SDL_RenderCopy(NULL, helpSpaceBG.imgBitmap, NULL, &primary) != 0 ||
        (shift > 0 && SDL_RenderCopy(NULL, helpSpaceBG.imgBitmap,
                                     NULL, &wrap) != 0))
        return 0;

    /* Translucent dark reading window: edit the ONE shared RGB565 frame,
     * no alpha texture/overlay allocation and no extra framebuffer. */
    for (y = 13; y <= 98; ++y) {
        for (x = 5; x <= 154; ++x) {
            uint16_t* pixel = &framebuffer[y * DOOMRPG_LOGICAL_WIDTH + x];
            *pixel = (uint16_t)((*pixel >> 1) & 0x7befU);
        }
        framebuffer[y * DOOMRPG_LOGICAL_WIDTH + 5] = dimAccent;
        framebuffer[y * DOOMRPG_LOGICAL_WIDTH + 154] = dimAccent;
    }
    for (x = 5; x <= 154; ++x) {
        framebuffer[12 * DOOMRPG_LOGICAL_WIDTH + x] = accent;
        framebuffer[99 * DOOMRPG_LOGICAL_WIDTH + x] = dimAccent;
    }
    helpSpaceShift = shift;
    return 1;
}

static uint32_t helpColor888(uint16_t ink) {
    uint32_t r = ((uint32_t)((ink >> 11) & 31U) * 255U) / 31U;
    uint32_t g = ((uint32_t)((ink >> 5) & 63U) * 255U) / 63U;
    uint32_t b = ((uint32_t)(ink & 31U) * 255U) / 31U;
    return 0xff000000U | (r << 16) | (g << 8) | b;
}

/* Nine-row native menu lettering is our theme reference. The existing
 * 9x12 game bitmap is reused as-is: three 4-row clipped passes provide
 * the same three-tone CLASSIC/INFERNO/PHOSPHORE gradient, without a font
 * resample, heap, a new atlas, or touching global font/text primitives.
 * Caller owns a full-frame presentation and no active clipping region. */
static void helpDrawThemedPageTitle(DoomRPG_t* doomRpg,
                                    DoomCanvas_t* canvas,
                                    const char* title, int x, int y) {
    int band;
    if (title == NULL || canvas == NULL) return;
    for (band = 0; band < 3; ++band) {
        const uint16_t ink = EspMainMenuSkin_letterTone(band * 3 + 1, 9);
        DoomRPG_setClipTrue(doomRpg, x, y + band * 4,
                            DOOMRPG_LOGICAL_WIDTH - x, 4);
        DoomRPG_setFontColor(doomRpg, helpColor888(ink));
        EspNativeCanvas_drawText(canvas, title, x, y, 0);
    }
    DoomRPG_setClipFalse(doomRpg);
}

/* Small © badge drawn in the existing RGB565 frame at (12,27).
 * The original a.bmp bitmap font doesn't contain a copyright glyph.
 * This fixed 11x9 ROM mask places the C inside the outer circle. */
static void helpPaintCopyrightMark(uint16_t* fb) {
    static const uint16_t rows[9] = {
        0x0f8U, 0x104U, 0x272U, 0x441U, 0x441U,
        0x441U, 0x272U, 0x104U, 0x0f8U
    };
    const uint16_t ink = EspMainMenuSkin_uiInk();
    int y, x;
    if (fb == NULL) return;
    for (y = 0; y < 9; ++y) {
        for (x = 0; x < 11; ++x) {
            if ((rows[y] & (uint16_t)(0x400U >> x)) != 0U) {
                fb[(27 + y) * DOOMRPG_LOGICAL_WIDTH + 12 + x] = ink;
            }
        }
    }
}

int DoomRPG_esp32MainMenuReturnToMain(struct DoomRPG_s* doomRpgBase,
                                      int expectedChildMenu,
                                      const char* source,
                                      uint32_t* finalFramebufferFNV) {
    DoomRPG_t* doomRpg = (DoomRPG_t*)doomRpgBase;
    EspNativeMenuState_t* menuSystem;
    uint32_t finalFNV = 0U;
    uint32_t startMs;
    uint32_t elapsedMs;

    if (finalFramebufferFNV != NULL) *finalFramebufferFNV = 0U;

    if (doomRpg == NULL || doomRpg->menuSystem == NULL ||
        doomRpg->doomCanvas == NULL ||
        (expectedChildMenu != MENU_MAIN_OPTIONS &&
         expectedChildMenu != MENU_MAIN_HELP_ABOUT)) {
        printf("[MAINBACK] FAILED source=%s child=%d object/model contract\n",
               source != NULL ? source : "unknown",
               expectedChildMenu);
        return 0;
    }

    menuSystem = doomRpg->menuSystem;
    printf("\n=== Doom RPG ESP32 native child -> MENU_MAIN Back ===\n");
    printf("[MAINBACK] BEGIN source=%s child=%d menu=%d old=%d selected=%d state=%d frame=%08x\n",
           source != NULL ? source : "unknown",
           expectedChildMenu,
           menuSystem->menu,
           menuSystem->oldMenu,
           menuSystem->selectedIndex,
           doomRpg->doomCanvas->state,
           (unsigned int)DoomRPG_esp32MainMenuFramebufferHash(doomRpg->render));

    if (!DoomRPG_esp32MainMenuGraphicsBoundaryIsSafe(doomRpg) ||
        doomRpg->doomCanvas->state != ST_MENU ||
        menuSystem->menu != expectedChildMenu ||
        menuSystem->oldMenu != MENU_MAIN) {
        printf("[MAINBACK] FAILED source=%s precondition safe=%d state=%d menu=%d expectedChild=%d old=%d\n",
               source != NULL ? source : "unknown",
               DoomRPG_esp32MainMenuGraphicsBoundaryIsSafe(doomRpg),
               doomRpg->doomCanvas->state,
               menuSystem->menu,
               expectedChildMenu,
               menuSystem->oldMenu);
        return 0;
    }

    startMs = (uint32_t)DoomRPG_GetTimeMS();

    /* Exact legacy Back cue, without importing MenuSystem_back() or its generic
     * MenuSystem_setMenu(oldMenu) hierarchy router.
     */
    (void)EspNativeAudioIntent_publish(5042U, 0U, 3U);

    if (!DoomRPG_esp32MainMenuModelEnter(doomRpg, MENU_MAIN) ||
        !DoomRPG_esp32RepaintOpaqueMainMenu(doomRpg, &finalFNV)) {
        printf("[MAINBACK] FAILED source=%s native model/repaint\n",
               source != NULL ? source : "unknown");
        return 0;
    }

    elapsedMs = (uint32_t)DoomRPG_GetTimeMS() - startMs;

    if (menuSystem->menu != MENU_MAIN ||
        menuSystem->selectedIndex != 0 ||
        menuSystem->numItems != 4 ||
        finalFNV == 0U ||
        finalFNV != DoomRPG_esp32MainMenuSelectionFramebufferFNV(0) ||
        finalFNV != DoomRPG_esp32MainMenuFramebufferHash(doomRpg->render) ||
        !DoomRPG_esp32MainMenuTouchIsActive() ||
        !DoomRPG_esp32MainMenuGraphicsBoundaryIsSafe(doomRpg)) {
        printf("[MAINBACK] FAILED source=%s final invariant menu=%d selected=%d items=%d frame=%08x expected=%08x touch=%d\n",
               source != NULL ? source : "unknown",
               menuSystem->menu,
               menuSystem->selectedIndex,
               menuSystem->numItems,
               (unsigned int)finalFNV,
               (unsigned int)DoomRPG_esp32MainMenuSelectionFramebufferFNV(0),
               DoomRPG_esp32MainMenuTouchIsActive());
        return 0;
    }

    if (finalFramebufferFNV != NULL) *finalFramebufferFNV = finalFNV;
    printf("[MAINBACK] READY source=%s child=%d->%d frame=%08x touch=rearmed elapsedMs=%u sound=5042 router=native noMenuSystemBack=yes noSetMenu=yes\n",
           source != NULL ? source : "unknown",
           expectedChildMenu,
           menuSystem->menu,
           (unsigned int)finalFNV,
           (unsigned int)elapsedMs);
    return 1;
}

static int paintHelp(DoomRPG_t* doomRpg, uint32_t* outFrameFNV) {
    DoomCanvas_t* canvas;
    EspNativeMenuState_t* menuSystem;
    uint16_t* framebuffer;
    int visible, maxScroll, end, i, y;
    char page[12];
    uint32_t frameFNV;

    if (outFrameFNV != NULL) *outFrameFNV = 0U;
    if (doomRpg == NULL || doomRpg->doomCanvas == NULL ||
        doomRpg->menuSystem == NULL || !helpSpaceActive ||
        !DoomRPG_esp32MainMenuGraphicsBoundaryIsSafe(doomRpg))
        return 0;
    canvas = doomRpg->doomCanvas;
    menuSystem = doomRpg->menuSystem;
    framebuffer = (uint16_t*)Esp32PlatformVideo_framebuffer();
    if (menuSystem->menu != MENU_MAIN_HELP_ABOUT ||
        menuSystem->type != 5 || menuSystem->oldMenu != MENU_MAIN ||
        menuSystem->numItems <= 0 ||
        menuSystem->numItems != DoomRPG_esp32MainMenuHelpLineCount() ||
        framebuffer == NULL)
        return 0;

    menuSystem->maxItems = MAIN_HELP_VISIBLE_LINES;
    maxScroll = menuSystem->numItems > menuSystem->maxItems
                    ? menuSystem->numItems - menuSystem->maxItems : 0;
    if (menuSystem->scrollIndex < 0) menuSystem->scrollIndex = 0;
    if (menuSystem->scrollIndex > maxScroll)
        menuSystem->scrollIndex = maxScroll;
    menuSystem->selectedIndex = menuSystem->scrollIndex;
    visible = menuSystem->numItems - menuSystem->scrollIndex;
    if (visible > menuSystem->maxItems) visible = menuSystem->maxItems;
    end = menuSystem->scrollIndex + visible;

    if (!helpDrawStarfield(doomRpg)) return 0;
    DoomRPG_setFontColor(doomRpg, helpColor888(EspMainMenuSkin_uiInk()));
    EspNativeCanvas_drawText(canvas, "HELP / CYD", 12, 0, 0);
    SDL_snprintf(page, sizeof(page), "%d/%d",
                 (menuSystem->scrollIndex + MAIN_HELP_VISIBLE_LINES - 1) /
                     MAIN_HELP_VISIBLE_LINES + 1,
                 (menuSystem->numItems + MAIN_HELP_VISIBLE_LINES - 1) /
                     MAIN_HELP_VISIBLE_LINES);
    EspNativeCanvas_drawText(canvas, page, 121, 0, 0);

    DoomRPG_setFontColor(doomRpg, 0xffffffffU);
    y = 14;
    for (i = menuSystem->scrollIndex; i < end; ++i) {
        const char* line = DoomRPG_esp32MainMenuHelpLine(i);
        if (line == NULL) return 0;
        if (i % MAIN_HELP_VISIBLE_LINES == 0) {
            /* First visible row is the section title on every fixed page. */
            helpDrawThemedPageTitle(doomRpg, canvas, line, 12, y);
        }
        else if (i == 1 && menuSystem->scrollIndex == 0) {
            /* Signature only on the cover, directly below its title. */
            DoomRPG_setFontColor(doomRpg,
                helpColor888(EspMainMenuSkin_uiInk()));
            EspNativeCanvas_drawText(canvas, line, 27, y, 0);
            helpPaintCopyrightMark(framebuffer);
            DoomRPG_setFontColor(doomRpg, 0xffffffffU);
        }
        else if (line[0] != '\0') {
            DoomRPG_setFontColor(doomRpg, 0xffffffffU);
            EspNativeCanvas_drawText(canvas, line, 12, y, 0);
        }
        y += MAIN_HELP_LINE_HEIGHT;
    }
    /* Exactly the finger hitboxes below: each chassis stays 100..119.
     * Shared painter gives CLASSIC/INFERNO/PHOSPHORE the same industrial
     * relief and 9-row glyph gradients as LOAD GAME. */
    if (!EspMainMenuSkin_paintPlate(framebuffer, DOOMRPG_LOGICAL_WIDTH,
            DOOMRPG_LOGICAL_WIDTH, DOOMRPG_LOGICAL_HEIGHT,
            MAIN_HELP_BACK_LEFT, MAIN_HELP_FOOTER_TOP,
            MAIN_HELP_BACK_RIGHT, MAIN_HELP_BUTTON_BOTTOM,
            ESP_MAIN_MENU_SKIN_IDLE) ||
        !EspMainMenuSkin_paintText(framebuffer, DOOMRPG_LOGICAL_WIDTH,
            DOOMRPG_LOGICAL_WIDTH, DOOMRPG_LOGICAL_HEIGHT,
            MAIN_HELP_BACK_LEFT, MAIN_HELP_FOOTER_TOP,
            MAIN_HELP_BACK_RIGHT, MAIN_HELP_BUTTON_BOTTOM,
            "BACK", 0, 1) ||
        !EspMainMenuSkin_paintPlate(framebuffer, DOOMRPG_LOGICAL_WIDTH,
            DOOMRPG_LOGICAL_WIDTH, DOOMRPG_LOGICAL_HEIGHT,
            MAIN_HELP_UP_LEFT, MAIN_HELP_FOOTER_TOP,
            MAIN_HELP_UP_RIGHT, MAIN_HELP_BUTTON_BOTTOM,
            ESP_MAIN_MENU_SKIN_IDLE) ||
        !EspMainMenuSkin_paintText(framebuffer, DOOMRPG_LOGICAL_WIDTH,
            DOOMRPG_LOGICAL_WIDTH, DOOMRPG_LOGICAL_HEIGHT,
            MAIN_HELP_UP_LEFT, MAIN_HELP_FOOTER_TOP,
            MAIN_HELP_UP_RIGHT, MAIN_HELP_BUTTON_BOTTOM,
            "UP", 0, menuSystem->scrollIndex > 0) ||
        !EspMainMenuSkin_paintPlate(framebuffer, DOOMRPG_LOGICAL_WIDTH,
            DOOMRPG_LOGICAL_WIDTH, DOOMRPG_LOGICAL_HEIGHT,
            MAIN_HELP_DOWN_LEFT, MAIN_HELP_FOOTER_TOP,
            MAIN_HELP_DOWN_RIGHT, MAIN_HELP_BUTTON_BOTTOM,
            ESP_MAIN_MENU_SKIN_IDLE) ||
        !EspMainMenuSkin_paintText(framebuffer, DOOMRPG_LOGICAL_WIDTH,
            DOOMRPG_LOGICAL_WIDTH, DOOMRPG_LOGICAL_HEIGHT,
            MAIN_HELP_DOWN_LEFT, MAIN_HELP_FOOTER_TOP,
            MAIN_HELP_DOWN_RIGHT, MAIN_HELP_BUTTON_BOTTOM,
            "DOWN", 0, menuSystem->scrollIndex < maxScroll))
        return 0;

    DoomRPG_setFontColor(doomRpg, 0xffffffffU);
    frameFNV = DoomRPG_esp32MainMenuFramebufferHash(doomRpg->render);
    if (frameFNV == 0U) return 0;
    SDL_RenderPresent(NULL);
    if (outFrameFNV != NULL) *outFrameFNV = frameFNV;
    if (helpLoggedScroll != menuSystem->scrollIndex) {
        printf("[MAINHELP] PAINT lines=%d visible=%d range=%d..%d maxScroll=%d frame=%08x background=intro-c.bmp shift=%d buttons=industrial\n",
               menuSystem->numItems, visible, menuSystem->scrollIndex,
               end > menuSystem->scrollIndex ? end - 1 : menuSystem->scrollIndex,
               maxScroll, (unsigned int)frameFNV, helpSpaceShift);
        helpLoggedScroll = menuSystem->scrollIndex;
    }
    return 1;
}

void DoomRPG_esp32MainMenuHelpService(void) {
    uint32_t now, finalFNV = 0U;
    int phase, shift;
    if (!helpSpaceActive) return;
    if (helpDoomRpg == NULL || helpDoomRpg != helpSpaceOwner ||
        helpDoomRpg->menuSystem == NULL ||
        helpDoomRpg->menuSystem->menu != MENU_MAIN_HELP_ABOUT ||
        !DoomRPG_esp32MainMenuGraphicsBoundaryIsSafe(helpDoomRpg)) {
        printf("[MAINHELP] STOP invalid animation ownership\n");
        helpStop();
        helpDoomRpg = NULL;
        helpExpectedFrameFNV = 0U;
        PlatformInput_setTapCallback(NULL);
        return;
    }
    now = DoomRPG_GetUpTimeMS();
    phase = (int)(((now - helpSpaceEpoch) / MAIN_HELP_STAR_PERIOD_MS) %
                  MAIN_HELP_STAR_VIRTUAL_WIDTH);
    shift = (phase * DOOMRPG_LOGICAL_WIDTH) /
            (int)MAIN_HELP_STAR_VIRTUAL_WIDTH;
    if (shift == helpSpaceShift) return;
    if (!paintHelp(helpDoomRpg, &finalFNV)) {
        DoomRPG_t* owner = helpDoomRpg;
        printf("[MAINHELP] FAILED animation repaint\n");
        helpStop();
        helpDoomRpg = NULL;
        helpExpectedFrameFNV = 0U;
        PlatformInput_setTapCallback(NULL);
        DoomRPG_esp32MainMenuRecover(owner, "help-animation-repaint");
        return;
    }
    helpExpectedFrameFNV = finalFNV;
}

static void helpTap(int16_t screenX,
                    int16_t screenY,
                    uint16_t pressure,
                    uint16_t rawX,
                    uint16_t rawY) {
    DoomRPG_t* doomRpg = helpDoomRpg;
    EspNativeMenuState_t* menuSystem;
    int logicalX;
    int logicalY;
    int maxScroll;
    int beforeScroll;
    uint32_t before;
    uint32_t finalFNV = 0U;

    (void)pressure;

    if (doomRpg == NULL || doomRpg->menuSystem == NULL) return;
    menuSystem = doomRpg->menuSystem;
    before = DoomRPG_esp32MainMenuFramebufferHash(doomRpg->render);
    logicalX = screenX / DOOMRPG_INTEGER_SCALE;
    logicalY = screenY / DOOMRPG_INTEGER_SCALE;

    if (!DoomRPG_esp32MainMenuGraphicsBoundaryIsSafe(doomRpg) ||
        menuSystem->menu != MENU_MAIN_HELP_ABOUT ||
        helpExpectedFrameFNV == 0U ||
        before != helpExpectedFrameFNV) {
        printf("[MAINHELP] FAILED touch precondition frame=%08x expected=%08x menu=%d\n",
               (unsigned int)before,
               (unsigned int)helpExpectedFrameFNV,
               menuSystem->menu);
        PlatformInput_setTapCallback(NULL);
        helpDoomRpg = NULL;
        helpExpectedFrameFNV = 0U;
        helpStop();
        DoomRPG_esp32MainMenuRecover(doomRpg, "help-touch-precondition");
        return;
    }

    printf("[MAINHELP] TAP raw=%u,%u physical=%d,%d logical=%d,%d scroll=%d frame=%08x\n",
           rawX,
           rawY,
           screenX,
           screenY,
           logicalX,
           logicalY,
           menuSystem->scrollIndex,
           (unsigned int)before);

    if (logicalY < MAIN_HELP_FOOTER_TOP ||
        logicalY > MAIN_HELP_BUTTON_BOTTOM) {
        printf("[MAINHELP] CONTENT tap=no-action footerTop=%d\n",
               MAIN_HELP_FOOTER_TOP);
        return;
    }

    if (logicalX >= MAIN_HELP_BACK_LEFT &&
        logicalX <= MAIN_HELP_BACK_RIGHT) {
        PlatformInput_setTapCallback(NULL);
        helpStop();

        if (!DoomRPG_esp32MainMenuReturnToMain(
                doomRpg,
                MENU_MAIN_HELP_ABOUT,
                "help",
                &finalFNV)) {
            printf("[MAINHELP] FAILED native Back to MENU_MAIN menu=%d\n",
                   menuSystem->menu);
            helpDoomRpg = NULL;
            helpExpectedFrameFNV = 0U;
            DoomRPG_esp32MainMenuRecover(doomRpg, "help-back-failed");
            return;
        }

        printf("[MAINHELP] READY back=native mainFNV=%08x touch=armed\n",
               (unsigned int)finalFNV);
        helpDoomRpg = NULL;
        helpExpectedFrameFNV = 0U;
        return;
    }

    beforeScroll = menuSystem->scrollIndex;
    maxScroll = menuSystem->numItems > MAIN_HELP_VISIBLE_LINES
                    ? menuSystem->numItems - MAIN_HELP_VISIBLE_LINES
                    : 0;

    if (logicalX >= MAIN_HELP_UP_LEFT &&
        logicalX <= MAIN_HELP_UP_RIGHT) {
        menuSystem->scrollIndex -= MAIN_HELP_VISIBLE_LINES;
        if (menuSystem->scrollIndex < 0) menuSystem->scrollIndex = 0;
        printf("[MAINHELP] PAGE-UP scroll=%d->%d\n",
               beforeScroll,
               menuSystem->scrollIndex);
    }
    else if (logicalX >= MAIN_HELP_DOWN_LEFT &&
             logicalX <= MAIN_HELP_DOWN_RIGHT) {
        menuSystem->scrollIndex += MAIN_HELP_VISIBLE_LINES;
        if (menuSystem->scrollIndex > maxScroll) {
            menuSystem->scrollIndex = maxScroll;
        }
        printf("[MAINHELP] PAGE-DOWN scroll=%d->%d max=%d\n",
               beforeScroll,
               menuSystem->scrollIndex,
               maxScroll);
    }

    else return; /* gaps between plates are deliberately not clickable */

    menuSystem->selectedIndex = menuSystem->scrollIndex;
    if (!paintHelp(doomRpg, &finalFNV)) {
        printf("[MAINHELP] FAILED repaint scroll=%d\n",
               menuSystem->scrollIndex);
        PlatformInput_setTapCallback(NULL);
        helpDoomRpg = NULL;
        helpExpectedFrameFNV = 0U;
        helpStop();
        DoomRPG_esp32MainMenuRecover(doomRpg, "help-repaint-failed");
        return;
    }

    helpExpectedFrameFNV = finalFNV;
    printf("[MAINHELP] READY scroll=%d frame=%08x input=footer-back+page-up+page-down\n",
           menuSystem->scrollIndex,
           (unsigned int)finalFNV);
}

static int activateHelp(DoomRPG_t* doomRpg) {
    EspNativeMenuState_t* menuSystem;
    uint32_t inputFNV;
    uint32_t expectedFNV;
    uint32_t helpFNV = 0U;

    if (!DoomRPG_esp32MainMenuGraphicsBoundaryIsSafe(doomRpg)) {
        printf("[MAINHELP] FAILED graphics boundary\n");
        return 0;
    }

    menuSystem = doomRpg->menuSystem;
    inputFNV = DoomRPG_esp32MainMenuFramebufferHash(doomRpg->render);
    expectedFNV = DoomRPG_esp32MainMenuSelectionFramebufferFNV(
        DOOMRPG_ESP32_MAIN_MENU_ACTION_HELP);

    if (menuSystem->menu != MENU_MAIN ||
        menuSystem->selectedIndex != DOOMRPG_ESP32_MAIN_MENU_ACTION_HELP ||
        expectedFNV == 0U || inputFNV != expectedFNV) {
        printf("[MAINHELP] FAILED precondition menu=%d selected=%d frame=%08x expected=%08x\n",
               menuSystem->menu,
               menuSystem->selectedIndex,
               (unsigned int)inputFNV,
               (unsigned int)expectedFNV);
        return 0;
    }

    (void)EspNativeAudioIntent_publish(5046U, 0U, 3U);
    if (!DoomRPG_esp32MainMenuModelEnter(
            doomRpg, MENU_MAIN_HELP_ABOUT) ||
        !helpStart(doomRpg) || !paintHelp(doomRpg, &helpFNV)) {
        printf("[MAINHELP] FAILED model/presentation\n");
        helpStop();
        return 0;
    }

    helpDoomRpg = doomRpg;
    helpExpectedFrameFNV = helpFNV;
    PlatformInput_setTapCallback(helpTap);

    printf("[MAINHELP] READY menu=%d type=%d old=%d frame=%08x input=footer-back+page-up+page-down backOwner=native\n",
           menuSystem->menu,
           menuSystem->type,
           menuSystem->oldMenu,
           (unsigned int)helpFNV);
    return 1;
}

DoomRpgEsp32MainMenuDispatchResult
DoomRPG_esp32MainMenuDispatchConfirmed(struct DoomRPG_s* doomRpgBase,
                                       int action) {
    DoomRPG_t* doomRpg = (DoomRPG_t*)doomRpgBase;
    uint32_t optionsFNV = 0U;

    if (doomRpg == NULL) {
        return DOOMRPG_ESP32_MAIN_MENU_DISPATCH_FAILED;
    }

    printf("[MAINACTION] DISPATCH action=%d menu=%d selected=%d\n",
           action,
           doomRpg->menuSystem != NULL ? doomRpg->menuSystem->menu : -999,
           doomRpg->menuSystem != NULL
               ? doomRpg->menuSystem->selectedIndex : -999);

    switch (action) {
        case DOOMRPG_ESP32_MAIN_MENU_ACTION_START:
            return DoomRPG_esp32ActivateMainMenuStart(doomRpg)
                       ? DOOMRPG_ESP32_MAIN_MENU_DISPATCH_TRANSITIONED
                       : DOOMRPG_ESP32_MAIN_MENU_DISPATCH_FAILED;

        case DOOMRPG_ESP32_MAIN_MENU_ACTION_LOAD: {
            DoomRpgEsp32MainMenuLoadResult loadResult =
                DoomRPG_esp32ActivateMainMenuLoad(doomRpg);

            printf("[MAINACTION] LOAD result=%d\n", (int)loadResult);
            if (loadResult == DOOMRPG_ESP32_MAIN_MENU_LOAD_TRANSITIONED) {
                return DOOMRPG_ESP32_MAIN_MENU_DISPATCH_TRANSITIONED;
            }
            if (loadResult == DOOMRPG_ESP32_MAIN_MENU_LOAD_NO_SAVE ||
                loadResult == DOOMRPG_ESP32_MAIN_MENU_LOAD_RECOVERED) {
                return DOOMRPG_ESP32_MAIN_MENU_DISPATCH_STAY_MAIN;
            }
            return DOOMRPG_ESP32_MAIN_MENU_DISPATCH_FAILED;
        }

        case DOOMRPG_ESP32_MAIN_MENU_ACTION_OPTIONS:
            if (!DoomRPG_esp32ActivateMainMenuOptions(
                    doomRpg, &optionsFNV) ||
                !DoomRPG_esp32OptionsBackActivate(
                    doomRpg, optionsFNV)) {
                return DOOMRPG_ESP32_MAIN_MENU_DISPATCH_FAILED;
            }
            return DOOMRPG_ESP32_MAIN_MENU_DISPATCH_TRANSITIONED;

        case DOOMRPG_ESP32_MAIN_MENU_ACTION_HELP:
            return activateHelp(doomRpg)
                       ? DOOMRPG_ESP32_MAIN_MENU_DISPATCH_TRANSITIONED
                       : DOOMRPG_ESP32_MAIN_MENU_DISPATCH_FAILED;

        default:
            printf("[MAINACTION] FAILED unsupported action=%d mutation=no\n",
                   action);
            return DOOMRPG_ESP32_MAIN_MENU_DISPATCH_FAILED;
    }
}


/* Consolidated main-menu LOAD action. Keep action ownership in one TU. */
#include <SDL.h>
#include <stdint.h>
#include <stdio.h>

#include "DoomRPG.h"
#include "DoomCanvas.h"
#include "Menu.h"
#include "MenuSystem.h"
#include "Render.h"

#include "esp_asset_pack.h"
#include "esp_native_gameplay_save_ui.h"
#include "esp_native_transition_presentation.h"
#include "native_main_menu_160x120_layout.h"
#include "native_main_menu_load_action.h"
#include "native_main_menu_present.h"
#include "native_main_menu_start_action.h"
#include "native_main_menu_touch.h"
#include "native_main_menu_touch_layout.h"
#include "native_sprite_lru_cache.h"
#include "native_wall_lru_cache.h"
#include "platform_video_config.h"

#define MAIN_LOAD_ITEM_INDEX 1
#define MAIN_LOAD_GLYPH_ADVANCE 7

static char noSaveLabel[] = "NO SAVE";

static void showNoSaveFeedback(DoomRPG_t* doomRpg) {
    DoomCanvas_t* canvas = doomRpg->doomCanvas;
    int left, top, right, bottom;
    /* LOAD is item 1 of the vertical rail, not column 1/row 0.
     * Confine NO SAVE to its new 63x20 hitbox and rebase the frame hash. */
    DoomRPG_esp32MainMenuCardRect(MAIN_LOAD_ITEM_INDEX,
                                 &left, &top, &right, &bottom);
    const int width = (int)(sizeof(noSaveLabel) - 1U) *
                      MAIN_LOAD_GLYPH_ADVANCE;
    const int x = ((left + right + 1) >> 1) - (width >> 1);
    const int y = top + (((bottom - top + 1) -
                          DOOMRPG_ESP32_MAIN_MENU_FONT_HEIGHT) >> 1);
    uint32_t frameFNV;

    DoomRPG_setColor(doomRpg, 0x000000);
    DoomRPG_fillRect(doomRpg,
                     left + 5,
                     top + 4,
                     right - left - 9,
                     bottom - top - 7);
    DoomRPG_setFontColor(doomRpg, 0xffff0000);
    EspNativeCanvas_drawText(canvas, noSaveLabel, x, y, 0);
    DoomRPG_setFontColor(doomRpg, 0xffffffff);
    SDL_RenderPresent(NULL);

    frameFNV = DoomRPG_esp32MainMenuFramebufferHash(doomRpg->render);
    DoomRPG_esp32MainMenuTouchRebaseFrame(MAIN_LOAD_ITEM_INDEX, frameFNV);
    printf("[MAINLOAD] FEEDBACK text=\"NO SAVE\" card=LOAD color=red framebufferFNV=%08x\n",
           (unsigned int)frameFNV);
}

DoomRpgEsp32MainMenuLoadResult
DoomRPG_esp32ActivateMainMenuLoad(struct DoomRPG_s* doomRpgBase) {
    DoomRPG_t* doomRpg = (DoomRPG_t*)doomRpgBase;
    uint32_t inputHash;
    uint32_t expectedHash;

    printf("\n=== Doom RPG ESP32 MENU_MAIN -> Load Game ===\n");

    /* MENU_MAIN load must never inherit an in-level presentation heartbeat.
     * Keep its hardware-validated map-flash path synchronous/sequence-only. */
    EspAssetPack_mapFlashSetProgressCallback(NULL);
    EspNativeTransitionPresentation_reset();
    printf("[MAINLOAD] TRANSITION-UI reset=yes mapFlashProgress=off\n");

    if (!EspNativeGameplaySave_mainSelectorReady() &&
        !DoomRPG_esp32MainMenuGraphicsBoundaryIsSafe(doomRpg)) {
        printf("[MAINLOAD] FAILED core/graphics boundary unavailable\n");
        return DOOMRPG_ESP32_MAIN_MENU_LOAD_FATAL;
    }

    inputHash = DoomRPG_esp32MainMenuFramebufferHash(doomRpg->render);
    expectedHash = DoomRPG_esp32MainMenuSelectionFramebufferFNV(
        MAIN_LOAD_ITEM_INDEX);
    if (doomRpg->menuSystem->menu != MENU_MAIN ||
        doomRpg->menuSystem->selectedIndex != MAIN_LOAD_ITEM_INDEX ||
        doomRpg->doomCanvas->state != ST_MENU ||
        (!EspNativeGameplaySave_mainSelectorReady() && (expectedHash == 0U || inputHash != expectedHash))) {
        printf("[MAINLOAD] FAILED precondition menu=%d selected=%d state=%d framebuffer=%08x expected=%08x\n",
               doomRpg->menuSystem->menu,
               doomRpg->menuSystem->selectedIndex,
               doomRpg->doomCanvas->state,
               (unsigned int)inputHash,
               (unsigned int)expectedHash);
        return DOOMRPG_ESP32_MAIN_MENU_LOAD_FATAL;
    }

    if (!EspNativeGameplaySave_mainSelectorActive()) {
        (void)EspNativeGameplaySave_mainSelectorBegin();
        return DOOMRPG_ESP32_MAIN_MENU_LOAD_NO_SAVE;
    }
    if (!EspNativeGameplaySave_mainSelectorReady()) {
        return DOOMRPG_ESP32_MAIN_MENU_LOAD_NO_SAVE;
    }
    if (!EspNativeGameplaySave_hasReadableCheckpoint()) {
        printf("[MAINLOAD] NO-SAVE missing-or-invalid; MENU_MAIN remains active\n");
        showNoSaveFeedback(doomRpg);
        return DOOMRPG_ESP32_MAIN_MENU_LOAD_NO_SAVE;
    }

    if (!DoomRPG_esp32ReleaseMainMenuMemory(doomRpg)) {
        printf("[MAINLOAD] FAILED menu runtime cleanup\n");
        return DOOMRPG_ESP32_MAIN_MENU_LOAD_FATAL;
    }

    if (!EspNativeGameplaySave_loadCheckpoint()) {
        printf("[MAINLOAD] FAILED checkpoint restore; attempting MENU_MAIN recovery\n");
        EspNativeTransitionPresentation_reset();
        if (DoomRPG_esp32MainMenuRecover(doomRpg, "load-restore-failed")) {
            EspNativeGameplaySave_mainSelectorFinish();
            return DOOMRPG_ESP32_MAIN_MENU_LOAD_RECOVERED;
        }
        return DOOMRPG_ESP32_MAIN_MENU_LOAD_FATAL;
    }

    EspNativeGameplaySave_mainSelectorFinish();
    doomRpg->menuSystem->menu = MENU_NONE;
    doomRpg->menuSystem->numItems = 0;
    EspNativeMainMenu_transitionCanvasState(doomRpg->doomCanvas, ST_PLAYING);

    printf("[MAINLOAD] READY checkpoint restored; intro=skipped session=resume-pending state=%d\n",
           doomRpg->doomCanvas->state);
    return DOOMRPG_ESP32_MAIN_MENU_LOAD_TRANSITIONED;
}
