#ifndef DOOMRPG_ESP32_NATIVE_ASSET_PACK_PROBE_H
#define DOOMRPG_ESP32_NATIVE_ASSET_PACK_PROBE_H

#ifdef __cplusplus
extern "C" {
#endif

/* Hardware probe for the ESP32-native full asset-pack path.
 * Requires the real menu map structures/resource reference lists to be resident,
 * the original ZIP directory to be indexed for cross-checking, and a v2
 * /DoomRPG-ESP32.pak on the already-mounted SD card.
 */
int DoomRPG_probeNativeAssetPack(int resourcePlanReady);

#ifdef __cplusplus
}
#endif

#endif
