#ifndef DOOMRPG_ESP32_NATIVE_GAMEPLAY_PRESENT_GATE_H
#define DOOMRPG_ESP32_NATIVE_GAMEPLAY_PRESENT_GATE_H

#ifdef __cplusplus
extern "C" {
#endif

/* Arm exactly one suppressed Esp32PlatformVideo_present() call. This is used
 * only while the gameplay TURN compositor reuses the historical first-frame
 * world route, whose intermediate presentation is redundant. Calls are
 * fail-closed and non-nestable. */
int EspNativeGameplayPresentGate_armOne(void);
void EspNativeGameplayPresentGate_cancel(void);
int EspNativeGameplayPresentGate_isArmed(void);
unsigned int EspNativeGameplayPresentGate_suppressedCount(void);

#ifdef __cplusplus
}
#endif

#endif
