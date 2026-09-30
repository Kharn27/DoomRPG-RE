#include <SDL.h>
#include <stdint.h>
#include <stdio.h>

#include "DoomRPG.h"
#include "DoomCanvas.h"
#include "Menu.h"
#include "MenuSystem.h"
#include "Render.h"

#include "native_main_menu_model.h"
#include "native_main_menu_present.h"
#include "native_main_menu_touch_layout.h"
#include "native_sprite_lru_cache.h"
#include "native_wall_lru_cache.h"
#include "platform_touch_events.h"
#include "platform_video_config.h"

static uint32_t fnv1a32(const uint8_t* data, uint32_t length) {
    uint32_t hash = 2166136261U;
    uint32_t i;

    for (i = 0; i < length; ++i) {
        hash ^= data[i];
        hash *= 16777619U;
    }
    return hash;
}

uint32_t DoomRPG_esp32MainMenuFramebufferHash(const struct Render_s* renderBase) {
    const Render_t* render = (const Render_t*)renderBase;

    if (render == NULL || render->framebuffer == NULL || render->pitch <= 0) {
        return 0U;
    }
    return fnv1a32((const uint8_t*)render->framebuffer,
                   (uint32_t)render->pitch * DOOMRPG_LOGICAL_HEIGHT);
}

int DoomRPG_esp32MainMenuGraphicsBoundaryIsSafe(
    const struct DoomRPG_s* doomRpgBase) {
    const DoomRPG_t* doomRpg = (const DoomRPG_t*)doomRpgBase;
    const Render_t* render;

    if (doomRpg == NULL || doomRpg->render == NULL ||
        doomRpg->doomCanvas == NULL || doomRpg->menuSystem == NULL ||
        doomRpg->menu == NULL) {
        return 0;
    }

    render = doomRpg->render;
    return render->framebuffer != NULL &&
           render->shapeData == NULL &&
           render->mediaTexels == NULL &&
           !EspNativeWallCache_isActive() &&
           !EspNativeSpriteCache_isActive();
}

int DoomRPG_esp32MainMenuRecover(struct DoomRPG_s* doomRpgBase,
                                 const char* reason) {
    DoomRPG_t* doomRpg = (DoomRPG_t*)doomRpgBase;
    uint32_t frameFNV = 0U;

    if (doomRpg == NULL || doomRpg->doomCanvas == NULL ||
        doomRpg->menuSystem == NULL) {
        printf("[MAINRECOVER] FAILED reason=%s objectGraph\n",
               reason != NULL ? reason : "unknown");
        return 0;
    }

    if (doomRpg->doomCanvas->state != ST_MENU) {
        printf("[MAINRECOVER] REFUSE reason=%s state=%d menu=%d transitionAlreadyLeftMenu=yes\n",
               reason != NULL ? reason : "unknown",
               doomRpg->doomCanvas->state,
               doomRpg->menuSystem->menu);
        return 0;
    }

    if (!DoomRPG_esp32MainMenuGraphicsBoundaryIsSafe(doomRpg)) {
        printf("[MAINRECOVER] FAILED reason=%s graphicsBoundary\n",
               reason != NULL ? reason : "unknown");
        return 0;
    }

    PlatformInput_setTapCallback(NULL);

    if (!DoomRPG_esp32MainMenuModelEnter(doomRpg, MENU_MAIN) ||
        !DoomRPG_esp32RepaintOpaqueMainMenu(doomRpg, &frameFNV)) {
        printf("[MAINRECOVER] FAILED reason=%s rebuild/repaint menu=%d state=%d\n",
               reason != NULL ? reason : "unknown",
               doomRpg->menuSystem->menu,
               doomRpg->doomCanvas->state);
        return 0;
    }

    printf("[MAINRECOVER] READY reason=%s menu=%d selected=%d frame=%08x touch=rearmed\n",
           reason != NULL ? reason : "unknown",
           doomRpg->menuSystem->menu,
           doomRpg->menuSystem->selectedIndex,
           (unsigned int)frameFNV);
    return 1;
}
