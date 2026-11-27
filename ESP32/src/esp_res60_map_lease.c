#include "platform_video_config.h"
#if DOOMRPG_RES60_P13_MAP_LEASE
#include <stdint.h>
#include <string.h>
#include "esp_asset_pack.h"
#include "esp_map_runtime.h"
#include "esp_res60_inventory_cache.h"
#include "esp_res60_map_lease.h"
#if DOOMRPG_RES60_P28_DESCRIPTOR_LEASE
#include "esp_native_plane_renderer.h"
#endif

/* No new frame/world buffer: only the original EspMapRuntime arena survives.
 * The owner bit cannot outlive the compact runtime after MENU/START/failure. */
static uint8_t leaseOwned;

static uint32_t hashArena(const uint8_t* bytes, uint32_t length) {
    uint32_t hash = 2166136261U;
    uint32_t i;
    if (bytes == NULL || length == 0U) return 0U;
    for (i = 0U; i < length; ++i) {
        hash = (hash ^ bytes[i]) * 16777619U;
    }
    return hash;
}

static int matches(const EspBspInventory* inventory) {
    const EspMapRuntimeView* view = EspMapRuntime_view();
    return inventory != NULL && view != NULL &&
           view->arena != NULL && view->arenaBytes > 0U &&
           view->arenaBytes == inventory->plan.persistentBytes &&
           view->sourceBytes == inventory->sourceBytes &&
           view->sourceCrc32 == inventory->crc32 &&
           view->nodeCount == inventory->nodes &&
           view->lineCount == inventory->lines &&
           view->arenaFNV1a != 0U &&
           hashArena(view->arena, view->arenaBytes) == view->arenaFNV1a;
}

static int packedEntryMatches(const char* name,
                              const EspBspInventory* inventory, int keepOpen) {
    EspAssetPackEntry entry;
    int ok;
    if (EspAssetPack_isOpen() ||
        !EspAssetPack_open(ESP_ASSET_PACK_DEFAULT_PATH)) return 0;
    memset(&entry, 0, sizeof(entry));
    ok = EspAssetPack_findEntry(name, &entry) &&
         entry.size == inventory->sourceBytes &&
         entry.crc32 == inventory->crc32 &&
         (entry.flags & ESP_ASSET_PACK_FLAG_DIRECTORY) == 0U;
    if (ok && keepOpen) return EspAssetPack_isOpen();
    EspAssetPack_close();
    return ok && !EspAssetPack_isOpen();
}

void EspRes60MapLease_reset(void) {
    /* This probe owns the runtime only when P13 is enabled. Never borrow a
     * pointer to a renderer/scene/legacy world; reset is a session boundary. */
#if DOOMRPG_RES60_P28_DESCRIPTOR_LEASE
    EspNativePlaneRenderer_resetRes60Metadata();
#endif
    EspMapRuntime_reset();
    leaseOwned = 0U;
}

int EspRes60MapLease_isReady(void) {
    return leaseOwned && EspMapRuntime_isLoaded() && !EspAssetPack_isOpen();
}

static int acquireCore(const char* name,
                       const EspBspInventory* inventory,
                       uint8_t inventoryHit, uint8_t* leaseHit,
                       int keepOpen) {
    if (leaseHit != NULL) *leaseHit = 0U;
    if (name == NULL || strcmp(name, "/intro.bsp") != 0 ||
        inventory == NULL || leaseHit == NULL ||
        inventoryHit > 1U || inventory->sourceBytes == 0U ||
        inventory->crc32 == 0U ||
        inventory->expectedCrc32 != inventory->crc32 ||
        inventory->plan.persistentBytes == 0U ||
        EspAssetPack_isOpen()) return 0;

    if (inventoryHit) {
        /* Failure is never an implicit new map load. In-session mutation,
         * cache/header mismatch, changed PAK identity: all fail closed. */
        if (!EspRes60MapLease_isReady() ||
            !matches(inventory) ||
            !EspRes60InventoryCache_verify(name, inventory,
                EspMapRuntime_view()->arenaFNV1a) ||
            !packedEntryMatches(name, inventory, keepOpen)) {
            EspRes60MapLease_reset();
            return 0;
        }
        *leaseHit = 1U;
        return 1;
    }

    if (leaseOwned || EspMapRuntime_isLoaded()) return 0;
    /* The cold owner still validates and builds exactly the real native PAK
     * compact runtime. The map's 14KiB arena becomes a single retained lease. */
    if (!EspMapRuntime_loadPackEntry(name, inventory) ||
        !matches(inventory)) {
        EspRes60MapLease_reset();
        return 0;
    }
    leaseOwned = 1U;
    return 1;
}
int EspRes60MapLease_acquire(const char* name,
                            const EspBspInventory* inventory,
                            uint8_t inventoryHit, uint8_t* leaseHit) {
    return acquireCore(name, inventory, inventoryHit, leaseHit, 0);
}

int EspRes60MapLease_acquireForPlanes(const char* name,
                            const EspBspInventory* inventory,
                            uint8_t inventoryHit, uint8_t* leaseHit) {
    /* Success + HIT transfers one open PAK to the caller. A MISS and
     * every failure keep the original closed-owner contract. */
    return acquireCore(name, inventory, inventoryHit, leaseHit, 1);
}
#endif /* isolated P13 profile */
