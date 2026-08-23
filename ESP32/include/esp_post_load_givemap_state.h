#ifndef DOOMRPG_ESP32_POST_LOAD_GIVEMAP_STATE_H
#define DOOMRPG_ESP32_POST_LOAD_GIVEMAP_STATE_H

#include <stdint.h>

#include "esp_hud_post_load_clear_state.h"
#include "esp_map_automap_state.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum EspPostLoadGiveMapStatus_e {
    ESP_POST_LOAD_GIVEMAP_INVALID = 0,
    ESP_POST_LOAD_GIVEMAP_HUD_CLEAR_INVALID = 1,
    ESP_POST_LOAD_GIVEMAP_UNSUPPORTED_CONTEXT = 2,
    ESP_POST_LOAD_GIVEMAP_UNSUPPORTED_ORDER = 3,
    ESP_POST_LOAD_GIVEMAP_WORLD_NOT_READY = 4,
    ESP_POST_LOAD_GIVEMAP_APPLY_FAILED = 5,
    ESP_POST_LOAD_GIVEMAP_ALREADY_ACTIVE = 6,
    ESP_POST_LOAD_GIVEMAP_OK = 7
} EspPostLoadGiveMapStatus;

/*
 * Compact caller-order marker for the direct Junction Game_givemap() that
 * immediately follows the hardware-proven post-load HUD clear. The durable
 * mutation itself remains owned by EspMapAutomapState + EspMapState; this state
 * only records the exact target/mutation counts and map identity needed by the
 * next caller-side milestone.
 */
typedef struct EspPostLoadGiveMapState_s {
    uint16_t lineTargetCount;
    uint16_t spriteTargetCount;
    uint16_t entranceTargetCount;
    uint16_t linesMutated;
    uint16_t spritesMutated;
    uint16_t tilesMutated;
    uint8_t targetMapId;
    uint8_t gameplayLoadMapId;
    uint8_t loadType;
    uint8_t active;
} EspPostLoadGiveMapState;

void EspPostLoadGiveMap_reset(void);
int EspPostLoadGiveMap_isReady(void);
const EspPostLoadGiveMapState* EspPostLoadGiveMap_view(void);

/*
 * Pure preflight of the currently proven fresh Junction caller path. Requires
 * the exact post-load HUD-clear owner and the untouched Junction native world.
 * Invalid/refused input zeroes outState and performs no mutation.
 */
EspPostLoadGiveMapStatus EspPostLoadGiveMap_prepare(
    const EspHudPostLoadClearState* hudClear,
    EspPostLoadGiveMapState* outState);

/*
 * Execute direct native Game_givemap exactly once and park the marker. No
 * legacy Game/Render/Hud/Player/DoomCanvas state is touched and ST_PLAYING stays
 * outside this boundary.
 */
EspPostLoadGiveMapStatus EspPostLoadGiveMap_route(void);

#ifdef __cplusplus
}
#endif

#endif
