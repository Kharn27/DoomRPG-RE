#ifndef ESP_NATIVE_ROOT_STATE_H
#define ESP_NATIVE_ROOT_STATE_H

#include "DoomRPG.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Three true process-lifetime object owners, not a map/session mirror.
 * This API owns identities only: allocations retain their existing bounded
 * constructors/destructors and the shared RGB565 framebuffer is never freed. */
typedef struct EspNativeRootOwners_s {
    struct DoomCanvas_s* doomCanvas;
    struct Render_s* render;
    struct EspNativeMenuState_s* menuSystem;
} EspNativeRootOwners_t;

EspNativeRootOwners_t* EspNativeRootOwners_live(void);

/* Process-lifetime, non-world state. No root pointer, heap buffer or RNG. */
void EspNativeImageResources_resetAccounting(void);
int EspNativeImageResources_accountedBytes(void);
void EspNativePresentation_resetClipState(void);
boolean EspNativePresentation_clipEnabled(void);

#ifdef __cplusplus
}
#endif
#endif
