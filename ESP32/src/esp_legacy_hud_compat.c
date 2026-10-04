#include <SDL.h>
#include <string.h>

#include "DoomRPG.h"
#include "DoomCanvas.h"
#include "Hud.h"

/*
 * Transitional ESP32 HUD compatibility.
 *
 * The production HUD is EspNativeGameplayHud and reads bounded assets directly
 * from DoomRPG-ESP32.pak. This file keeps only the tiny scalar/message ABI
 * required by still-linked desktop-shaped helpers while src/Hud.c is excluded.
 * No legacy HUD Image_t owner becomes resident and no legacy HUD rasterization
 * is performed here.
 */

Hud_t* Hud_init(Hud_t* hud, DoomRPG_t* doomRpg)
{
    if (hud == NULL) {
        hud = (Hud_t*)SDL_malloc(sizeof(Hud_t));
        if (hud == NULL) {
            return NULL;
        }
    }

    SDL_memset(hud, 0, sizeof(Hud_t));
    hud->doomRpg = doomRpg;

    printf("[HUDCOMPAT] INIT bytes=%u images=NULL renderer=native-pak-stream\n",
           (unsigned int)sizeof(Hud_t));
    return hud;
}

void Hud_free(Hud_t* hud, boolean freePtr)
{
    if (hud == NULL) {
        return;
    }

    /* Legacy bitmap residency is forbidden on ESP32 at this boundary. */
    if (freePtr) {
        SDL_free(hud);
    }
}

void Hud_addMessage(Hud_t* hud, char* str)
{
    Hud_addMessageForce(hud, str, false);
}

void Hud_addMessageForce(Hud_t* hud, char* str, boolean force)
{
    if (hud == NULL || str == NULL) {
        return;
    }

    if (force) {
        hud->msgCount = 0;
    }

    if (hud->msgCount > 0 &&
        strcmp(str, hud->messages[hud->msgCount - 1]) == 0) {
        return;
    }

    if (hud->msgCount >= MAX_MESSAGES) {
        Hud_shiftMsgs(hud);
    }

    strncpy(hud->messages[hud->msgCount], str, MS_PER_CHAR - 1);
    hud->messages[hud->msgCount][MS_PER_CHAR - 1] = '\0';
    hud->msgCount++;

    if (hud->msgCount == 1) {
        Hud_calcMsgTime(hud);
        if (force) {
            hud->msgDuration *= 2;
        }
    }
}

void Hud_calcMsgTime(Hud_t* hud)
{
    int len;

    if (hud == NULL) {
        return;
    }

    hud->msgTime =
        (hud->doomRpg != NULL && hud->doomRpg->doomCanvas != NULL)
            ? hud->doomRpg->doomCanvas->time
            : 0;

    len = (int)strlen(hud->messages[0]);
    hud->msgDuration =
        (len <= hud->msgMaxChars) ? MSG_DISPLAY_TIME : len * 100;
}

void Hud_finishMessageBufferForce(Hud_t* hud, boolean force)
{
    if (hud == NULL) {
        return;
    }

    if (hud->msgCount < MAX_MESSAGES) {
        hud->messages[hud->msgCount][MS_PER_CHAR - 1] = '\0';
        hud->msgCount++;
    }

    if (hud->msgCount == 1) {
        Hud_calcMsgTime(hud);
        if (force) {
            hud->msgDuration *= 2;
        }
    }
}

void Hud_finishMessageBuffer(Hud_t* hud)
{
    Hud_finishMessageBufferForce(hud, false);
}

char* Hud_getMessageBufferForce(Hud_t* hud, boolean force)
{
    if (hud == NULL) {
        return NULL;
    }

    if (force) {
        hud->msgCount = 0;
    }

    if (hud->msgCount >= MAX_MESSAGES) {
        Hud_shiftMsgs(hud);
    }

    hud->messages[hud->msgCount][0] = '\0';
    return hud->messages[hud->msgCount];
}

char* Hud_getMessageBuffer(Hud_t* hud)
{
    return Hud_getMessageBufferForce(hud, false);
}

void Hud_shiftMsgs(Hud_t* hud)
{
    int i;

    if (hud == NULL || hud->msgCount <= 0) {
        return;
    }

    for (i = 0; i < hud->msgCount - 1; ++i) {
        memcpy(hud->messages[i], hud->messages[i + 1], MS_PER_CHAR);
    }

    hud->msgCount--;
    hud->messages[hud->msgCount][0] = '\0';

    if (hud->msgCount > 0) {
        Hud_calcMsgTime(hud);
    }
}

void Hud_startup(Hud_t* hud, boolean largeStatus)
{
    if (hud == NULL) {
        return;
    }

    hud->msgMaxChars =
        (hud->doomRpg != NULL && hud->doomRpg->doomCanvas != NULL)
            ? (hud->doomRpg->doomCanvas->displayRect.w - 4) / 7
            : 22;
    hud->statusTopBarHeight = 20;
    hud->statusBarHeight = 20;
    hud->largeHud = largeStatus;

    /* Geometry retained for legacy-shaped callers; bitmaps remain NULL. */
    hud->hudFaceWidth = largeStatus ? 20 : 18;
    hud->hudFaceHeight = largeStatus ? 28 : 20;
    hud->iconSheetWidth = largeStatus ? 18 : 13;
    hud->iconSheetHeight = largeStatus ? 18 : 13;
    hud->statusHealthXpos = largeStatus ? 15 : 2;
    hud->statusArmorXpos = largeStatus ? 54 : 35;
    hud->statusHudFacesXpos = largeStatus ? 88 : 66;
    hud->statusAmmoXpos = largeStatus ? 111 : 85;
    hud->statusOrientationXpos = largeStatus ? 152 : 126;
    hud->statusOrientationArrowXpos = largeStatus ? 154 : 128;
    hud->statusLine1Xpos = largeStatus ? 51 : 33;
    hud->statusLine2Xpos = largeStatus ? 205 : 155;
}

/*
 * The desktop raster HUD is not a production renderer on ESP32. Any remaining
 * calls originate from retained desktop-shaped state helpers. Rendering is
 * deliberately a no-op; EspNativeGameplayHud owns visible HUD composition.
 */
void Hud_drawBarTiles(Hud_t* hud, int x, int y, int width, boolean isLargerStatusBar)
{
    (void)hud;
    (void)x;
    (void)y;
    (void)width;
    (void)isLargerStatusBar;
}

void Hud_drawBottomBar(Hud_t* hud)
{
    (void)hud;
}

void Hud_drawEffects(Hud_t* hud)
{
    (void)hud;
}

void Hud_drawTopBar(Hud_t* hud)
{
    (void)hud;
}
