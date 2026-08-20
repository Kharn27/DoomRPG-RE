#include "native_intro_dispose.h"
#include "native_map1_bsp_pass1.h"
#include "native_map1_runtime_load.h"

/*
 * Keep the hardware-validated intro clock/dispose and native BSP pass-1
 * implementations untouched. These wrappers are temporary lifecycle
 * scaffolding only; the reusable BSP reader/runtime remain legacy-engine free.
 */
void __real_Esp32IntroDispose_reset(void);
void __real_Esp32IntroDispose_service(struct DoomRPG_s* doomRpg);

void __wrap_Esp32IntroDispose_reset(void) {
    __real_Esp32IntroDispose_reset();
    Esp32Map1BspPass1_reset();
    Esp32Map1RuntimeLoad_reset();
}

void __wrap_Esp32IntroDispose_service(struct DoomRPG_s* doomRpg) {
    __real_Esp32IntroDispose_service(doomRpg);

    /*
     * Final Continue -> validated intro teardown -> native BSP pass 1.
     * Once pass 1 is complete, the next bounded service owns allocation and
     * direct .pak population of the compact immutable MAP_INTRO base.
     */
    Esp32Map1BspPass1_service(doomRpg);
    Esp32Map1RuntimeLoad_service(doomRpg);
}
