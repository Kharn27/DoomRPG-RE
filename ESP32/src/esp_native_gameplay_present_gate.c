#include <stdint.h>

#include "esp_native_gameplay_present_gate.h"

static uint8_t gateArmed;
static unsigned int suppressedCount;

/* The action layer is a leaf behind this permanent gate.  Keeping the gate as
 * the sole linker wrapper preserves the already-validated one-shot suppression
 * contract while allowing Action feedback to decorate a frame immediately
 * before the physical present. */
extern int EspNativeGameplayActionEngine_present(void);

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
    return EspNativeGameplayActionEngine_present();
}
