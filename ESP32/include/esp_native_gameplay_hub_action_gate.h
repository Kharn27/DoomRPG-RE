#ifndef DOOMRPG_ESP32_NATIVE_GAMEPLAY_HUB_ACTION_GATE_H
#define DOOMRPG_ESP32_NATIVE_GAMEPLAY_HUB_ACTION_GATE_H

#ifdef __cplusplus
extern "C" {
#endif

struct DoomRPG_s;

/*
 * Explicit composition boundary between MonsterCombat's action-service chain
 * and HUB/automap framebuffer ownership. While an overlay owns presentation,
 * pause only world/action feedback expiry; the underlying MonsterState action
 * service resumes unchanged when world presentation becomes active again.
 */
int EspNativeGameplayHubActionGate_service(struct DoomRPG_s* doomRpg);

#ifdef __cplusplus
}
#endif

#endif
