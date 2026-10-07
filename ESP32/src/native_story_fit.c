#include <SDL.h>
#include <stdint.h>
#include <stdio.h>

#include "DoomRPG.h"
#include "DoomCanvas.h"
#include "MenuSystem.h"
#include "SDL_Video.h"

#include "esp_native_intro_state.h"
#include "native_story_fit.h"
#include "platform_video_config.h"

#define STORY_FONT_ADVANCE 7
#define STORY_FONT_WIDTH 9
#define STORY_FONT_HEIGHT 12

static int geometryLogged;
static Image_t storyHand;
static DoomRPG_t* storyHandOwner;

static Image_t* acquireStoryHand(DoomRPG_t* doomRpg) {
    if (doomRpg == NULL) return NULL;
    if (storyHand.imgBitmap != NULL && storyHandOwner == doomRpg) {
        return &storyHand;
    }
    if (storyHand.imgBitmap != NULL && storyHandOwner != NULL) {
        DoomRPG_freeImage(storyHandOwner, &storyHand);
        SDL_memset(&storyHand, 0, sizeof(storyHand));
    }
    DoomRPG_createImage(doomRpg, "p.bmp", true, &storyHand);
    if (storyHand.imgBitmap == NULL) {
        storyHandOwner = NULL;
        printf("[INTROFIT] HAND-FAILED asset=p.bmp owner=native-story\n");
        return NULL;
    }
    storyHandOwner = doomRpg;
    printf("[INTROFIT] HAND-READY asset=p.bmp bytes=bounded owner=native-story\n");
    return &storyHand;
}

int Esp32StoryFit_prepare(struct DoomCanvas_s* doomCanvasBase) {
    DoomCanvas_t* doomCanvas = (DoomCanvas_t*)doomCanvasBase;
    uint32_t before;
    uint32_t after;
    Image_t* hand;
    if (doomCanvas == NULL || doomCanvas->doomRpg == NULL) return 0;
    before = (uint32_t)SDL_GetTicks();
    hand = acquireStoryHand(doomCanvas->doomRpg);
    after = (uint32_t)SDL_GetTicks();
    if (hand == NULL) return 0;
    printf("[INTROFIT] PREPARE hand=%dx%d asset=p.bmp owner=native-story elapsedMs=%u drawAllocation=no\n",
           hand->width, hand->height, (unsigned int)(after - before));
    return 1;
}

int Esp32StoryFit_hasHand(void) {
    return storyHand.imgBitmap != NULL;
}

void Esp32StoryFit_release(struct DoomCanvas_s* doomCanvasBase) {
    DoomCanvas_t* doomCanvas = (DoomCanvas_t*)doomCanvasBase;
    DoomRPG_t* owner = storyHandOwner;
    if (owner == NULL && doomCanvas != NULL) owner = doomCanvas->doomRpg;
    if (storyHand.imgBitmap != NULL && owner != NULL) {
        DoomRPG_freeImage(owner, &storyHand);
        printf("[INTROFIT] HAND-RELEASE asset=p.bmp owner=native-story\n");
    }
    SDL_memset(&storyHand, 0, sizeof(storyHand));
    storyHandOwner = NULL;
}

static int advanceAnimationPageBounded(EspNativeIntroState_t* introState) {
    if (introState == NULL || introState->storyPage != 1) {
        return 0;
    }

    introState->storyPage = 2;
    introState->storyTextPage = 0;
    introState->storyAnimTime = -1;
    introState->storyTextTime = -1;
    return 1;
}

static int scaleOffsetFloor(int value, int viewportSize) {
    const int64_t numerator =
        (int64_t)value * (int64_t)viewportSize;

    if (numerator >= 0) {
        return (int)(numerator / ESP32_STORY_VIRTUAL_SIZE);
    }

    return -(int)((-numerator + ESP32_STORY_VIRTUAL_SIZE - 1) /
                  ESP32_STORY_VIRTUAL_SIZE);
}

static int virtualLeft(const DoomCanvas_t* doomCanvas) {
    return DOOMRPG_CANVAS_CENTER_X - (ESP32_STORY_VIRTUAL_SIZE / 2);
}

static int virtualTop(const DoomCanvas_t* doomCanvas) {
    return DOOMRPG_CANVAS_CENTER_Y - (ESP32_STORY_VIRTUAL_SIZE / 2);
}

static int mapX(const DoomCanvas_t* doomCanvas, int x) {
    return ESP32_STORY_VIEWPORT_X +
           scaleOffsetFloor(x - virtualLeft(doomCanvas),
                            ESP32_STORY_VIEWPORT_SIZE);
}

static int mapBackgroundX(const DoomCanvas_t* doomCanvas, int x) {
    return ESP32_STORY_BACKGROUND_X +
           scaleOffsetFloor(x - virtualLeft(doomCanvas),
                            ESP32_STORY_BACKGROUND_WIDTH);
}

static int mapTextX(const DoomCanvas_t* doomCanvas, int x) {
    return ESP32_STORY_TEXT_X +
           scaleOffsetFloor(x - virtualLeft(doomCanvas),
                            ESP32_STORY_TEXT_WIDTH);
}

static int mapY(const DoomCanvas_t* doomCanvas, int y) {
    return ESP32_STORY_VIEWPORT_Y +
           scaleOffsetFloor(y - virtualTop(doomCanvas),
                            ESP32_STORY_VIEWPORT_SIZE);
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

static void setAnimationVirtualClip(DoomCanvas_t* doomCanvas,
                                    int x, int y, int width, int height) {
    const int left = ESP32_STORY_ANIMATION_X +
                     scaleOffsetFloor(x - virtualLeft(doomCanvas),
                                      ESP32_STORY_ANIMATION_WIDTH);
    const int top = mapY(doomCanvas, y);
    const int right = ESP32_STORY_ANIMATION_X +
                      scaleOffsetFloor(x + width - virtualLeft(doomCanvas),
                                       ESP32_STORY_ANIMATION_WIDTH);
    const int bottom = mapY(doomCanvas, y + height);

    DoomRPG_setClipTrue(doomCanvas->doomRpg,
                        left,
                        top,
                        right - left,
                        bottom - top);
}

static void drawImageSpecialMapped(DoomCanvas_t* doomCanvas,
                                   Image_t* img,
                                   int xSrc,
                                   int ySrc,
                                   int width,
                                   int height,
                                   int xDst,
                                   int yDst,
                                   int flags,
                                   int wideBackground) {
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

    left = wideBackground ? mapBackgroundX(doomCanvas, xDst)
                          : mapX(doomCanvas, xDst);
    top = mapY(doomCanvas, yDst);
    right = wideBackground ? mapBackgroundX(doomCanvas, xDst + width)
                           : mapX(doomCanvas, xDst + width);
    bottom = mapY(doomCanvas, yDst + height);

    if (right <= left || bottom <= top) {
        return;
    }

    source.x = xSrc;
    source.y = ySrc;
    source.w = width;
    source.h = height;

    destination.x = DOOMRPG_CANVAS_X + left;
    destination.y = DOOMRPG_CANVAS_Y + top;
    destination.w = right - left;
    destination.h = bottom - top;

    SDL_RenderCopy(sdlVideo.renderer,
                   img->imgBitmap,
                   &source,
                   &destination);
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
    drawImageSpecialMapped(doomCanvas, img, xSrc, ySrc, width, height,
                           xDst, yDst, flags, 0);
}

static void drawImage(DoomCanvas_t* doomCanvas,
                      Image_t* img,
                      int x,
                      int y,
                      int flags) {
    drawImageSpecialMapped(doomCanvas, img, 0, 0, 0, 0, x, y, flags, 0);
}

static void drawBackgroundImage(DoomCanvas_t* doomCanvas,
                                Image_t* img,
                                int x,
                                int y) {
    drawImageSpecialMapped(doomCanvas, img, 0, 0, 0, 0, x, y, 0, 1);
}

static void drawAnimationImage(DoomCanvas_t* doomCanvas,
                               Image_t* img,
                               int x,
                               int y,
                               int flags) {
    drawImageSpecialMapped(doomCanvas, img, 0, 0, 0, 0, x, y, flags, 1);
}

static void drawTextGlyph(DoomCanvas_t* doomCanvas,
                          Image_t* img,
                          int xSrc,
                          int ySrc,
                          int width,
                          int height,
                          int xDst,
                          int yDst) {
    SDL_Rect source;
    SDL_Rect destination;
    int left;
    int top;
    int right;
    int bottom;

    if (img == NULL || img->imgBitmap == NULL) return;

    left = mapTextX(doomCanvas, xDst);
    top = mapY(doomCanvas, yDst);
    right = mapTextX(doomCanvas, xDst + width);
    bottom = mapY(doomCanvas, yDst + height);
    if (right <= left || bottom <= top) return;

    source.x = xSrc;
    source.y = ySrc;
    source.w = width;
    source.h = height;
    destination.x = DOOMRPG_CANVAS_X + left;
    destination.y = DOOMRPG_CANVAS_Y + top;
    destination.w = right - left;
    destination.h = bottom - top;
    SDL_RenderCopy(sdlVideo.renderer, img->imgBitmap, &source, &destination);
}

static void drawFont(DoomCanvas_t* doomCanvas,
                     const char* text,
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

            drawTextGlyph(doomCanvas,
                          imgFont,
                          STORY_FONT_WIDTH * (glyph & 0x0f),
                          STORY_FONT_HEIGHT * ((glyph >> 4) & 0x0f),
                          STORY_FONT_WIDTH,
                          STORY_FONT_HEIGHT,
                          xpos,
                          y);
        }

        xpos += STORY_FONT_ADVANCE;
    }
}

static void drawString1(DoomCanvas_t* doomCanvas,
                        const char* text,
                        int x,
                        int y,
                        int flags) {
    drawFont(doomCanvas, text, x, y, flags, 0, -1);
}

static void drawString2(DoomCanvas_t* doomCanvas,
                        const char* text,
                        int x,
                        int y,
                        int flags,
                        int startTime) {
    const int strEnd =
        startTime < 0 ? -1 : (doomCanvas->time - startTime) / 25;

    drawFont(doomCanvas,
             text,
             x,
             y,
             flags,
             0,
             strEnd);
}

static void scrollSpaceBG(DoomCanvas_t* doomCanvas,
                          EspNativeIntroState_t* introState) {
    const int i = -((doomCanvas->time / 157) % 192);
    int i2 = i;
    const int i3 = i + 192;
    const int left = virtualLeft(doomCanvas);
    const int top = virtualTop(doomCanvas);

    if (i2 <= -192) {
        i2 += 384;
    }

    DoomRPG_setClipTrue(doomCanvas->doomRpg,
                        0, 0,
                        DOOMRPG_LOGICAL_WIDTH,
                        DOOMRPG_LOGICAL_HEIGHT);
    drawBackgroundImage(doomCanvas,
                        &introState->imgSpaceBG,
                        left - i2,
                        top);
    drawBackgroundImage(doomCanvas,
                        &introState->imgSpaceBG,
                        left - i3,
                        top);
    setVirtualClip(doomCanvas,
                   left,
                   top,
                   ESP32_STORY_VIRTUAL_SIZE,
                   ESP32_STORY_VIRTUAL_SIZE);
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

static void drawAnimationMappedLine(DoomCanvas_t* doomCanvas,
                                    int x1,
                                    int y1,
                                    int x2,
                                    int y2) {
    DoomRPG_drawLine(
        doomCanvas->doomRpg,
        ESP32_STORY_ANIMATION_X +
            scaleOffsetFloor(x1 - virtualLeft(doomCanvas),
                             ESP32_STORY_ANIMATION_WIDTH),
        mapY(doomCanvas, y1),
        ESP32_STORY_ANIMATION_X +
            scaleOffsetFloor(x2 - virtualLeft(doomCanvas),
                             ESP32_STORY_ANIMATION_WIDTH),
        mapY(doomCanvas, y2));
}

void Esp32StoryFit_draw(struct DoomCanvas_s* doomCanvasBase) {
    DoomCanvas_t* doomCanvas = (DoomCanvas_t*)doomCanvasBase;
    EspNativeIntroState_t* introState;
    char** text;
    Image_t* promptHand;
    int textPageCount;
    int elapsedAnim;
    int elapsedText;
    int left;
    int top;

    if (doomCanvas == NULL || doomCanvas->doomRpg == NULL) {
        return;
    }

    introState = EspNativeIntroState_get(doomCanvas->doomRpg);
    if (introState == NULL) {
        printf("[INTROFIT] REFUSE draw native intro state unavailable\n");
        return;
    }

    left = virtualLeft(doomCanvas);
    top = virtualTop(doomCanvas);

    if (!geometryLogged) {
        printf("[INTROFIT] virtual=128x128 content=%dx%d@(%d,%d) background=%dx%d@(%d,0) animation=%dx%d@(%d,0) text=%dx%d@(%d,0) fit=aspect-content+full-animation+soft-wide-text extraFrameBytes=0\n",
               ESP32_STORY_VIEWPORT_SIZE,
               ESP32_STORY_VIEWPORT_SIZE,
               ESP32_STORY_VIEWPORT_X,
               ESP32_STORY_VIEWPORT_Y,
               ESP32_STORY_BACKGROUND_WIDTH,
               DOOMRPG_LOGICAL_HEIGHT,
               ESP32_STORY_BACKGROUND_X,
               ESP32_STORY_ANIMATION_WIDTH,
               DOOMRPG_LOGICAL_HEIGHT,
               ESP32_STORY_ANIMATION_X,
               ESP32_STORY_TEXT_WIDTH,
               DOOMRPG_LOGICAL_HEIGHT,
               ESP32_STORY_TEXT_X);
        geometryLogged = 1;
    }

    if (introState->storyAnimTime == -1) {
        introState->storyAnimTime = DoomRPG_GetUpTimeMS();
    }
    if (introState->storyTextTime == -1) {
        introState->storyTextTime = DoomRPG_GetUpTimeMS();
    }

    if (doomCanvas->time < introState->storyTextTime ||
        doomCanvas->time < introState->storyAnimTime) {
        return;
    }

    elapsedAnim = doomCanvas->time - introState->storyAnimTime;
    elapsedText = doomCanvas->time - introState->storyTextTime;

    if (doomCanvas->doomRpg->graphSetCliping) {
        DoomRPG_setClipFalse(doomCanvas->doomRpg);
    }

    DoomRPG_setColor(doomCanvas->doomRpg, 0x000000);
    DoomRPG_fillRect(doomCanvas->doomRpg,
                     0,
                     0,
                     DOOMRPG_CANVAS_WIDTH,
                     DOOMRPG_CANVAS_HEIGHT);

    if (introState->storyPage == 0 || introState->storyPage == 2) {
        if (introState->storyPage == 0) {
            text = introState->storyText1;
            textPageCount = 2;
        }
        else {
            text = &introState->storyText2;
            textPageCount = 1;
        }

        if (textPageCount <= introState->storyTextPage) {
            printf("[INTROFIT] REFUSE legacy-exit page=%d\n",
                   introState->storyPage);
            return;
        }

        scrollSpaceBG(doomCanvas, introState);
        promptHand = acquireStoryHand(doomCanvas->doomRpg);

        /* Text uses its own hardware-tuned soft-wide mapping: wider than the
         * 120x120 content viewport but slightly narrower than the full display. */
        DoomRPG_setClipTrue(doomCanvas->doomRpg,
                            ESP32_STORY_TEXT_X,
                            0,
                            ESP32_STORY_TEXT_WIDTH,
                            DOOMRPG_LOGICAL_HEIGHT);

        if (introState->showTextDone) {
            drawString2(doomCanvas,
                        text[introState->storyTextPage],
                        left,
                        top,
                        0,
                        -1);
        }
        else {
            drawString2(doomCanvas,
                        text[introState->storyTextPage],
                        left,
                        top,
                        0,
                        introState->storyTextTime);
        }

        if (introState->storyTextPage < textPageCount - 1) {
            drawImage(doomCanvas,
                      promptHand,
                      (DOOMRPG_CANVAS_CENTER_X + 36) - 4,
                      (DOOMRPG_CANVAS_CENTER_Y + 64) - 2,
                      10);
            drawString1(doomCanvas,
                        "More",
                        (DOOMRPG_CANVAS_CENTER_X + 64) - 4,
                        DOOMRPG_CANVAS_CENTER_Y + 64,
                        10);
        }
        else {
            drawImage(doomCanvas,
                      promptHand,
                      (DOOMRPG_CANVAS_CENTER_X + 8) - 4,
                      (DOOMRPG_CANVAS_CENTER_Y + 64) - 2,
                      10);
            drawString1(doomCanvas,
                        "Continue",
                        (DOOMRPG_CANVAS_CENTER_X + 64) - 4,
                        DOOMRPG_CANVAS_CENTER_Y + 64,
                        10);
        }

        if (elapsedText >
            ((int)SDL_strlen(text[introState->storyTextPage]) * 25)) {
            introState->showTextDone = true;
        }

        return;
    }

    if (elapsedAnim > 10000) {
        if (!advanceAnimationPageBounded(introState)) {
            printf("[INTROFIT] REFUSE legacy-exit page=%d\n",
                   introState->storyPage);
            return;
        }
    }

    {
        const int bgOffset = elapsedAnim / 457;
        const int linesOffset = elapsedAnim / 157;
        const int shipX = left + (elapsedAnim / 142);
        const int shipY = (DOOMRPG_CANVAS_CENTER_Y + 22) + (elapsedAnim / -333);

        DoomRPG_setClipTrue(doomCanvas->doomRpg,
                            0, 0,
                            DOOMRPG_LOGICAL_WIDTH,
                            DOOMRPG_LOGICAL_HEIGHT);
        drawBackgroundImage(doomCanvas,
                            &introState->imgSpaceBG,
                            left - bgOffset,
                            top);
        setAnimationVirtualClip(doomCanvas,
                                left,
                                top,
                                ESP32_STORY_VIRTUAL_SIZE,
                                ESP32_STORY_VIRTUAL_SIZE);
        drawAnimationImage(doomCanvas,
                           &introState->imgLinesLayer,
                           left - linesOffset,
                           top,
                           0);
        drawAnimationImage(doomCanvas,
                           &introState->imgPlanetLayer,
                           left,
                           top,
                           0);
        drawAnimationImage(doomCanvas,
                           &introState->imgSpaceship,
                           shipX,
                           shipY,
                           0);

        if ((elapsedAnim / 500) % 2 == 0) {
            setAnimationVirtualClip(doomCanvas,
                                    left + 1,
                                    top + 1,
                                    126,
                                    126);

            DoomRPG_setColor(doomCanvas->doomRpg, 0xBB0000);
            drawAnimationMappedLine(doomCanvas,
                           shipX,
                           shipY - 1,
                           shipX + 9,
                           shipY - 1);
            drawAnimationMappedLine(doomCanvas,
                           shipX + 4,
                           top,
                           shipX + 4,
                           shipY - 1);
            drawAnimationMappedLine(doomCanvas,
                           shipX,
                           shipY + 9,
                           shipX + 9,
                           shipY + 9);
            drawAnimationMappedLine(doomCanvas,
                           shipX + 4,
                           shipY + 9,
                           shipX + 4,
                           top + ESP32_STORY_VIRTUAL_SIZE);
            drawAnimationMappedLine(doomCanvas,
                           shipX - 1,
                           shipY,
                           shipX - 1,
                           shipY + 9);
            drawAnimationMappedLine(doomCanvas,
                           left,
                           shipY + 4,
                           shipX - 1,
                           shipY + 4);
            drawAnimationMappedLine(doomCanvas,
                           shipX + 9,
                           shipY,
                           shipX + 9,
                           shipY + 9);
            drawAnimationMappedLine(doomCanvas,
                           shipX + 9,
                           shipY + 4,
                           left + ESP32_STORY_VIRTUAL_SIZE,
                           shipY + 4);
        }
    }
}
