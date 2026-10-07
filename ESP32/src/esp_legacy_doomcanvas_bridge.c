#include <SDL.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "DoomRPG.h"
#include "DoomCanvas.h"
#include "Player.h"
#include "Render.h"
#include "SDL_Video.h"

#include "esp_native_audio_intent.h"
#include "native_story_fit.h"
#include "platform_video_config.h"

#define ESP32_DOOMCANVAS_DESKTOP_BYTES 3740U
#define ESP32_DOOMCANVAS_RETIRED_DIALOG_BYTES (2048U + 512U)
#define ESP32_DOOMCANVAS_RETIRED_ZEROREF_FIELD_BYTES 239U
#define ESP32_DOOMCANVAS_RETIRED_ZEROREF_LAYOUT_BYTES 240U
#define ESP32_DOOMCANVAS_RETIRED_DORMANT_TEXT_BYTES (300U + 128U)
#define ESP32_DOOMCANVAS_COMPACT_BYTES \
    (ESP32_DOOMCANVAS_DESKTOP_BYTES - ESP32_DOOMCANVAS_RETIRED_DIALOG_BYTES - \
     ESP32_DOOMCANVAS_RETIRED_ZEROREF_LAYOUT_BYTES - \
     ESP32_DOOMCANVAS_RETIRED_DORMANT_TEXT_BYTES)

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

static char processing[] = "Processing...";
static char justAMoment[] = "(Just a moment!)";

static char storyTextA[] =
    "You have been\n"
    "dispatched in re - \n"
    "sponse to a dis - \n"
    "tress call from\n"
    "Union Aerospace\n"
    "Corporation's re-\n"
    "search facility\n"
    "on Mars. The base\n"
    "is under attack";

static char storyTextB[] =
    "by an unknown\n"
    "force and your\n"
    "mission is to ac-\n"
    "quire intelli-\n"
    "gence and neu-\n"
    "tralize the\n"
    "threat.";

static char storyTextC[] =
    "Insertion com-\n"
    "plete. For fur-\n"
    "ther instruc-\n"
    "tions, rendezvous\n"
    "with the other\n"
    "Marines at Junc-\n"
    "tion. Expect\n"
    "heavy resistance.\n"
    "Good luck!";

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
    doomCanvas->skipShakeX = false;
    doomCanvas->insufficientSpace = false;
    doomCanvas->creditsText = NULL;
    doomCanvas->castEntity = NULL;
    doomCanvas->oldState = -1;
    doomCanvas->imgFont.imgBitmap = NULL;
    doomCanvas->imgLargerFont.imgBitmap = NULL;
    doomCanvas->imgLegals.imgBitmap = NULL;
    doomCanvas->imgMapCursor.imgBitmap = NULL;
    doomCanvas->imgSpaceBG.imgBitmap = NULL;
    doomCanvas->imgLinesLayer.imgBitmap = NULL;
    doomCanvas->imgPlanetLayer.imgBitmap = NULL;
    doomCanvas->imgSpaceship.imgBitmap = NULL;
    doomCanvas->storyText1[0] = NULL;
    doomCanvas->storyText1[1] = NULL;
    doomCanvas->storyText2 = NULL;
    doomCanvas->softKeyRight[0] = '\0';
    doomCanvas->softKeyLeft[0] = '\0';
    doomCanvas->clipRect.x = 0;
    doomCanvas->clipRect.y = 0;
    doomCanvas->clipRect.w = sdlVideo.rendererW;
    doomCanvas->clipRect.h = sdlVideo.rendererH;
    doomCanvas->fontColor = 0xffffffff;
    doomCanvas->mouseSensitivity = 50;
    doomCanvas->mouseYMove = true;
    doomCanvas->vibrateEnabled = true;
    doomCanvas->renderFloorCeilingTextures = true;

    printf("[DOOMCANVASBRIDGE] INIT exports=14 desktopTU=no bytes=%u retiredDialogStores=%u retiredZeroRefLayout=%u retiredDormantText=%u clip=%dx%d\n",
           (unsigned int)sizeof(DoomCanvas_t),
           (unsigned int)ESP32_DOOMCANVAS_RETIRED_DIALOG_BYTES,
           (unsigned int)ESP32_DOOMCANVAS_RETIRED_ZEROREF_LAYOUT_BYTES,
           (unsigned int)ESP32_DOOMCANVAS_RETIRED_DORMANT_TEXT_BYTES,
           doomCanvas->clipRect.w,
           doomCanvas->clipRect.h);
    return doomCanvas;
}

void DoomCanvas_free(DoomCanvas_t* doomCanvas, boolean freePtr)
{
    if (doomCanvas == NULL) {
        return;
    }

    Esp32StoryFit_release(doomCanvas);
    DoomRPG_freeImage(doomCanvas->doomRpg, &doomCanvas->imgFont);
    DoomRPG_freeImage(doomCanvas->doomRpg, &doomCanvas->imgLargerFont);
    DoomRPG_freeImage(doomCanvas->doomRpg, &doomCanvas->imgLegals);
    DoomRPG_freeImage(doomCanvas->doomRpg, &doomCanvas->imgMapCursor);
    DoomRPG_freeImage(doomCanvas->doomRpg, &doomCanvas->imgSpaceBG);
    DoomRPG_freeImage(doomCanvas->doomRpg, &doomCanvas->imgLinesLayer);
    DoomRPG_freeImage(doomCanvas->doomRpg, &doomCanvas->imgPlanetLayer);
    DoomRPG_freeImage(doomCanvas->doomRpg, &doomCanvas->imgSpaceship);

    SDL_free(doomCanvas->storyText1[0]);
    SDL_free(doomCanvas->storyText1[1]);
    SDL_free(doomCanvas->storyText2);
    SDL_free(doomCanvas->creditsText);

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

                renderQuad.x = doomCanvas->displayRect.x + drawX;
                renderQuad.y = doomCanvas->displayRect.y + yDst;
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
    DoomCanvas_drawFont(doomCanvas, text, x, y, flags, 0, -1, false);
}

void DoomCanvas_drawFont(DoomCanvas_t* doomCanvas,
                         char* text,
                         int x,
                         int y,
                         int flags,
                         int strBeg,
                         int strEnd,
                         boolean isLargerFont)
{
    Image_t* imgFont;
    int advance;
    int width;
    int height;
    int len;
    int xpos;
    int i;
    unsigned int c;

    if (doomCanvas == NULL || text == NULL || strEnd == 0) {
        return;
    }

    if (!isLargerFont) {
        imgFont = &doomCanvas->imgFont;
        advance = 7;
        width = 9;
        height = 12;
    } else {
        imgFont = &doomCanvas->imgLargerFont;
        advance = 10;
        width = 13;
        height = 17;
    }

    if (imgFont->imgBitmap == NULL) {
        return;
    }

    {
        byte r = (byte)((doomCanvas->fontColor & 0x00ff0000) >> 16);
        byte g = (byte)((doomCanvas->fontColor & 0x0000ff00) >> 8);
        byte b = (byte)(doomCanvas->fontColor & 0x000000ff);
        SDL_SetTextureColorMod(imgFont->imgBitmap, r, g, b);
    }

    len = (int)SDL_strlen(text) - strBeg;
    if (len < 0) {
        return;
    }
    if (len > strEnd && strEnd >= 0) {
        len = strEnd;
    }

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

    len += strBeg;
    xpos = x;
    for (i = strBeg; i < len; ++i) {
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

void DoomCanvas_drawSoftKeys(DoomCanvas_t* doomCanvas,
                             char* softKeyLeft,
                             char* softKeyRight)
{
    int x;
    int y;
    int x1;
    int y1;

    if (doomCanvas == NULL || !doomCanvas->displaySoftKeys) {
        return;
    }

    doomCanvas->restoreSoftKeys = true;
    x1 = x = -doomCanvas->displayRect.x;
    y1 = y = doomCanvas->softKeyY - doomCanvas->displayRect.y;

    if (softKeyLeft == NULL) {
        doomCanvas->softKeyLeft[0] = '\0';
    } else {
        if (doomCanvas->softKeyLeft != softKeyLeft) {
            strncpy(doomCanvas->softKeyLeft,
                    softKeyLeft,
                    sizeof(doomCanvas->softKeyLeft));
            doomCanvas->softKeyLeft[sizeof(doomCanvas->softKeyLeft) - 1] = '\0';
        }

        DoomRPG_setColor(doomCanvas->doomRpg, 0x313131);
        DoomRPG_drawLine(doomCanvas->doomRpg, x1 + 52, y1, x1 + 52, y1 + 19);
        DoomRPG_setColor(doomCanvas->doomRpg, 0x808591);
        DoomRPG_drawLine(doomCanvas->doomRpg, x1 + 53, y1, x1 + 53, y1 + 19);
        DoomCanvas_drawString1(doomCanvas, softKeyLeft, x + 26, y + 5, 17);
    }

    if (softKeyRight == NULL) {
        doomCanvas->softKeyRight[0] = '\0';
    } else {
        if (doomCanvas->softKeyRight != softKeyRight) {
            strncpy(doomCanvas->softKeyRight,
                    softKeyRight,
                    sizeof(doomCanvas->softKeyRight));
            doomCanvas->softKeyRight[sizeof(doomCanvas->softKeyRight) - 1] = '\0';
        }

        DoomRPG_setColor(doomCanvas->doomRpg, 0x313131);
        x1 += doomCanvas->clipRect.w - 52;
        DoomRPG_drawLine(doomCanvas->doomRpg, x1, y1, x1, y1 + 19);
        DoomRPG_setColor(doomCanvas->doomRpg, 0x808591);
        DoomRPG_drawLine(doomCanvas->doomRpg, x1 + 1, y1, x1 + 1, y1 + 19);
        DoomCanvas_drawString1(
            doomCanvas, softKeyRight, doomCanvas->clipRect.w + x - 28, y + 5, 17);
    }
}

void DoomCanvas_initCredits(DoomCanvas_t* doomCanvas)
{
    if (doomCanvas == NULL) {
        return;
    }
    DoomRPG_createImage(doomCanvas->doomRpg, "c.bmp", false, &doomCanvas->imgSpaceBG);
    doomCanvas->creditsTextTime = -1;
}

void DoomCanvas_loadEpilogueText(DoomCanvas_t* doomCanvas)
{
    if (doomCanvas == NULL) {
        return;
    }

    /*
     * The desktop run loop consumed two 150-byte epilogue text pages directly
     * from DoomCanvas_t. That renderer/state machine is retired on ESP32.
     * Preserve the remaining compatibility lifecycle side effects here, but
     * keep no permanent text payload. Any future visible epilogue must be owned
     * by the native UI/string path rather than reviving desktop Canvas storage.
     */
    doomCanvas->epilogueTextPage = 0;
    doomCanvas->showTextDone = false;
    DoomRPG_createImage(doomCanvas->doomRpg, "c.bmp", false, &doomCanvas->imgSpaceBG);
    Sound_playSound(doomCanvas->doomRpg->sound,
                    5039,
                    ESP32_SND_FLG_LOOP | ESP32_SND_FLG_STOPSOUNDS |
                        ESP32_SND_FLG_ISMUSIC,
                    5);
    doomCanvas->epilogueTextTime = -1;
}
void DoomCanvas_loadPrologueText(DoomCanvas_t* doomCanvas)
{
    int textLen;

    if (doomCanvas == NULL) {
        return;
    }

    DoomRPG_setColor(doomCanvas->doomRpg, 0x000000);
    DoomRPG_fillRect(doomCanvas->doomRpg,
                     0,
                     0,
                     doomCanvas->displayRect.w,
                     doomCanvas->displayRect.h);
    DoomCanvas_drawString1(
        doomCanvas, "Loading...", doomCanvas->SCR_CX, doomCanvas->SCR_CY, 17);
    DoomRPG_flushGraphics(doomCanvas->doomRpg);
    Sound_playSound(doomCanvas->doomRpg->sound,
                    5039,
                    ESP32_SND_FLG_LOOP | ESP32_SND_FLG_STOPSOUNDS |
                        ESP32_SND_FLG_ISMUSIC,
                    5);

    textLen = (int)SDL_strlen(storyTextA);
    doomCanvas->storyText1[0] = SDL_calloc((size_t)textLen + 1U, sizeof(char));
    if (doomCanvas->storyText1[0] != NULL) {
        strncpy(doomCanvas->storyText1[0], storyTextA, (size_t)textLen);
    }

    textLen = (int)SDL_strlen(storyTextB);
    doomCanvas->storyText1[1] = SDL_calloc((size_t)textLen + 1U, sizeof(char));
    if (doomCanvas->storyText1[1] != NULL) {
        strncpy(doomCanvas->storyText1[1], storyTextB, (size_t)textLen);
    }

    textLen = (int)SDL_strlen(storyTextC);
    doomCanvas->storyText2 = SDL_calloc((size_t)textLen + 1U, sizeof(char));
    if (doomCanvas->storyText2 != NULL) {
        strncpy(doomCanvas->storyText2, storyTextC, (size_t)textLen);
    }

    DoomRPG_createImage(doomCanvas->doomRpg, "c.bmp", false, &doomCanvas->imgSpaceBG);
    DoomRPG_createImage(doomCanvas->doomRpg, "d.bmp", true, &doomCanvas->imgLinesLayer);
    DoomRPG_createImage(doomCanvas->doomRpg, "e.bmp", true, &doomCanvas->imgPlanetLayer);
    DoomRPG_createImage(doomCanvas->doomRpg, "f.bmp", true, &doomCanvas->imgSpaceship);
    doomCanvas->storyTextTime = -1;
    doomCanvas->storyAnimTime = -1;
    doomCanvas->showTextDone = false;
    doomCanvas->storyPage = 0;
    doomCanvas->storyTextPage = 0;

    DoomRPG_setColor(doomCanvas->doomRpg, 0x000000);
    DoomRPG_fillRect(doomCanvas->doomRpg,
                     0,
                     0,
                     doomCanvas->displayRect.w,
                     doomCanvas->displayRect.h);
    DoomRPG_flushGraphics(doomCanvas->doomRpg);
}

void DoomCanvas_renderScene(DoomCanvas_t* doomCanvas, int x, int y, int angle)
{
    if (doomCanvas == NULL || doomCanvas->render == NULL) {
        return;
    }

    doomCanvas->lastFrameTime = doomCanvas->time;
    doomCanvas->beforeRender = (int)DoomRPG_GetUpTimeMS();
    Render_render(doomCanvas->render, x, y, doomCanvas->viewZ, angle);
    doomCanvas->afterRender = (int)DoomRPG_GetUpTimeMS();
}

void DoomCanvas_setAnimFrames(DoomCanvas_t* doomCanvas, int frames)
{
    if (doomCanvas == NULL || frames <= 0) {
        return;
    }

    doomCanvas->animFrames = frames;
    doomCanvas->animPos = ((64 + frames) - 1) / frames;
    doomCanvas->animAngle = ((64 + frames) - 1) / frames;
}

void DoomCanvas_setState(DoomCanvas_t* doomCanvas, int stateNum)
{
    int oldState;
    int len;
    int width;
    char* msg;

    if (doomCanvas == NULL) {
        return;
    }

    if (doomCanvas->state == ST_AUTOMAP) {
        doomCanvas->isUpdateView = true;
        DoomRPG_setColor(doomCanvas->doomRpg, 0x000000);
        DoomRPG_fillRect(
            doomCanvas->doomRpg, 0, 0, doomCanvas->clipRect.w, doomCanvas->clipRect.h);

        if (stateNum == ST_DIALOG || stateNum == ST_DIALOGPASSWORD) {
            if (doomCanvas->render != NULL) {
                doomCanvas->render->skipStretch = false;
            }
            DoomCanvas_renderScene(
                doomCanvas, doomCanvas->viewX, doomCanvas->viewY, doomCanvas->viewAngle);
        }
    } else if (doomCanvas->state == ST_MENU) {
        if (stateNum == ST_MENU) {
            if (doomCanvas->unloadMedia) {
                DoomRPG_setColor(doomCanvas->doomRpg, 0x000000);
                DoomRPG_fillRect(doomCanvas->doomRpg,
                                 0,
                                 0,
                                 doomCanvas->clipRect.w,
                                 doomCanvas->clipRect.h);
            }
        } else {
            Sound_stopSounds(doomCanvas->doomRpg->sound);
        }
    }

    oldState = doomCanvas->state;
    doomCanvas->state = stateNum;
    if (stateNum != oldState) {
        doomCanvas->restoreSoftKeys = false;
    }

    if (stateNum == ST_SORRY) {
        DoomRPG_createImage(doomCanvas->doomRpg, "c.bmp", false, &doomCanvas->imgSpaceBG);
    } else if (stateNum == ST_COMBAT) {
        DoomCanvas_drawSoftKeys(doomCanvas, NULL, NULL);
        doomCanvas->combatDone = false;
    } else if (stateNum == ST_PLAYING) {
        DoomCanvas_drawSoftKeys(doomCanvas, "Menu", "Map");
        doomCanvas->skipCheckState = true;
    } else if (stateNum == ST_DIALOG || stateNum == ST_DIALOGPASSWORD) {
        DoomCanvas_drawSoftKeys(doomCanvas, NULL, NULL);
        doomCanvas->passwordTime = 0;
        doomCanvas->numEvents = 0;
    } else if (stateNum == ST_DYING) {
        DoomCanvas_drawSoftKeys(doomCanvas, NULL, NULL);
        doomCanvas->deathTime = doomCanvas->time;
        return;
    } else if (stateNum == ST_EPILOGUE) {
        DoomCanvas_drawSoftKeys(doomCanvas, NULL, NULL);
        DoomCanvas_loadEpilogueText(doomCanvas);
    } else if (stateNum == ST_CREDITS) {
        DoomCanvas_initCredits(doomCanvas);
    } else if (stateNum == ST_INTRO) {
        DoomCanvas_drawSoftKeys(doomCanvas, NULL, NULL);
        DoomCanvas_loadPrologueText(doomCanvas);
    } else if (stateNum == ST_LOADING || stateNum == ST_SAVING) {
        DoomRPG_setColor(doomCanvas->doomRpg, 0x000000);
        DoomRPG_fillRect(
            doomCanvas->doomRpg, 0, 0, doomCanvas->clipRect.w, doomCanvas->softKeyY);

        len = (int)SDL_strlen(justAMoment);
        width = len * 7 + 10;

        DoomRPG_setColor(doomCanvas->doomRpg, 0xffffff);
        DoomRPG_drawRect(doomCanvas->doomRpg,
                         doomCanvas->SCR_CX - (width >> 1),
                         doomCanvas->SCR_CY - 24,
                         width,
                         48);

        /*
         * No compiled ESP32 owner writes the retired desktop printMsg buffer.
         * The observable compatibility path therefore always used the fallback.
         */
        msg = processing;
        DoomCanvas_drawString1(
            doomCanvas, msg, doomCanvas->SCR_CX, doomCanvas->SCR_CY - 12, 0x11);
        DoomCanvas_drawString1(
            doomCanvas, justAMoment, doomCanvas->SCR_CX, doomCanvas->SCR_CY, 0x11);
        DoomCanvas_drawSoftKeys(doomCanvas, NULL, NULL);
        DoomRPG_flushGraphics(doomCanvas->doomRpg);
    } else if (stateNum == ST_PARTICLE) {
        doomCanvas->skipCheckState = true;
    } else if (stateNum == ST_AUTOMAP) {
        doomCanvas->f438d =
            doomCanvas->openDoorsCount > 0 || doomCanvas->isUpdateView;
        doomCanvas->automapDrawn = false;
        doomCanvas->staleView = true;
        doomCanvas->isUpdateView = true;
        DoomCanvas_drawSoftKeys(doomCanvas, "Menu", "Leave");
    } else if (stateNum == ST_CAST) {
        doomCanvas->castSeq = -1;
        doomCanvas->castTime = 0;
        doomCanvas->castEntity = NULL;
        doomCanvas->castEntityX = 0;
        doomCanvas->castEntityY = 28;
    } else if (stateNum == ST_MENU) {
        if (oldState == ST_PLAYING) {
            (void)EspNativeAudioIntent_publish(5042U, 0U, 3U);
            (void)EspNativeAudioIntent_publish(5067U, 0U, 3U);
        }
    }
}

void DoomCanvas_startup(DoomCanvas_t* doomCanvas)
{
    int frames;
    int map;
    int width;
    int height;
    int displayH;
    int clipH;
    int deltaH;
    int softKeyY;
    DoomRPG_t* doomRpg;

    if (doomCanvas == NULL || doomCanvas->doomRpg == NULL) {
        return;
    }

    doomRpg = doomCanvas->doomRpg;
    doomCanvas->render = doomRpg->render;
    doomCanvas->player = doomRpg->player;
    doomCanvas->game = doomRpg->game;
    doomCanvas->entityDef = doomRpg->entityDef;
    doomCanvas->combat = doomRpg->combat;
    doomCanvas->hud = NULL;
    doomCanvas->menuSystem = doomRpg->menuSystem;
    doomCanvas->particleSystem = doomRpg->particleSystem;

    doomCanvas->displayRect.w = 0;
    doomCanvas->displayRect.h = 0;
    width = doomCanvas->clipRect.w;
    if ((width & 1) != 0) {
        doomCanvas->clipRect.w = width - 1;
    }

    doomCanvas->displayRect.w = doomCanvas->clipRect.w;
    doomCanvas->displayRect.h = doomCanvas->clipRect.h;
    if (doomCanvas->displayRect.w < 0x80) {
        doomCanvas->displayRect.w = 0x80;
    }
    if (doomCanvas->displayRect.h < DOOMRPG_LOGICAL_HEIGHT) {
        doomCanvas->displayRect.h = DOOMRPG_LOGICAL_HEIGHT;
    }

    doomCanvas->softKeyY = doomCanvas->clipRect.h - 20;
    doomCanvas->largeStatus = doomCanvas->displayRect.w >= 176;

    displayH = doomCanvas->displayRect.h;
    clipH = doomCanvas->clipRect.h;
    height = displayH - 40;

    if (sdlVideo.displaySoftKeys && clipH >= 148) {
        deltaH = clipH - displayH;
        if (deltaH < 20) {
            height -= 20 - deltaH;
        }
        doomCanvas->displaySoftKeys = true;
    }

    if ((height & 1) != 0) {
        --height;
    }

    doomCanvas->displayRect.h = 20 + height + 20;
    doomCanvas->displayRect.x =
        (doomCanvas->clipRect.w - doomCanvas->displayRect.w +
         (doomCanvas->clipRect.w < doomCanvas->displayRect.w)) /
        2;

    if (doomCanvas->displaySoftKeys) {
        clipH = doomCanvas->softKeyY;
    }

    doomCanvas->displayRect.y = (clipH - doomCanvas->displayRect.h) / 2;
    doomCanvas->SCR_CY = doomCanvas->displayRect.h / 2;
    doomCanvas->SCR_CX = doomCanvas->displayRect.w / 2;
    doomCanvas->screenRect.x = doomCanvas->displayRect.x;
    doomCanvas->screenRect.y = doomCanvas->displayRect.y + 20;
    doomCanvas->screenRect.w = doomCanvas->displayRect.w;
    doomCanvas->screenRect.h = height;

    Render_setup(doomCanvas->render, &doomCanvas->screenRect);

    softKeyY = doomCanvas->softKeyY - 1;
    if (doomCanvas->displayRect.y + doomCanvas->displayRect.h == softKeyY) {
        doomCanvas->softKeyY = softKeyY;
    }

    frames = 4;
    DoomCanvas_setAnimFrames(doomCanvas, frames);

    map = 1;
    doomCanvas->startupMap = (short)map;
    doomCanvas->skipIntro = false;
    doomCanvas->skipShakeX = false;
    doomCanvas->sndFXOnly = false;

    DoomRPG_createImage(doomCanvas->doomRpg, "a.bmp", true, &doomCanvas->imgFont);
    DoomRPG_createImage(
        doomCanvas->doomRpg, "larger_font.bmp", true, &doomCanvas->imgLargerFont);
    DoomRPG_createImage(doomCanvas->doomRpg, "b.bmp", true, &doomCanvas->imgMapCursor);

    printf("[DOOMCANVASBRIDGE] STARTUP desktopTU=no display=%dx%d screen=%dx%d@%d,%d startupMap=%d hud=native\n",
           doomCanvas->displayRect.w,
           doomCanvas->displayRect.h,
           doomCanvas->screenRect.w,
           doomCanvas->screenRect.h,
           doomCanvas->screenRect.x,
           doomCanvas->screenRect.y,
           doomCanvas->startupMap);
}

void DoomCanvas_invalidateRectAndUpdateView(DoomCanvas_t* doomCanvas)
{
    if (doomCanvas == NULL) {
        return;
    }
    doomCanvas->staleView = true;
    doomCanvas->isUpdateView = true;
}
