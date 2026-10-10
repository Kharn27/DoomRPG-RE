#ifndef DOOMRPG_ESP32_NATIVE_GAMEPLAY_STORE_H
#define DOOMRPG_ESP32_NATIVE_GAMEPLAY_STORE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Pure native EV_OPENSTORE modal: no legacy MenuSystem, no map-wide storage. */
void EspNativeGameplayStore_reset(void);
int EspNativeGameplayStore_begin(uint8_t storeId,
                                  uint16_t eventIndex,
                                  uint8_t commandOffset);
int EspNativeGameplayStore_isActive(void);
int EspNativeGameplayStore_closePending(void);
void EspNativeGameplayStore_finishClose(void);
/* Touch is completely modal: 0 ignored, 1 repainted, 2 close requested,
 * -1 internal failure. Never forwards touches to gameplay. */
int EspNativeGameplayStore_handleTap(int logicalX, int logicalY);

#ifdef __cplusplus
}
#endif

#endif
