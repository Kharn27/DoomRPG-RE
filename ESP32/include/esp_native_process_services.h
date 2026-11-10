#ifndef DOOMRPG_ESP32_NATIVE_PROCESS_SERVICES_H
#define DOOMRPG_ESP32_NATIVE_PROCESS_SERVICES_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* SDL-F1: process-global 32-bit Arduino tick source. No new epoch or timer. */
uint32_t EspNativeProcess_ticksMs(void);

/* The fatal error's already-formatted bounded stack message is logged to
 * the same Serial channel as the former SDL_ShowMessageBox adapter.
 * This does not own the subsequent DoomRPG cleanup or process exit. */
void EspNativeProcess_fatalMessage(const char* title, const char* message);

#ifdef __cplusplus
}
#endif
#endif
