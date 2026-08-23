#ifndef DOOMRPG_ESP32_MAP_AUTOMAP_STATE_H
#define DOOMRPG_ESP32_MAP_AUTOMAP_STATE_H

#include <stdint.h>

#include "esp_map_events.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ESP_MAP_OPCODE_GIVEMAP 9U

#define ESP_MAP_LINE_FLAG_NO_AUTOMAP 0x00000020UL
#define ESP_MAP_LINE_FLAG_AUTOMAP_REVEALED 0x00000080UL
#define ESP_MAP_SPRITE_INFO_AUTOMAP_REVEALED 0x10000000UL
#define ESP_MAP_GIVEMAP_COMMAND_FLAG_REMOVE 0x00000200UL

typedef struct EspMapAutomapStateView_s {
    const uint8_t* lineRevealedBits;
    const uint8_t* spriteRevealedBits;
    uint32_t lineCount;
    uint32_t spriteCount;
    uint32_t lineBitsetBytes;
    uint32_t spriteBitsetBytes;
    uint32_t storageBytes;
    uint32_t stateFNV1a;
    uint32_t lineRevealedCount;
    uint32_t spriteRevealedCount;
} EspMapAutomapStateView;

typedef enum EspMapGiveMapStatus_e {
    ESP_MAP_GIVEMAP_INVALID = 0,
    ESP_MAP_GIVEMAP_UNSUPPORTED = 1,
    ESP_MAP_GIVEMAP_NOT_READY = 2,
    ESP_MAP_GIVEMAP_OK = 3
} EspMapGiveMapStatus;

/*
 * Event-independent Game_givemap semantic result. This is deliberately smaller
 * than EspMapGiveMapResult: caller-side Game_givemap has no source event,
 * command index or remove-if-handled metadata.
 */
typedef struct EspMapGiveMapDirectResult_s {
    uint16_t lineTargetCount;
    uint16_t spriteTargetCount;
    uint16_t entranceTargetCount;
    uint16_t linesMutated;
    uint16_t spritesMutated;
    uint16_t tilesMutated;
} EspMapGiveMapDirectResult;

typedef struct EspMapGiveMapResult_s {
    uint16_t sourceEventIndex;
    uint16_t globalCommandIndex;
    uint16_t lineTargetCount;
    uint16_t spriteTargetCount;
    uint16_t entranceTargetCount;
    uint16_t linesMutated;
    uint16_t spritesMutated;
    uint16_t tilesMutated;
    uint8_t sourceCommandOffset;
    uint8_t mutated;
    uint8_t legacyReturnValue;
    uint8_t removeCommandIfHandled;
} EspMapGiveMapResult;

/*
 * Compact mutable automap reveal owner for the immutable map runtime.
 * One bit per line mirrors source line flag 0x80; one bit per map sprite mirrors
 * source sprite info bit 0x10000000. Tile BIT_AM_VISITED remains owned by the
 * existing EspMapState instead of being duplicated here.
 */
void EspMapAutomapState_reset(void);
int EspMapAutomapState_buildFromRuntime(void);
int EspMapAutomapState_isReady(void);
const EspMapAutomapStateView* EspMapAutomapState_view(void);
int EspMapAutomapState_getLineRevealed(uint32_t lineIndex,
                                       uint8_t* outRevealed);
int EspMapAutomapState_setLineRevealed(uint32_t lineIndex,
                                       uint8_t revealed);
int EspMapAutomapState_getSpriteRevealed(uint32_t spriteIndex,
                                         uint8_t* outRevealed);
int EspMapAutomapState_setSpriteRevealed(uint32_t spriteIndex,
                                         uint8_t revealed);

/*
 * Event-independent Game_givemap primitive.
 *
 * planGiveMapDirect() is pure and reports both total targets and how many
 * mutations the current native owners would require. applyGiveMapDirect()
 * performs exactly those three recovered legacy effects:
 *   - reveal every line without flag 0x20,
 *   - reveal every map sprite,
 *   - mark every BIT_AM_ENTRANCE tile BIT_AM_VISITED.
 *
 * Neither function touches renderer/entity/UI/presentation state or allocates.
 */
EspMapGiveMapStatus EspMapAutomapState_planGiveMapDirect(
    EspMapGiveMapDirectResult* outResult);
EspMapGiveMapStatus EspMapAutomapState_applyGiveMapDirect(
    EspMapGiveMapDirectResult* outResult);

/*
 * Execute only 9 / EV_GIVEMAP.
 *
 * The event wrapper validates canonical descriptor/opcode/removal metadata and
 * then delegates the world mutation to the same direct Game_givemap primitive.
 * A valid command is handled even when every target is already revealed.
 */
EspMapGiveMapStatus EspMapAutomapState_applyGiveMapCommand(
    const EspMapEventDescriptor* descriptor,
    uint32_t commandOffset,
    EspMapGiveMapResult* outResult);

#ifdef __cplusplus
}
#endif

#endif
