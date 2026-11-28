#ifndef DOOMRPG_ESP32_RES60_INVENTORY_CACHE_H
#define DOOMRPG_ESP32_RES60_INVENTORY_CACHE_H

#include "platform_video_config.h"
#if DOOMRPG_RES60_P12_INVENTORY_CACHE
#include <stdint.h>
#include "esp_bsp_reader.h"

#ifdef __cplusplus
extern "C" {
#endif

/* RES60-P12 only. Snapshot of the verified MAP_INTRO inventory, NOT an
 * ownership lease on the PAK or the compact map. Cold reads use the original
 * reader; on a hit the caller still loads/validates the compact arena from
 * the real native PAK for this turn. No data pointers are stored here. */
int EspRes60InventoryCache_read(const char* mapName,
                                EspBspInventory* inventory,
                                uint8_t* cacheHit);

/* Verify the reconstructed compact arena FNV before rendering a warm hit.
 * Must be called after EspMapRuntime_loadPackEntry() (which independently
 * checks the real PAK entry CRC and size on EVERY rotation). */
int EspRes60InventoryCache_verify(const char* mapName,
                                  const EspBspInventory* inventory,
                                  uint32_t arenaFNV);

/* Publish ONLY after successful render + release + closed PAK. */
int EspRes60InventoryCache_commit(const char* mapName,
                                  const EspBspInventory* inventory,
                                  uint32_t arenaFNV);

/* Explicit ownership reset at new START, successful MENU and any failure. */
void EspRes60InventoryCache_reset(void);

#ifdef __cplusplus
}
#endif
#endif /* P12 */
#endif
