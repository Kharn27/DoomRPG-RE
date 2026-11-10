#ifndef ESP_NATIVE_ROOT_STATE_H
#define ESP_NATIVE_ROOT_STATE_H

#include "DoomRPG.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Process-lifetime, non-world state. No root pointer, heap buffer or RNG. */
void EspNativeImageResources_resetAccounting(void);
int EspNativeImageResources_accountedBytes(void);
void EspNativePresentation_resetClipState(void);
boolean EspNativePresentation_clipEnabled(void);

#ifdef __cplusplus
}
#endif
#endif
