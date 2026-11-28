#include "platform_video_config.h"
#if DOOMRPG_RES60_P12_INVENTORY_CACHE

#include <string.h>
#include "esp_bsp_reader.h"
#include "esp_res60_inventory_cache.h"
#if DOOMRPG_RES60_P35_BSP_WINDOW
#include "esp_res60_world_preview.h"
#endif

/* Only the original Entrance preview is admitted in P12. The inventory is
 * compact scalar metadata + resource-ID bitsets, never a map/texture buffer,
 * and remains owned by this RES60-only module while the map arena is freed. */
static EspBspInventory cachedInventory;
static uint32_t cachedArenaFNV;
static uint8_t cacheValid;

static int introName(const char* name) {
    return name != NULL && strcmp(name, "/intro.bsp") == 0;
}

static int inventorySupported(const EspBspInventory* inv) {
    return inv != NULL && inv->sourceBytes != 0U &&
           inv->crc32 != 0U && inv->crc32 == inv->expectedCrc32 &&
           inv->plan.persistentBytes != 0U &&
           inv->lines > 0U && inv->lines <= 512U &&
           inv->spawnIndex < 1024U &&
           (inv->spawnDirection & 63U) == 0U;
}

void EspRes60InventoryCache_reset(void) {
    cacheValid = 0U;
    cachedArenaFNV = 0U;
    memset(&cachedInventory, 0, sizeof(cachedInventory));
}

int EspRes60InventoryCache_read(const char* mapName,
                                EspBspInventory* inventory,
                                uint8_t* cacheHit) {
    if (cacheHit != NULL) *cacheHit = 0U;
    if (!introName(mapName) || inventory == NULL || cacheHit == NULL)
        return 0;
    if (cacheValid) {
        if (!inventorySupported(&cachedInventory) || cachedArenaFNV == 0U)
            return 0;
        *inventory = cachedInventory;
        *cacheHit = 1U;
        return 1;
    }
#if DOOMRPG_RES60_P35_BSP_WINDOW
    /* Cold MISS only; the snapshot will be repopulated before raycast.
     * Release the same exclusive P29 loan on every acquired path. */
    size_t bytes=0U;
    void* scratch=EspRes60Preview_borrowPlaneScratch(&bytes);
    if (scratch==NULL) return 0;
    int ok=bytes>=2048U &&
        EspBspReader_inventoryPackEntryWithScratch(mapName,inventory,scratch,2048U);
    EspRes60Preview_returnPlaneScratch();
    return ok;
#else
    return EspBspReader_inventoryPackEntry(mapName, inventory);
#endif
}

int EspRes60InventoryCache_verify(const char* mapName,
                                  const EspBspInventory* inventory,
                                  uint32_t arenaFNV) {
    if (!introName(mapName) || !inventorySupported(inventory) ||
        arenaFNV == 0U) return 0;
    if (!cacheValid) return 1;
    return cachedArenaFNV == arenaFNV &&
           inventory->crc32 == cachedInventory.crc32 &&
           inventory->sourceBytes == cachedInventory.sourceBytes &&
           inventory->plan.persistentBytes ==
               cachedInventory.plan.persistentBytes &&
           inventory->nodes == cachedInventory.nodes &&
           inventory->lines == cachedInventory.lines &&
           inventory->spawnIndex == cachedInventory.spawnIndex &&
           inventory->spawnDirection == cachedInventory.spawnDirection;
}

int EspRes60InventoryCache_commit(const char* mapName,
                                  const EspBspInventory* inventory,
                                  uint32_t arenaFNV) {
    if (!EspRes60InventoryCache_verify(mapName, inventory, arenaFNV))
        return 0;
    cachedInventory = *inventory;
    cachedArenaFNV = arenaFNV;
    cacheValid = 1U;
    return 1;
}
#endif /* P12: production never stores the inventory snapshot */
