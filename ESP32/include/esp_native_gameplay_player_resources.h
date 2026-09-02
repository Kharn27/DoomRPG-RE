#ifndef DOOMRPG_ESP32_NATIVE_GAMEPLAY_PLAYER_RESOURCES_H
#define DOOMRPG_ESP32_NATIVE_GAMEPLAY_PLAYER_RESOURCES_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct DoomRPG_s;

typedef struct EspNativeGameplayPlayerResourcesView_s {
    uint32_t sourceArenaFNV1a;
    uint32_t consumedCount;
    uint32_t playerFNV1a;
    uint32_t spriteCount;
    uint32_t consumedBytes;
    uint8_t targetMapId;
    uint8_t pendingMove;
    uint8_t active;
    uint8_t fatal;
} EspNativeGameplayPlayerResourcesView;

void EspNativeGameplayPlayerResources_reset(void);
int EspNativeGameplayPlayerResources_isConsumed(uint32_t spriteIndex);
const EspNativeGameplayPlayerResourcesView*
EspNativeGameplayPlayerResources_view(void);

/*
 * Player resources historically own the public native gameplay-session wrappers.
 * Keep those implementations as private chain leaves so bounded presentation
 * owners can service expiry/redraw work after the already-proven gameplay chain
 * without duplicating or bypassing resource/action/session ownership.
 */
void EspNativeGameplayPlayerResources_sessionReset(void);
void EspNativeGameplayPlayerResources_sessionService(struct DoomRPG_s* doomRpg);
#define __wrap_EspNativeGameplaySession_reset \
    EspNativeGameplayPlayerResources_sessionReset
#define __wrap_EspNativeGameplaySession_service \
    EspNativeGameplayPlayerResources_sessionService

#ifdef __cplusplus
}
#endif

#endif
