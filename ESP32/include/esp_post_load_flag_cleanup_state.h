#ifndef DOOMRPG_ESP32_POST_LOAD_FLAG_CLEANUP_STATE_H
#define DOOMRPG_ESP32_POST_LOAD_FLAG_CLEANUP_STATE_H

#include <stdint.h>

#include "esp_post_load_initial_save_intent.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum EspPostLoadFlagCleanupStatus_e {
    ESP_POST_LOAD_FLAG_CLEANUP_INVALID = 0,
    ESP_POST_LOAD_FLAG_CLEANUP_SAVE_INTENT_INVALID = 1,
    ESP_POST_LOAD_FLAG_CLEANUP_UNSUPPORTED_CONTEXT = 2,
    ESP_POST_LOAD_FLAG_CLEANUP_WORLD_NOT_READY = 3,
    ESP_POST_LOAD_FLAG_CLEANUP_ALREADY_ACTIVE = 4,
    ESP_POST_LOAD_FLAG_CLEANUP_OK = 5
} EspPostLoadFlagCleanupStatus;

/*
 * Exact caller-order owner for the three scalar Game bookkeeping writes that
 * immediately follow the fresh-map Game_saveState() call:
 *
 *   game->isLoaded = false;
 *   game->isSaved = false;
 *   game->activeLoadType = 0;
 *
 * The permanent ESP32 path records the semantic before/after values only. It
 * does not mutate legacy Game_t; the temporary hardware probe supplies the
 * three incoming scalars read-only until a broader native load-state owner
 * exists.
 */
typedef struct EspPostLoadFlagCleanupState_s {
    uint8_t isLoadedBefore;
    uint8_t isSavedBefore;
    uint8_t activeLoadTypeBefore;
    uint8_t isLoadedAfter;
    uint8_t isSavedAfter;
    uint8_t activeLoadTypeAfter;
    uint8_t targetMapId;
    uint8_t active;
} EspPostLoadFlagCleanupState;

void EspPostLoadFlagCleanup_reset(void);
int EspPostLoadFlagCleanup_isReady(void);
const EspPostLoadFlagCleanupState* EspPostLoadFlagCleanup_view(void);

EspPostLoadFlagCleanupStatus EspPostLoadFlagCleanup_prepare(
    const EspPostLoadInitialSaveIntentState* saveIntent,
    uint8_t isLoadedBefore,
    uint8_t isSavedBefore,
    uint8_t activeLoadTypeBefore,
    EspPostLoadFlagCleanupState* outState);

EspPostLoadFlagCleanupStatus EspPostLoadFlagCleanup_route(
    uint8_t isLoadedBefore,
    uint8_t isSavedBefore,
    uint8_t activeLoadTypeBefore);

#ifdef __cplusplus
}
#endif

#endif
