#include "DoomRPG.h"

#include "native_intro_dispose.h"
#include "native_map1_bsp_pass1.h"

/*
 * Keep the already hardware-validated intro clock/dispose implementation
 * untouched. These wrappers are temporary lifecycle scaffolding only: the
 * native BSP reader itself has no dependency on DoomCanvas/Render/Game.
 */
void __real_Esp32IntroDispose_reset(void);
void __real_Esp32IntroDispose_service(struct DoomRPG_s* doomRpg);

void __wrap_Esp32IntroDispose_reset(void) {
    __real_Esp32IntroDispose_reset();
    Esp32Map1BspPass1_reset();
}

void __wrap_Esp32IntroDispose_service(struct DoomRPG_s* doomRpg) {
    __real_Esp32IntroDispose_service(doomRpg);

    /*
     * First service after final Continue performs the validated intro teardown
     * and only arms the native BSP inventory. The following Arduino loop walks
     * /intro.bsp directly from DoomRPG-ESP32.pak through a 256-byte window.
     */
    Esp32Map1BspPass1_service(doomRpg);
}
