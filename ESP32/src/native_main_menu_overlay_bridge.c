#include <SDL.h>
#include <stdio.h>

#include "DoomRPG.h"
#include "Render.h"

#include "native_main_menu_overlay_probe.h"
#include "native_menu_sprite_frame_probe.h"

extern DoomRPG_t* doomRpg;

/*
 * Keep the hardware-validated native scene probe untouched. The ESP32 linker
 * chains the real MENU_MAIN composition only after that probe has completely
 * validated ffe0995e and torn down both wall/sprite caches.
 */
int __real_DoomRPG_probeNativeMenuSpriteFrame(struct Render_s* render);

int __wrap_DoomRPG_probeNativeMenuSpriteFrame(struct Render_s* render) {
    if (!__real_DoomRPG_probeNativeMenuSpriteFrame(render)) {
        return 0;
    }

    if (doomRpg == NULL) {
        printf("[MAINMENU] FAILED global DoomRPG unavailable after native scene\n");
        return 0;
    }

    return DoomRPG_probeNativeMainMenuOverlay(doomRpg);
}
