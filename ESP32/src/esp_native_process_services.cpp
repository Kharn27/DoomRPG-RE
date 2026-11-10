#include "esp_native_process_services.h"

#include <Arduino.h>

/* Permanent, allocation-free ESP32-native clock/fatal output boundary.
 * The legacy shim's SDL_GetTicks still exists for host/oracle parity,
 * but runtime DoomRPG uses this owner directly. */
extern "C" uint32_t EspNativeProcess_ticksMs(void) {
    return static_cast<uint32_t>(millis());
}

extern "C" void EspNativeProcess_fatalMessage(const char* title,
                                               const char* message) {
    Serial.printf("[DOOM ERROR] %s: %s\n",
                  title == nullptr ? "" : title,
                  message == nullptr ? "" : message);
}
