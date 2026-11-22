#ifndef ESP_AUDIO_PICKUP_CUE_H
#define ESP_AUDIO_PICKUP_CUE_H
#include <stdint.h>

/* Original src/Entity.c Entity_touched(), eType=5:
 * Axe (subtype 0) and Fire Extinguisher (1) use the generic pickup 5060.
 * Every other acquired weapon (including duplicate acquisitions) uses
 * weapon-ready 5057. World/inventory/ammo pickup routing stays untouched.
 * Pure, bounded and usable by host C with no desktop or audio backend. */
static inline uint16_t EspAudioPickupCue_weapon(uint8_t subtype) {
    return (subtype==0U || subtype==1U) ? 5060U : 5057U;
}
#endif
