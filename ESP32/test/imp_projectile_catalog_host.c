/* Native PAK catalog regression: the Imp's transient sprite is NOT an
 * immutable map sprite, and needs one extra sparse 40-byte resource record.
 * Test the real catalog owner with a tiny deterministic in-memory PAK fixture.
 * No SD, renderer, monster RNG or gameplay allocations are involved. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "esp_asset_pack.h"
#include "esp_map_runtime.h"
#include "esp_native_graphics_catalog.h"

#define PAIR_COUNT 205U
#define MAPPINGS_SIZE (16U + 8U + PAIR_COUNT * 8U)
static uint8_t mappings[MAPPINGS_SIZE];
static uint8_t palettes[4U + 32U];
static uint8_t dummyArena[1];
static EspMapRuntimeView runtime = {
    .arena = dummyArena, .arenaBytes = 1U
};
static int packOpen;
static int rejectImpPair;

static void put32(uint8_t* dst, uint32_t value) {
    dst[0] = (uint8_t)value;
    dst[1] = (uint8_t)(value >> 8);
    dst[2] = (uint8_t)(value >> 16);
    dst[3] = (uint8_t)(value >> 24);
}

static void fixture(void) {
    uint32_t i;
    memset(mappings, 0, sizeof(mappings));
    memset(palettes, 0, sizeof(palettes));
    put32(mappings, 1U);  /* wall mapping pairs */
    put32(mappings + 4U, PAIR_COUNT); /* sprite mapping pairs */
    put32(palettes, 32U);
    for (i = 0U; i < PAIR_COUNT; ++i) {
        put32(mappings + 24U + i * 8U, 0x100U + i);
        put32(mappings + 24U + i * 8U + 4U, 0U);
    }
    put32(mappings + 16U, 0x200U);
    rejectImpPair = 0;
}

int EspMapRuntime_isLoaded(void) { return 1; }
const EspMapRuntimeView* EspMapRuntime_view(void) { return &runtime; }
int EspMapRuntime_textureRequired(uint32_t id) { return id == 0U; }
int EspMapRuntime_planeTextureUsed(uint32_t id) { (void)id; return 0; }
int EspMapRuntime_spriteRequired(uint32_t id) { return id == 3U; }
int EspAssetPack_isOpen(void) { return packOpen; }

int EspAssetPack_open(const char* path) {
    if (packOpen || strcmp(path, ESP_ASSET_PACK_DEFAULT_PATH) != 0) return 0;
    packOpen = 1;
    return 1;
}
void EspAssetPack_close(void) { packOpen = 0; }

int EspAssetPack_findEntry(const char* name, EspAssetPackEntry* out) {
    if (!packOpen || out == NULL) return 0;
    memset(out, 0, sizeof(*out));
    if (strcmp(name, "mappings.bin") == 0) {
        out->nameHash = 1U;
        out->size = (uint32_t)sizeof(mappings);
    }
    else if (strcmp(name, "palettes.bin") == 0) {
        out->nameHash = 2U;
        out->size = (uint32_t)sizeof(palettes);
    }
    else return 0;
    return 1;
}
int EspAssetPack_readRange(const EspAssetPackEntry* entry,
                           uint32_t offset, void* destination, size_t size) {
    const uint8_t* source;
    if (!packOpen || entry == NULL || destination == NULL ||
        offset > entry->size || size > entry->size - offset) return 0;
    if (rejectImpPair && entry->nameHash == 1U &&
        offset == 24U + 204U * 8U) return 0;
    source = entry->nameHash == 1U ? mappings :
             entry->nameHash == 2U ? palettes : NULL;
    if (source == NULL) return 0;
    memcpy(destination, source + offset, size);
    return 1;
}

int main(void) {
    const EspNativeGraphicsCatalogView* view;
    const EspNativeGraphicsCatalogRecord* imp;
    uint32_t fnv;
    uint32_t bytes;

    fixture();
    assert(EspNativeGraphicsCatalog_buildFromRuntime() ==
           ESP_NATIVE_GRAPHICS_CATALOG_OK);
    assert(!packOpen);
    view = EspNativeGraphicsCatalog_view();
    assert(view && view->textureCount == 1U && view->spriteCount == 1U);
    assert(EspNativeGraphicsCatalog_findSprite(3U) != NULL);
    assert(EspNativeGraphicsCatalog_findSprite(204U) == NULL);
    bytes = view->storageBytes;
    fnv = view->stateFNV1a;

    assert(EspNativeGraphicsCatalog_ensureSprite(204U) ==
           ESP_NATIVE_GRAPHICS_CATALOG_OK);
    assert(!packOpen);
    view = EspNativeGraphicsCatalog_view();
    assert(view && view->spriteCount == 2U);
    assert(view->storageBytes == bytes + 40U);
    assert(view->stateFNV1a != fnv);
    assert(view->sprites[0].resourceId == 3U);
    assert(view->sprites[1].resourceId == 204U);
    imp = EspNativeGraphicsCatalog_findSprite(204U);
    assert(imp && imp->sourceOffset == 0x100U + 204U);
    fnv = view->stateFNV1a;
    assert(EspNativeGraphicsCatalog_ensureSprite(204U) ==
           ESP_NATIVE_GRAPHICS_CATALOG_ALREADY_ACTIVE);
    assert(EspNativeGraphicsCatalog_view()->stateFNV1a == fnv);
    assert(EspNativeGraphicsCatalog_ensureSprite(256U) ==
           ESP_NATIVE_GRAPHICS_CATALOG_RESOURCE_UNSUPPORTED);

    EspNativeGraphicsCatalog_reset();
    assert(!EspNativeGraphicsCatalog_isReady());
    assert(EspNativeGraphicsCatalog_buildFromRuntime() ==
           ESP_NATIVE_GRAPHICS_CATALOG_OK);
    rejectImpPair = 1;
    view = EspNativeGraphicsCatalog_view();
    fnv = view->stateFNV1a;
    bytes = view->storageBytes;
    assert(EspNativeGraphicsCatalog_ensureSprite(204U) ==
           ESP_NATIVE_GRAPHICS_CATALOG_READ_FAILED);
    view = EspNativeGraphicsCatalog_view();
    assert(view && view->stateFNV1a == fnv && view->storageBytes == bytes);
    assert(EspNativeGraphicsCatalog_findSprite(204U) == NULL);
    assert(!packOpen);
    EspNativeGraphicsCatalog_reset();
    puts("IMP_PROJECTILE_CATALOG_HOST PASS id=204 sparse=+40B idempotent=yes rollback=yes");
    return 0;
}
