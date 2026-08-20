#include <SDL.h>
#include <stdio.h>

#include "DoomRPG.h"
#include "Render.h"

#include "native_intro_dispose.h"
#include "native_map1_structural_load.h"

/*
 * Keep the already hardware-validated intro clock/dispose implementation
 * untouched. The linker redirects only these cross-object calls on ESP32.
 */
void __real_Esp32IntroDispose_reset(void);
void __real_Esp32IntroDispose_service(struct DoomRPG_s* doomRpg);
boolean __real_Render_loadBitShapes(Render_t* render);

void __wrap_Esp32IntroDispose_reset(void) {
    __real_Esp32IntroDispose_reset();
    Esp32Map1StructuralLoad_reset();
}

void __wrap_Esp32IntroDispose_service(struct DoomRPG_s* doomRpg) {
    __real_Esp32IntroDispose_service(doomRpg);

    /*
     * First call after final Continue performs the validated intro teardown and
     * only arms MAP_INTRO. The next Arduino loop reaches this wrapper again and
     * performs the structural load, preserving a visible lifecycle boundary.
     */
    Esp32Map1StructuralLoad_service(doomRpg);
}

boolean __wrap_Render_loadBitShapes(Render_t* render) {
    /*
     * Render_beginLoadMapData() calls this immediately after it has finished
     * nodes/lines/sprites/events/strings/blockmap/plane references and freed the
     * raw BSP buffer. For the one armed MAP_INTRO load, capture that exact point
     * and return false so the legacy bitshape/texel tail cannot execute.
     */
    if (Esp32Map1StructuralLoad_captureBoundary(render)) {
        if (render != NULL) {
            /* The original function has already SDL_free()'d this buffer. */
            render->ioBuffer = NULL;
        }
        printf("[MAP1STRUCT] GATE Render_loadBitShapes blocked; legacy graphics tail not entered\n");
        return false;
    }

    return __real_Render_loadBitShapes(render);
}
