#ifndef DOOMRPG_ESP32_MAP_FACING_INDEX_H
#define DOOMRPG_ESP32_MAP_FACING_INDEX_H

#include <stdint.h>

#include "esp_asset_pack.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ESP_MAP_FACING_LINE_NO_ENTITY 0xffU
#define ESP_MAP_FACING_DEFAULT_WALL_TYPE 9U

typedef struct EspMapFacingIndexView_s {
    const uint8_t* lineEntityTypes;
    uint32_t lineCount;
    uint32_t storageBytes;
    uint32_t stateFNV1a;
    uint32_t lineEntityCount;
    uint32_t entityDefCount;
} EspMapFacingIndexView;

/*
 * Small map-derived gameplay index for the line entities synthesized by
 * recovered Game_loadMapEntities(). One byte per immutable line records only
 * the EntityDef eType used by Game_trace(); 0xff means the legacy loader would
 * not create a line entity for that line.
 *
 * The caller owns the PAK session for buildFromRuntime(). Query-time access is
 * allocation-free and performs no PAK I/O. This derived owner is deliberately
 * outside EspMapResidentSnapshot so the seven hardware-proven core map owners
 * and their existing payload/FNV canons remain stable.
 */
void EspMapFacingIndex_reset(void);
int EspMapFacingIndex_buildFromRuntime(const EspAssetPackEntry* entityDefsEntry);
int EspMapFacingIndex_isReady(void);
const EspMapFacingIndexView* EspMapFacingIndex_view(void);
int EspMapFacingIndex_getLineEntityType(uint32_t lineIndex, uint8_t* outType);

#ifdef __cplusplus
}
#endif

#endif
