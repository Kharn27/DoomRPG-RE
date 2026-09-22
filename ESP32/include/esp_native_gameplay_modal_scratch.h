#ifndef DOOMRPG_ESP32_NATIVE_GAMEPLAY_MODAL_SCRATCH_H
#define DOOMRPG_ESP32_NATIVE_GAMEPLAY_MODAL_SCRATCH_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ESP_NATIVE_GAMEPLAY_MODAL_SCRATCH_PASSWORD 2U

/*
 * Lease the already-existing lazy NOTE owner's transient candidate+scratch
 * region to another mutually-exclusive modal. This adds no new BSS pointer.
 * Persistent notebook state is outside the leased span and remains untouched.
 */
void* EspNativeGameplayModalScratch_acquire(uint8_t owner, size_t bytes);
void* EspNativeGameplayModalScratch_view(uint8_t owner);
int EspNativeGameplayModalScratch_release(uint8_t owner, void* storage);

#ifdef __cplusplus
}
#endif

#endif
