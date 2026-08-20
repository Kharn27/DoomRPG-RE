#include <SDL.h>
#include <stdint.h>
#include <stdio.h>

#include "DoomRPG.h"
#include "DoomCanvas.h"
#include "MenuSystem.h"
#include "SDL_Video.h"

#include "native_story_fit.h"

#define STORY_FONT_ADVANCE 7
#define STORY_FONT_WIDTH 9
#define STORY_FONT_HEIGHT 12

static int geometryLogged;

static int scaleOffsetFloor(int value) {
    const int64_t numerator =
        (int64_t)value * (int64_t)ESP32_STORY_VIEWPORT_SIZE;

    if (numerator >= 0) {
        return (int)(numerator / ESP32_STORY_VIRTUAL_SIZE);
    }

    return -(int)((-numerator + ESP32_STORY_VIRTUAL_SIZE - 1) /
                  ESP32_STORY_VIRTUAL_SIZE);
}

static int virtualLeft(const DoomCanvas_t* doomCanvas) {
    return doomCanvas->SCR_CX - (ESP32_STORY_VIRTUAL_SIZE / 2);
}

static int virtualTop(const DoomCanvas_t* doomCanvas) {
    return doomCanvas->SCR_CY - (ESP32_STORY_VIRTUAL_SIZE / 2);
}

static int mapX(const DoomCanvas_t* doomCanvas, int x) {
    return ESP32_STORY_VIEWPORT_X +
           scaleOffsetFloor(x - virtualLeft(doomCanvas));
}

static int mapY(const DoomCanvas_t* doomCanvas, int y) {
    return ESP32_STORY_VIEWPORT_Y +
           scaleOffsetFloor(y - virtualTop(doomCanvas));
}

static void setVirtualClip(DoomCanvas_t* doomCanvas,
                           int x, int y, int width, int height) {
    const int left = mapX(doomCanvas, x);
    const int top = mapY(doomCanvas, y);
    const int right = mapX(doomCanvas, x + width);
    const int bottom = mapY(doomCanvas, y + height);

    DoomRPG_setClipTrue(doomCanvas->doomRpg,
                        left,
                        top,
                        right - left,
                        bottom - top);
}

static void drawImageSpecial(DoomCanvas_t* doomCanvas,
                             Image_t* img,
                             int xSrc,
                             int ySrc,
                             int width,
                             int height,
                             int xDst,
                             int yDst,
                             int flags) {
    SDL_Rect source;
    SDL_Rect destination;
    int left;
    int top;
    int right;
    int bottom;

    if (img == NULL || img->imgBitmap == NULL) {
        return;
    }

    if (width == 0) {
        width = img->width;
    }
    if (height == 0) {
        height = img->height;
    }

    if ((flags & 16) == 0) {
        if (flags & 8) {
            xDst -= width;
        }
    }
    else {
        xDst -= width >> 1;
    }

    if ((flags & 32) == 0) {
        if (flags & 2) {
            yDst -= height;
        }
    }
    else {
        yDst -= height >> 1;
    }

    left = mapX(doomCanvas, xDst);
    top = mapY(doomCanvas, yDst);
    right = mapX(doomCanvas, xDst + width);
    bottom = mapY(doomCanvas, yDst + height);

    if (right <= left || bottom <= top) {
        return;
    }

    source.x = xSrc;
    source.y = ySrc;
    source.w = width;
    source.h = height;

    destination.x = doomCanvas->displayRect.x + left;
    destination.y = doomCanvas->displayRect.y + top;
    destination.w = right - left;
    destination.h = bottom - top;

    SDL_RenderCopy(sdlVideo.renderer,
                   img->imgBitmap,
                   &source,
                   &destination);
}

static void drawImage(DoomCanvas_t* doomCanvas,
                      Image_t* img,
                      int x,
                      int y,
                      int flags) {
    drawImageSpecial(doomCanvas, img, 0, 0, 0, 0, x, y, flags);
}

static void drawFont(DoomCanvas_t* doomCanvas,
                     char* text,
                     int x,
                     int y,
                     int flags,
                     int strBeg,
                     int strEnd) {
    Image_t* imgFont = &doomCanvas->imgFont;
    int len;
    int xpos;
    int i;
    byte r;
    byte g;
    byte b;

    if (text == NULL || imgFont->imgBitmap == NULL || strEnd == 0) {
        return;
    }

    r = (byte)((doomCanvas->fontColor & 0x00ff0000) >> 16);
    g = (byte)((doomCanvas->fontColor & 0x0000ff00) >> 8);
    b = (byte)(doomCanvas->fontColor & 0x000000ff);
    SDL_SetTextureColorMod(imgFont->imgBitmap, r, g, b);

    len = (int)SDL_strlen(text) - strBeg;
    if (len < 0) {
        return;
    }
    if (strEnd >= 0 && len > strEnd) {
        len = strEnd;
    }

    if (flags & 8) {
        x -= len * STORY_FONT_ADVANCE;
    }
    else if (flags & 16) {
        x -= (len * STORY_FONT_ADVANCE) / 2;
    }

    if (flags & 2) {
        y -= STORY_FONT_HEIGHT;
    }
    else if (flags & 32) {
        y -= STORY_FONT_HEIGHT / 2;
    }

    len += strBeg;
    xpos = x;

    for (i = strBeg; i < len; ++i) {
        const unsigned int c = (unsigned char)text[i];

        if (c == '\n') {
            y += STORY_FONT_HEIGHT;
            xpos = x;
            continue;
        }

        if (c != ' ') {
            const int glyph = (int)c - 33;

            drawImageSpecial(doomCanvas,
                             imgFont,
                             STORY_FONT_WIDTH * (glyph & 0x0f),
                             STORY_FONT_HEIGHT * ((glyph >> 4) & 0x0f),
                             STORY_FONT_WIDTH,
                             STORY_FONT_HEIGHT,
                             xpos,
                             y,
                             0);
        }

        xpos += STORY_FONT_ADVANCE;
    }
}

static void drawString1(DoomCanvas_t* doomCanvas,
                        char* text,
                        int x,
                        int y,
                        int flags) {
    drawFont(doomCanvas, text, x, y, flags, 0, -1);
}

static void drawString2(DoomCanvas_t* doomCanvas,
                        char* text,
                        int x,
                        int y,
                        int flags,
                        int startTime) {
    drawFont(doomCanvas,
             text,
             x,
             y,
             flags,
             0,
             (doomCanvas->time - startTime) / 25);
}

static void scrollSpaceBG(DoomCanvas_t* doomCanvas) {
    const int i = -((doomCanvas->time / 157) % 192);
    int i2 = i;
    const int i3 = i + 192;
    const int left = virtualLeft(doomCanvas);
    const int top = virtualTop(doomCanvas);

    if (i2 <= -192) {
        i2 += 384;
    }

    drawImage(doomCanvas,
              &doomCanvas->imgSpaceBG,
              left - i2,
              top,
              0);
    drawImage(doomCanvas,
              &doomCanvas->imgSpaceBG,
              left - i3,
              top,
              0);
}

static void drawMappedLine(DoomCanvas_t* doomCanvas,
                           int x1,
                           int y1,
                           int x2,
                           int y2) {
    DoomRPG_drawLine(doomCanvas->doomRpg,
                     mapX(doomCanvas, x1),
                     mapY(doomCanvas, y1),
                     mapX(doomCanvas, x2),
                     mapY(doomCanvas, y2));
}

void Esp32StoryFit_draw(struct DoomCanvas_s* doomCanvasBase) {
    DoomCanvas_t* doomCanvas = (DoomCanvas_t*)doomCanvasBase;
    char** text;
    int textPageCount;
    int elapsedAnim;
    int elapsedText;
    const int left = virtualLeft(doomCanvas);
    const int top = virtualTop(doomCanvas);

    if (!geometryLogged) {
        printf("[INTROFIT] virtual=128x128 -> viewport=%dx%d@(%d,%d) direct-to-framebuffer; no intermediate buffer\n",
               ESP32_STORY_VIEWPORT_SIZE,
               ESP32_STORY_VIEWPORT_SIZE,
               ESP32_STORY_VIEWPORT_X,
               ESP32_STORY_VIEWPORT_Y);
        geometryLogged = 1;
    }

    if (doomCanvas->storyAnimTime == -1) {
        doomCanvas->storyAnimTime = DoomRPG_GetUpTimeMS();
    }
    if (doomCanvas->storyTextTime == -1) {
        doomCanvas->storyTextTime = DoomRPG_GetUpTimeMS();
    }

    if (doomCanvas->time < doomCanvas->storyTextTime ||
        doomCanvas->time < doomCanvas->storyAnimTime) {
        return;
    }

    elapsedAnim = doomCanvas->time - doomCanvas->storyAnimTime;
    elapsedText = doomCanvas->time - doomCanvas->storyTextTime;

    if (doomCanvas->doomRpg->graphSetCliping) {
        DoomRPG_setClipFalse(doomCanvas->doomRpg);
    }

    DoomRPG_setColor(doomCanvas->doomRpg, 0x000000);
    DoomRPG_fillRect(doomCanvas->doomRpg,
                     0,
                     0,
                     doomCanvas->displayRect.w,
                     doomCanvas->displayRect.h);

    setVirtualClip(doomCanvas,
                   left,
                   top,
                   ESP32_STORY_VIRTUAL_SIZE,
                   ESP32_STORY_VIRTUAL_SIZE);

    if (doomCanvas->storyPage == 0 || doomCanvas->storyPage == 2) {
        if (doomCanvas->storyPage == 0) {
            text = doomCanvas->storyText1;
            textPageCount = 2;
        }
        else {
            text = &doomCanvas->storyText2;
            textPageCount = 1;
        }

        if (textPageCount <= doomCanvas->storyTextPage) {
            DoomCanvas_changeStoryPage(doomCanvas);
            return;
        }

        scrollSpaceBG(doomCanvas);

        if (doomCanvas->showTextDone) {
            drawString2(doomCanvas,
                        text[doomCanvas->storyTextPage],
                        left,
                        top,
                        0,
                        -1);
        }
        else {
            drawString2(doomCanvas,
                        text[doomCanvas->storyTextPage],
                        left,
                        top,
                        0,
                        doomCanvas->storyTextTime);
        }

        if (doomCanvas->storyTextPage < textPageCount - 1) {
            drawImage(doomCanvas,
                      &doomCanvas->doomRpg->menuSystem->imgHand,
                      (doomCanvas->SCR_CX + 36) - 4,
                      (doomCanvas->SCR_CY + 64) - 2,
                      10);
            drawString1(doomCanvas,
                        "More",
                        (doomCanvas->SCR_CX + 64) - 4,
                        doomCanvas->SCR_CY + 64,
                        10);
        }
        else {
            drawImage(doomCanvas,
                      &doomCanvas->doomRpg->menuSystem->imgHand,
                      (doomCanvas->SCR_CX + 8) - 4,
                      (doomCanvas->SCR_CY + 64) - 2,
                      10);
            drawString1(doomCanvas,
                        "Continue",
                        (doomCanvas->SCR_CX + 64) - 4,
                        doomCanvas->SCR_CY + 64,
                        10);
        }

        if (elapsedText >
            ((int)SDL_strlen(text[doomCanvas->storyTextPage]) * 25)) {
            doomCanvas->showTextDone = true;
        }

        return;
    }

    if (elapsedAnim > 10000) {
        DoomCanvas_changeStoryPage(doomCanvas);
    }

    {
        const int bgOffset = elapsedAnim / 457;
        const int linesOffset = elapsedAnim / 157;
        const int shipX = left + (elapsedAnim / 142);
        const int shipY = (doomCanvas->SCR_CY + 22) + (elapsedAnim / -333);

        drawImage(doomCanvas,
                  &doomCanvas->imgSpaceBG,
                  left - bgOffset,
                  top,
                  0);
        drawImage(doomCanvas,
                  &doomCanvas->imgLinesLayer,
                  left - linesOffset,
                  top,
                  0);
        drawImage(doomCanvas,
                  &doomCanvas->imgPlanetLayer,
                  left,
                  top,
                  0);
        drawImage(doomCanvas,
                  &doomCanvas->imgSpaceship,
                  shipX,
                  shipY,
                  0);

        if ((elapsedAnim / 500) % 2 == 0) {
            setVirtualClip(doomCanvas,
                           left + 1,
                           top + 1,
                           126,
                           126);

            DoomRPG_setColor(doomCanvas->doomRpg, 0xBB0000);
            drawMappedLine(doomCanvas,
                           shipX,
                           shipY - 1,
                           shipX + 9,
                           shipY - 1);
            drawMappedLine(doomCanvas,
                           shipX + 4,
                           0,
                           shipX + 4,
                           shipY - 1);
            drawMappedLine(doomCanvas,
                           shipX,
                           shipY + 9,
                           shipX + 9,
                           shipY + 9);
            drawMappedLine(doomCanvas,
                           shipX + 4,
                           shipY + 9,
                           shipX + 4,
                           doomCanvas->displayRect.h);
            drawMappedLine(doomCanvas,
                           shipX - 1,
                           shipY,
                           shipX - 1,
                           shipY + 9);
            drawMappedLine(doomCanvas,
                           0,
                           shipY + 4,
                           shipX - 1,
                           shipY + 4);
            drawMappedLine(doomCanvas,
                           shipX + 9,
                           shipY,
                           shipX + 9,
                           shipY + 9);
            drawMappedLine(doomCanvas,
                           shipX + 9,
                           shipY + 4,
                           doomCanvas->displayRect.w,
                           shipY + 4);
        }
    }
}
