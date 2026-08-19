#include <SDL.h>
#include <stdint.h>
#include <stdio.h>

#include "DoomRPG.h"
#include "Menu.h"
#include "MenuSystem.h"
#include "Render.h"

#include "native_menu_wall_frame_probe.h"
#include "platform_video_config.h"

#define EXPECTED_OPTIONS_FRAMEBUFFER_FNV 0x6058d47dU

static int skipNextGrayPass = 0;

static uint32_t fnv1a32(const uint8_t* data, uint32_t length) {
    uint32_t hash = 2166136261U;
    uint32_t i;

    for (i = 0; i < length; ++i) {
        hash ^= data[i];
        hash *= 16777619U;
    }
    return hash;
}

static uint32_t framebufferHash(const Render_t* render) {
    if (render == NULL || render->framebuffer == NULL || render->pitch <= 0) {
        return 0U;
    }

    return fnv1a32((const uint8_t*)render->framebuffer,
                   (uint32_t)render->pitch * DOOMRPG_LOGICAL_HEIGHT);
}

int __real_DoomRPG_probeNativeMenuWallFrame(struct Render_s* render);
void __real_Render_setGrayPalettes(Render_t* render);

/* Render_setGrayPalettes() mutates mediaPalettes and floor/ceiling colors in
 * place. Calling it a second time on the already-gray menu runtime introduces
 * another RGB565 rounding step and breaks deterministic re-entry.
 *
 * Arm exactly one skip only when the wall probe is being re-entered directly
 * from the validated Options framebuffer after MenuSystem_back(). Normal boot
 * and any unrelated Render_setGrayPalettes() call remain transparent.
 */
void __wrap_Render_setGrayPalettes(Render_t* render) {
    if (skipNextGrayPass) {
        skipNextGrayPass = 0;
        printf("[MENUWALL] REENTRY grayscale already applied; skipping destructive second Render_setGrayPalettes pass\n");
        return;
    }

    __real_Render_setGrayPalettes(render);
}

int __wrap_DoomRPG_probeNativeMenuWallFrame(struct Render_s* renderBase) {
    Render_t* render = (Render_t*)renderBase;
    int isOptionsBackReentry = 0;
    int result;

    if (render != NULL && render->doomRpg != NULL &&
        render->doomRpg->menuSystem != NULL &&
        render->doomRpg->menuSystem->menu == MENU_MAIN &&
        render->doomRpg->menuSystem->selectedIndex == 0 &&
        framebufferHash(render) == EXPECTED_OPTIONS_FRAMEBUFFER_FNV) {
        isOptionsBackReentry = 1;
        skipNextGrayPass = 1;
        printf("[MENUWALL] REENTRY detected from Options framebuffer=%08x; preserving already-gray palette\n",
               (unsigned int)EXPECTED_OPTIONS_FRAMEBUFFER_FNV);
    }

    result = __real_DoomRPG_probeNativeMenuWallFrame(renderBase);

    /* If the real probe failed before reaching Render_setGrayPalettes(), never
     * leak the one-shot skip into a later unrelated call.
     */
    if (isOptionsBackReentry && skipNextGrayPass) {
        skipNextGrayPass = 0;
        printf("[MENUWALL] REENTRY gray-skip cancelled because wall probe exited before palette preparation\n");
    }

    return result;
}
