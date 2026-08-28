#include "esp_native_gameplay_interaction_inventory.h"
#include "esp_native_gameplay_pickup.h"

void __real_EspNativeGameplayPickup_logCorpus(void);

void __wrap_EspNativeGameplayPickup_logCorpus(void) {
    __real_EspNativeGameplayPickup_logCorpus();
    EspNativeGameplayInteractionInventory_log();
}
