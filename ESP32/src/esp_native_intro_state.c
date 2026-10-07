#include <SDL.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "DoomRPG.h"

#include "esp_native_intro_state.h"

#define ESP32_NATIVE_INTRO_STATE_BYTES 96U

_Static_assert(sizeof(EspNativeIntroState_t) == ESP32_NATIVE_INTRO_STATE_BYTES,
               "ESP32 native intro state layout changed; audit transient owner");

static EspNativeIntroState_t* introState;
static DoomRPG_t* introOwner;

static const char storyTextA[] =
    "You have been\n"
    "dispatched in re - \n"
    "sponse to a dis - \n"
    "tress call from\n"
    "Union Aerospace\n"
    "Corporation's re-\n"
    "search facility\n"
    "on Mars. The base\n"
    "is under attack";

static const char storyTextB[] =
    "by an unknown\n"
    "force and your\n"
    "mission is to ac-\n"
    "quire intelli-\n"
    "gence and neu-\n"
    "tralize the\n"
    "threat.";

static const char storyTextC[] =
    "Insertion com-\n"
    "plete. For fur-\n"
    "ther instruc-\n"
    "tions, rendezvous\n"
    "with the other\n"
    "Marines at Junc-\n"
    "tion. Expect\n"
    "heavy resistance.\n"
    "Good luck!";

static char* copyText(const char* text) {
    size_t bytes;
    char* copy;

    if (text == NULL) {
        return NULL;
    }

    bytes = SDL_strlen(text) + 1U;
    copy = (char*)SDL_calloc(bytes, sizeof(char));
    if (copy == NULL) {
        return NULL;
    }

    SDL_memcpy(copy, text, bytes);
    return copy;
}

static void releaseOwnedState(DoomRPG_t* owner) {
    if (introState == NULL) {
        introOwner = NULL;
        return;
    }

    if (owner != NULL) {
        DoomRPG_freeImage(owner, &introState->imgSpaceBG);
        DoomRPG_freeImage(owner, &introState->imgLinesLayer);
        DoomRPG_freeImage(owner, &introState->imgPlanetLayer);
        DoomRPG_freeImage(owner, &introState->imgSpaceship);
    }

    SDL_free(introState->storyText1[0]);
    SDL_free(introState->storyText1[1]);
    SDL_free(introState->storyText2);
    SDL_free(introState);
    introState = NULL;
    introOwner = NULL;
}

int EspNativeIntroState_begin(struct DoomRPG_s* doomRpgBase) {
    DoomRPG_t* doomRpg = (DoomRPG_t*)doomRpgBase;

    if (doomRpg == NULL || introState != NULL) {
        printf("[INTROSTATE] REFUSE begin owner=%p active=%d\n",
               (void*)doomRpg,
               introState != NULL ? 1 : 0);
        return 0;
    }

    introState = (EspNativeIntroState_t*)SDL_calloc(
        1U, sizeof(EspNativeIntroState_t));
    if (introState == NULL) {
        printf("[INTROSTATE] FAILED state allocation bytes=%u\n",
               (unsigned int)sizeof(EspNativeIntroState_t));
        return 0;
    }
    introOwner = doomRpg;

    introState->storyText1[0] = copyText(storyTextA);
    introState->storyText1[1] = copyText(storyTextB);
    introState->storyText2 = copyText(storyTextC);
    if (introState->storyText1[0] == NULL ||
        introState->storyText1[1] == NULL ||
        introState->storyText2 == NULL) {
        printf("[INTROSTATE] FAILED story text allocation\n");
        releaseOwnedState(doomRpg);
        return 0;
    }

    DoomRPG_createImage(doomRpg, "c.bmp", false, &introState->imgSpaceBG);
    DoomRPG_createImage(doomRpg, "d.bmp", true, &introState->imgLinesLayer);
    DoomRPG_createImage(doomRpg, "e.bmp", true, &introState->imgPlanetLayer);
    DoomRPG_createImage(doomRpg, "f.bmp", true, &introState->imgSpaceship);

    if (introState->imgSpaceBG.imgBitmap == NULL ||
        introState->imgLinesLayer.imgBitmap == NULL ||
        introState->imgPlanetLayer.imgBitmap == NULL ||
        introState->imgSpaceship.imgBitmap == NULL) {
        printf("[INTROSTATE] FAILED intro image allocation c=%p d=%p e=%p f=%p\n",
               (void*)introState->imgSpaceBG.imgBitmap,
               (void*)introState->imgLinesLayer.imgBitmap,
               (void*)introState->imgPlanetLayer.imgBitmap,
               (void*)introState->imgSpaceship.imgBitmap);
        releaseOwnedState(doomRpg);
        return 0;
    }

    introState->storyTextTime = -1;
    introState->storyAnimTime = -1;
    introState->showTextDone = false;
    introState->storyPage = 0;
    introState->storyTextPage = 0;

    printf("[INTROSTATE] READY bytes=%u owner=native-transient images=4 texts=3 page=0 textPage=0\n",
           (unsigned int)sizeof(EspNativeIntroState_t));
    return 1;
}

EspNativeIntroState_t* EspNativeIntroState_get(struct DoomRPG_s* doomRpgBase) {
    DoomRPG_t* doomRpg = (DoomRPG_t*)doomRpgBase;

    if (doomRpg == NULL || introOwner != doomRpg) {
        return NULL;
    }
    return introState;
}

const EspNativeIntroState_t* EspNativeIntroState_view(
    const struct DoomRPG_s* doomRpgBase) {
    const DoomRPG_t* doomRpg = (const DoomRPG_t*)doomRpgBase;

    if (doomRpg == NULL || introOwner != doomRpg) {
        return NULL;
    }
    return introState;
}

void EspNativeIntroState_release(struct DoomRPG_s* doomRpgBase) {
    DoomRPG_t* doomRpg = (DoomRPG_t*)doomRpgBase;

    if (introState == NULL) {
        return;
    }
    if (doomRpg == NULL || introOwner != doomRpg) {
        printf("[INTROSTATE] REFUSE release owner=%p expected=%p\n",
               (void*)doomRpg,
               (void*)introOwner);
        return;
    }

    releaseOwnedState(doomRpg);
    printf("[INTROSTATE] RELEASE owner=native-transient state=NULL\n");
}
