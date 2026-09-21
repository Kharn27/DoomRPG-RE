#ifndef DOOMRPG_ESP32_MAP_LINE_CHECKPOINT_H
#define DOOMRPG_ESP32_MAP_LINE_CHECKPOINT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ESP_MAP_LINE_CHECKPOINT_MAX_LINES 1024U
#define ESP_MAP_LINE_CHECKPOINT_MAX_BYTES \
    ((ESP_MAP_LINE_CHECKPOINT_MAX_LINES + 7U) >> 3)

typedef struct EspMapLineCheckpointSnapshot_s {
    uint32_t sourceArenaFNV1a;
    uint32_t lineStateFNV1a;
    uint32_t textureStateFNV1a;
    uint32_t lineCount;
    uint16_t bitsetBytes;
    uint16_t reserved0;
    uint8_t openBits[ESP_MAP_LINE_CHECKPOINT_MAX_BYTES];
    uint8_t lockedBits[ESP_MAP_LINE_CHECKPOINT_MAX_BYTES];
    uint8_t texture10Bits[ESP_MAP_LINE_CHECKPOINT_MAX_BYTES];
} EspMapLineCheckpointSnapshot;

/* Pure record-shape validation. This does not require a resident map and is
 * safe to use while validating an on-disk checkpoint before world rebuild. */
int EspMapLineCheckpoint_shapeValid(
    const EspMapLineCheckpointSnapshot* snapshot,
    uint32_t expectedArenaFNV1a);

/* Snapshot/restore only the compact mutable line-family owners:
 * - open bit per line;
 * - locked bit per line;
 * - texture-10 variant bit for immutable source textures 9/10.
 *
 * No immutable BSP line data, renderer state, topology objects or allocations
 * are serialized. Restore mutates the already rebuilt owners in place. */
int EspMapLineCheckpoint_snapshot(EspMapLineCheckpointSnapshot* outSnapshot);
int EspMapLineCheckpoint_restore(
    const EspMapLineCheckpointSnapshot* snapshot);

#ifdef __cplusplus
}
#endif

#endif
