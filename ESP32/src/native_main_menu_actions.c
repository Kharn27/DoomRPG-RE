#include <SDL.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "DoomRPG.h"
#include "esp_native_root_state.h"
#include "DoomCanvas.h"
#include "Menu.h"
#include "esp_native_menu_state.h"
#include "Render.h"
#include "esp_native_audio_intent.h"
#include "esp_native_canvas_text.h"
#include "esp_main_menu_skin.h"
#include "esp_asset_pack.h"
#include "esp_native_help_scene.h"
#include "esp_native_gameplay_hud.h"
#include "esp_native_gameplay_hud_weapon_button.h"
#include "esp_native_gameplay_hub_weapon_grid.h"
#include "esp_native_gameplay_player_state.h"
#include "esp_entity_def_type_catalog.h"
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
/* Fifth guide page: an 80%-scale, centred illustration, NOT the live
 * top screen bar. Logical game hitboxes are 0..31 / 32..127 / 128..159.
 * Multiply by 4/5 to keep their 1:3:1 proportion in the miniature. */
#define MAIN_HELP_HUD_PAGE_SCROLL (4 * MAIN_HELP_VISIBLE_LINES)
#define MAIN_HELP_HUD_BAR_X 16
#define MAIN_HELP_HUD_BAR_Y 43
#define MAIN_HELP_HUD_BAR_WIDTH 128
#define MAIN_HELP_HUD_BAR_HEIGHT 16
#define MAIN_HELP_HUD_SOURCE_TILE 20
#define MAIN_HELP_HUD_MINI_TILE 16
#define MAIN_HELP_HUD_MENU_RIGHT 25
#define MAIN_HELP_HUD_PASS_LEFT 26
#define MAIN_HELP_HUD_PASS_RIGHT 102
#define MAIN_HELP_HUD_MAP_LEFT 103
/* Breath constants and easing now live in esp_native_help_scene.h. */
#define MAIN_HELP_SCENE_PAGE_SCROLL (5 * MAIN_HELP_VISIBLE_LINES)
#define MAIN_HELP_ROW_FIRST_SCROLL (6 * MAIN_HELP_VISIBLE_LINES)
#define MAIN_HELP_ROW_LAST_SCROLL (8 * MAIN_HELP_VISIBLE_LINES)
#define MAIN_HELP_BOTTOM_HUD_PAGE_SCROLL (9 * MAIN_HELP_VISIBLE_LINES)
#define MAIN_HELP_WEAPON_PAGE_SCROLL (10 * MAIN_HELP_VISIBLE_LINES)
/* Full available HELP viewport under the caption (y27..38) and above
 * the navigation footer (y100..119): 144x60, x8..151/y39..98. */
#define MAIN_HELP_WEAPON_MINI_X 8
#define MAIN_HELP_WEAPON_MINI_Y 39
#define MAIN_HELP_WEAPON_MINI_W 144
#define MAIN_HELP_WEAPON_MINI_H 60

static DoomRPG_t* helpDoomRpg;
static uint32_t helpExpectedFrameFNV;
static Image_t helpSpaceBG;
static Image_t helpHudBarBG;
static DoomRPG_t* helpSpaceOwner;
static uint32_t helpSpaceEpoch;
static int helpSpaceShift;
static int helpSpaceActive;
static int helpLoggedScroll = -1;
static int helpHudBreathStep = -1;
static int helpSceneBreathStep = -1;
/* Cached 128x16 RGB565 downsample of the *real* gameplay HUD (k/m/l/o/a).
 * One 4096-byte bounded BSS miniature, built only on first visit to page 10.
 * Never stores a second full framebuffer or a world/gameplay state owner. */
static uint16_t helpBottomHudMini[MAIN_HELP_HUD_BAR_WIDTH *
                                  MAIN_HELP_HUD_BAR_HEIGHT];
static uint8_t helpBottomHudReady;
/* The exact 4x3 native weapon-selector grid is sampled once from its PAK
 * icons, before repainting the HELP background. 144x60x2=17280 B BSS.
 * Enlarged into the former bottom-caption space, not a second framebuffer.
 * No active PlayerState is created. */
static uint16_t helpWeaponMini[MAIN_HELP_WEAPON_MINI_W *
                               MAIN_HELP_WEAPON_MINI_H];
static uint8_t helpWeaponMiniReady;
static uint8_t helpWeaponMiniAttempted;

static void helpStop(void) {
    if (helpSpaceOwner != NULL) {
        if (helpHudBarBG.imgBitmap != NULL)
            DoomRPG_freeImage(helpSpaceOwner, &helpHudBarBG);
        DoomRPG_freeImage(helpSpaceOwner, &helpSpaceBG);
    }
    SDL_memset(&helpSpaceBG, 0, sizeof(helpSpaceBG));
    SDL_memset(&helpHudBarBG, 0, sizeof(helpHudBarBG));
    helpSpaceOwner = NULL;
    helpSpaceEpoch = 0U;
    helpSpaceShift = 0;
    helpSpaceActive = 0;
    helpLoggedScroll = -1;
    helpHudBreathStep = -1;
    helpSceneBreathStep = -1;
    helpBottomHudReady = 0U;
    helpWeaponMiniReady = 0U;
    helpWeaponMiniAttempted = 0U;
    EspNativeHelpScene_reset();
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
    /* k.bmp is the real, opaque 20x20 tiled gameplay HUD background.
     * Retain only its tiny texture during HELP, not a screenshot or world. */
    DoomRPG_createImage(doomRpg, "k.bmp", false, &helpHudBarBG);
    if (helpHudBarBG.imgBitmap == NULL ||
        helpHudBarBG.width != MAIN_HELP_HUD_SOURCE_TILE ||
        helpHudBarBG.height != MAIN_HELP_HUD_SOURCE_TILE) {
        printf("[MAINHELP] FAILED k.bmp native top HUD asset 20x20\n");
        helpStop();
        return 0;
    }
    helpSpaceEpoch = DoomRPG_GetUpTimeMS();
    helpSpaceShift = -1;
    helpLoggedScroll = -1;
    helpHudBreathStep = -1;
    helpSceneBreathStep = -1;
    helpBottomHudReady = 0U;
    helpWeaponMiniReady = 0U;
    helpWeaponMiniAttempted = 0U;
    /* Once the gameplay lease has closed, this read-only peek still finds
     * the last COMMITTED raw-flash map header. It never stages/loads a map. */
    {
        const uint8_t cachedMapId = EspAssetPack_mapFlashCachedMapId();
        const int ready = EspNativeHelpScene_begin(cachedMapId,
                                                     helpSpaceEpoch);
        printf("[MAINHELP] PREVIEW source=original-PAK-wall ready=%u flashMap=%u previewMap=%u textureId=%u wallMaterials=%u bytes=2080 epoch=%u geometry=classic-perspective world-owner=no\n",
               ready ? 1U : 0U,
               (unsigned int)cachedMapId,
               (unsigned int)EspNativeHelpScene_previewMapId(),
               (unsigned int)EspNativeHelpScene_textureId(),
               (unsigned int)EspNativeHelpScene_materialCount(),
               (unsigned int)helpSpaceEpoch);
    }
    helpSpaceActive = 1;
    printf("[MAINHELP] BACKGROUND asset=c.bmp source=pak scroll=intro-157ms/192virtual owner=help hud=k.bmp/20x20\n");
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

    if (EspNativePresentation_clipEnabled()) DoomRPG_setClipFalse(doomRpg);
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

/* Reuse the *same* upper and middle rows as the industrial menu lettering.
 * With no additional palette, each HELP repaint follows the selected theme. */
static uint16_t helpTitleTopTone(void) {
    return EspMainMenuSkin_letterTone(1, 9);
}

static uint16_t helpBodyMiddleTone(void) {
    return EspMainMenuSkin_letterTone(4, 9);
}
static uint16_t helpThirdTone(void) {
    return EspMainMenuSkin_letterTone(7, 9);
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

/* One 12x12 copyright glyph repeated in three skin tones.
 * The a.bmp font lacks ©. Compact display attributions stand for
 * Kharn27; Erick Vásquez García ([GEC], DoomRPG-RE); and the original
 * developers id Software + Fountainhead Entertainment. */
static void helpPaintCopyrightMark(uint16_t* fb, int top, uint16_t ink) {
    static const uint16_t rows[12] = {
        0x0f0U, 0x30cU, 0x402U, 0x8f1U, 0x901U, 0x901U,
        0x901U, 0x901U, 0x8f1U, 0x402U, 0x30cU, 0x0f0U
    };
    int y, x;
    if (fb == NULL || top < 0 || top + 12 > MAIN_HELP_FOOTER_TOP)
        return;
    for (y = 0; y < 12; ++y)
        for (x = 0; x < 12; ++x)
            if ((rows[y] & (uint16_t)(0x800U >> x)) != 0U)
                fb[(top + y) * DOOMRPG_LOGICAL_WIDTH + 12 + x] = ink;
}

/* Every illustrated page reads the SAME pure 32-step, 80ms smoothstep
 * breath from EspNativeHelpScene. No duplicate level curves or hard cuts. */
static int helpBreathStepAt(uint32_t now) {
    return (int)EspNativeHelpScene_breathStep(now - helpSpaceEpoch);
}
static uint8_t helpBreathLevel(int step) {
    return EspNativeHelpScene_breathLevel((uint8_t)step);
}

/* All four scene pages share one phase and one loaded PAK wall material. */
static int helpIsRowHelpPage(int scroll) {
    return scroll >= MAIN_HELP_ROW_FIRST_SCROLL &&
           scroll <= MAIN_HELP_ROW_LAST_SCROLL &&
           scroll % MAIN_HELP_VISIBLE_LINES == 0;
}
static int helpIsAnimatedScenePage(int scroll) {
    return scroll == MAIN_HELP_SCENE_PAGE_SCROLL ||
           helpIsRowHelpPage(scroll);
}
static int helpIsAnimatedHudPage(int scroll) {
    return scroll == MAIN_HELP_HUD_PAGE_SCROLL ||
           scroll == MAIN_HELP_BOTTOM_HUD_PAGE_SCROLL;
}

/* Compact white 5x7 ROM glyphs for TOP HUD labels only.
 * The BOTTOM HUD now uses the actual native a.bmp glyphs and sprite sheets;
 * keep no extra digit or hand-drawn-face glyph tables in HELP. */
typedef struct HelpHudGlyph_s {
    char c;
    uint8_t rows[7];
} HelpHudGlyph;

static const HelpHudGlyph helpHudGlyphs[] = {
    {'M', {0x11, 0x1b, 0x15, 0x15, 0x11, 0x11, 0x11}},
    {'E', {0x1f, 0x10, 0x1e, 0x10, 0x10, 0x10, 0x1f}},
    {'N', {0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11}},
    {'U', {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0e}},
    {'P', {0x1e, 0x11, 0x11, 0x1e, 0x10, 0x10, 0x10}},
    {'A', {0x0e, 0x11, 0x11, 0x1f, 0x11, 0x11, 0x11}},
    {'S', {0x0f, 0x10, 0x10, 0x0e, 0x01, 0x01, 0x1e}},
    {'T', {0x1f, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04}},
    {'R', {0x1e, 0x11, 0x11, 0x1e, 0x14, 0x12, 0x11}}
};

static void helpPaintMiniHudLabel(uint16_t* framebuffer,
                                  const char* label, int left, int right,
                                  int y, uint8_t breathPower) {
    const unsigned int scale =
        164U + (uint32_t)breathPower * 92U / 255U;
    const uint16_t white = (uint16_t)(
        ((31U * scale / 256U) << 11) |
        ((63U * scale / 256U) << 5) |
        (31U * scale / 256U));
    const int length = (int)SDL_strlen(label);
    const int pixels = length * 6 - 1;
    int pen = (left + right - pixels + 1) / 2;
    int i, j, x, row;
    for (i = 0; i < length; ++i) {
        const char c = label[i];
        for (j = 0; j < (int)(sizeof(helpHudGlyphs) /
                                sizeof(helpHudGlyphs[0])); ++j) {
            if (helpHudGlyphs[j].c != c) continue;
            for (row = 0; row < 7; ++row) {
                for (x = 0; x < 5; ++x) {
                    const int dstX = pen + x;
                    if (dstX > left && dstX < right &&
                        (helpHudGlyphs[j].rows[row] & (0x10U >> x)) != 0U) {
                        framebuffer[(y + row) * DOOMRPG_LOGICAL_WIDTH +
                                    dstX] = white; /* readable white breath, never blinking */
                    }
                }
            }
            break;
        }
        pen += 6;
    }
}

/* Visual-only miniature of the *real* k.bmp game HUD backing texture:
 * eight 20x20 tiles scaled by the native SDL nearest-neighbour renderer
 * to 16x16, centered at x16..143/y43..58. Exactly three 1:3:1 zones,
 * tinted with a softly breathing neon blue, and white 5x7 labels.
 * No input routing, extra framebuffer, or asset read during repaint. */
/* One positional legend renderer for ALL illustrated help pages:
 * theme tone 1/4/7 for LEFT/CENTER/RIGHT, white descriptions. */
static void helpDrawPositionCaption(DoomRPG_t* doomRpg, DoomCanvas_t* canvas,
                                    const char* label, int zone, int y) {
    static const char* const prefixes[3] = {"LEFT:", "CENTER:", "RIGHT:"};
    static const int tones[3] = {1, 4, 7};
    size_t n;
    if (doomRpg == NULL || canvas == NULL || label == NULL ||
        zone < 0 || zone > 2) return;
    n = SDL_strlen(prefixes[zone]);
    if (strncmp(label, prefixes[zone], n) != 0) return;
    DoomRPG_setFontColor(doomRpg,
                         helpColor888(EspMainMenuSkin_letterTone(tones[zone], 9)));
    EspNativeCanvas_drawText(canvas, prefixes[zone], 12, y, 0);
    DoomRPG_setFontColor(doomRpg, 0xffffffffU);
    EspNativeCanvas_drawText(canvas, label + n, 12 + (int)n * 7, y, 0);
}
/* Real k.bmp backing shared by BOTH help HUD miniatures. One scratch
 * framebuffer (the live 160x120 canvas), no extra retained bitmap. */
static int helpPaintMiniHudBacking(uint16_t* fb) {
    int tileX;
    if (fb == NULL || helpHudBarBG.imgBitmap == NULL ||
        helpHudBarBG.width != MAIN_HELP_HUD_SOURCE_TILE ||
        helpHudBarBG.height != MAIN_HELP_HUD_SOURCE_TILE ||
        MAIN_HELP_HUD_BAR_X < 5 ||
        MAIN_HELP_HUD_BAR_X + MAIN_HELP_HUD_BAR_WIDTH > 155 ||
        MAIN_HELP_HUD_BAR_Y < 39 ||
        MAIN_HELP_HUD_BAR_Y + MAIN_HELP_HUD_BAR_HEIGHT > 64)
        return 0;
    for (tileX = 0; tileX < MAIN_HELP_HUD_BAR_WIDTH;
         tileX += MAIN_HELP_HUD_MINI_TILE) {
        SDL_Rect dst = {MAIN_HELP_HUD_BAR_X + tileX,
                        MAIN_HELP_HUD_BAR_Y, MAIN_HELP_HUD_MINI_TILE,
                        MAIN_HELP_HUD_BAR_HEIGHT};
        if (SDL_RenderCopy(NULL, helpHudBarBG.imgBitmap, NULL, &dst) != 0)
            return 0;
    }
    return 1;
}

static int helpPaintTopHudIllustration(uint16_t* framebuffer) {
    static const int left[3] = {
        MAIN_HELP_HUD_BAR_X,
        MAIN_HELP_HUD_BAR_X + MAIN_HELP_HUD_PASS_LEFT,
        MAIN_HELP_HUD_BAR_X + MAIN_HELP_HUD_MAP_LEFT
    };
    static const int right[3] = {
        MAIN_HELP_HUD_BAR_X + MAIN_HELP_HUD_MENU_RIGHT,
        MAIN_HELP_HUD_BAR_X + MAIN_HELP_HUD_PASS_RIGHT,
        MAIN_HELP_HUD_BAR_X + MAIN_HELP_HUD_BAR_WIDTH - 1
    };
    static const char* const labels[3] = {"MENU", "PASS TURN", "MAP"};
    const int step = helpBreathStepAt(DoomRPG_GetUpTimeMS());
    const uint8_t power = EspNativeHelpScene_breathPower(
        helpBreathLevel(step));
    const uint16_t neon = (uint16_t)(((uint32_t)power * 31U) / 255U);
    int zone, x, y;

    if (!helpPaintMiniHudBacking(framebuffer)) return 0;
    /* The thin HUD borders and the MENU/PASS/MAP letters breathe on the
     * same clock, but their k.bmp background is never flooded blue. */

    /* The imaginary boundaries alone breathe: x42 and x119 of the
     * 128x16 real k.bmp miniature. Never blue-tint its interior or glyphs. */
    if (neon != 0U) {
        for (zone = 1; zone <= 2; ++zone) {
            x = left[zone];
            for (y = MAIN_HELP_HUD_BAR_Y;
                 y < MAIN_HELP_HUD_BAR_Y + MAIN_HELP_HUD_BAR_HEIGHT; ++y) {
                uint16_t* ink = &framebuffer[
                    y * DOOMRPG_LOGICAL_WIDTH + x];
                *ink = (uint16_t)((*ink & 0xffe0U) | neon);
            }
        }
    }

    for (zone = 0; zone < 3; ++zone)
        helpPaintMiniHudLabel(framebuffer, labels[zone],
                              left[zone], right[zone],
                              MAIN_HELP_HUD_BAR_Y + 4, power);
    helpHudBreathStep = step;
    return 1;
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

    if (doomRpg == NULL || EspNativeRootOwners_live()->menuSystem == NULL ||
        EspNativeRootOwners_live()->doomCanvas == NULL ||
        (expectedChildMenu != MENU_MAIN_OPTIONS &&
         expectedChildMenu != MENU_MAIN_HELP_ABOUT)) {
        printf("[MAINBACK] FAILED source=%s child=%d object/model contract\n",
               source != NULL ? source : "unknown",
               expectedChildMenu);
        return 0;
    }

    menuSystem = EspNativeRootOwners_live()->menuSystem;
    printf("\n=== Doom RPG ESP32 native child -> MENU_MAIN Back ===\n");
    printf("[MAINBACK] BEGIN source=%s child=%d menu=%d old=%d selected=%d state=%d frame=%08x\n",
           source != NULL ? source : "unknown",
           expectedChildMenu,
           menuSystem->menu,
           menuSystem->oldMenu,
           menuSystem->selectedIndex,
           EspNativeRootOwners_live()->doomCanvas->state,
           (unsigned int)DoomRPG_esp32MainMenuFramebufferHash(EspNativeRootOwners_live()->render));

    if (!DoomRPG_esp32MainMenuGraphicsBoundaryIsSafe(doomRpg) ||
        EspNativeRootOwners_live()->doomCanvas->state != ST_MENU ||
        menuSystem->menu != expectedChildMenu ||
        menuSystem->oldMenu != MENU_MAIN) {
        printf("[MAINBACK] FAILED source=%s precondition safe=%d state=%d menu=%d expectedChild=%d old=%d\n",
               source != NULL ? source : "unknown",
               DoomRPG_esp32MainMenuGraphicsBoundaryIsSafe(doomRpg),
               EspNativeRootOwners_live()->doomCanvas->state,
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
        finalFNV != DoomRPG_esp32MainMenuFramebufferHash(EspNativeRootOwners_live()->render) ||
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

/* Original k/m/l/o/a BMPs, exact gameplay HUD painter, no substitutes.
 * Stage y100..119 once before HELP footer plates overwrite those pixels,
 * and keep only a 128x16 RGB565 thumbnail for subsequent soft repaints. */
static int helpPaintBottomHudIllustration(uint16_t* fb) {
    const int step = helpBreathStepAt(DoomRPG_GetUpTimeMS());
    const uint8_t power = EspNativeHelpScene_breathPower(
        helpBreathLevel(step));
    int x, y;
    if (fb == NULL) return 0;
    if (!helpBottomHudReady) {
        if (!EspNativeGameplayHud_paintHelpBottomBand(fb)) {
            printf("[MAINHELP] BOTTOM-HUD FAIL native k/m/l/o/a ownership\n");
            return 0;
        }
        for (y = 0; y < MAIN_HELP_HUD_BAR_HEIGHT; ++y) {
            for (x = 0; x < MAIN_HELP_HUD_BAR_WIDTH; ++x) {
                const int sy = 100 + y * 20 / MAIN_HELP_HUD_BAR_HEIGHT;
                const int sx = x * 160 / MAIN_HELP_HUD_BAR_WIDTH;
                helpBottomHudMini[y * MAIN_HELP_HUD_BAR_WIDTH + x] =
                    fb[sy * DOOMRPG_LOGICAL_WIDTH + sx];
            }
        }
        helpBottomHudReady = 1U;
    }
    for (y = 0; y < MAIN_HELP_HUD_BAR_HEIGHT; ++y)
        for (x = 0; x < MAIN_HELP_HUD_BAR_WIDTH; ++x)
            fb[(MAIN_HELP_HUD_BAR_Y + y) * DOOMRPG_LOGICAL_WIDTH +
               MAIN_HELP_HUD_BAR_X + x] =
                helpBottomHudMini[y * MAIN_HELP_HUD_BAR_WIDTH + x];

    /* Scale the ACTUAL weapon hitbox x100..159 to x96..143 of this
     * 128px miniature. Highlight the same four 3px touch corners as the
     * canonical gameplay HUD (not arbitrary full-height blue dividers).
     * x97/x142 and y44..46/y55..57; no framebuffer region is tinted. */
    {
        const int buttonLeft = MAIN_HELP_HUD_BAR_X +
            ESP_NATIVE_HUD_WEAPON_LEFT * MAIN_HELP_HUD_BAR_WIDTH /
            DOOMRPG_LOGICAL_WIDTH;
        const int buttonRight = MAIN_HELP_HUD_BAR_X +
            (ESP_NATIVE_HUD_WEAPON_RIGHT + 1) *
            MAIN_HELP_HUD_BAR_WIDTH / DOOMRPG_LOGICAL_WIDTH - 1;
        const uint16_t amber = (uint16_t)(
            (((31U * (uint32_t)power) / 255U) << 11) |
            (((40U * (uint32_t)power) / 255U) << 5));
        int edge, corner;
        if (power > 0U) {
            for (edge = 0; edge < 2; ++edge) {
                const int buttonX = edge == 0
                    ? buttonLeft + 1 : buttonRight - 1;
                for (corner = 0; corner < 2; ++corner) {
                    const int firstY = MAIN_HELP_HUD_BAR_Y +
                        (corner == 0 ? 1 : MAIN_HELP_HUD_BAR_HEIGHT - 4);
                    for (y = firstY; y < firstY + 3; ++y)
                        fb[y * DOOMRPG_LOGICAL_WIDTH + buttonX] = amber;
                }
            }
        }
    }
    helpHudBreathStep = step;
    return 1;
}

/* Build the exact native 4x3 WEAPONS panel from its real sprite PAK
 * decoder, *without* a resident player, world, modal, turn or gameplay UI.
 * The compact immutable entity-type catalog may be initialized once from
 * entities.db; the normal game uses that same catalog later. During this
 * one-time HELP paint, 160x120 RGB565 serves as a scratch surface. The next
 * helpDrawStarfield() immediately replaces it before presentation. */
static int helpBuildWeaponGridMini(uint16_t* framebuffer) {
    EspAssetPackEntry entities;
    EspNativeGameplayPlayerState demo;
    int x, y, ok = 0;
    if (helpWeaponMiniReady) return 1;
    if (helpWeaponMiniAttempted || framebuffer == NULL ||
        EspAssetPack_isOpen()) return 0;
    helpWeaponMiniAttempted = 1U;
    if (!EspAssetPack_open(ESP_ASSET_PACK_DEFAULT_PATH)) return 0;
    if (!EspEntityDefTypeCatalog_isReady() &&
        (!EspAssetPack_findEntry("/entities.db", &entities) ||
         !EspEntityDefTypeCatalog_buildFromPackEntry(&entities)))
        goto done;
    memset(&demo, 0, sizeof(demo));
    demo.active = 1U;
    demo.weapon = 2U;   /* Pistol equipped: sample only, never applied. */
    demo.weapons = 0x0fffU; /* HELP demonstration: all 12 PAK-native weapons owned, 4x3 full-color grid. */
    if (!EspNativeGameplayHubWeaponGrid_paint(framebuffer, &demo, 2U))
        goto done;
    /* Source grid has EXACT native bounds x2..157 / y35..118.
     * Downsample nearest-neighbour over the FULL closed source rectangle
     * (including right and bottom borders) to x8..151 / y39..98.
     * Retain just one bounded static RGB565 miniature (17,280 B). */
    for (y = 0; y < MAIN_HELP_WEAPON_MINI_H; ++y) {
        const int sy = 35 + y * 83 / (MAIN_HELP_WEAPON_MINI_H - 1);
        for (x = 0; x < MAIN_HELP_WEAPON_MINI_W; ++x) {
            const int sx = 2 + x * 155 / (MAIN_HELP_WEAPON_MINI_W - 1);
            helpWeaponMini[y * MAIN_HELP_WEAPON_MINI_W + x] =
                framebuffer[sy * DOOMRPG_LOGICAL_WIDTH + sx];
        }
    }
    helpWeaponMiniReady = 1U;
    ok = 1;
done:
    EspAssetPack_close();
    printf("[MAINHELP] WEAPON-PREVIEW ready=%u source=real-native-grid/PAK icons=12 layout=4x3 mini=%ux%u bytes=%u world=no playerMutation=no packClosed=%u\n",
           ok ? 1U : 0U,
           (unsigned int)MAIN_HELP_WEAPON_MINI_W,
           (unsigned int)MAIN_HELP_WEAPON_MINI_H,
           (unsigned int)sizeof(helpWeaponMini),
           EspAssetPack_isOpen() ? 0U : 1U);
    return ok;
}
static void helpDrawWeaponGridMini(uint16_t* fb) {
    int x, y;
    if (fb == NULL || !helpWeaponMiniReady) return;
    for (y = 0; y < MAIN_HELP_WEAPON_MINI_H; ++y)
        for (x = 0; x < MAIN_HELP_WEAPON_MINI_W; ++x)
            fb[(MAIN_HELP_WEAPON_MINI_Y + y) * DOOMRPG_LOGICAL_WIDTH +
               MAIN_HELP_WEAPON_MINI_X + x] =
                helpWeaponMini[y * MAIN_HELP_WEAPON_MINI_W + x];
}

/* On the WEAPON SELECT help page: crop the REAL weapon touch target
 * [100..159] from the canonical HUD thumbnail. Downsample to 36x12 so
 * its miniature sits on the SAME BASELINE as the themed title at y14.
 * No recreated weapon icon, no cropped face/compass neighbours. */
static void helpDrawWeaponButtonCue(uint16_t* fb) {
    const int sourceX = ESP_NATIVE_HUD_WEAPON_LEFT *
                        MAIN_HELP_HUD_BAR_WIDTH / DOOMRPG_LOGICAL_WIDTH;
    const int sourceWidth = (ESP_NATIVE_HUD_WEAPON_RIGHT + 1 -
                             ESP_NATIVE_HUD_WEAPON_LEFT) *
                            MAIN_HELP_HUD_BAR_WIDTH / DOOMRPG_LOGICAL_WIDTH;
    const int dstX = 116, dstY = 14, dstW = 36, dstH = 12;
    int x, y;
    if (!helpBottomHudReady || fb == NULL) return;
    for (y = 0; y < dstH; ++y) {
        const int sy = y * MAIN_HELP_HUD_BAR_HEIGHT / dstH;
        for (x = 0; x < dstW; ++x) {
            const int sx = sourceX + x * sourceWidth / dstW;
            fb[(dstY + y) * DOOMRPG_LOGICAL_WIDTH + dstX + x] =
                helpBottomHudMini[sy * MAIN_HELP_HUD_BAR_WIDTH + sx];
        }
    }
}

static int paintHelp(DoomRPG_t* doomRpg, uint32_t* outFrameFNV) {
    DoomCanvas_t* canvas;
    EspNativeMenuState_t* menuSystem;
    uint16_t* framebuffer;
    int visible, maxScroll, end, i, y;
    char page[12];
    uint32_t frameFNV;

    if (outFrameFNV != NULL) *outFrameFNV = 0U;
    if (doomRpg == NULL || EspNativeRootOwners_live()->doomCanvas == NULL ||
        EspNativeRootOwners_live()->menuSystem == NULL || !helpSpaceActive ||
        !DoomRPG_esp32MainMenuGraphicsBoundaryIsSafe(doomRpg))
        return 0;
    canvas = EspNativeRootOwners_live()->doomCanvas;
    menuSystem = EspNativeRootOwners_live()->menuSystem;
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

    if (menuSystem->scrollIndex == MAIN_HELP_WEAPON_PAGE_SCROLL) {
        /* Also works when the user directly opens this help page without
         * visiting BOTTOM HUD BAR first. Every source is PAK-native. */
        if (!helpBottomHudReady)
            (void)helpPaintBottomHudIllustration(framebuffer);
        if (!helpWeaponMiniAttempted)
            (void)helpBuildWeaponGridMini(framebuffer);
    }
    if (!helpDrawStarfield(doomRpg)) return 0;
    DoomRPG_setFontColor(doomRpg, helpColor888(helpTitleTopTone()));
    EspNativeCanvas_drawText(canvas, "HELP / CYD", 12, 0, 0);
    SDL_snprintf(page, sizeof(page), "%d/%d",
                 (menuSystem->scrollIndex + MAIN_HELP_VISIBLE_LINES - 1) /
                     MAIN_HELP_VISIBLE_LINES + 1,
                 (menuSystem->numItems + MAIN_HELP_VISIBLE_LINES - 1) /
                     MAIN_HELP_VISIBLE_LINES);
    EspNativeCanvas_drawText(canvas, page, 121, 0, 0);

    if (menuSystem->scrollIndex == MAIN_HELP_HUD_PAGE_SCROLL) {
        /* This one page replaces three abstract rows with the genuine
         * gameplay bar, plus three labels below. No touch owners change. */
        const char* title = DoomRPG_esp32MainMenuHelpLine(
            MAIN_HELP_HUD_PAGE_SCROLL);
        const char* caption = DoomRPG_esp32MainMenuHelpLine(
            MAIN_HELP_HUD_PAGE_SCROLL + 1);
        if (title == NULL || caption == NULL || visible != 7)
            return 0;
        helpDrawThemedPageTitle(doomRpg, canvas, title, 12, 14);
        /* The control-identification caption stays white. The three
         * position labels below take their own theme tone. */
        DoomRPG_setFontColor(doomRpg, 0xffffffffU);
        EspNativeCanvas_drawText(canvas, caption, 12, 27, 0);
        if (!helpPaintTopHudIllustration(framebuffer))
            return 0;
        for (i = 2; i <= 4; ++i) {
            const char* label = DoomRPG_esp32MainMenuHelpLine(
                MAIN_HELP_HUD_PAGE_SCROLL + i);
            if (label == NULL || label[0] == '\0') return 0;
            helpDrawPositionCaption(doomRpg, canvas, label, i - 2,
                               64 + (i - 2) * MAIN_HELP_LINE_HEIGHT);
        }
    }
    else if (menuSystem->scrollIndex == MAIN_HELP_BOTTOM_HUD_PAGE_SCROLL) {
        const char* title = DoomRPG_esp32MainMenuHelpLine(
            MAIN_HELP_BOTTOM_HUD_PAGE_SCROLL);
        const char* caption = DoomRPG_esp32MainMenuHelpLine(
            MAIN_HELP_BOTTOM_HUD_PAGE_SCROLL + 1);
        if (title == NULL || caption == NULL || visible != 7) return 0;
        helpDrawThemedPageTitle(doomRpg, canvas, title, 12, 14);
        DoomRPG_setFontColor(doomRpg, 0xffffffffU);
        EspNativeCanvas_drawText(canvas, caption, 12, 27, 0);
        if (!helpPaintBottomHudIllustration(framebuffer)) return 0;
        for (i = 2; i <= 4; ++i) {
            const char* label = DoomRPG_esp32MainMenuHelpLine(
                MAIN_HELP_BOTTOM_HUD_PAGE_SCROLL + i);
            if (label == NULL || label[0] == '\0') return 0;
            helpDrawPositionCaption(doomRpg, canvas, label, i - 2,
                                    64 + (i - 2) * MAIN_HELP_LINE_HEIGHT);
        }
    }
    else if (menuSystem->scrollIndex == MAIN_HELP_WEAPON_PAGE_SCROLL) {
        const char* title = DoomRPG_esp32MainMenuHelpLine(
            MAIN_HELP_WEAPON_PAGE_SCROLL);
        const char* caption = DoomRPG_esp32MainMenuHelpLine(
            MAIN_HELP_WEAPON_PAGE_SCROLL + 1);
        if (title == NULL || caption == NULL ||
            visible != MAIN_HELP_VISIBLE_LINES) return 0;
        helpDrawThemedPageTitle(doomRpg, canvas, title, 12, 14);
        DoomRPG_setFontColor(doomRpg, 0xffffffffU);
        EspNativeCanvas_drawText(canvas, caption, 12, 27, 0);
        helpDrawWeaponButtonCue(framebuffer);
        if (helpWeaponMiniReady) {
            helpDrawWeaponGridMini(framebuffer);
        }
        else {
            /* Corrupt/missing PAK fails closed, never a fabricated weapon. */
            EspNativeCanvas_drawText(
                canvas, "Preview unavailable", 12, 54, 0);
        }
    }
    else if (menuSystem->scrollIndex == MAIN_HELP_SCENE_PAGE_SCROLL &&
             EspNativeHelpScene_isReady()) {
        /* MAIN GAMEPLAY: illustrative PAK scene with nine touch areas.
         * The next three pages detail one control row each. */
        const char* title = DoomRPG_esp32MainMenuHelpLine(
            MAIN_HELP_SCENE_PAGE_SCROLL);
        const int step = helpBreathStepAt(DoomRPG_GetUpTimeMS());
        if (title == NULL || visible != 7) return 0;
        helpDrawThemedPageTitle(doomRpg, canvas, title, 12, 14);
        /* No TOP caption: the larger preview begins at y29. */
        if (!EspNativeHelpScene_paint(
                 framebuffer, helpBreathLevel(step))) return 0;
        helpSceneBreathStep = step;
    }
    else if (helpIsRowHelpPage(menuSystem->scrollIndex) &&
             EspNativeHelpScene_isReady()) {
        const int sourceRow = (menuSystem->scrollIndex -
                               MAIN_HELP_ROW_FIRST_SCROLL) /
                              MAIN_HELP_VISIBLE_LINES;
        const int step = helpBreathStepAt(DoomRPG_GetUpTimeMS());
        const char* title = DoomRPG_esp32MainMenuHelpLine(
            menuSystem->scrollIndex);
        const char* caption = DoomRPG_esp32MainMenuHelpLine(
            menuSystem->scrollIndex + 1);
        if (title == NULL || caption == NULL || visible != 7 ||
            sourceRow < 0 || sourceRow > 2) return 0;
        helpDrawThemedPageTitle(doomRpg, canvas, title, 12, 14);
        DoomRPG_setFontColor(doomRpg, 0xffffffffU);
        EspNativeCanvas_drawText(canvas, caption, 12, 27, 0);
        if (!EspNativeHelpScene_paintRow(
                framebuffer, (uint8_t)sourceRow,
                helpBreathLevel(step))) return 0;
        /* Three explicit explanations below the illustrated 3-button row,
         * last baseline at y87, glyphs ending y98, above border y99. */
        for (i = 2; i <= 4; ++i) {
            const char* label = DoomRPG_esp32MainMenuHelpLine(
                menuSystem->scrollIndex + i);
            if (label == NULL || label[0] == '\0') return 0;
            helpDrawPositionCaption(doomRpg, canvas, label, i - 2,
                                    63 + (i - 2) * MAIN_HELP_LINE_HEIGHT);
        }
        helpSceneBreathStep = step;
    }
    else {
        DoomRPG_setFontColor(doomRpg, 0xffffffffU);
        y = 14;
        for (i = menuSystem->scrollIndex; i < end; ++i) {
        const char* line = DoomRPG_esp32MainMenuHelpLine(i);
        if (line == NULL) return 0;
        if (i % MAIN_HELP_VISIBLE_LINES == 0) {
            /* First visible row is the section title on every fixed page. */
            helpDrawThemedPageTitle(doomRpg, canvas, line, 12, y);
        }
        else if (i >= 1 && i <= 3 && menuSystem->scrollIndex == 0) {
            const uint16_t tone = i == 1 ? helpTitleTopTone() :
                                  i == 2 ? helpBodyMiddleTone() :
                                           helpThirdTone();
            DoomRPG_setFontColor(doomRpg, helpColor888(tone));
            EspNativeCanvas_drawText(canvas, line, 27, y, 0);
            helpPaintCopyrightMark(framebuffer, y, tone);
        }
        else if (line[0] != '\0') {
            /* John Carmack's tribute closes the cover. */
            DoomRPG_setFontColor(doomRpg,
                (menuSystem->scrollIndex == 0 && i == 5)
                    ? 0xffffffffU : helpColor888(helpBodyMiddleTone()));
            EspNativeCanvas_drawText(canvas, line, 12, y, 0);
        }
        y += MAIN_HELP_LINE_HEIGHT;
    }
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
    frameFNV = DoomRPG_esp32MainMenuFramebufferHash(EspNativeRootOwners_live()->render);
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
        EspNativeRootOwners_live()->menuSystem == NULL ||
        EspNativeRootOwners_live()->menuSystem->menu != MENU_MAIN_HELP_ABOUT ||
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
    /* HUD plus four illustrated gameplay pages redraw for breath updates;
     * the other pages use the starfield cadence. No extra buffers. */
    if (shift == helpSpaceShift &&
        (!helpIsAnimatedHudPage(EspNativeRootOwners_live()->menuSystem->scrollIndex) ||
         helpHudBreathStep == helpBreathStepAt(now)) &&
        (!helpIsAnimatedScenePage(
             EspNativeRootOwners_live()->menuSystem->scrollIndex) ||
         !EspNativeHelpScene_isReady() ||
         helpSceneBreathStep == helpBreathStepAt(now)))
        return;
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

    if (doomRpg == NULL || EspNativeRootOwners_live()->menuSystem == NULL) return;
    menuSystem = EspNativeRootOwners_live()->menuSystem;
    before = DoomRPG_esp32MainMenuFramebufferHash(EspNativeRootOwners_live()->render);
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

    menuSystem = EspNativeRootOwners_live()->menuSystem;
    inputFNV = DoomRPG_esp32MainMenuFramebufferHash(EspNativeRootOwners_live()->render);
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
           EspNativeRootOwners_live()->menuSystem != NULL ? EspNativeRootOwners_live()->menuSystem->menu : -999,
           EspNativeRootOwners_live()->menuSystem != NULL
               ? EspNativeRootOwners_live()->menuSystem->selectedIndex : -999);

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
    DoomCanvas_t* canvas = EspNativeRootOwners_live()->doomCanvas;
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

    frameFNV = DoomRPG_esp32MainMenuFramebufferHash(EspNativeRootOwners_live()->render);
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

    inputHash = DoomRPG_esp32MainMenuFramebufferHash(EspNativeRootOwners_live()->render);
    expectedHash = DoomRPG_esp32MainMenuSelectionFramebufferFNV(
        MAIN_LOAD_ITEM_INDEX);
    if (EspNativeRootOwners_live()->menuSystem->menu != MENU_MAIN ||
        EspNativeRootOwners_live()->menuSystem->selectedIndex != MAIN_LOAD_ITEM_INDEX ||
        EspNativeRootOwners_live()->doomCanvas->state != ST_MENU ||
        (!EspNativeGameplaySave_mainSelectorReady() && (expectedHash == 0U || inputHash != expectedHash))) {
        printf("[MAINLOAD] FAILED precondition menu=%d selected=%d state=%d framebuffer=%08x expected=%08x\n",
               EspNativeRootOwners_live()->menuSystem->menu,
               EspNativeRootOwners_live()->menuSystem->selectedIndex,
               EspNativeRootOwners_live()->doomCanvas->state,
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
    EspNativeRootOwners_live()->menuSystem->menu = MENU_NONE;
    EspNativeRootOwners_live()->menuSystem->numItems = 0;
    EspNativeMainMenu_transitionCanvasState(EspNativeRootOwners_live()->doomCanvas, ST_PLAYING);

    printf("[MAINLOAD] READY checkpoint restored; intro=skipped session=resume-pending state=%d\n",
           EspNativeRootOwners_live()->doomCanvas->state);
    return DOOMRPG_ESP32_MAIN_MENU_LOAD_TRANSITIONED;
}
