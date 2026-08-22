#ifndef DOOMRPG_ESP32_MAP_LINE_TEXTURE_STATE_H
#define DOOMRPG_ESP32_MAP_LINE_TEXTURE_STATE_H

#include <stdint.h>

#include "esp_map_line_state.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ESP_MAP_OPCODE_UNLOCK 13U

#define ESP_MAP_LINE_TEXTURE_LOCKED 9U
#define ESP_MAP_LINE_TEXTURE_UNLOCKED 10U
#define ESP_MAP_LINE_UNLOCK_SOUND 5067U
#define ESP_MAP_LINE_UNLOCK_ENTITY_SUBTYPE 2U

#define ESP_MAP_LINE_UNLOCK_EFFECT_PLAY_SOUND              0x01U
#define ESP_MAP_LINE_UNLOCK_EFFECT_SPECIAL_ENTITY_DEF_SYNC 0x02U
#define ESP_MAP_LINE_UNLOCK_EFFECT_REFRESH_IF_ENTITY       0x04U
#define ESP_MAP_LINE_UNLOCK_EFFECT_ALL                     0x07U

typedef struct EspMapLineTextureStateView_s {
    const uint8_t* texture10Bits;
    uint32_t lineCount;
    uint32_t bitsetBytes;
    uint32_t storageBytes;
    uint32_t stateFNV1a;
    uint32_t variantCount;
    uint32_t texture10Count;
} EspMapLineTextureStateView;

typedef enum EspMapLineUnlockStatus_e {
    ESP_MAP_LINE_UNLOCK_INVALID = 0,
    ESP_MAP_LINE_UNLOCK_UNSUPPORTED = 1,
    ESP_MAP_LINE_UNLOCK_NOT_READY = 2,
    ESP_MAP_LINE_UNLOCK_LINE_OUT_OF_RANGE = 3,
    ESP_MAP_LINE_UNLOCK_OK = 4
} EspMapLineUnlockStatus;

typedef struct EspMapLineUnlockResult_s {
    uint16_t sourceEventIndex;
    uint16_t globalCommandIndex;
    uint16_t lineIndex;
    uint16_t soundId;
    uint16_t textureBefore;
    uint16_t textureAfter;
    uint8_t sourceCommandOffset;
    uint8_t lockedBefore;
    uint8_t lockedAfter;
    uint8_t lockMutated;
    uint8_t textureMutated;
    uint8_t effectFlags;
    uint8_t legacyReturnValue;
    uint8_t removeCommandIfHandled;
} EspMapLineUnlockResult;

/*
 * Own only the mutable 9/10 lock-door texture variant. The immutable source
 * texture remains in EspMapRuntime. One bit per line is sufficient: for source
 * texture 9/10, 0 means current texture 9 and 1 means current texture 10.
 * Non-9/10 lines keep their immutable source texture and consume no extra
 * per-line representation beyond the shared bitset.
 */
void EspMapLineTextureState_reset(void);
int EspMapLineTextureState_buildFromRuntime(void);
int EspMapLineTextureState_isReady(void);
const EspMapLineTextureStateView* EspMapLineTextureState_view(void);
int EspMapLineTextureState_getEffectiveTexture(uint32_t lineIndex,
                                               uint16_t* outTexture);
int EspMapLineTextureState_setDoorTexture(uint32_t lineIndex,
                                          uint16_t texture);

/*
 * Execute only 13 / EV_UNLOCK against native world owners.
 *
 * Legacy Game_setLineLocked(false,false) always clears the lock bit. If the
 * current line texture is 9 it also becomes 10, plays sound 5067 and updates
 * the matching special line entity definition/view if such an entity exists.
 * This native executor owns only the lock + texture world mutations. Sound and
 * special-entity/view work are returned as deferred effect metadata.
 *
 * Unlike OPEN/CLOSE, a valid UNLOCK is handled even when it causes no mutation;
 * legacyReturnValue is therefore 1 for every ESP_MAP_LINE_UNLOCK_OK result and
 * removeCommandIfHandled mirrors the outer Game_runEvent() arg2 & 0x200 rule.
 */
EspMapLineUnlockStatus EspMapLineTextureState_applyUnlockCommand(
    const EspMapEventDescriptor* descriptor,
    uint32_t commandOffset,
    EspMapLineUnlockResult* outResult);

#ifdef __cplusplus
}
#endif

#endif
