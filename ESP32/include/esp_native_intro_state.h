#ifndef DOOMRPG_ESP32_NATIVE_INTRO_STATE_H
#define DOOMRPG_ESP32_NATIVE_INTRO_STATE_H

#include "DoomRPG.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Transient native owner for ST_INTRO-only assets and mutable story state.
 *
 * The owner exists only between MENU_MAIN -> Start and bounded intro disposal.
 * DoomCanvas retains shared layout/font/state handoff fields, but no longer
 * permanently carries the four prologue images, story text pointers, or page
 * epochs after the intro has finished.
 */
typedef struct EspNativeIntroState_s {
    Image_t imgSpaceBG;
    Image_t imgLinesLayer;
    Image_t imgPlanetLayer;
    Image_t imgSpaceship;
    int storyTextTime;
    int storyAnimTime;
    char* storyText1[2];
    char* storyText2;
    int storyPage;
    int storyTextPage;
    boolean showTextDone;
} EspNativeIntroState_t;

int EspNativeIntroState_begin(struct DoomRPG_s* doomRpg);
EspNativeIntroState_t* EspNativeIntroState_get(struct DoomRPG_s* doomRpg);
const EspNativeIntroState_t* EspNativeIntroState_view(const struct DoomRPG_s* doomRpg);
void EspNativeIntroState_release(struct DoomRPG_s* doomRpg);

#ifdef __cplusplus
}
#endif

#endif
