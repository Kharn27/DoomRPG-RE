#include <SDL.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "DoomRPG.h"
#include "DoomCanvas.h"
#include "Render.h"
#include "SDL_Video.h"

#include "esp_native_canvas_text.h"
#include "esp_native_intro_state.h"
#include "native_story_fit.h"
#include "platform_video_config.h"

/*
 * Permanent ESP32 compatibility bridge for the small DoomCanvas ABI that
 * survived the hardware-proven linked-root audit.
 *
 * Desktop DoomCanvas.c is intentionally not compiled for ESP32. Native owners
 * handle gameplay, menus, dialogs, world mutation and presentation. This file
 * keeps only the final legacy free hook and a private text rasterizer
 * while native root and layout own construction and startup.
 */

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

/* Private glyph blitter, called solely by the const-safe native text entry.
 * Keep clipping/tiling semantics exact. */
static void EspDoomCanvas_drawImageSpecial(DoomCanvas_t* doomCanvas,
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

void EspNativeCanvas_drawText(DoomCanvas_t* doomCanvas,
                              const char* text,
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
                EspDoomCanvas_drawImageSpecial(
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

#if defined(DOOMRPG_ESP32_BRINGUP_PROBES)
/* Historical bringup linker wrapper still references this retired visual
 * callback. No legacy progress UI owns the shared native framebuffer. */
void DoomCanvas_updateLoadingBar(DoomCanvas_t* doomCanvas) {
    (void)doomCanvas;
}
#endif
