#include "native_intro_dispose.h"
#include "native_map1_access_probe.h"
#include "native_map1_bsp_pass1.h"
#include "native_map1_runtime_load.h"

/*
 * Keep the hardware-validated intro clock/dispose, native BSP pass-1 and native
 * resident-runtime implementations untouched. These wrappers are temporary
 * lifecycle scaffolding only; reusable native map components remain
 * legacy-engine free.
 */
void __real_Esp32IntroDispose_reset(void);
void __real_Esp32IntroDispose_service(struct DoomRPG_s* doomRpg);

void __wrap_Esp32IntroDispose_reset(void) {
    __real_Esp32IntroDispose_reset();
    Esp32Map1BspPass1_reset();
    Esp32Map1RuntimeLoad_reset();
    Esp32Map1AccessProbe_reset();
}

void __wrap_Esp32IntroDispose_service(struct DoomRPG_s* doomRpg) {
    __real_Esp32IntroDispose_service(doomRpg);

    /*
     * Final Continue -> validated intro teardown -> native BSP pass 1 -> native
     * compact resident arena -> allocation-free indexed accessor validation.
     * Each stage arms first and executes on a later Arduino loop service.
     */
    Esp32Map1BspPass1_service(doomRpg);
    Esp32Map1RuntimeLoad_service(doomRpg);
    Esp32Map1AccessProbe_service(doomRpg);
}
