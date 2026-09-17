#ifndef DOOMRPG_ESP32_NATIVE_GAMEPLAY_PLAYER_RESOURCES_H
#define DOOMRPG_ESP32_NATIVE_GAMEPLAY_PLAYER_RESOURCES_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct DoomRPG_s;

#define ESP_NATIVE_GAMEPLAY_PLAYER_RESOURCES_SNAPSHOT_MAX_BYTES 128U

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

/*
 * Pointer-free checkpoint payload for the map-local consumed resource overlay.
 * The fixed 128-byte payload bounds this save section to maps with at most
 * 1024 immutable BSP sprites. Runtime/map identity and the exact used byte
 * count are carried explicitly; callers must never persist ResourceOwner or
 * its heap pointer.
 */
typedef struct EspNativeGameplayPlayerResourcesSnapshot_s {
    uint32_t sourceArenaFNV1a;
    uint32_t spriteCount;
    uint32_t consumedCount;
    uint16_t consumedBytes;
    uint8_t targetMapId;
    uint8_t reserved0;
    uint8_t consumedBits[ESP_NATIVE_GAMEPLAY_PLAYER_RESOURCES_SNAPSHOT_MAX_BYTES];
} EspNativeGameplayPlayerResourcesSnapshot;

void EspNativeGameplayPlayerResources_reset(void);
int EspNativeGameplayPlayerResources_isConsumed(uint32_t spriteIndex);
const EspNativeGameplayPlayerResourcesView*
EspNativeGameplayPlayerResources_view(void);

/*
 * Export/import only the persistent semantic consumed overlay. Snapshot is
 * allocation-free. Restore validates current immutable runtime identity and
 * initializes the normal ResourceOwner through ensureOwner() before copying
 * the bounded bytes. No PlayerState, topology, renderer or legacy object is
 * mutated by these APIs.
 */
int EspNativeGameplayPlayerResources_snapshot(
    EspNativeGameplayPlayerResourcesSnapshot* outSnapshot);
int EspNativeGameplayPlayerResources_restore(
    const EspNativeGameplayPlayerResourcesSnapshot* snapshot);

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
