#include <SDL.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "DoomRPG.h"
#include "DoomCanvas.h"
#include "Render.h"
#include "SDL_Video.h"

#include "esp_native_audio_intent.h"
#include "esp_native_intro_state.h"
#include "native_story_fit.h"
#include "platform_video_config.h"

#define ESP32_DOOMCANVAS_DESKTOP_BYTES 3740U
#define ESP32_DOOMCANVAS_RETIRED_DIALOG_BYTES (2048U + 512U)
#define ESP32_DOOMCANVAS_RETIRED_ZEROREF_FIELD_BYTES 239U
#define ESP32_DOOMCANVAS_RETIRED_ZEROREF_LAYOUT_BYTES 240U
#define ESP32_DOOMCANVAS_RETIRED_DORMANT_TEXT_BYTES (300U + 128U)
#define ESP32_DOOMCANVAS_RETIRED_STATE_FIELD_BYTES 113U
#define ESP32_DOOMCANVAS_RETIRED_STATE_LAYOUT_BYTES 116U
#define ESP32_DOOMCANVAS_RETIRED_GRAPH_MIRROR_BYTES (7U * sizeof(void*))
#define ESP32_DOOMCANVAS_RETIRED_INTRO_STATE_BYTES 96U
#define ESP32_DOOMCANVAS_RETIRED_DEAD_SHELL_BYTES 56U
#define ESP32_DOOMCANVAS_RETIRED_INERT_CONTROL_BYTES 72U
#define ESP32_DOOMCANVAS_RETIRED_LARGE_FONT_BYTES 16U
#define ESP32_DOOMCANVAS_RETIRED_FIXED_GEOMETRY_BYTES 56U
#define ESP32_DOOMCANVAS_RETIRED_VIEW_SHAKE_ALIAS_BYTES 28U
#define ESP32_DOOMCANVAS_COMPACT_BYTES \
    (ESP32_DOOMCANVAS_DESKTOP_BYTES - ESP32_DOOMCANVAS_RETIRED_DIALOG_BYTES - \
     ESP32_DOOMCANVAS_RETIRED_ZEROREF_LAYOUT_BYTES - \
     ESP32_DOOMCANVAS_RETIRED_DORMANT_TEXT_BYTES - \
     ESP32_DOOMCANVAS_RETIRED_STATE_LAYOUT_BYTES - \
     ESP32_DOOMCANVAS_RETIRED_GRAPH_MIRROR_BYTES - \
     ESP32_DOOMCANVAS_RETIRED_INTRO_STATE_BYTES - \
     ESP32_DOOMCANVAS_RETIRED_DEAD_SHELL_BYTES - \
     ESP32_DOOMCANVAS_RETIRED_INERT_CONTROL_BYTES - \
     ESP32_DOOMCANVAS_RETIRED_LARGE_FONT_BYTES - \
     ESP32_DOOMCANVAS_RETIRED_FIXED_GEOMETRY_BYTES - \
     ESP32_DOOMCANVAS_RETIRED_VIEW_SHAKE_ALIAS_BYTES)

_Static_assert(sizeof(DoomCanvas_t) == ESP32_DOOMCANVAS_COMPACT_BYTES,
               "ESP32 DoomCanvas_t layout changed; audit compatibility owners before proceeding");

/*
 * Permanent ESP32 compatibility bridge for the small DoomCanvas ABI that
 * survived the hardware-proven linked-root audit.
 *
 * Desktop DoomCanvas.c is intentionally not compiled for ESP32. Native owners
 * handle gameplay, menus, dialogs, world mutation and presentation. This file
 * keeps only the legacy construction/layout/text/state hooks still referenced
 * by the native runtime while those call sites are migrated individually.
 */

void Sound_stopSounds(struct Sound_s* sound);
void Sound_playSound(struct Sound_s* sound, int resourceID, byte flags, int priority);

#define ESP32_SND_FLG_LOOP       1U
#define ESP32_SND_FLG_STOPSOUNDS 2U
#define ESP32_SND_FLG_ISMUSIC    8U

DoomCanvas_t* DoomCanvas_init(DoomCanvas_t* doomCanvas, DoomRPG_t* doomRpg)
{
    printf("DoomCanvas_init\n");

    if (doomCanvas == NULL) {
        doomCanvas = SDL_malloc(sizeof(DoomCanvas_t));
        if (doomCanvas == NULL) {
            return NULL;
        }
    }
    SDL_memset(doomCanvas, 0, sizeof(DoomCanvas_t));

    doomCanvas->doomRpg = doomRpg;
    doomCanvas->imgFont.imgBitmap = NULL;
    doomCanvas->fontColor = 0xffffffff;
    doomCanvas->renderFloorCeilingTextures = true;

    printf("[DOOMCANVASBRIDGE] INIT exports=6 desktopTU=no bytes=%u retiredDialogStores=%u retiredZeroRefLayout=%u retiredDormantText=%u, retiredStateLayout=%u retiredGraphMirrors=%u retiredIntroState=%u retiredDeadShell=%u retiredInertControl=%u retiredLargeFont=%u retiredFixedGeometry=%u retiredViewShakeAlias=%u clip=%dx%d\n",
           (unsigned int)sizeof(DoomCanvas_t),
           (unsigned int)ESP32_DOOMCANVAS_RETIRED_DIALOG_BYTES,
           (unsigned int)ESP32_DOOMCANVAS_RETIRED_ZEROREF_LAYOUT_BYTES,
           (unsigned int)ESP32_DOOMCANVAS_RETIRED_DORMANT_TEXT_BYTES,
           (unsigned int)ESP32_DOOMCANVAS_RETIRED_STATE_LAYOUT_BYTES,
           (unsigned int)ESP32_DOOMCANVAS_RETIRED_GRAPH_MIRROR_BYTES,
           (unsigned int)ESP32_DOOMCANVAS_RETIRED_INTRO_STATE_BYTES,
           (unsigned int)ESP32_DOOMCANVAS_RETIRED_DEAD_SHELL_BYTES,
           (unsigned int)ESP32_DOOMCANVAS_RETIRED_INERT_CONTROL_BYTES,
           (unsigned int)ESP32_DOOMCANVAS_RETIRED_LARGE_FONT_BYTES,
           (unsigned int)ESP32_DOOMCANVAS_RETIRED_FIXED_GEOMETRY_BYTES,
           (unsigned int)ESP32_DOOMCANVAS_RETIRED_VIEW_SHAKE_ALIAS_BYTES,
           DOOMRPG_CANVAS_WIDTH,
           DOOMRPG_CANVAS_HEIGHT);
    return doomCanvas;
}

void DoomCanvas_free(DoomCanvas_t* doomCanvas, boolean freePtr)
{
    if (doomCanvas == NULL) {
        return;
    }

    Esp32StoryFit_release(doomCanvas);
    EspNativeIntroState_release(doomCanvas->doomRpg);
    DoomRPG_freeImage(doomCanvas->doomRpg, &doomCanvas->imgFont);

    if (freePtr) {
        SDL_free(doomCanvas);
    }
}

void DoomCanvas_drawImageSpecial(DoomCanvas_t* doomCanvas,
                                 Image_t* img,
                                 int xSrc,
                                 int ySrc,
                                 int width,
                                 int height,
                                 int param_7,
                                 int xDst,
                                 int yDst,
                                 int flags)
{
    SDL_Rect renderQuad;
    SDL_Rect clip;
    (void)param_7;

    if (doomCanvas == NULL || img == NULL) {
        return;
    }
    if (width == 0) {
        width = img->width;
    }
    if (height == 0) {
        height = img->height;
    }

    if ((flags & 16) == 0) {
        if ((flags & 8) != 0) {
            xDst -= width;
        }
    } else {
        xDst -= (int)((unsigned int)width >> 1);
    }

    if ((flags & 32) == 0) {
        if ((flags & 2) != 0) {
            yDst -= height;
        }
    } else {
        yDst -= (int)((unsigned int)height >> 1);
    }

    renderQuad.w = img->width;
    renderQuad.h = img->height;

    if (img->imgBitmap != NULL) {
        int remainingWidth = width;
        int drawX = xDst;

        do {
            do {
                clip.x = xSrc;
                clip.y = ySrc;
                clip.w = remainingWidth;
                clip.h = height;

                renderQuad.x = DOOMRPG_CANVAS_X + drawX;
                renderQuad.y = DOOMRPG_CANVAS_Y + yDst;
                renderQuad.w = img->width;
                renderQuad.h = img->height;
                if (clip.w <= renderQuad.w) {
                    renderQuad.w = clip.w;
                }
                if (clip.h <= renderQuad.h) {
                    renderQuad.h = clip.h;
                }

                if ((flags & 64) != 0) {
                    renderQuad.x = (renderQuad.x + renderQuad.w / 2) -
                                   (((renderQuad.w / 2) * 3) / 2);
                    renderQuad.y = (renderQuad.y + renderQuad.h / 2) -
                                   (((renderQuad.h / 2) * 3) / 2);
                    renderQuad.w = (renderQuad.w * 3) / 2;
                    renderQuad.h = (renderQuad.h * 3) / 2;
                }

                SDL_RenderCopy(sdlVideo.renderer, img->imgBitmap, &clip, &renderQuad);

                remainingWidth -= img->width;
                drawX += img->width;
            } while (remainingWidth > 0);

            height -= img->height;
            yDst += img->height;
            remainingWidth = width;
            drawX = xDst;
        } while (height > 0);
    }
}

void DoomCanvas_drawString1(DoomCanvas_t* doomCanvas,
                            char* text,
                            int x,
                            int y,
                            int flags)
{
    Image_t* imgFont;
    const int advance = 7;
    const int width = 9;
    const int height = 12;
    int len;
    int xpos;
    int i;
    unsigned int c;

    if (doomCanvas == NULL || text == NULL) {
        return;
    }

    imgFont = &doomCanvas->imgFont;
    if (imgFont->imgBitmap == NULL) {
        return;
    }

    {
        byte r = (byte)((doomCanvas->fontColor & 0x00ff0000) >> 16);
        byte g = (byte)((doomCanvas->fontColor & 0x0000ff00) >> 8);
        byte b = (byte)(doomCanvas->fontColor & 0x000000ff);
        SDL_SetTextureColorMod(imgFont->imgBitmap, r, g, b);
    }

    len = (int)SDL_strlen(text);
    if ((flags & 8) != 0) {
        x -= len * advance;
    } else if ((flags & 16) != 0) {
        x -= (advance * ((len << FRACBITS) / 512)) >> 8;
    }

    if ((flags & 2) != 0) {
        y -= height;
    } else if ((flags & 32) != 0) {
        y -= height >> 1;
    }

    xpos = x;
    for (i = 0; i < len; ++i) {
        c = (unsigned char)text[i];
        if (c == 10U) {
            y += height;
            xpos = x;
        } else {
            if (c != (unsigned int)' ') {
                DoomCanvas_drawImageSpecial(
                    doomCanvas,
                    imgFont,
                    width * ((c - 33U) & 0x0fU),
                    height * ((unsigned int)((c - 33U) << 24) >> 28),
                    width,
                    height,
                    0,
                    xpos,
                    y,
                    0);
            }
            xpos += advance;
        }
    }
}

static int doomCanvasBeginIntro(DoomCanvas_t* doomCanvas)
{
    if (doomCanvas == NULL || doomCanvas->doomRpg == NULL) {
        return 0;
    }

    DoomRPG_setColor(doomCanvas->doomRpg, 0x000000);
    DoomRPG_fillRect(doomCanvas->doomRpg,
                     0,
                     0,
                     DOOMRPG_CANVAS_WIDTH,
                     DOOMRPG_CANVAS_HEIGHT);
    DoomCanvas_drawString1(
        doomCanvas, "Loading...", DOOMRPG_CANVAS_CENTER_X, DOOMRPG_CANVAS_CENTER_Y, 17);
    DoomRPG_flushGraphics(doomCanvas->doomRpg);
    Sound_playSound(doomCanvas->doomRpg->sound,
                    5039,
                    ESP32_SND_FLG_LOOP | ESP32_SND_FLG_STOPSOUNDS |
                        ESP32_SND_FLG_ISMUSIC,
                    5);

    if (!EspNativeIntroState_begin(doomCanvas->doomRpg)) {
        Sound_stopSounds(doomCanvas->doomRpg->sound);
        printf("[DOOMCANVASBRIDGE] INTRO-REJECT owner=native-transient allocation/load failed\n");
        return 0;
    }

    DoomRPG_setColor(doomCanvas->doomRpg, 0x000000);
    DoomRPG_fillRect(doomCanvas->doomRpg,
                     0,
                     0,
                     DOOMRPG_CANVAS_WIDTH,
                     DOOMRPG_CANVAS_HEIGHT);
    DoomRPG_flushGraphics(doomCanvas->doomRpg);
    return 1;
}

void DoomCanvas_setState(DoomCanvas_t* doomCanvas, int stateNum)
{
    int oldState;

    if (doomCanvas == NULL) {
        return;
    }

    /*
     * ESP32 native owners handle gameplay dialogs, combat, automap, loading,
     * death, cast, credits and epilogue presentation. Only the three canvas
     * states still used as compatibility handoff markers remain accepted here.
     * Any future inherited state dependency must receive its own native owner
     * instead of silently widening this bridge.
     */
    if (stateNum != ST_MENU && stateNum != ST_PLAYING && stateNum != ST_INTRO) {
        printf("[DOOMCANVASBRIDGE] STATE-REJECT requested=%d current=%d owner=native\n",
               stateNum,
               doomCanvas->state);
        return;
    }

    oldState = doomCanvas->state;
    if (oldState == ST_MENU && stateNum != ST_MENU) {
        Sound_stopSounds(doomCanvas->doomRpg->sound);
    }

    if (stateNum == ST_INTRO && stateNum != oldState) {
        if (!doomCanvasBeginIntro(doomCanvas)) {
            return;
        }
    }

    doomCanvas->state = stateNum;

    if (stateNum == ST_MENU && oldState == ST_PLAYING) {
        (void)EspNativeAudioIntent_publish(5042U, 0U, 3U);
        (void)EspNativeAudioIntent_publish(5067U, 0U, 3U);
    }
}
void DoomCanvas_startup(DoomCanvas_t* doomCanvas)
{
    SDL_Rect screenRect;
    DoomRPG_t* doomRpg;

    if (doomCanvas == NULL || doomCanvas->doomRpg == NULL) return;

    doomRpg = doomCanvas->doomRpg;
    screenRect.x = DOOMRPG_VIEWPORT_X;
    screenRect.y = DOOMRPG_VIEWPORT_Y;
    screenRect.w = DOOMRPG_VIEWPORT_WIDTH;
    screenRect.h = DOOMRPG_VIEWPORT_HEIGHT;
    Render_setup(doomRpg->render, &screenRect);
    doomCanvas->startupMap = 1;
    doomCanvas->skipIntro = false;
    DoomRPG_createImage(doomCanvas->doomRpg, "a.bmp", true, &doomCanvas->imgFont);
    printf("[DOOMCANVASBRIDGE] STARTUP desktopTU=no display=%dx%d screen=%dx%d@%d,%d startupMap=%d hud=native geometry=fixed-cyd\n",
           DOOMRPG_CANVAS_WIDTH, DOOMRPG_CANVAS_HEIGHT,
           DOOMRPG_VIEWPORT_WIDTH, DOOMRPG_VIEWPORT_HEIGHT,
           DOOMRPG_VIEWPORT_X, DOOMRPG_VIEWPORT_Y,
           doomCanvas->startupMap);
}

#if defined(DOOMRPG_ESP32_BRINGUP_PROBES)
/* Historical bringup linker wrapper still references this retired visual
 * callback. No legacy progress UI owns the shared native framebuffer. */
void DoomCanvas_updateLoadingBar(DoomCanvas_t* doomCanvas) {
    (void)doomCanvas;
}
#endif
