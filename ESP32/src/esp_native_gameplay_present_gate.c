#include <stdint.h>

#include "esp_native_gameplay_present_gate.h"

static uint8_t gateArmed;
static unsigned int suppressedCount;

extern int __real_Esp32PlatformVideo_present(void);

int EspNativeGameplayPresentGate_armOne(void) {
    if (gateArmed != 0U) return 0;
    gateArmed = 1U;
    return 1;
}

void EspNativeGameplayPresentGate_cancel(void) {
    gateArmed = 0U;
}

int EspNativeGameplayPresentGate_isArmed(void) {
    return gateArmed != 0U;
}

unsigned int EspNativeGameplayPresentGate_suppressedCount(void) {
    return suppressedCount;
}

int __wrap_Esp32PlatformVideo_present(void) {
    if (gateArmed != 0U) {
        gateArmed = 0U;
        ++suppressedCount;
        return 1;
    }
    return __real_Esp32PlatformVideo_present();
}
