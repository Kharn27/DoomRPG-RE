#ifndef ESP_NATIVE_SFX_PROBE_H
#define ESP_NATIVE_SFX_PROBE_H
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif
/* One-shot real CYD PAK format inventory. Absolutely no audio playback.
 * Called after game service closes logical PAK leases and before SD music. */
void EspNativeSfxProbe_service(bool gameplayActive);
#ifdef __cplusplus
}
#endif
#endif
