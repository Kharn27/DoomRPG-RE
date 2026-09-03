#include "esp_native_gameplay_monster_retaliation.h"

/*
 * Link-only compatibility for the two historical __wrap_Retaliation_* helper
 * sections still present in esp_native_gameplay_monster_movement.c. The normal
 * esp32-cyd link no longer uses --wrap=EspNativeGameplayMonsterRetaliation_*;
 * session composition calls the boundary-safe movement probe explicitly.
 *
 * These leaves therefore have no runtime caller in the supported build. Keeping
 * them resolvable avoids relying on linker garbage-collection semantics until
 * the historical wrapper sections are removed in a later source cleanup.
 */
void __real_EspNativeGameplayMonsterRetaliation_service(struct DoomRPG_s* doomRpg) {
    EspNativeGameplayMonsterRetaliation_service(doomRpg);
}

void __real_EspNativeGameplayMonsterRetaliation_reset(void) {
    EspNativeGameplayMonsterRetaliation_reset();
}
