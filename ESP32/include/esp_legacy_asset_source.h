#ifndef DOOMRPG_ESP32_LEGACY_ASSET_SOURCE_H
#define DOOMRPG_ESP32_LEGACY_ASSET_SOURCE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Transitional source bridge for the remaining desktop-derived startup/menu
 * code. The authoritative bytes come only from DoomRPG-ESP32.pak through
 * EspAssetPack. No ZIP parsing, decompression, or runtime fallback is allowed.
 *
 * Every call owns one short logical PAK lease and fails closed if another
 * logical lease is already active.
 */
int EspLegacyAssetSource_validate(uint32_t* outEntryCount);
int EspLegacyAssetSource_stat(const char* name, uint32_t* outSize);
uint8_t* EspLegacyAssetSource_readAlloc(const char* name, int* outSize);
int EspLegacyAssetSource_readInto(const char* name,
                                  void* destination,
                                  size_t capacity,
                                  int* outSize);

#ifdef __cplusplus
}
#endif

#endif
