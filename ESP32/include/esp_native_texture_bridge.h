#ifndef DOOMRPG_ESP32_NATIVE_TEXTURE_BRIDGE_H
#define DOOMRPG_ESP32_NATIVE_TEXTURE_BRIDGE_H

#include "esp_native_texture.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Permanent native indexed-texture adoption contract, without SDL types.
 * On success pixels and palette move with no memcpy; on failure the caller
 * retains both allocations. Desktop Image_t keeps its historical ABI. */
EspNativeTexture* EspNativeTexture_adoptIndexed(EspNativeIndexedImage* image);

#ifdef __cplusplus
}
#endif
#endif
