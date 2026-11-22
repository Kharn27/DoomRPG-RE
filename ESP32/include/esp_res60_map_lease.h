#ifndef DOOMRPG_ESP32_RES60_MAP_LEASE_H
#define DOOMRPG_ESP32_RES60_MAP_LEASE_H
#include <stdint.h>
#include "esp_bsp_reader.h"
#ifdef __cplusplus
extern "C" {
#endif
/* P13 only: one retained compact immutable map runtime per BSP preview
 * session, never an additional arena. The PAK is always CLOSED between views.
 * Revalidation reads exact entry metadata and hashes the resident arena;
 * it never reloads the 14KiB immutable records on a warm turn. */
void EspRes60MapLease_reset(void);
int EspRes60MapLease_isReady(void);
int EspRes60MapLease_acquire(const char* mapName,
                            const EspBspInventory* inventory,
                            uint8_t inventoryHit, uint8_t* leaseHit);
#ifdef __cplusplus
}
#endif
#endif
