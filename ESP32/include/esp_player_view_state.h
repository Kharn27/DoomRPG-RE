#ifndef DOOMRPG_ESP32_PLAYER_VIEW_STATE_H
#define DOOMRPG_ESP32_PLAYER_VIEW_STATE_H

#include <stdint.h>

#include "esp_player_spawn_state.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum EspPlayerViewApplyStatus_e {
    ESP_PLAYER_VIEW_APPLY_INVALID = 0,
    ESP_PLAYER_VIEW_APPLY_SPAWN_INVALID = 1,
    ESP_PLAYER_VIEW_APPLY_ALREADY_ACTIVE = 2,
    ESP_PLAYER_VIEW_APPLY_OK = 3
} EspPlayerViewApplyStatus;

/*
 * Small permanent native owner for the placement fields written by recovered
 * Game_spawnPlayer() before facing/setup/tile-enter side effects begin.
 *
 * int32_t mirrors the legacy DoomCanvas/Render field width instead of
 * prematurely narrowing gameplay coordinates. The HUD/facing/setup/tile-enter
 * bits are explicit semantic follow-ups; this owner does not touch legacy
 * DoomCanvas, Render, Hud, Game or Player state.
 */
typedef struct EspPlayerViewState_s {
    int32_t viewX;
    int32_t viewY;
    int32_t viewZ;
    int32_t viewAngle;
    int32_t destX;
    int32_t destY;
    int32_t destAngle;
    int32_t viewZOld;

    uint8_t targetMapId;
    uint8_t gameplayLoadMapId;
    uint8_t loadType;
    uint8_t spawnApplied;
    uint8_t hudRefreshPending;
    uint8_t facingRefreshPending;
    uint8_t playerSetupPending;
    uint8_t tileEnterPending;
    uint8_t active;
    uint8_t reserved[3];
} EspPlayerViewState;

void EspPlayerView_reset(void);
int EspPlayerView_isReady(void);
const EspPlayerViewState* EspPlayerView_view(void);

EspPlayerViewApplyStatus EspPlayerView_applySpawn(
    const EspPlayerSpawnState* spawn);

int EspPlayerView_consumeHudRefresh(uint8_t targetMapId,
                                    uint8_t gameplayLoadMapId,
                                    uint8_t loadType);
int EspPlayerView_consumePlayerSetup(uint8_t targetMapId,
                                     uint8_t gameplayLoadMapId,
                                     uint8_t loadType);
int EspPlayerView_consumeTileEnter(uint8_t targetMapId,
                                   uint8_t gameplayLoadMapId,
                                   uint8_t loadType);

/*
 * Consume the final durable checkFacingEntity responsibility after both
 * finishRotation tile/orientation boundaries are complete. This clears only
 * facingRefreshPending. HUD/setup/tile-enter must already be fully consumed.
 */
int EspPlayerView_consumeFacing(uint8_t targetMapId,
                                uint8_t gameplayLoadMapId,
                                uint8_t loadType);

#ifdef __cplusplus
}
#endif

#endif
