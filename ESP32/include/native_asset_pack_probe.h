#ifndef DOOMRPG_ESP32_NATIVE_ASSET_PACK_PROBE_H
#define DOOMRPG_ESP32_NATIVE_ASSET_PACK_PROBE_H

#ifdef __cplusplus
extern "C" {
#endif

/* Hardware probe for the first ESP32-native asset path.
 * Requires the real menu map structures/resource reference lists to be resident
 * and /DoomRPG-ESP32.pak to exist on the already-mounted SD card.
 */
int DoomRPG_probeNativeAssetPack(int resourcePlanReady);

#ifdef __cplusplus
}
#endif

#endif
