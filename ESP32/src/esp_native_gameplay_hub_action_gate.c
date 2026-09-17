#include <stdint.h>
#include <stdio.h>

#include "esp_native_gameplay_hub.h"

struct DoomRPG_s;

int __real_EspNativeGameplayMonsterState_actionService(
    struct DoomRPG_s* doomRpg);

/*
 * Resident gameplay already gives HUB exclusive input ownership, but the
 * action/feedback service sits higher in the composed session chain. Without
 * this gate a pickup message or viewport flash can expire while HUB owns the
 * framebuffer, restoring stale world pixels underneath the menu overlay.
 *
 * Pause only this world/action presentation leaf. The HUB itself, checkpoint
 * SAVE/LOAD input, and the generic resident gameplay service remain live. Time
 * is not rebased: once HUB closes, the real service observes the actual elapsed
 * lease and expires feedback immediately when it is already due.
 */
int __wrap_EspNativeGameplayMonsterState_actionService(
    struct DoomRPG_s* doomRpg) {
    static uint8_t paused;

    if (EspNativeGameplayHub_isActive()) {
        if (paused == 0U) {
            paused = 1U;
            printf("[HUBACTIONGATE] PAUSE worldActionFeedback=yes timer=realtime mutation=no\n");
        }
        return 1;
    }

    if (paused != 0U) {
        paused = 0U;
        printf("[HUBACTIONGATE] RESUME worldActionFeedback=yes timer=realtime mutation=no\n");
    }
    return __real_EspNativeGameplayMonsterState_actionService(doomRpg);
}
